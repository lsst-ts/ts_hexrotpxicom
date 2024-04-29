#include "gtest/gtest.h"
#include <libgen.h>

extern "C" {
#include "utility.h"
}

struct UtilityTest : testing::Test {
    char *modulePath = "";

    UtilityTest() { modulePath = getModulePath(); }

    ~UtilityTest() {}
};

TEST_F(UtilityTest, getModulePath) {
    std::string moduleNameFromPath(basename(modulePath));

    std::size_t found = moduleNameFromPath.find("hexrotpxicom");
    EXPECT_TRUE(found != std::string::npos);
}

TEST_F(UtilityTest, isFile) {
    const char *pFilePath = joinStr(modulePath, "/tests/main.cpp");
    EXPECT_TRUE(isFile(pFilePath));
}

TEST_F(UtilityTest, isFileWrong) { EXPECT_FALSE(isFile(modulePath)); }

TEST_F(UtilityTest, isDir) { EXPECT_TRUE(isDir(modulePath)); }

TEST_F(UtilityTest, isDirWrong) {
    const char *pFilePath = joinStr(modulePath, "/tests/main.cpp");

    EXPECT_FALSE(isDir(pFilePath));
}

TEST(utility, isAbsPath) {
    const char *absPath = "/opt/home";

    bool isAbs = isAbsPath(absPath);
    EXPECT_TRUE(isAbs);
}

TEST(utility, isAbsPathWrong) {
    const char *absPath = "opt/home";

    bool isAbs = isAbsPath(absPath);
    EXPECT_FALSE(isAbs);
}

TEST(utility, joinStr) {
    char *str1 = "abc";
    char *str2 = "def";
    const char *strJoin = joinStr(str1, str2);

    const char *ans = "abcdef";
    EXPECT_STREQ(ans, strJoin);
}

TEST(utility, isLimitSwitchOn) {
    // EXTEND_SW_MASK
    // 0x20080 is 131200 (= 2^7 + 2^17)
    uint32_t mask = 0x20080;

    // Hit the limit switch
    bool limitSwitchHit = isLimitSwitchOn(0x0, mask);
    EXPECT_TRUE(limitSwitchHit);

    // 0x80 is 128 = 2^7
    limitSwitchHit = isLimitSwitchOn(0x80, mask);
    EXPECT_TRUE(limitSwitchHit);

    // 0x20000 is 131072 = 2^17
    limitSwitchHit = isLimitSwitchOn(0x20000, mask);
    EXPECT_TRUE(limitSwitchHit);

    // EXTEND_SW_MASK = 0x10040 is 65600 (= 2^6 + 2^16)
    limitSwitchHit = isLimitSwitchOn(0x10040, mask);
    EXPECT_TRUE(limitSwitchHit);

    // Not hit the limit switch
    limitSwitchHit = isLimitSwitchOn(0x20080, mask);
    EXPECT_FALSE(limitSwitchHit);

    // Experimental data on summit
    limitSwitchHit = isLimitSwitchOn(0x3BFEFF, mask);
    EXPECT_FALSE(limitSwitchHit);
}

TEST(utility, calcTimeDiff) {
    struct timespec timeStart, timeEnd, timeDiff;
    timeStart.tv_sec = 1;
    timeStart.tv_nsec = 100;

    timeEnd.tv_sec = 2;
    timeEnd.tv_nsec = 123;

    calcTimeDiff(&timeStart, &timeEnd, &timeDiff);

    EXPECT_EQ(1, timeDiff.tv_sec);
    EXPECT_EQ(23, timeDiff.tv_nsec);

    timeEnd.tv_nsec = 12;
    calcTimeDiff(&timeStart, &timeEnd, &timeDiff);

    EXPECT_EQ(0, timeDiff.tv_sec);
    EXPECT_EQ(999999912, timeDiff.tv_nsec);
}

TEST(utility, calcTimeLeft) {
    struct timespec timePassed, timeLeft;
    timePassed.tv_sec = 0;
    timePassed.tv_nsec = 1000;

    EXPECT_EQ(-1, calcTimeLeft(&timePassed, 100, &timeLeft));
    EXPECT_EQ(0, timeLeft.tv_sec);
    EXPECT_EQ(0, timeLeft.tv_nsec);

    EXPECT_EQ(0, calcTimeLeft(&timePassed, 2123, &timeLeft));
    EXPECT_EQ(0, timeLeft.tv_sec);
    EXPECT_EQ(1123, timeLeft.tv_nsec);

    timePassed.tv_sec = 1;
    EXPECT_EQ(0, calcTimeLeft(&timePassed, 3000000123, &timeLeft));
    EXPECT_EQ(1, timeLeft.tv_sec);
    EXPECT_EQ(999999123, timeLeft.tv_nsec);
}
