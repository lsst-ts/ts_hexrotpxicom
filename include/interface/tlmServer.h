#ifndef TLMSERVER_H
#define TLMSERVER_H

#include <stdint.h>

typedef enum {
    // Many compilers assign the first element in an enumerated type to the
    // value 0. This could be used for unknown status.
    ServerStatus_Unknown = 0,
    // TCP/IP server is disconnected with TCP/IP client
    ServerStatus_Disconnected = 1,
    // TCP/IP server is connected with TCP/IP client
    ServerStatus_Connected = 2,
    // TCP/IP server exists
    ServerStatus_Exit = 3,
} ServerStatus;

typedef struct _serverInfo {
    // Protocol family
    int family;
    // Port of command
    uint16_t cmdPort;
    // Port of telemetry
    uint16_t tlmPort;
    // Telemetry send rate
    uint16_t tlmSendRate;
    // Timeout in millisecond
    int timeout;
    // Maximum number of telemetry messages that can be queued for send at once
    long maxNumQueueTel;
} serverInfo_t;

// Initialize the telemetry server of graphical user interface (GUI) with the
// specified server information and size of telemetry structure in byte (>0).
// If there are multiple telemetry structures, use the biggest size.
// Return 0 if success, otherwise, return -1.
int telServer_initGui(serverInfo_t *telServerGui, unsigned int sizeTel);

// Close the telemetry server and the client connection, if any for the
// graphical user interface (GUI). This is used in the shutdown process.
void telServer_closeGui(void);

// Initialize the telemetry server of data distribution system (DDS) with the
// specified server information and size of telemetry structure in byte (>0).
// If there are multiple telemetry structures, use the biggest size.
// Return 0 if success, otherwise, return -1.
int telServer_initDds(serverInfo_t *telServerDds, unsigned int sizeTel);

// Close the telemetry server and the client connection, if any for the data
// distribution system (DDS). This is used in the shutdown process.
void telServer_closeDds(void);

#endif
