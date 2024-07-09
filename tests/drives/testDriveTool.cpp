#include "gtest/gtest.h"

extern "C" {
#include "copley.h"
#include "driveTool.h"
}

TEST(driveTool, getFaultName) {
    // Short buffer
    char nameShort[4] = {0};

    driveTool_getFaultName(ERROR_CODE_SHORT_CIRCUIT, sizeof(nameShort),
                           nameShort);
    EXPECT_STREQ("sho", nameShort);

    // Long buffer
    char name[100] = {0};

    driveTool_getFaultName(0, sizeof(name), name);
    EXPECT_STREQ("no error", name);

    driveTool_getFaultName(ERROR_CODE_SHORT_CIRCUIT, sizeof(name), name);
    EXPECT_STREQ("short circuit", name);

    driveTool_getFaultName(ERROR_CODE_AMP_OVER_TEMP, sizeof(name), name);
    EXPECT_STREQ("amp overtemp", name);

    driveTool_getFaultName(ERROR_CODE_AMP_OVER_VOLTAGE, sizeof(name), name);
    EXPECT_STREQ("amp overvoltage", name);

    driveTool_getFaultName(ERROR_CODE_AMP_UNDER_VOLTAGE, sizeof(name), name);
    EXPECT_STREQ("amp undervoltage", name);

    driveTool_getFaultName(ERROR_CODE_MOTOR_OVER_TEMP, sizeof(name), name);
    EXPECT_STREQ("motor overtemp", name);

    driveTool_getFaultName(ERROR_CODE_FEEDBACK_ERROR, sizeof(name), name);
    EXPECT_STREQ("feedback error", name);

    driveTool_getFaultName(ERROR_CODE_PHASING_ERROR, sizeof(name), name);
    EXPECT_STREQ("phasing error", name);

    driveTool_getFaultName(ERROR_CODE_CURRENT_LIMIT, sizeof(name), name);
    EXPECT_STREQ("current limit", name);

    driveTool_getFaultName(ERROR_CODE_VOLTAGE_LIMIT, sizeof(name), name);
    EXPECT_STREQ("voltage limit", name);

    driveTool_getFaultName(ERROR_CODE_POSITIVE_POSITION_LIMIT, sizeof(name),
                           name);
    EXPECT_STREQ("- limit switch", name);

    driveTool_getFaultName(ERROR_CODE_NEGATIVE_POSITION_LIMIT, sizeof(name),
                           name);
    EXPECT_STREQ("+ limit switch", name);

    driveTool_getFaultName(ERROR_CODE_TRACKING_ERROR, sizeof(name), name);
    EXPECT_STREQ("position tracking", name);

    driveTool_getFaultName(ERROR_CODE_POSITION_WRAP, sizeof(name), name);
    EXPECT_STREQ("position wrap", name);

    driveTool_getFaultName(ERROR_CODE_FAULTS_WITH_NO_OTHER_EMERGENCY,
                           sizeof(name), name);
    EXPECT_STREQ("generic fault", name);

    driveTool_getFaultName(ERROR_CODE_NODE_GUARDING_ERROR, sizeof(name), name);
    EXPECT_STREQ("node guarding error", name);

    driveTool_getFaultName(ERROR_CODE_COMMAND_ERROR, sizeof(name), name);
    EXPECT_STREQ("command error", name);

    driveTool_getFaultName(ERROR_CODE_STO_FAULT, sizeof(name), name);
    EXPECT_STREQ("interlock open", name);

    driveTool_getFaultName(31, sizeof(name), name);
    EXPECT_STREQ("unknown code 0x1f", name);
}

TEST(driveTool, getDs402StateName) {
    EXPECT_STREQ(
        "Not ready to switch on",
        driveTool_getDs402StateName(DS402_STATE_NOT_READY_TO_SWITCH_ON));
    EXPECT_STREQ("Switch on disabled",
                 driveTool_getDs402StateName(DS402_STATE_SWITCH_ON_DISABLED));
    EXPECT_STREQ(
        "Fault reaction active",
        driveTool_getDs402StateName(DS402_STATE_FAULT_REACTION_ACTIVE));
    EXPECT_STREQ("Fault", driveTool_getDs402StateName(DS402_STATE_FAULT));
    EXPECT_STREQ("Ready to switch on",
                 driveTool_getDs402StateName(DS402_STATE_READY_TO_SWITCH_ON));
    EXPECT_STREQ("Switched on",
                 driveTool_getDs402StateName(DS402_STATE_SWITCHED_ON));
    EXPECT_STREQ("Operation enabled",
                 driveTool_getDs402StateName(DS402_STATE_OPERATION_ENABLED));
    EXPECT_STREQ("Quick stop active",
                 driveTool_getDs402StateName(DS402_STATE_QUICK_STOP_ACTIVE));
    EXPECT_STREQ("Unknown state",
                 driveTool_getDs402StateName(DS402_STATE_START));
}
