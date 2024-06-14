#ifndef LOGTLM_H
#define LOGTLM_H

#include <stdbool.h>
#include <stddef.h>

// Get the filename.
char *logTlm_getFilename(void);

// Get the buffer with the specific id, which equals 1 or 2. Put 0 if you want
// to get the current buffer in use.
// Return the specific buffer. Otherwise, NULL if no specific buffer found.
void *logTlm_getBuffer(int id);

// Free the buffer and set to NULL.
void logTlm_freeBuffer(void);

// Create the buffers.
// Return 0 if success. Otherwise, -1.
int logTlm_createBuffer(int sizeBuffer, size_t sizeElement);

// Close the file. This function is safe to call even though there is no file.
// Return 0 if success. Otherwise, fail.
int logTlm_close(void);

// Open the file with the format such as "tlm_%m_%d_%Y_%H_%M_%S.log" or others.
// The "recordPerFile" should be >= "sizeBuffer" in logTlm_createBuffer().
// Return 0 if success. Otherwise, -1.
int logTlm_open(char *pathDir, char *formatFilename, int recordPerFile);

// Write the data. The size of data should be the same as "sizeElement" in
// logTlm_open().
// Return 0 if success and there is still the space in buffer to write.
// Return -1 if success but the space is full after this writing.
// Return -2 if no buffer or file to write.
int logTlm_write(void *pData);

// Flush the data in buffer to file.
// Return 0 if success. Otherwise, -1.
// Note: If there is the running thread and the data is still under the
// flushing, you will get -1 as well.
int logTlm_flush(void);

// The data in buffer is flushing to the file or not. If the logTlm_write() and
// flushing to file are at the different threads, you need to call this function
// to know the flushing status.
// Return true if yes. Otherwise, false.
bool logTlm_isFlushing(void);

// Close the running thread. This is used in the shutdown process.
void logTlm_closeThread(void);

// Run the flush job in a new thread with the checking time in ms. If you put
// the "isImmediate" to be false in the logTlm_flush(), the flush will happen in
// this new thread.
// Return 0 if success, otherwise, return -1.
int logTlm_runFlushInNewThread(int *pTimeInMs);

#endif // LOGTLM_H
