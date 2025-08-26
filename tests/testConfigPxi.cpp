#include <cstring>
#include <sys/socket.h>
#include <syslog.h>

#include "gtest/gtest.h"

extern "C" {
#include "configPxi.h"
#include "utility.h"
}

struct ConfigPxiTest : testing::Test {
    char *testConfigFilePath = "";

    ConfigPxiTest() {
        testConfigFilePath =
            joinStr(getModulePath(), "/tests/testData/default.yaml");
        configPxi_setConfigFile(testConfigFilePath);

        openlog("ConfigPxi", LOG_CONS, LOG_SYSLOG);
    }

    ~ConfigPxiTest() {
        free(testConfigFilePath);
        testConfigFilePath = NULL;

        configPxi_setConfigFile("");

        closelog();
    }
};

TEST_F(ConfigPxiTest, configPxiGetConfigFile) {
    const char *configFile = configPxi_getConfigFile();
    EXPECT_STREQ(testConfigFilePath, configFile);
}

TEST_F(ConfigPxiTest, configPxiSetConfigFile) {
    const char *ansConfigFile = "testConfigFile.yaml";
    configPxi_setConfigFile(ansConfigFile);

    const char *configFile = configPxi_getConfigFile();
    EXPECT_STREQ(ansConfigFile, configFile);
}

TEST_F(ConfigPxiTest, configPxiGetSettingWrongSetting) {
    const char *settingName = "WRONG_SETTING";
    const char *val = configPxi_getSetting(testConfigFilePath, settingName);

    const char *ans = "";
    EXPECT_STREQ(ans, val);
}

TEST_F(ConfigPxiTest, configPxiGetSettingWrongFile) {
    const char *filePath = "wrongFile.yaml";
    const char *settingName = "SERVER_IP_DDS";
    EXPECT_EXIT(configPxi_getSetting(filePath, settingName),
                ::testing::ExitedWithCode(EXIT_FAILURE),
                "No such file or directory");
}

TEST_F(ConfigPxiTest, configPxiGetValDouble) {
    EXPECT_DOUBLE_EQ(1.2, configPxi_getValDouble("VAL_DOUBLE"));
    EXPECT_DOUBLE_EQ(1.3, configPxi_getValDouble("VAL_DOUBLE_OTHER"));
}

TEST_F(ConfigPxiTest, configPxiGetValInt) {
    EXPECT_EQ(2, configPxi_getValInt("VAL_INTEGER"));
    EXPECT_EQ(3, configPxi_getValInt("VAL_INTEGER_OTHER"));
}
