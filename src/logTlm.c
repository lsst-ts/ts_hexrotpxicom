#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>

#include "utility.h"
#include "logTlm.h"

// File name
static char *filename = "";

// Pointer of the file
static FILE *pFile = NULL;

// Pointer of the buffer
static void *pBuffer = NULL;

// Size of each element in buffer
static size_t sizeElementInBuffer = 0;

// Counter of the rotating file
static int countRotatingFile = 0;

// Maximum count per file
static int countFileMax = 0;

// Current count of the file
static int countFile = 0;

// Maximum count of the buffer
static int countBufferMax = 0;

// Current count in the buffer
static int countBufferCurrent = 0;

// Mutex lock
static pthread_mutex_t lock;

// Data in buffer is flushing to the file or not
static bool isFlushing = false;

// Thread to flush the data to file
static pthread_t thread;

// Thread is ready or not
static bool isThreadReady = false;

char *logTlm_getFilename(void) { return filename; }

void *logTlm_getBuffer(void) { return pBuffer; }

void logTlm_freeBuffer(void) {
    if (pBuffer != NULL) {
        free(pBuffer);
        pBuffer = NULL;
    }

    if (pthread_mutex_destroy(&lock) != 0) {
        syslog(LOG_ERR, "Mutex destroy has failed in the telemetry file.");
        exit(1);
    }
}

int logTlm_createBuffer(int sizeBuffer, size_t sizeElement) {
    if ((sizeBuffer <= 0) || (sizeElement <= 0)) {
        syslog(LOG_ERR, "The buffer/element size should be > 0.");
        return -1;
    }

    // Allocate the memory
    pBuffer = calloc(sizeBuffer, sizeElement);
    if (pBuffer == NULL) {
        syslog(LOG_ERR,
               "Failed to allocate the memory of buffer of telemetry file.");
        return -1;
    }

    if (pthread_mutex_init(&lock, NULL) != 0) {
        syslog(LOG_ERR, "Mutex init has failed in the telemetry file.");
        return -1;
    }

    // Update the internal data
    countBufferMax = sizeBuffer;
    sizeElementInBuffer = sizeElement;

    return 0;
}

int logTlm_close(void) {
    int status = 0;
    if (pFile != NULL) {
        status = fclose(pFile);
        pFile = NULL;
    }

    if (status != 0) {
        syslog(LOG_ERR, "Failed to close the telemetry file.");
    }

    return status;
}

// Set the filename. The user needs to free the memory of "filename" if it is
// not needed anymore.
static void logTlm_setFilename(char *pathDir, char *formatFileName) {

    // Getting current time
    time_t timeCurrent;
    time(&timeCurrent);
    struct tm *timeLocal = localtime(&timeCurrent);

    // Formatting time
    char timeFormatted[200];
    strftime(timeFormatted, sizeof(timeFormatted), formatFileName, timeLocal);

    // Before setting the filename, check we need to free the memory or not.
    if (strlen(filename) != 0) {
        free(filename);
    }

    filename = joinStr(pathDir, timeFormatted);
}

// Hold the mutex lock.
static inline void logTlm_lock(void) {
    if (pthread_mutex_lock(&lock) != 0) {
        syslog(LOG_ERR, "Mutex lock has failed in the telemetry file.");
        exit(1);
    }
}

// Release the mutex lock.
static inline void logTlm_unlock(void) {
    if (pthread_mutex_unlock(&lock) != 0) {
        syslog(LOG_ERR, "Mutex unlock has failed in the telemetry file.");
        exit(1);
    }
}

int logTlm_open(char *pathDir, char *formatFilename, int recordPerFile) {

    // Check the input
    if ((recordPerFile <= 0) || (recordPerFile < countBufferMax)) {
        syslog(LOG_ERR,
               "The size or file record should be > 0 or size of buffer.");
        return -1;
    }

    // Check there is still the flushing or not
    bool isFlushingOnGoing = true;
    if (pthread_mutex_trylock(&lock) == 0) {
        isFlushingOnGoing = isFlushing;
        logTlm_unlock();
    }

    if (isFlushingOnGoing) {
        syslog(LOG_ERR, "Can not open a new telemetry file because the "
                        "flushing is still ongoing.");
        return -1;
    }

    // Always close the file first if there is any
    if (logTlm_close() != 0) {
        return -1;
    }

    // Open a new file
    logTlm_setFilename(pathDir, formatFilename);
    pFile = fopen(filename, "w");
    if (pFile == NULL) {
        syslog(LOG_ERR, "Failed to open the telemetry file.");
        return -1;
    }

    // Update the internal data
    countFileMax = recordPerFile;
    countFile = 0;

    countRotatingFile = 0;
    countBufferCurrent = 0;

    return 0;
}

// Get the rotated file name. The user needs to free the output memory if it is
// not needed anymore.
static char *logTlm_getRotatedFilename(void) {
    char extension[100];
    sprintf(extension, ".%d", countRotatingFile);

    return joinStr(filename, extension);
}

// Rotate the file to the next one.
// Return 0 if success. Otherwise, -1.
static int logTlm_rotateFile(void) {

    // Close the current file
    if (logTlm_close() != 0) {
        return -1;
    }

    // Rename the file
    char *filenameNew = logTlm_getRotatedFilename();
    if (rename(filename, filenameNew) == -1) {
        // Release the resource
        free(filenameNew);
        filenameNew = NULL;

        return -1;
    }

    // Release the resource
    free(filenameNew);
    filenameNew = NULL;

    // Update the counter
    countRotatingFile += 1;

    // Open a new file pointer
    pFile = fopen(filename, "w");
    if (pFile == NULL) {
        return -1;
    }

    return 0;
}

int logTlm_write(void *pData) {

    // Check there is the buffer or file available or not
    if ((pBuffer == NULL) || (pFile == NULL)) {
        return -2;
    }

    // Copy the data to memory and update the counter
    if (countBufferCurrent < countBufferMax) {
        memcpy(pBuffer + countBufferCurrent * sizeElementInBuffer, pData,
               sizeElementInBuffer);

        countBufferCurrent += 1;
    }

    // Check the buffer is full or not
    if (countBufferCurrent < countBufferMax) {
        return 0;
    } else {
        return -1;
    }
}

// Write to the file.
// Return 0 if success. Otherwise -1.
static int logTlm_writeToFile(void *pBuffer, size_t size) {
    if (fwrite(pBuffer, size, 1, pFile) <= 0) {
        logTlm_close();

        syslog(LOG_ERR,
               "Failed to write the data to telemetry log. Closing...");
        return -1;
    }
    return 0;
}

// Flush the buffer data to file.
static void logTlm_flushToFile(void) {

    // Return immediately if no file
    if (pFile == NULL) {
        syslog(LOG_ERR, "No telemetry file to flush. Returning ...");
        return;
    }

    // Check the current space
    int spaceAvailable = countFileMax - countFile;
    bool isSpaceEnough = (countBufferCurrent <= spaceAvailable);

    int numToFile = isSpaceEnough ? countBufferCurrent : spaceAvailable;
    int status = logTlm_writeToFile(pBuffer, numToFile * sizeElementInBuffer);

    // Rotate to a new file
    bool isRotated = false;
    if ((status == 0) && (spaceAvailable == numToFile)) {
        status = logTlm_rotateFile();
        isRotated = (status == 0);
    }

    // If the rotation is needed, write the left data.
    if ((!isSpaceEnough) && isRotated) {
        status = logTlm_writeToFile(pBuffer + numToFile * sizeElementInBuffer,
                                    (countBufferCurrent - numToFile) *
                                        sizeElementInBuffer);
    }

    if (status == -1) {
        logTlm_close();

        syslog(LOG_ERR,
               "Failed to write the data to telemetry log. Closing...");
        return;
    }

    // Update the counters
    countFile =
        isRotated ? (countBufferCurrent - numToFile) : (countFile + numToFile);
    countBufferCurrent = 0;

    // Update the "isFlushing"
    logTlm_lock();
    isFlushing = false;
    logTlm_unlock();
}

void logTlm_flush(bool isImmediate) {
    // Check there is the file or buffer available or not
    if ((pFile == NULL) || (pBuffer == NULL)) {
        return;
    }

    // Update the "isFlushing"
    logTlm_lock();
    isFlushing = true;
    logTlm_unlock();

    if (isImmediate) {
        logTlm_flushToFile();
    }
}

bool logTlm_isFlushing(void) {

    // By default, assume the flushing status is still ongoing.
    bool isFlushingOnGoing = true;

    if (pthread_mutex_trylock(&lock) == 0) {
        isFlushingOnGoing = isFlushing;
        logTlm_unlock();
    }

    return isFlushingOnGoing;
};

void logTlm_closeThread(void) {

    int error = 0;
    if (isThreadReady) {
        isThreadReady = false;
        error = pthread_join(thread, NULL);
    }

    if (error != 0) {
        syslog(LOG_ERR,
               "Failed the waiting of telemetry file thread. Cancelling it...");

        error = pthread_cancel(thread);
    }

    if (error != 0) {
        syslog(LOG_ERR, "Failed to cancel the telemetry file thread.");
    }
}

// Run the thread job to flush the data automatically.
// Note: The data types of input and output are required for pthread_create().
// The input needs to cast to the correct data type when needed.
static void *logTlm_run(void *pData) {

    // Get the checking time in ms
    int *pTimeInMs = (int *)pData;
    syslog(LOG_INFO, "Checking time in telemetry file thread is %d ms.",
           *pTimeInMs);

    // Wait until the thread is ready
    while (!isThreadReady) {
        sleep(1);
    }

    // Loop delay time
    struct timespec sleepTime;
    sleepTime.tv_nsec = (*pTimeInMs) * 1e6;
    sleepTime.tv_sec = 0;

    // Flush the data to file
    bool isFlushingOnGoing;
    while (isThreadReady) {
        isFlushingOnGoing = false;
        if (pthread_mutex_trylock(&lock) == 0) {
            isFlushingOnGoing = isFlushing;
            logTlm_unlock();
        }

        if (isFlushingOnGoing) {
            logTlm_flushToFile();
        } else {
            // Give other functions a chance to run
            nanosleep(&sleepTime, NULL);
        }
    }

    syslog(LOG_INFO, "Close the thread of telemetry file.");

    return 0;
}

int logTlm_runFlushInNewThread(int *pTimeInMs) {

    // Check the input time
    if ((*pTimeInMs) <= 0) {
        syslog(LOG_ERR, "The checking time in telemetry file should be > 0.");
        return -1;
    }

    // Create the thread
    int error = pthread_create(&thread, NULL, logTlm_run, pTimeInMs);
    struct sched_param param;
    if (error != 0) {
        syslog(LOG_ERR, "Failed to create the thread in telemetry file.");
        return -1;
    } else {
        // Set priority of this thread
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if ((error = pthread_setschedparam(thread, SCHED_OTHER, &param)) != 0) {
            syslog(LOG_ERR,
                   "Can't initialize the thread priority in telemetry file.");

            pthread_cancel(thread);
            return -1;
        }
    }

    isThreadReady = true;

    return 0;
}
