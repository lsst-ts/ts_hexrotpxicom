#ifndef LOGTLM_H
#define LOGTLM_H

#include <stddef.h>

// Get the file name.
char *logTlm_getFilename(void);

// Close the file. This function is safe to call even though there is no file.
// Return 0 if success. Otherwise, fail.
int logTlm_close(void);

// Open the file with the format such as "tlm_%m_%d_%Y_%H_%M_%S.log" and the
// record per file.
// Return 0 if success. Otherwise, -1.
int logTlm_open(char *pathDir, char *formatFilename, int recordPerFile);

// Write the data up to 'recordPerFile' times in logTlm_open(). After reaching
// the limit, a new file will be opened.
// The function will return immediately if fail to write.
void logTlm_write(void *pData, size_t sizeData);

#endif // LOGTLM_H
