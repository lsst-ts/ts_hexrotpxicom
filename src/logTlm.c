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

// Counter of the rotating file.
static int countRotatingFile = 0;

// Maximum count per file.
static int countFileMax = 0;

// Current count of file
static int countFile = 0;

char *logTlm_getFilename(void) { return filename; }

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

int logTlm_open(char *pathDir, char *formatFilename, int recordPerFile) {

    if (recordPerFile <= 0) {
        syslog(LOG_ERR, "When opening the telemetry file, the record of file "
                        "should be > 0.");
        return -1;
    }

    // Always close the file first if there is any
    logTlm_close();

    // Open a new file
    logTlm_setFilename(pathDir, formatFilename);
    pFile = fopen(filename, "w");
    if (pFile == NULL) {
        syslog(LOG_ERR, "Failed to open the telemetry file.");
        return -1;
    }

    // Update the internal data
    countRotatingFile = 0;

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

void logTlm_write(void *pData, size_t sizeData) {

    // Check there is the file available to write or not
    if (pFile == NULL) {
        return;
    }

    // The buffer is full. Write to the file and update the counters
    if (fwrite(pData, sizeData, 1, pFile) > 0) {
        countFile += 1;
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
