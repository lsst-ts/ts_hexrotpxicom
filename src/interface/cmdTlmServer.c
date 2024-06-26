#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include "cmdTlmServer.h"
#include "tcpServer.h"
#include "utility.h"

// Exit the running thread. The arguments are:
// - thread: running thread
// - pIsReady: pointer to the status of thread
// - pNameThread: pointer to the name of thread
// - pNameServer: pointer to the name of server
static void cmdTlmServer_exitThread(pthread_t thread, bool *pIsReady,
                                    char *pNameThread, char *pNameServer) {
    int error = 0;
    if (*pIsReady) {
        *pIsReady = false;
        error = pthread_join(thread, NULL);
    }

    if (error != 0) {
        syslog(LOG_ERR,
               "Failed the waiting of %s thread in the %s server. "
               "Cancelling it...",
               pNameThread, pNameServer);

        error = pthread_cancel(thread);
    }

    if (error != 0) {
        syslog(LOG_ERR, "Failed to cancel the %s thread in the %s server.",
               pNameThread, pNameServer);
    }
}

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

    // Notice the order here is reversed from cmdTlmServer_runInNewThread()
    cmdTlmServer_exitThread(pServerInfo->threadTlm, &pServerInfo->isReadyTlm,
                            "telemetry", pServerInfo->pName);
    cmdTlmServer_exitThread(pServerInfo->threadServer,
                            &pServerInfo->isReadyServer, "server",
                            pServerInfo->pName);

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

    pServerInfo->isReadyServer = false;
    pServerInfo->isReadyTlm = false;
    pServerInfo->isCloseConnDetected = false;
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
// The identifier is needed if we have multiple servers such as GUI and CSC.
// At that time, we can use the port to be the identifier to have the unique
// name for different servers.
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

    // Note that the value of 4 is a random number, which is used for: the Max.
    // # of messages on the queue.
    // The value here is just by try-and-error. If this value is too big, the
    // kernel will reject it for the memory allocation.
    // It is noted that the kernel will restrict the allocated memory for the
    // message queue. Therefore, this value can not be too big.
    long MAX_NUM_MSG_CMD_STATUS = 4;
    pServerInfo->msgQueueCmdStatus = cmdTlmServer_createMsgQueue(
        MAX_NUM_MSG_CMD_STATUS, (long)sizeof(commandStatusStructure_t),
        pServerInfo->pQueueNameCmdStatus);
    if (pServerInfo->msgQueueCmdStatus == (mqd_t)(-1)) {
        syslog(LOG_ERR,
               "Failed to create the message queue of command status "
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
// poll() or recv(). If 0, it means the client closed the connection.
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

// Pop the oldest command status message from the message queue, if one is
// available, and send it to the client, which depends on 'pServerInfo' (can be
// CSC or GUI).
// Return 0 if there was no message, or there was a message and it was sent
// successfully.
// Return -1 if there was a message and sending failed because the socket was
// closed. The message will still be popped from the message queue.
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
    if ((bytesReceived > 0) &&
        (pServerInfo->serverStatus == ServerStatus_Connected)) {
        error = send(pServerInfo->socketConnect, pServerInfo->pMsgCmdStatus,
                     bytesReceived, MSG_NOSIGNAL);
        if (error < 0) {
            // Return if the connection is broken
            return -1;
        }
    }

    return 0;
}

// Pop the oldest telemetry message from the message queue, if one is
// available, and send it to the client, which depends on 'pServerInfo' (can be
// CSC or GUI).
// Return 0 if there was no message, or there was a message and it was sent
// successfully.
// Return -1 if there was a message and sending failed because the socket was
// closed. The message will still be popped from the message queue.
static int cmdTlmServer_sendLastTlmInMsgQueue(serverInfo_t *pServerInfo) {
    int bytesReceived =
        mq_receive(pServerInfo->msgQueueTlm, (char *)pServerInfo->pMsgTlm,
                   pServerInfo->sizeMsgTlm, NULL);

    // Send the message only when there is the available data.
    // Writing to a closed socket will raise SIGPIPE.
    // Check cmdTlmServer_sendLastCmdStateInMsgQueue() for the details
    // (references).
    int error = 0;
    if ((bytesReceived > 0) &&
        (pServerInfo->serverStatus == ServerStatus_Connected)) {
        error = send(pServerInfo->socketConnect, pServerInfo->pMsgTlm,
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

// Check if the command ('pCmdMsg') is authorized or not.
// Return true if authorized, return false and fill in 'pCmdStatus' if not.
// Commands from the GUI are always authorized.
// Commands from the CSC are only authorized if the CSC has control.
// Commands with an invalid commander field are never authorized.
// User needs to fill in the information that this server is the commander or
// not by 'isCommander'.
static bool cmdTlmServer_isCmdAuthorized(commandStatusStructure_t *pCmdStatus,
                                         commandStreamStructure_t *pCmdMsg,
                                         bool isCommander) {
    char *reasonCmdFail = "";
    bool isCmdAuthorized = true;

    // Check the commander is from CSC/GUI or not
    if (isCmdAuthorized && !((pCmdMsg->commander == Commander_GUI) ||
                             (pCmdMsg->commander == Commander_CSC))) {
        reasonCmdFail = "Unrecognized commander; must be one of GUI or CSC";
        isCmdAuthorized = false;
    }

    // Check the CSC is the commander or not
    // For the GUI, it can always give the command
    if (isCmdAuthorized && !isCommander &&
        (pCmdMsg->commander == Commander_CSC)) {
        reasonCmdFail = "The CSC is not the commander";
        isCmdAuthorized = false;
    }

    if (!isCmdAuthorized) {
        pCmdStatus->header.frameId = FrameId_CmdStatus;
        pCmdStatus->header.counter = pCmdMsg->counter;
        pCmdStatus->cmdStatus = CmdStatus_NotOK;
        pCmdStatus->duration = 0;
        strncpy(&pCmdStatus->reason[0], reasonCmdFail,
                LENGTH_CMD_STATUS_REASON);
        pCmdStatus->reason[LENGTH_CMD_STATUS_REASON - 1] = '\0';
    }

    return isCmdAuthorized;
}

// Send the telemetry and command status to the client.
// Note: The data types of input and output are required for pthread_create().
// The input needs to cast to the correct data type.
static void *cmdTlmServer_sendTlmAndCmdStatus(void *pData) {

    // Get the server information
    serverInfo_t *pServerInfo = (serverInfo_t *)pData;

    // Loop delay time
    // 20 milliseconds (= 50 Hz)
    struct timespec ts20;
    ts20.tv_nsec = 20000000;
    ts20.tv_sec = 0;

    // Wait until begins to write the telemetry
    while (!pServerInfo->isReadyTlm) {
        nanosleep(&ts20, NULL);
    }

    // Write the telemetry and command status
    syslog(LOG_NOTICE, "The telemetry thread is running in the %s server.",
           pServerInfo->pName);

    int lastServerStatus = ServerStatus_Disconnected;
    while (pServerInfo->isReadyTlm) {
        // Reset the record of finding the closed connection and update the last
        // server status
        if ((lastServerStatus == ServerStatus_Disconnected) &&
            (pServerInfo->serverStatus == ServerStatus_Connected)) {
            pServerInfo->isCloseConnDetected = false;
        }

        lastServerStatus = pServerInfo->serverStatus;

        // Reply the last command status from commanding.c in controller code
        int error = 0;
        if (!pServerInfo->isCloseConnDetected && (error == 0)) {
            error = cmdTlmServer_sendLastCmdStateInMsgQueue(pServerInfo);
        }

        // Send the telemetry if any
        if (!pServerInfo->isCloseConnDetected && (error == 0)) {
            error = cmdTlmServer_sendLastTlmInMsgQueue(pServerInfo);
        }

        // Report if sending failed. The server thread will notice this and
        // change the server status to be disconnected in cmdTlmServer_run()
        if (error == -1) {
            pServerInfo->isCloseConnDetected = true;
        }

        nanosleep(&ts20, NULL);
    }

    return 0;
}

// Run the server.
// Note: The data types of input and output are required for pthread_create().
// The input needs to cast to the correct data type.
static void *cmdTlmServer_run(void *pData) {

    // Get the server information
    serverInfo_t *pServerInfo = (serverInfo_t *)pData;

    // Wait until begins to run the server's job
    while (!pServerInfo->isReadyServer) {
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
    while (pServerInfo->isReadyServer) {

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
                // Set the socket option of TCP_NODELAY
                int optVal = 1;
                error = setsockopt(pServerInfo->socketConnect, IPPROTO_TCP,
                                   TCP_NODELAY, &optVal, sizeof(optVal));
                if (error == -1) {
                    syslog(LOG_ERR,
                           "Failed to set the TCP_NODELAY in "
                           "connected socket in the %s server",
                           pServerInfo->pName);

                    cmdTlmServer_closeConn(pServerInfo, &fds);
                    break;
                }

                // Update the server status
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

            // Close the connection if the telemetry thread detected the
            // connection was closed
            if (pServerInfo->isCloseConnDetected) {
                cmdTlmServer_closeConn(pServerInfo, &fds);

                syslog(LOG_NOTICE,
                       "The telemetry thread found the connection "
                       "is closed in the %s server.",
                       pServerInfo->pName);
                break;
            }

            // Check the new command
            commandStreamStructure_t cmdMsg;
            int nbytes = cmdTlmServer_recv(&cmdMsg, &fds, pServerInfo->timeout);

            // Ignore the timeout or error
            if (nbytes < 0) {
                break;
            }

            // Client closes the connection
            if (nbytes == 0) {
                cmdTlmServer_closeConn(pServerInfo, &fds);
                break;
            }

            commandStatusStructure_t cmdStatus;
            bool isCmdAuthorized = cmdTlmServer_isCmdAuthorized(
                &cmdStatus, &cmdMsg, pServerInfo->isCommander);
            if (isCmdAuthorized) {
                // Write command to command message buffer
                if (circular_buf_put(pServerInfo->cmdMsgBuffer, cmdMsg)) {
                    syslog(LOG_NOTICE,
                           "The command message is overwritten in %s server.",
                           pServerInfo->pName);
                }
            }

            // Send the NotOK message to client by the message queue if needed
            if (!isCmdAuthorized) {
                error =
                    mq_send(pServerInfo->msgQueueCmdStatus, (char *)&cmdStatus,
                            sizeof(commandStatusStructure_t), 0);
            } else {
                error = 0;
            }

            if (error < 0) {
                syslog(LOG_ERR, "Fail to send the command status: %s",
                       strerror(errno));
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
    // Run the server thread
    int error = pthread_create(&pServerInfo->threadServer, NULL,
                               cmdTlmServer_run, (void *)pServerInfo);
    struct sched_param param;
    if (error != 0) {
        syslog(LOG_ERR, "Failed to create the server thread in %s server.",
               pServerInfo->pName);
        return -1;
    } else {
        // Set priority of this thread
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if ((error = pthread_setschedparam(pServerInfo->threadServer,
                                           SCHED_OTHER, &param)) != 0) {
            syslog(LOG_ERR,
                   "Can't initialize the server thread priority in %s server.",
                   pServerInfo->pName);

            pthread_cancel(pServerInfo->threadServer);
            return -1;
        }
    }

    // Run the telemetry thread
    error =
        pthread_create(&pServerInfo->threadTlm, NULL,
                       cmdTlmServer_sendTlmAndCmdStatus, (void *)pServerInfo);
    if (error != 0) {
        syslog(LOG_ERR, "Failed to create the telemetry thread in %s server.",
               pServerInfo->pName);

        pthread_cancel(pServerInfo->threadServer);
        return -1;
    }

    // Ready to run the server
    pServerInfo->isReadyServer = true;
    pServerInfo->isReadyTlm = true;

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
                                         double duration, const char *pReason) {

    // Fill the command status
    commandStatusStructure_t cmdStatusSend;
    cmdStatusSend.header.frameId = FrameId_CmdStatus;
    cmdStatusSend.header.counter = counter;
    cmdStatusSend.cmdStatus = cmdStatus;
    cmdStatusSend.duration = duration;

    strncpy(&cmdStatusSend.reason[0], pReason, LENGTH_CMD_STATUS_REASON);
    cmdStatusSend.reason[LENGTH_CMD_STATUS_REASON - 1] = '\0';

    // Send the message
    int error = mq_send(pServerInfo->msgQueueCmdStatus, (char *)&cmdStatusSend,
                        sizeof(commandStatusStructure_t), 0);
    if (error < 0) {
        syslog(LOG_ERR, "Fail to send the command status: %s", strerror(errno));
    }

    return error;
}

int cmdTlmServer_sendTlmToMsgQueue(serverInfo_t *pServerInfo, const char *pMsg,
                                   size_t sizeMsg) {
    int error = mq_send(pServerInfo->msgQueueTlm, pMsg, sizeMsg, 0);
    if (error < 0) {
        syslog(LOG_ERR, "Fail to send the telemetry: %s", strerror(errno));
    }

    return error;
}
