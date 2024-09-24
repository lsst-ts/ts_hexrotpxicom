#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

void calcTimeDiff(struct timespec *pTimeStart, struct timespec *pTimeEnd,
                  struct timespec *pTimeDiff) {

    const long SEC_TO_NS = 1000000000;
    if ((pTimeEnd->tv_nsec - pTimeStart->tv_nsec) < 0) {
        pTimeDiff->tv_sec = pTimeEnd->tv_sec - pTimeStart->tv_sec - 1;
        pTimeDiff->tv_nsec =
            SEC_TO_NS + pTimeEnd->tv_nsec - pTimeStart->tv_nsec;
    } else {
        pTimeDiff->tv_sec = pTimeEnd->tv_sec - pTimeStart->tv_sec;
        pTimeDiff->tv_nsec = pTimeEnd->tv_nsec - pTimeStart->tv_nsec;
    }
}

int calcTimeLeft(struct timespec *pTimePassed, long maxTimeInNs,
                 struct timespec *pTimeLeft) {

    const long SEC_TO_NS = 1000000000;
    long passedTimeInNs =
        pTimePassed->tv_sec * SEC_TO_NS + pTimePassed->tv_nsec;
    if (passedTimeInNs >= maxTimeInNs) {
        pTimeLeft->tv_sec = 0;
        pTimeLeft->tv_nsec = 0;
        return -1;
    } else {
        long leftTimeInNs = maxTimeInNs - passedTimeInNs;
        pTimeLeft->tv_sec = leftTimeInNs / SEC_TO_NS;
        pTimeLeft->tv_nsec = leftTimeInNs % SEC_TO_NS;
    }

    return 0;
}

// Get the leap seconds. Return -1 if it fails.
static int getLeapSeconds(void) {
    struct timespec ts_tai, ts_host;

    if (clock_gettime(CLOCK_TAI, &ts_tai) != 0) {
        return -1;
    }

    if (clock_gettime(CLOCK_REALTIME, &ts_host) != 0) {
        return -1;
    }

    return (int)(ts_tai.tv_sec - ts_host.tv_sec);
}

int waitNtpLeapSeconds(int timeout, int checkInterval) {
    int leapSeconds = getLeapSeconds();
    while ((timeout > 0) && (leapSeconds <= 0)) {
        if (leapSeconds < 0) {
            return -1;
        }

        sleep(checkInterval);
        timeout -= checkInterval;
    }

    if (leapSeconds > 0) {
        return leapSeconds;
    } else {
        return -1;
    }
}
