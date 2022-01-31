#ifndef CMDTLMSERVER_H
#define CMDTLMSERVER_H

#include <stdbool.h>
#include <pthread.h>
#include <glib.h>
#include <mqueue.h>

#include "circular_buffer.h"

typedef struct _serverInfo {
    // Server name
    char *pName;
    // Timeout for blocking call in millisecond. If -1, means never timeout.
    int timeout;
    // The maximum size of data structure that will be received from controller,
    // in bytes. The received data will be sent to the TCP/IP client.
    // For the telemetry, this is the size of the configuration or telemetry
    // struct, whichever is larger.
    unsigned int sizeMsgTlm;
    // Socket to listen to the connection request
    int socketListen;
    // Socket to connect to the TCP/IP client
    int socketConnect;
    // Thread to run the server
    pthread_t threadServer;
    // Thread to send the telemetry and command status to the client
    pthread_t threadTlm;
    // Is ready to listen to the command or not.
    // This value is set to be true when the thread is ready, false when the
    // software is ready to close the server.
    bool isReadyServer;
    // Is ready to write the telemetry or not.
    // This value is set to be true when the thread is ready, false when the
    // software is ready to close the server.
    bool isReadyTlm;
    // Is the close of connection detected when sending the data to the
    // connected socket or not
    bool isCloseConnDetected;
    // Server status with the enum 'ServerStatus'
    int serverStatus;
    // Pointer to the name of command status queue. This is required by
    // mq_open() to identify the queue.
    char *pQueueNameCmdStatus;
    // Message queue of the command status
    mqd_t msgQueueCmdStatus;
    // Pointer to the command status used in message queue
    gpointer *pMsgCmdStatus;
    // Pointer to the name of telemetry queue. This is required by mq_open()
    // to identify the queue.
    char *pQueueNameTlm;
    // Message queue of the telemetry
    mqd_t msgQueueTlm;
    // Pointer to the telemetry message in message queue
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
// - pName: Pointer to the server name
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

// Send a command status to the message queue. The arguments are:
// - pServerInfo: pointer to the server information
// - counter: counter value of command being acknowledged
// - cmdStatus: command status (enum: 'CmdStatus')
// - duration: estimated duration (seconds); 0 if already done
// - pReason: reason the command failed... (put "" if nothing to report. If
//   "pReason" is longer than the buffer defined in commandStatusStructure_t,
//   it will be truncated to fit.)
// Return 0 if success, otherwise, return -1.
int cmdTlmServer_sendCmdStatusToMsgQueue(serverInfo_t *pServerInfo,
                                         unsigned int counter,
                                         unsigned int cmdStatus,
                                         double duration, const char *pReason);

// Send a telemetry message to the message queue. The arguments are:
// - pServerInfo: pointer to the server information
// - pMsg: pointer to the telemetry message
// - sizeMsg: size of the message in bytes
// - priority: A nonnegative integer that specifies the priority of this
//   message. See the mq_send() for the details.
// Return 0 if success, otherwise, return -1.
int cmdTlmServer_sendTlmToMsgQueue(serverInfo_t *pServerInfo, const char *pMsg,
                                   size_t sizeMsg, unsigned int priority);

#endif // CMDTLMSERVER_H
