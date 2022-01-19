#include <sys/socket.h>
#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "utility.h"
#include "configPxi.h"
}

struct ConfigPxiTest : testing::Test {
    char *testConfigFilePath = "";

    ConfigPxiTest() {
        testConfigFilePath =
            joinStr(getModulePath(), "/tests/testData/default.yaml");
        configPxi_setConfigFile(testConfigFilePath);
    }

    ~ConfigPxiTest() {
        free(testConfigFilePath);
        testConfigFilePath = NULL;

        configPxi_setConfigFile("");
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
}

TEST_F(ConfigPxiTest, configPxiGetValInt) {
    EXPECT_EQ(2, configPxi_getValInt("VAL_INTEGER"));
}
