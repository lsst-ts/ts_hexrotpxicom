#ifndef DS402_H
#define DS402_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum ds402_state {
    DS402_STATE_START = 0,
    DS402_STATE_NOT_READY_TO_SWITCH_ON,
    DS402_STATE_SWITCH_ON_DISABLED,
    DS402_STATE_READY_TO_SWITCH_ON,
    DS402_STATE_SWITCHED_ON,
    DS402_STATE_OPERATION_ENABLED,
    DS402_STATE_QUICK_STOP_ACTIVE,
    DS402_STATE_FAULT_REACTION_ACTIVE,
    DS402_STATE_FAULT
} ds402_state;

typedef union {
    struct {
        uint16_t ready_to_switch_on : 1;      /* 1 */
        uint16_t switched_on : 1;             /* 2 */
        uint16_t operation_enabled : 1;       /* 4 */
        uint16_t fault : 1;                   /* 8 */
        uint16_t voltage_enabled : 1;         /* 10 */
        uint16_t quick_stop : 1;              /* 20 */
        uint16_t switch_on_disabled : 1;      /* 40 */
        uint16_t warning : 1;                 /* 80 */
        uint16_t manuf_specific_1 : 1;        /* 100 */
        uint16_t remote : 1;                  /* 200 */
        uint16_t target_reached : 1;          /* 400 */
        uint16_t internal_limit_active : 1;   /* 800 */
        uint16_t operation_mode_specific : 2; /* 3000 */
        uint16_t manuf_specific_2 : 2;        /* C000 */
    };
    uint16_t val;
} ds402_status_word;

enum {
    DS402_STATUS_PP_SET_POINT_ACK = (1 << 12),
    DS402_STATUS_PP_FOLLOWING_ERROR = (1 << 13),
};

enum {
    DS402_STATUS_IP_INTERPOLATION_MODE_ACTIVE = (1 << 12),
};

enum {
    DS402_STATUS_HM_HOMING_ATTAINED = (1 << 12),
    DS402_STATUS_HM_HOMING_ERROR = (1 << 13),
};

static inline enum ds402_state
ds402_status_to_state(ds402_status_word status_word) {
    /* regardless of quick stop or switch_on_disabled */
    if ((status_word.val & 0xf) == 0xf)
        return DS402_STATE_FAULT_REACTION_ACTIVE;

    if (status_word.fault)
        return DS402_STATE_FAULT;

    if ((status_word.val & 0x6f) == 0x7)
        return DS402_STATE_QUICK_STOP_ACTIVE;

    if (status_word.switch_on_disabled)
        return DS402_STATE_SWITCH_ON_DISABLED;

    switch (status_word.val & 0x4f) {
    case 0:
        return DS402_STATE_NOT_READY_TO_SWITCH_ON;
    case 0x1:
        return DS402_STATE_READY_TO_SWITCH_ON;
    case 0x2:
    case 0x3:
        return DS402_STATE_SWITCHED_ON;
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
        return DS402_STATE_OPERATION_ENABLED;
    }

    /* shouldn't get here */
    assert("invalid ds402 status" == NULL);
}

typedef struct {
    union {
        uint16_t switch_on : 1;
        uint16_t enable_voltage : 1;
        uint16_t quick_stop : 1;
        uint16_t enable_operation : 1;
        uint16_t mode_specific : 3;
        uint16_t fault_reset : 1;
        uint16_t halt : 1;
        uint16_t reserved : 2;
        uint16_t manuf_specific : 5;
    };
    uint16_t val;
} ds402_control_word;

enum {
    DS402_COMMAND_NOCHANGE = 0x0,
    DS402_COMMAND_SHUTDOWN = 0x6,
    DS402_COMMAND_SWITCH_ON = 0x7,
    DS402_COMMAND_DISABLE_VOLTAGE = 0x4,
    DS402_COMMAND_QUICK_STOP = 0xB,
    DS402_COMMAND_DISABLE_OPERATION = 0x7,
    DS402_COMMAND_ENABLE_OPERATION = 0xF,
    DS402_COMMAND_RESET_FAULT = 0x80,
    DS402_COMMAND_MASK = 0x8F,
};

enum {
    DS402_CONTROL_PP_NEW_SETPOINT = (1 << 4),
    DS402_CONTROL_PP_CHANGE_SETPOINT_IMMEDIATELY = (1 << 5),
    DS402_CONTROL_PP_RELATIVE = (1 << 6),
    DS402_CONTROL_PP_HALT = (1 << 8)
};

enum {
    DS402_CONTROL_HOMING_ACTIVE = (1 << 4),
    DS402_CONTROL_HOMING_HALT = (1 << 8),
};

enum {
    DS402_CONTROL_IP_INTERPOLATION_ACTIVE = (1 << 4),
    DS402_CONTROL_IP_PAUSE = (1 << 8),
};

/* 0x6060: mode of operation */
enum ds402_mode {
    DS402_MODE_PROFILE_POSITION = 1,
    DS402_MODE_PROFILE_VELOCITY = 3,
    DS402_MODE_PROFILE_TORQUE = 4,
    DS402_MODE_HOMING = 6,
    DS402_MODE_INTERPOLATED_POSITION = 7,
    DS402_MODE_CYCLIC_SYNCHRONOUS_POSITION = 8,
    DS402_MODE_CYCLIC_SYNCHRONOUS_VELOCITY = 9,
    DS402_MODE_CYCLIC_SYNCHRONOUS_TORQUE = 10,
};

/* 0x6098: homing method */
enum {
    DS402_HOMING_METHOD_CURRENT_POSITION = 0,
    DS402_HOMING_METHOD_LIMIT_SWITCH_OUT_TO_INDEX_NEGATIVE = 1,
    DS402_HOMING_METHOD_LIMIT_SWITCH_OUT_TO_INDEX_POSITIVE = 2,
    DS402_HOMING_METHOD_HOME_OUT_TO_INDEX_POSITIVE = 3,
    DS402_HOMING_METHOD_HOME_IN_TO_INDEX_POSITIVE = 4,
    DS402_HOMING_METHOD_HOME_OUT_TO_INDEX_NEGATIVE = 5,
    DS402_HOMING_METHOD_HOME_IN_TO_INDEX_NEGATIVE = 6,
    DS402_HOMING_METHOD_LOWER_HOME_OUTSIDE_INDEX_POSITIVE = 7,
    DS402_HOMING_METHOD_LOWER_HOME_INSIDE_INDEX_POSITIVE = 8,
    DS402_HOMING_METHOD_UPPER_HOME_INSIDE_INDEX_POSITIVE = 9,
    DS402_HOMING_METHOD_UPPER_HOME_OUTSIDE_INDEX_POSITIVE = 10,
    DS402_HOMING_METHOD_UPPER_HOME_OUTSIDE_INDEX_NEGATIVE = 11,
    DS402_HOMING_METHOD_UPPER_HOME_INSIDE_INDEX_NEGATIVE = 12,
    DS402_HOMING_METHOD_LOWER_HOME_INSIDE_INDEX_NEGATIVE = 13,
    DS402_HOMING_METHOD_LOWER_HOME_OUTSIDE_INDEX_NEGATIVE = 14,
    DS402_HOMING_METHOD_LIMIT_SWITCH_NEGATIVE = 17,
    DS402_HOMING_METHOD_LIMIT_SWITCH_POSITIVE = 18,
    DS402_HOMING_METHOD_HOME_SWITCH_POSITIVE = 19,
    DS402_HOMING_METHOD_HOME_SWITCH_NEGATIVE = 21,
    DS402_HOMING_METHOD_LOWER_HOME_POSITIVE = 23,
    DS402_HOMING_METHOD_UPPER_HOME_POSITIVE = 25,
    DS402_HOMING_METHOD_UPPER_HOME_NEGATIVE = 27,
    DS402_HOMING_METHOD_LOWER_HOME_NEGATIVE = 29,
    DS402_HOMING_METHOD_NEXT_INDEX_NEGATIVE = 33,
    DS402_HOMING_METHOD_NEXT_INDEX_POSITIVE = 34,
    DS402_HOMING_METHOD_CURRENT_POSITION_MOVE_TO_NEW_ZERO = 35,
};

/* 0x60C0: interpolation sub-mode select */
enum {
    DS402_INTERPOLATION_MODE_LINEAR = 0,
    DS402_INTERPOLATION_MODE_LINEAR_VARIABLE_TIME = -1,
    DS402_INTERPOLATION_MODE_CUBIC = -2,
};

/* 0x60C1: interpolation data record */
enum {
    DS402_INTERPOLATION_DATA_HIGHEST_SUBINDEX,
    DS402_INTERPOLATION_DATA_TARGET_POSITION,
    DS402_INTERPOLATION_DATA_TIME,
    DS402_INTERPOLATION_DATA_TARGET_VELOCITY,
};

/* 0x60C4: interpolation data configuration */
enum {
    DS402_INTERPOLATION_DATA_CONFIG_HIGHEST_SUBINDEX,
    DS402_INTERPOLATION_DATA_CONFIG_MAX_SIZE,
    DS402_INTERPOLATION_DATA_CONFIG_ACTUAL_SIZE,
    DS402_INTERPOLATION_DATA_CONFIG_BUFFER_ORGANIZATION,
    DS402_INTERPOLATION_DATA_CONFIG_BUFFER_POSITION,
    DS402_INTERPOLATION_DATA_CONFIG_RECORD_SIZE,
    DS402_INTERPOLATION_DATA_CONFIG_BUFFER_CLEAR
};

enum {
    DS402_ABORT_CONNECTION_OPTION_CODE = 0x6007,
    DS402_ERROR_CODE = 0x603F,
    DS402_CONTROL_WORD = 0x6040,
    DS402_STATUS_WORD = 0x6041,
    DS402_QUICK_STOP_OPTION_CODE = 0x605A,
    DS402_SHUTDOWN_OPTION_CODE = 0x605B,
    DS402_DISABLE_OPTION_CODE = 0x605C,
    DS402_HALT_OPTION_CODE = 0x605D,
    DS402_FAULT_REACTION_OPTION_CODE = 0x605E,
    DS402_MODES_OF_OPERATION = 0x6060,
    DS402_MODES_OF_OPERATION_DISPLAY = 0x6061,
    DS402_POSITION_DEMAND_VALUE = 0x6062,
    DS402_POSITION_ACTUAL_VALUE_INC = 0x6063,
    DS402_POSITION_ACTUAL_VALUE = 0x6064,
    DS402_FOLLOWING_ERROR_WINDOW = 0x6065,
    DS402_FOLLOWING_ERROR_TIMEOUT = 0x6066,
    DS402_POSITION_WINDOW = 0x6067,
    DS402_POSITION_WINDOW_TIME = 0x6068,
    DS402_VELOCITY_SENSOR_ACTUAL_VALUE = 0x6069,
    DS402_SENSOR_SELECTION_CODE = 0x606A,
    DS402_VELOCITY_DEMAND_VALUE = 0x606B,
    DS402_VELOCITY_ACTUAL_VALUE = 0x606C,
    DS402_VELOCITY_WINDOW = 0x606D,
    DS402_VELOCITY_WINDOW_TIME = 0x606E,
    DS402_VELOCITY_THRESHOLD = 0x0606F,
    DS402_VELOCITY_THRESHOLD_TIME = 0x6070,
    DS402_TARGET_TORQUE = 0x6071,
    DS402_MAX_TORQUE = 0x6072,
    DS402_MAX_CURRENT = 0x6073,
    DS402_TORQUE_DEMAND_VALUE = 0x6074,
    DS402_MOTOR_RATED_CURRENT = 0x6075,
    DS402_MOTOR_RATED_TORQUE = 0x6076,
    DS402_TORQUE_ACTUAL_VALUE = 0x6077,
    DS402_CURRENT_ACTUAL_VALUE = 0x6078,
    DS402_DC_LINK_CIRCUIT_VOLTAGE = 0x6079,
    DS402_TARGET_POSITION = 0x607A,
    DS402_POSITION_RANGE_LIMIT = 0x607B,
    DS402_HOME_OFFSET = 0x607C,
    DS402_SOFTWARE_POSITION_LIMIT = 0x607D,
    DS402_POLARITY = 0x607E,
    DS402_MAX_PROFILE_VELOCITY = 0x607F,
    DS402_MAX_MOTOR_SPEED = 0x6080,
    DS402_PROFILE_VELOCITY = 0x6081,
    DS402_END_VELOCITY = 0x6082,
    DS402_PROFILE_ACCELERATION = 0x6083,
    DS402_PROFILE_DECELERATION = 0x6084,
    DS402_QUICK_STOP_DECELERATION = 0x6085,
    DS402_MOTION_PROFILE_TYPE = 0x6086,
    DS402_TORQUE_SLOPE = 0x6087,
    DS402_TORQUE_PROFILE_TYPE = 0x6088,
    DS402_POSITION_ENCODER_RESOLUTION = 0x608F,
    DS402_VELOCITY_ENCODER_RESOLUTION = 0x6090,
    DS402_GEAR_RATIO = 0x6091,
    DS402_FEED_CONSTANT = 0x6092,
    DS402_HOMING_METHOD = 0x6098,
    DS402_HOMING_SPEEDS = 0x6099,
    DS402_HOMING_ACCELERATION = 0x609A,
    DS402_VELOCITY_OFFSET = 0x60B1,
    DS402_TORQUE_OFFSET = 0x60B2,
    DS402_INTERPOLATION_SUB_MODE_SELECT = 0x60C0,
    DS402_INTERPOLATION_DATA_RECORD = 0x60C1,
    DS402_INTERPOLATION_TIME_PERIOD = 0x60C2,
    DS402_INTERPOLATION_SYNC_DEFINITION = 0x60C3,
    DS402_INTERPOLATION_DATA_CONFIGURATION = 0x60C4,
    DS402_MAX_ACCELERATION = 0x60C5,
    DS402_MAX_DECELERATION = 0x60C6,
    DS402_FOLLOWING_ERROR_ACTUAL_VALUE = 0x60F4,
    DS402_TORQUE_CONTROL_PARAMETERS = 0x60F6,
    DS402_POWER_STAGE_PARAMETERS = 0x60F7,
    DS402_MAX_SLIPPAGE = 0x60F8,
    DS402_VELOCITY_CONTROL_PARAMETER_SET = 0x60F9,
    DS402_CONTROL_EFFORT = 0x60FA,
    DS402_POSITION_CONTROL_PARAMETER_SET = 0x60FB,
    DS402_POSITION_DEMAND_VALUE_INC = 0x60FC,
    DS402_DIGITAL_INPUTS = 0x60FD,
    DS402_DIGITAL_OUTPUTS = 0x60FE,
    DS402_TARGET_VELOCITY = 0x60FF,
    DS402_MOTOR_TYPE = 0x6402,
    DS402_MOTOR_CATALOG_NUMBER = 0x6403,
    DS402_MOTOR_MANUFACTURER = 0x6404,
    DS402_MOTOR_CATALOG_ADDRESS = 0x6405,
    DS402_MOTOR_CALIBRATION_DATE = 0x6406,
    DS402_MOTOR_SERVICE_PERIOD = 0x6407,
    DS402_MOTOR_DATA = 0x6410,
    DS402_SUPPORTED_DRIVE_MODES = 0x6502,
    DS402_DRIVE_CATALOG_NUMBER = 0x6503,
    DS402_DRIVE_MANUFACTURER = 0x6504,
    DS402_DRIVE_CATALOG_ADDRESS = 0x6505,
    DS402_DRIVE_DATA = 0x6510,
};

#endif // DS402_H
