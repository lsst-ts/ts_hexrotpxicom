#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "copley.h"
#include "driveTool.h"

void driveTool_getFaultName(uint16_t code, size_t len, char *pName) {

    bool isKnown = true;
    char *error;
    switch (code) {
    case 0x0:
        error = "no error";
        break;
    case ERROR_CODE_SHORT_CIRCUIT:
        error = "short circuit";
        break;
    case ERROR_CODE_AMP_OVER_TEMP:
        error = "amp overtemp";
        break;
    case ERROR_CODE_AMP_OVER_VOLTAGE:
        error = "amp overvoltage";
        break;
    case ERROR_CODE_AMP_UNDER_VOLTAGE:
        error = "amp undervoltage";
        break;
    case ERROR_CODE_MOTOR_OVER_TEMP:
        error = "motor overtemp";
        break;
    case ERROR_CODE_FEEDBACK_ERROR:
        error = "feedback error";
        break;
    case ERROR_CODE_PHASING_ERROR:
        error = "phasing error";
        break;
    case ERROR_CODE_CURRENT_LIMIT:
        error = "current limit";
        break;
    case ERROR_CODE_VOLTAGE_LIMIT:
        error = "voltage limit";
        break;
    case ERROR_CODE_POSITIVE_POSITION_LIMIT:
        error = "- limit switch";
        break;
    case ERROR_CODE_NEGATIVE_POSITION_LIMIT:
        error = "+ limit switch";
        break;
    case ERROR_CODE_TRACKING_ERROR:
        error = "position tracking";
        break;
    case ERROR_CODE_POSITION_WRAP:
        error = "position wrap";
        break;
    case ERROR_CODE_FAULTS_WITH_NO_OTHER_EMERGENCY:
        error = "generic fault";
        break;
    case ERROR_CODE_NODE_GUARDING_ERROR:
        error = "node guarding error";
        break;
    case ERROR_CODE_COMMAND_ERROR:
        error = "command error";
        break;
    case ERROR_CODE_STO_FAULT:
        error = "interlock open";
        break;
    default:
        isKnown = false;
        break;
    }

    if (isKnown) {
        strncpy(pName, error, len - 1);
    } else {
        char errorUnknown[30] = {0};
        snprintf(errorUnknown, sizeof(errorUnknown), "unknown code 0x%x", code);
        strncpy(pName, errorUnknown, len - 1);
    }
}

const char *driveTool_getDs402StateName(enum ds402_state state) {

    switch (state) {
    case DS402_STATE_NOT_READY_TO_SWITCH_ON:
        return "Not ready to switch on";
    case DS402_STATE_SWITCH_ON_DISABLED:
        return "Switch on disabled";
    case DS402_STATE_FAULT_REACTION_ACTIVE:
        return "Fault reaction active";
    case DS402_STATE_FAULT:
        return "Fault";
    case DS402_STATE_READY_TO_SWITCH_ON:
        return "Ready to switch on";
    case DS402_STATE_SWITCHED_ON:
        return "Switched on";
    case DS402_STATE_OPERATION_ENABLED:
        return "Operation enabled";
    case DS402_STATE_QUICK_STOP_ACTIVE:
        return "Quick stop active";
    default:
        return "Unknown state";
    }
}

// Read the SDO data from the request. The user needs to provide the "ppData" to
// get the pointer to the request's internal SDO data memory.
// Return 0 on success, -2 on error. If the request is busy, return -1.
static int driveTool_readSdoData(ec_sdo_request_t *request, uint8_t **ppData) {

    int status = -1;
    switch (ecrt_sdo_request_state(request)) {
    // Trigger the reading
    case EC_REQUEST_UNUSED:
        ecrt_sdo_request_read(request);
        break;

    case EC_REQUEST_BUSY:
        break;

    case EC_REQUEST_SUCCESS:
        *ppData = ecrt_sdo_request_data(request);
        status = 0;
        break;

    case EC_REQUEST_ERROR:
        status = -2;
        break;

    default:
        break;
    }

    return status;
}

// Get the SDO parameter of the slave configuration "pSc" with the index,
// subindex, and size of data. The user needs to provide the "ppData" to
// get the pointer to the request's internal SDO data memory.
// Return 0 on success, -1 on failure.
static int driveTool_getSdoParam(ec_slave_config_t *pSc, uint16_t index,
                                 uint8_t subindex, size_t size,
                                 uint8_t **ppData) {

    // Create the SDO request. If fail, return -1 immediately.
    ec_sdo_request_t *request =
        ecrt_slave_config_create_sdo_request(pSc, index, subindex, size);

    if (request == NULL) {
        return -1;
    }

    // Timeout in milliseconds
    uint32_t timeout = 500;
    if (ecrt_sdo_request_timeout(request, timeout) != 0) {
        return -1;
    }

    // Get the pointer to the request's internal SDO data memory
    struct timespec timeSleep;
    timeSleep.tv_sec = 0;
    timeSleep.tv_nsec = 50000000;

    int status = -1;
    while (status != 0) {
        status = driveTool_readSdoData(request, ppData);

        // There is the error, return -1 immediately.
        if (status == -2) {
            return -1;
        }

        nanosleep(&timeSleep, NULL);
    }

    return 0;
}

int driveTool_getSdoParamU16(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, uint16_t *pValue) {

    uint8_t *pData;
    if (driveTool_getSdoParam(pSc, index, subindex, 2, &pData) == 0) {
        *pValue = EC_READ_U16(pData);
        return 0;
    }

    return -1;
}

int driveTool_getSdoParamU32(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, uint32_t *pValue) {
    uint8_t *pData;
    if (driveTool_getSdoParam(pSc, index, subindex, 4, &pData) == 0) {
        *pValue = EC_READ_U32(pData);
        return 0;
    }

    return -1;
}

int driveTool_getSdoParamS16(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, int16_t *pValue) {
    uint8_t *pData;
    if (driveTool_getSdoParam(pSc, index, subindex, 2, &pData) == 0) {
        *pValue = EC_READ_S16(pData);
        return 0;
    }

    return -1;
}

int driveTool_getSdoParamS32(ec_slave_config_t *pSc, uint16_t index,
                             uint8_t subindex, int32_t *pValue) {
    uint8_t *pData;
    if (driveTool_getSdoParam(pSc, index, subindex, 4, &pData) == 0) {
        *pValue = EC_READ_S32(pData);
        return 0;
    }

    return -1;
}

// Drives are in the target state or not according to the DS402 states.
// The user needs to provide the DS402 "states", number of states
// (nStates), and the target state.
// Return True if yes, else False.
static bool driveTool_areTargetState(ds402_state states[], int nStates,
                                     ds402_state targetState) {
    int idx;
    for (idx = 0; idx < nStates; idx++) {
        if (states[idx] != targetState) {
            return false;
        }
    }

    return true;
}

bool driveTool_areOperationEnabled(ds402_state states[], int nStates) {
    return driveTool_areTargetState(states, nStates,
                                    DS402_STATE_OPERATION_ENABLED);
}

bool driveTool_areSwitchOnDisabled(ds402_state states[], int nStates) {
    return driveTool_areTargetState(states, nStates,
                                    DS402_STATE_SWITCH_ON_DISABLED);
}

// Return the next command to the goal state from the disabled state.
static int driveTool_nextCommandFromDisabled(ds402_state goalState) {

    switch (goalState) {
    case DS402_STATE_SWITCH_ON_DISABLED:
        return DS402_COMMAND_DISABLE_VOLTAGE;
    case DS402_STATE_READY_TO_SWITCH_ON:
    case DS402_STATE_SWITCHED_ON:
    case DS402_STATE_OPERATION_ENABLED:
        return DS402_COMMAND_SHUTDOWN;
    default:
        syslog(LOG_ERR, "Don't know how to get from %s to %s",
               driveTool_getDs402StateName(DS402_STATE_SWITCH_ON_DISABLED),
               driveTool_getDs402StateName(goalState));
        return DS402_COMMAND_SHUTDOWN;
    }
}

// Return the next command to the goal state from the ready-to-switch-on state.
static int driveTool_nextCommandFromReadyToSwitchOn(ds402_state goalState) {

    switch (goalState) {
    case DS402_STATE_SWITCH_ON_DISABLED:
        return DS402_COMMAND_DISABLE_VOLTAGE;
    case DS402_STATE_READY_TO_SWITCH_ON:
        return DS402_COMMAND_SHUTDOWN;
    case DS402_STATE_SWITCHED_ON:
    case DS402_STATE_OPERATION_ENABLED:
        return DS402_COMMAND_SWITCH_ON;
    default:
        syslog(LOG_ERR, "Don't know how to get from %s to %s",
               driveTool_getDs402StateName(DS402_STATE_READY_TO_SWITCH_ON),
               driveTool_getDs402StateName(goalState));
        return DS402_COMMAND_SHUTDOWN;
    }
}

// Return the next command to the goal state from the switched-on state.
static int driveTool_nextCommandFromSwitchedOn(ds402_state goalState) {

    switch (goalState) {
    case DS402_STATE_SWITCH_ON_DISABLED:
    case DS402_STATE_READY_TO_SWITCH_ON:
        return DS402_COMMAND_SHUTDOWN;
    case DS402_STATE_SWITCHED_ON:
        return DS402_COMMAND_SWITCH_ON;
    case DS402_STATE_OPERATION_ENABLED:
        return DS402_COMMAND_ENABLE_OPERATION;
    default:
        syslog(LOG_ERR, "Don't know how to get from %s to %s",
               driveTool_getDs402StateName(DS402_STATE_SWITCHED_ON),
               driveTool_getDs402StateName(goalState));
        return DS402_COMMAND_SHUTDOWN;
    }
}

// Return the next command to the goal state from the enabled state.
static int driveTool_nextCommandFromEnabled(ds402_state goalState) {

    switch (goalState) {
    case DS402_STATE_SWITCH_ON_DISABLED:
    case DS402_STATE_READY_TO_SWITCH_ON:
        return DS402_COMMAND_SHUTDOWN;
    case DS402_STATE_SWITCHED_ON:
        return DS402_COMMAND_DISABLE_OPERATION;
    case DS402_STATE_OPERATION_ENABLED:
        return DS402_COMMAND_ENABLE_OPERATION;
    default:
        syslog(LOG_ERR, "Don't know how to get from %s to %s",
               driveTool_getDs402StateName(DS402_STATE_OPERATION_ENABLED),
               driveTool_getDs402StateName(goalState));
        return DS402_COMMAND_SHUTDOWN;
    }
}

// Return the next command to the goal state from the fault state.
static int driveTool_nextCommandFromFault(ds402_state goalState) {

    switch (goalState) {
    case DS402_STATE_FAULT:
        return DS402_COMMAND_SHUTDOWN;
    case DS402_STATE_SWITCH_ON_DISABLED:
        return DS402_COMMAND_RESET_FAULT;
    default:
        return DS402_COMMAND_NOCHANGE;
    }
}

int driveTool_nextCommandToGoalState(ds402_state stateCurrent,
                                     ds402_state stateGoal) {

    switch (stateCurrent) {
    case DS402_STATE_SWITCH_ON_DISABLED:
        return driveTool_nextCommandFromDisabled(stateGoal);
    case DS402_STATE_READY_TO_SWITCH_ON:
        return driveTool_nextCommandFromReadyToSwitchOn(stateGoal);
    case DS402_STATE_SWITCHED_ON:
        return driveTool_nextCommandFromSwitchedOn(stateGoal);
    case DS402_STATE_OPERATION_ENABLED:
        return driveTool_nextCommandFromEnabled(stateGoal);
    case DS402_STATE_FAULT:
    case DS402_STATE_FAULT_REACTION_ACTIVE:
        return driveTool_nextCommandFromFault(stateGoal);
    case DS402_STATE_QUICK_STOP_ACTIVE:
        return DS402_COMMAND_NOCHANGE;
    default:
        syslog(LOG_ERR, "Unknown DS402 state %d", stateCurrent);
        return -1;
    }
}

int driveTool_sdoDownload(ec_master_t *master, uint16_t slave_position,
                          uint16_t index, uint8_t subindex, const uint8_t *data,
                          size_t data_size, uint32_t *abort_code,
                          int maxRetryTimes, unsigned int sleepTime) {

    int count = 0;
    int ret = 0;
    while (count < maxRetryTimes) {
        ret = ecrt_master_sdo_download(master, slave_position, index, subindex,
                                       data, data_size, abort_code);
        if (ret == 0) {
            break;
        }

        count++;

        sleep(sleepTime);
    }

    return ret;
}
