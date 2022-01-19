#ifndef UTILITY_H
#define UTILITY_H

#include <stdbool.h>
#include <stdint.h>

// Get the module path based on the environment variable (PXI_CNTLR_HOME).
char *getModulePath(void);

// Is the absolute path or not. Return true if it is.
bool isAbsPath(const char *pPath);

// Join the strings. The user needs to free the output memory if it is not
// needed anymore.
char *joinStr(char *str1, char *str2);

// Input file path is a file or not. Return true if the answer is yes.
bool isFile(const char *pFilePath);

// Input directory path is a directory or not. Return ture if the answer is yes.
bool isDir(const char *pDirPath);

// Is the limit switch on or not. Return true if it is on.
bool isLimitSwitchOn(uint32_t pinsState, uint32_t limitSwitchMask);

#endif // UTILITY_H
