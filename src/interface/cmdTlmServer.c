#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <syslog.h>
#include <errno.h>

#include "utility.h"
#include "tcpServer.h"
#include "cmdTlmServer.h"

void cmdTlmServer_basicClose(serverInfo_t *pServerInfo) {
    // Close the sockets
    if (pServerInfo->socketConnect != -1) {
        tcpServer_close(pServerInfo->socketConnect);
        pServerInfo->socketConnect = -1;
    }

    if (pServerInfo->socketListen != -1) {
        tcpServer_close(pServerInfo->socketListen);
        pServerInfo->socketListen = -1;
    }

    // Close the message queues
    // Note that the order is reversed compared with the
    // cmdTlmServer_prepareMsgQueue()
    if (pServerInfo->pMsgTlm != NULL) {
        g_free(pServerInfo->pMsgTlm);
        pServerInfo->pMsgTlm = NULL;

        mq_close(pServerInfo->msgQueueTlm);
        mq_unlink(pServerInfo->pQueueNameTlm);
    }

    if (strncmp(pServerInfo->pQueueNameTlm, "", 1) != 0) {
        free(pServerInfo->pQueueNameTlm);
        pServerInfo->pQueueNameTlm = "";
    }

    if (pServerInfo->pMsgCmdStatus != NULL) {
        g_free(pServerInfo->pMsgCmdStatus);
        pServerInfo->pMsgCmdStatus = NULL;

        mq_close(pServerInfo->msgQueueCmdStatus);
        mq_unlink(pServerInfo->pQueueNameCmdStatus);
    }

    if (strncmp(pServerInfo->pQueueNameCmdStatus, "", 1) != 0) {
        free(pServerInfo->pQueueNameCmdStatus);
        pServerInfo->pQueueNameCmdStatus = "";
    }
}

void cmdTlmServer_close(serverInfo_t *pServerInfo) {
    syslog(LOG_NOTICE, "Closing the %s server.", pServerInfo->pName);

    int error = 0;
    if (pServerInfo->isStart) {
        pServerInfo->isStart = false;
        error = pthread_join(pServerInfo->thread, NULL);
    }

    if (error != 0) {
        syslog(LOG_ERR, "Failed the waiting of thread in the %s server.",
               pServerInfo->pName);
    }

    cmdTlmServer_basicClose(pServerInfo);
}

// Initialize the server information with the server name, timeout, size of
// telemetry message in byte, and the command buffer.
static void cmdTlmServer_initServerInfo(serverInfo_t *pServerInfo, char *pName,
                                        int timeout, unsigned int sizeMsgTlm,
                                        cbuf_handle_t cmdMsgBuffer) {
    pServerInfo->pName = pName;
    // Use the -1 instead if the input timeout is <= 0
    pServerInfo->timeout = (timeout <= 0) ? -1 : timeout;
    pServerInfo->sizeMsgTlm = sizeMsgTlm;

    pServerInfo->socketListen = -1;
    pServerInfo->socketConnect = -1;

    pServerInfo->isStart = false;
    pServerInfo->serverStatus = ServerStatus_Disconnected;

    pServerInfo->pQueueNameCmdStatus = "";
    pServerInfo->msgQueueCmdStatus = (mqd_t)(-1);
    pServerInfo->pMsgCmdStatus = NULL;

    pServerInfo->pQueueNameTlm = "";
    pServerInfo->msgQueueTlm = (mqd_t)(-1);
    pServerInfo->pMsgTlm = NULL;

    pServerInfo->isCommander = false;

    pServerInfo->cmdMsgBuffer = cmdMsgBuffer;
}

// Generate the name of message queue with the base name and identifier.
// Return the name.
static char *cmdTlmServer_genNameMsgQueue(char *pNameBase, int identifier) {
    // Allocate the enough size of string buffer (10 is a random chosen value)
    char str[10];
    sprintf(str, "%d", identifier);
    return joinStr(pNameBase, str);
}

// Create the message queue (non-blocking) with the maximum number of message,
// message size in byte, and queue's name.
// Return the message queue.
static mqd_t cmdTlmServer_createMsgQueue(long maxNumMsg, long sizeMsg,
                                         char *pName) {
    struct mq_attr attr;
    attr.mq_flags = O_NONBLOCK;
    attr.mq_maxmsg = maxNumMsg;
    attr.mq_msgsize = sizeMsg;
    return mq_open(pName, O_CREAT | O_RDWR | O_NONBLOCK, 0, &attr);
}

// Prepare the message queues with an identifier used to differentiate
// different queue messages. User also needs to provide the maximum number of
// element in telemetry queue.
// Return 0 if success, otherwise, return -1.
static int cmdTlmServer_prepareMsgQueue(serverInfo_t *pServerInfo,
                                        int identifier, long maxNumQueueTlm) {
    // Generate the names of message queues
    pServerInfo->pQueueNameCmdStatus =
        cmdTlmServer_genNameMsgQueue("/queueCmdStatus", identifier);
    pServerInfo->pQueueNameTlm =
        cmdTlmServer_genNameMsgQueue("/queueTlm", identifier);

    // Message queue of comomand status

    // Note that the value of 4 is a random number. I expect there will be only
    // 1 in the real running.
    // The value here is just by try-and-error. If this value is too big, the
    // kernel will reject it for the memory allocation.
    // It is noted that the kernel will restrict the allocated memory for the
    // message queue. Therefore, this value can not be too big.
    pServerInfo->msgQueueCmdStatus =
        cmdTlmServer_createMsgQueue(4, (long)sizeof(commandStatusStructure_t),
                                    pServerInfo->pQueueNameCmdStatus);
    if (pServerInfo->msgQueueCmdStatus == (mqd_t)(-1)) {
        syslog(LOG_ERR, "Failed to create the message queue of command status "
                        "in %s server.",
               pServerInfo->pName);
        return -1;
    }

    // Allocate the memory of message queue of command status
    pServerInfo->pMsgCmdStatus =
        g_malloc0_n((gsize)sizeof(commandStatusStructure_t), 2);

    // Message queue of telemetry
    pServerInfo->msgQueueTlm = cmdTlmServer_createMsgQueue(
        maxNumQueueTlm, pServerInfo->sizeMsgTlm, pServerInfo->pQueueNameTlm);
    if (pServerInfo->msgQueueTlm == (mqd_t)(-1)) {
        syslog(LOG_ERR,
               "Failed to create the message queue of telemetry in %s server.",
               pServerInfo->pName);
        return -1;
    }

    // Allocate the memory of message queue of telemetry
    pServerInfo->pMsgTlm = g_malloc0_n((gsize)pServerInfo->sizeMsgTlm, 2);

    return 0;
}

// Receive the message with a timeout value (in millisecond). The received
// message will be written to the memory pointed to by pCmdMsg.
// Return the error status or the received number of byte.
// If the return value < 0, it means the timeout from poll() or error from
// poll() or recv().
// Note. If we do not set the timeout when using the recv(), the server may not
// be able to close this thread because the recv() is a blocking call.
static int cmdTlmServer_recv(commandStreamStructure_t *pCmdMsg,
                             struct pollfd *pFds, int timeout) {

    int numEvent = poll(pFds, 1, timeout);
    if (numEvent <= 0) {
        // error (-1) or timeout (0)
        return -1;
    }

    return recv(pFds->fd, pCmdMsg, sizeof(commandStreamStructure_t), 0);
}

// Send the last command status in message queue if any.
// Return 0 if sending successfully. Else, -1.
static int cmdTlmServer_sendLastCmdStateInMsgQueue(serverInfo_t *pServerInfo) {
    int bytesReceived = mq_receive(pServerInfo->msgQueueCmdStatus,
                                   (char *)pServerInfo->pMsgCmdStatus,
                                   sizeof(commandStatusStructure_t), NULL);

    // Send the message only when there is the available data.
    // Writing to a closed socket will raise SIGPIPE.
    // For the refereces of this and how to avoid, follow:
    // https://newbedev.com/how-to-prevent-sigpipes-or-handle-them-properly
    // https://stackoverflow.com/questions/26752649/so-nosigpipe-was-not-declared
    // https://stackoverflow.com/questions/19172804/crash-when-sending-data-without-connection-via-socket-in-linux?rq=1
    int error = 0;
    if (bytesReceived > 0) {
        error = send(pServerInfo->socketConnect, pServerInfo->pMsgCmdStatus,
                     bytesReceived, MSG_NOSIGNAL);
        if (error < 0) {
            // Return if the connection is broken
            return -1;
        }
    }

    return 0;
}

// Close the connection with the TCP/IP client. This function will reset the
// file descriptor of 'pollfd' structure. The server will be put into the
// Disconnected state and wait for the new connection request.
static void cmdTlmServer_closeConn(serverInfo_t *pServerInfo,
                                   struct pollfd *pFds) {

    syslog(LOG_NOTICE, "Connection socket being reset in %s server.",
           pServerInfo->pName);
    tcpServer_close(pServerInfo->socketConnect);

    pServerInfo->socketConnect = -1;
    pFds->fd = -1;

    pServerInfo->serverStatus = ServerStatus_Disconnected;
    syslog(LOG_NOTICE, "The state of %s server is disconnected.",
           pServerInfo->pName);
}

// Check the command message (pCmdMsg) is valid or not. The status of command
// will be filled in pCmdStatus if the command is invalid. This function will
// check the commander as well.
// Return true if valid. Else, false.
static bool cmdTlmServer_isCmdValid(commandStatusStructure_t *pCmdStatus,
                                    commandStreamStructure_t *pCmdMsg,
                                    bool isCommander) {
    char *reasonCmdFail = "";
    bool isCmdValid = true;

    // Check the commander is from CSC/GUI or not
    if (isCmdValid && !((pCmdMsg->commander == Commander_GUI) ||
                        (pCmdMsg->commander == Commander_CSC))) {
        reasonCmdFail = "Invalid commander";
        isCmdValid = false;
    }

    // Check the CSC is the commander or not
    // For the GUI, it can always give the command
    if (isCmdValid && !isCommander && (pCmdMsg->commander == Commander_CSC)) {
        reasonCmdFail = "Is not the commander";
        isCmdValid = false;
    }

    if (!isCmdValid) {
        pCmdStatus->header.frameId = FrameId_CmdStatus;
        pCmdStatus->header.counter = pCmdMsg->counter;
        pCmdStatus->cmdStatus = CmdStatus_NotOK;
        pCmdStatus->duration = 0;
        strncpy(&pCmdStatus->reason[0], reasonCmdFail,
                LENGTH_CMD_STATUS_REASON);
        pCmdStatus->reason[LENGTH_CMD_STATUS_REASON - 1] = '\0';
    }

    return isCmdValid;
}

// Run the server.
// Note: The data types of input and output are required for pthread_create().
// The input needs to cast to the correct data type.
static void *cmdTlmServer_run(void *pData) {

    // Get the server information
    serverInfo_t *pServerInfo = (serverInfo_t *)pData;

    // Wait until begins to run the server's job
    while (!pServerInfo->isStart) {
        sleep(1);
    }

    // Loop delay time
    // 50 milliseconds (= 20 Hz)
    struct timespec ts50;
    ts50.tv_nsec = 50000000;
    ts50.tv_sec = 0;

    // Use poll() for the connected socket
    struct pollfd fds;
    fds.events = POLLIN;

    // Wait for the connection to the server
    syslog(LOG_NOTICE, "Waiting for the connection request in %s server.",
           pServerInfo->pName);

    // Run the server
    int error;
    while (pServerInfo->isStart) {

        // New command message
        commandStreamStructure_t cmdMsg;
        commandStatusStructure_t cmdStatus;

        // State machine to deal with the connection/disconnection with
        // TCP/IP client
        switch (pServerInfo->serverStatus) {

        // Look for the connection with TCP/IP client
        case ServerStatus_Disconnected:
            // Wait for the connection request
            pServerInfo->socketConnect = tcpServer_accept(
                pServerInfo->socketListen, pServerInfo->timeout);
            if (pServerInfo->socketConnect == -1) {
                break;
            } else {
                pServerInfo->serverStatus = ServerStatus_Connected;

                syslog(LOG_NOTICE,
                       "The state of %s server is connected. socket = %d.",
                       pServerInfo->pName, pServerInfo->socketConnect);

                // Make use of poll() to receive the message
                fds.fd = pServerInfo->socketConnect;
            }
            break;

        // Connected with the TCP/IP client, look for commands
        case ServerStatus_Connected:

            // Reply the last command status from commanding.c
            error = cmdTlmServer_sendLastCmdStateInMsgQueue(pServerInfo);

            // We were connected but the connection has been broken
            if (error < 0) {
                cmdTlmServer_closeConn(pServerInfo, &fds);
                break;
            }

            // Check the new command
            error = cmdTlmServer_recv(&cmdMsg, &fds, pServerInfo->timeout);

            // Ignore the timeout or error
            if (error < 0) {
                break;
            }

            // Client closes the connection
            if (error == 0) {
                cmdTlmServer_closeConn(pServerInfo, &fds);
                break;
            }

            // Check the command is valid or not
            bool isCmdValid = cmdTlmServer_isCmdValid(&cmdStatus, &cmdMsg,
                                                      pServerInfo->isCommander);

            if (isCmdValid) {
                // Write command to command message buffer
                if (circular_buf_put(pServerInfo->cmdMsgBuffer, cmdMsg)) {
                    syslog(LOG_NOTICE,
                           "The command message is overwritten in %s server.",
                           pServerInfo->pName);
                }
            }

            // Send the NotOK message to client if needed

            // Writing to a closed socket will raise SIGPIPE. Check
            // cmdTlmServer_sendLastCmdStateInMsgQueue() for the details
            // (references) that how to avoid the termination signal
            if (!isCmdValid) {
                error = send(pServerInfo->socketConnect, &cmdStatus,
                             sizeof(commandStatusStructure_t), MSG_NOSIGNAL);
            } else {
                error = 0;
            }

            // We were connected but the connection has been broken
            if (error < 0) {
                cmdTlmServer_closeConn(pServerInfo, &fds);
                break;
            }

            break;

        default:
            break;
        }

        // Give other functions a chance to run
        nanosleep(&ts50, NULL);
    }

    cmdTlmServer_basicClose(pServerInfo);
    pServerInfo->serverStatus = ServerStatus_Exit;

    return 0;
}

int cmdTlmServer_runInNewThread(serverInfo_t *pServerInfo) {
    int error = pthread_create(&pServerInfo->thread, NULL, cmdTlmServer_run,
                               (void *)pServerInfo);
    struct sched_param param;
    if (error != 0) {
        syslog(LOG_ERR, "Failed to create the thread in %s server.",
               pServerInfo->pName);
        return -1;
    } else {
        // Set priority of this thread
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if ((error = pthread_setschedparam(pServerInfo->thread, SCHED_OTHER,
                                           &param)) != 0) {
            syslog(LOG_ERR, "Can't initiaze the thread priority in %s server.",
                   pServerInfo->pName);

            pthread_exit(&pServerInfo->thread);
            return -1;
        }
        pServerInfo->isStart = true;
    }

    return 0;
}

int cmdTlmServer_init(serverInfo_t *pServerInfo, char *pName, int timeout,
                      unsigned int sizeMsgTlm, int port, long maxNumQueueTlm,
                      cbuf_handle_t cmdMsgBuffer) {
    // Initialize the server information
    cmdTlmServer_initServerInfo(pServerInfo, pName, timeout, sizeMsgTlm,
                                cmdMsgBuffer);

    // Create the socket to listen the connection request
    // Only allow a single connection
    // Hard-code to use the IPv4 at this moment to simplify the implementation
    pServerInfo->socketListen = tcpServer_open(AF_INET, port, 1);
    if (pServerInfo->socketListen == -1) {
        syslog(LOG_ERR,
               "Failed to create the socket to listen the connection request "
               "in %s server.",
               pName);

        return -1;
    }

    syslog(LOG_NOTICE, "Listening socket is opened at port %d for %s server.",
           port, pName);

    // Prepare the message queues
    int error = cmdTlmServer_prepareMsgQueue(pServerInfo, port, maxNumQueueTlm);
    if (error == -1) {
        cmdTlmServer_basicClose(pServerInfo);
        return -1;
    }

    return 0;
}

int cmdTlmServer_sendCmdStatusToMsgQueue(serverInfo_t *pServerInfo,
                                         unsigned int counter,
                                         unsigned int cmdStatus,
                                         double duration, const char *reason) {

    // Fill the command status
    commandStatusStructure_t cmdStatusSend;
    cmdStatusSend.header.frameId = FrameId_CmdStatus;
    cmdStatusSend.header.counter = counter;
    cmdStatusSend.cmdStatus = cmdStatus;
    cmdStatusSend.duration = duration;

    strncpy(&cmdStatusSend.reason[0], reason, LENGTH_CMD_STATUS_REASON);
    cmdStatusSend.reason[LENGTH_CMD_STATUS_REASON - 1] = '\0';

    // Send the message
    int error = mq_send(pServerInfo->msgQueueCmdStatus, (char *)&cmdStatusSend,
                        sizeof(commandStatusStructure_t), 0);
    if (error < 0) {
        syslog(LOG_ERR, "Fail to send the command status: %s", strerror(errno));
    }

    return error;
}
