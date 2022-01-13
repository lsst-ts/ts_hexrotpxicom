#ifndef COMMANDSTRUCTURE_H
#define COMMANDSTRUCTURE_H

#include <time.h>

// Buffer's length of reason in command status (commandStatusStructure_t)
#define LENGTH_CMD_STATUS_REASON 50

typedef struct __attribute__((__packed__)) _headerStructure {
    // Synchronization pattern used to differentiate the command is from the DDS
    // or GUI (enum: SyncPattern)
    unsigned short syncpattern;
    // Frame ID used by the LabVIEW EUI to identify the kind of incoming TCP/IP
    // packet, and decide the size of packet (in byte) to decode the message
    unsigned short frameId;
    // Counter of the packet, the value should increase by 1 after each time
    // of filling the data of TCP/IP packet
    unsigned int counter;
    // TAI (International Atomic Time) time in second (as provided by CLOCK_TAI)
    time_t tv_sec;
    // TAI (International Atomic Time) time fraction in nanosecond
    long tv_nsec;
} headerStructure_t;

typedef struct __attribute__((__packed__)) _commandStreamStructure {
    unsigned short syncpattern;
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
    // Command status (enum: CmdStatus)
    unsigned int cmdStatus;
    // Estimated duration of command execution in second. The value of 0 means
    // the command excution is supposed to be fast instead of "zero" time.
    double duration;
    // Reason of the failure of command
    char reason[LENGTH_CMD_STATUS_REASON];
} commandStatusStructure_t;

typedef enum {
    // Many compilers assign the first element in an enumerated type to the
    // value 0. This could be used for unknown sync pattern.
    SyncPattern_Unknown = 0,
    // Sync pattern of GUI
    SyncPattern_GUI = 0x4444,
    // Sync pattern of DDS
    SyncPattern_DDS = 0x5555,
} SyncPattern;

typedef enum {
    // Unkown status
    CmdStatus_Unknown = 0,
    // Command is OK to pass to the Simulink model
    CmdStatus_OK = 1,
    // Command is not OK to pass to the Simulink model
    CmdStatus_NotOK = 2,
} CmdStatus;

#endif // COMMANDSTRUCTURE_H
