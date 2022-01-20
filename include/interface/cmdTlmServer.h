#ifndef CMDTLMSERVER_H
#define CMDTLMSERVER_H

#include <stdbool.h>
#include <pthread.h>
#include <glib.h>
#include <mqueue.h>

#include "circular_buffer.h"

typedef struct _serverInfo {
    // Pointer of the server name
    char *pName;
    // Timeout for blocking call in millisecond. If -1, means never timeout.
    int timeout;
    // Size of the telemetry message in byte. If there are multiple telemetry
    // structures, use the biggest size.
    unsigned int sizeMsgTlm;
    // Socket to listen to the connection request
    int socketListen;
    // Socket to connect to the TCP/IP client
    int socketConnect;
    // Thread to run the server
    pthread_t thread;
    // Is started to listen to the command and write the telemetry or not.
    // This value is set to be true when the thread is ready, false when the
    // software is ready to close the server.
    bool isStart;
    // Server status with the enum 'ServerStatus'
    int serverStatus;
    // Pointer of the name of command status queue
    char *pQueueNameCmdStatus;
    // Message queue of the command status
    mqd_t msgQueueCmdStatus;
    // Pointer of the command status used in message queue
    gpointer *pMsgCmdStatus;
    // Pointer of the name of telemetry queue
    char *pQueueNameTlm;
    // Message queue of the telemetry
    mqd_t msgQueueTlm;
    // Pointer of the telemetry message in message queue
    gpointer *pMsgTlm;
    // Is the commander or not
    bool isCommander;
    // Command buffer to write the new command
    cbuf_handle_t cmdMsgBuffer;
} serverInfo_t;

typedef enum {
    // TCP/IP server is disconnected with the TCP/IP client
    ServerStatus_Disconnected = 1,
    // TCP/IP server is connected with the TCP/IP client
    ServerStatus_Connected = 2,
    // TCP/IP server exits
    ServerStatus_Exit = 3,
} ServerStatus;

// Initialize the server.
// The user needs to provide the following inputs:
// - pName: Pointer of the server name
// - timeout: Timeout for blocking call in millisecond. If the value is <=0,
//            it will be reset to be -1 internally, which means never timeout.
// - sizeMsgTlm: Size of the telemetry message in byte.
// - port: Port to listen to the connection request.
// - maxNumQueueTlm: Maximum number of telemetry messages that can be queued for
//                   send at once.
// - cmdMsgBuffer: Command buffer to write the new commands.
// The server imformation will be put in 'pServerInfo'.
// Return 0 if success, otherwise, return -1.
int cmdTlmServer_init(serverInfo_t *pServerInfo, char *pName, int timeout,
                      unsigned int sizeMsgTlm, int port, long maxNumQueueTlm,
                      cbuf_handle_t cmdMsgBuffer);

// Run the server in a new thread.
// Return 0 if success, otherwise, return -1.
int cmdTlmServer_runInNewThread(serverInfo_t *pServerInfo);

// Basic close of the server. This will close the sockets and free the allocated
// memory.
void cmdTlmServer_basicClose(serverInfo_t *pServerInfo);

// Close the server thoroughly. This is used in the shutdown process.
void cmdTlmServer_close(serverInfo_t *pServerInfo);

// Sent the command status to message queue with the inputs of server
// information, counter, command status (enum: CmdStatus), estimated duration of
// command in second, and reason if the command failed (put "" if nothing to
// report). If "reason" is longer than the buffer defined in
// commandStatusStructure_t, it will be truncated to fit.
// Return 0 if success, otherwise, return -1.
int cmdTlmServer_sendCmdStatusToMsgQueue(serverInfo_t *pServerInfo,
                                         unsigned int counter,
                                         unsigned int cmdStatus,
                                         double duration, const char *reason);

#endif // CMDTLMSERVER_H
