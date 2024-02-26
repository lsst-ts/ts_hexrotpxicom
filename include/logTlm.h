#ifndef LOGTLM_H
#define LOGTLM_H

// Get the file name.
char *logTlm_getFilename(void);

// Get the buffer.
void *logTlm_getBuffer(void);

// Free the buffer and set to NULL.
void logTlm_freeBuffer(void);

// Close the file. This function is safe to call even though there is no file.
// Return 0 if success. Otherwise, fail.
int logTlm_close(void);

// Open the file with the format such as "tlm_%m_%d_%Y_%H_%M_%S.log". The total
// number of records in each file will be 'recordPerFile' * 'sizeBuffer'.
// The data will be written into a buffer first. After the buffer is full, the
// data in buffer will be written into the file.
// Return 0 if success. Otherwise, -1.
int logTlm_open(char *pathDir, char *formatFilename, int sizeBuffer,
                size_t sizeElement, int recordPerFile);

// Write the data. The size of data should be the same as 'sizeElement' in
// logTlm_open(). The function will return immediately if fail to write.
void logTlm_write(void *pData);

#endif // LOGTLM_H
