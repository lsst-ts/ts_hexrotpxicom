#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "utility.h"

char *getModulePath(void) { return getenv("PXI_CNTLR_HOME"); }

bool isAbsPath(const char *pPath) {

    if (strncmp(&pPath[0], "/", 1) == 0) {
        return true;
    } else {
        return false;
    }
}

char *joinStr(char *str1, char *str2) {

    // A string of length n requires n+1 bytes of storage
    size_t lengStrJoin = strlen(str1) + strlen(str2) + 1;
    char *strJoin = (char *)calloc(lengStrJoin, sizeof(char));

    strncpy(strJoin, str1, strlen(str1) + 1);
    strncat(strJoin, str2, lengStrJoin - strlen(str1));

    return strJoin;
}

bool isFile(const char *pFilePath) {
    struct stat buf;
    stat(pFilePath, &buf);
    return S_ISREG(buf.st_mode);
}

bool isDir(const char *pDirPath) {
    struct stat buf;
    stat(pDirPath, &buf);
    return S_ISDIR(buf.st_mode);
}

bool isLimitSwitchOn(uint32_t pinsState, uint32_t limitSwitchMask) {
    return (pinsState & limitSwitchMask) != limitSwitchMask;
}
