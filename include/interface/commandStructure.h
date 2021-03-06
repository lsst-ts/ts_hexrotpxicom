#ifndef COMMANDSTRUCTURE_H
#define COMMANDSTRUCTURE_H

#include <time.h>

// Buffer's length of reason in command status (commandStatusStructure_t)
#define LENGTH_CMD_STATUS_REASON 50

typedef struct __attribute__((__packed__)) _headerStructure {
    // Frame ID to identify the kind of TCP/IP packet.
    // (enum: 'FrameId')
    unsigned int frameId;
    // Counter of the packet, the value should increase by 1 after each time
    // of filling the data of TCP/IP packet
    unsigned int counter;
    // TAI (International Atomic Time) time in second
    time_t tv_sec;
    // TAI (International Atomic Time) time fraction in nanosecond
    long tv_nsec;
} headerStructure_t;

typedef struct __attribute__((__packed__)) _commandStreamStructure {
    // Commander of command (enum: 'Commander').
    unsigned int commander;
    // Counter of command
    unsigned int counter;
    // Command name
    unsigned int cmd;
    // Parameters used in the command
    double param1;
    double param2;
    double param3;
    double param4;
    double param5;
    double param6;
} commandStreamStructure_t;

typedef struct __attribute__((__packed__)) _commandStatusStructure {
    // Header of packet
    headerStructure_t header;
    // Command status (enum: 'CmdStatus')
    unsigned int cmdStatus;
    // Estimated duration of command execution in second. The value of 0 means
    // the command excution is supposed to be fast instead of "zero" time.
    double duration;
    // Reason of the failure of command
    char reason[LENGTH_CMD_STATUS_REASON];
} commandStatusStructure_t;

typedef enum {
    // Graphical user interface
    Commander_GUI = 1,
    // Commandable SAL component
    Commander_CSC = 2,
    // Controller internal use. This is used in the hexapod controller code to
    // hack the Simulink model. This value should be removed after fixing the
    // hexapod Simulink model.
    Commander_Self = 3,
} Commander;

typedef enum {
    // Command is OK
    CmdStatus_OK = 1,
    // Command is not OK
    CmdStatus_NotOK = 2,
} CmdStatus;

typedef enum {
    // Command status
    FrameId_CmdStatus = 1,
    // Telemetry
    FrameId_Tlm = 2,
    // Configuration
    FrameId_Config = 3,
} FrameId;

#endif // COMMANDSTRUCTURE_H
