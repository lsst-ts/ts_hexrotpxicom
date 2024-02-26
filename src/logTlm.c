#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

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

// Counter of the rotating file.
static uint countRotatingFile = 0;

// Maximum count per file.
static uint countFileMax = 0;

// Current count of file
static uint countFile = 0;

// Maximum count of the buffer
static uint countBufferMax = 0;

// Current count in the buffer
static uint countBufferCurrent = 0;

char *logTlm_getFilename(void) { return filename; }

void *logTlm_getBuffer(void) { return pBuffer; }

void logTlm_freeBuffer(void) {
    if (pBuffer != NULL) {
        free(pBuffer);
        pBuffer = NULL;
    }
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

// Set the filename. The user needs to free the memory of 'filename' if it is
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

int logTlm_open(char *pathDir, char *formatFilename, int sizeBuffer,
                size_t sizeElement, int recordPerFile) {

    if ((sizeBuffer <= 0) || (sizeElement <= 0) || (recordPerFile <= 0)) {
        syslog(LOG_ERR, "When opening the telemetry file, the buffer/element "
                        "size or file record should be > 0.");
        return -1;
    }

    // Always close the file first if there is any
    logTlm_close();

    // Free the buffer first before the allocation
    logTlm_freeBuffer();

    // Allocate the memory
    pBuffer = calloc(sizeBuffer, sizeElement);
    if (pBuffer == NULL) {
        syslog(LOG_ERR, "Failed to allocate the memory of buffer.");
        return -1;
    }

    // Open a new file
    logTlm_setFilename(pathDir, formatFilename);
    pFile = fopen(filename, "w");
    if (pFile == NULL) {
        logTlm_freeBuffer();

        syslog(LOG_ERR, "Failed to open the telemetry file.");
        return -1;
    }

    // Update the internal data
    sizeElementInBuffer = sizeElement;

    countRotatingFile = 0;

    countBufferMax = sizeBuffer;
    countBufferCurrent = 0;

    countFileMax = recordPerFile;
    countFile = 0;

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
    logTlm_close();

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

void logTlm_write(void *pData) {

    // Check there is the file/buffer available to write or not
    if ((pFile == NULL) || (pBuffer == NULL)) {
        return;
    }

    // Copy the data to memory and update the counter
    if (countBufferCurrent < countBufferMax) {
        memcpy(pBuffer + countBufferCurrent * sizeElementInBuffer, pData,
               sizeElementInBuffer);

        countBufferCurrent += 1;
    }

    // If the buffer is not full yet, return immediately.
    if (countBufferCurrent < countBufferMax) {
        return;
    }

    // The buffer is full. Write to the file and update the counters
    if (fwrite(pBuffer, countBufferMax * sizeElementInBuffer, 1, pFile) > 0) {
        countFile += 1;
        countBufferCurrent = 0;
    } else {
        logTlm_close();

        syslog(LOG_ERR,
               "Failed to write the data to telemetry log. Closing...");
        return;
    }

    // Check the file is full or not. If yes, rotate the file.
    if (countFile < countFileMax) {
        return;
    }

    if (logTlm_rotateFile() == -1) {
        syslog(LOG_ERR, "Failed to rotate the telemetry log.");
        return;
    }

    // Reset the counter
    countFile = 0;
}
