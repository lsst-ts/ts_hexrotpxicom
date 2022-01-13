#include <pthread.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <poll.h>
#include <string.h>

#include "circular_buffer.h"
#include "tcpServer.h"
#include "tlmServer.h"
#include "cmdServer.h"

// Socket to listen the connection request
static int socketListen = 0;

// Socket to connect to the TCP/IP client
static int socketConnect = 0;

// Timeout in millisecond
static int timeout = 0;

// Thread to listen the command
static pthread_t threadListenCmd;

// Is started to listen the command or not
// This value is set to be true when the thread is ready. False when the
// software is ready to close the server.
static bool isStartListenCmd = false;

// Names of command status queue
static const char queueNameCmdStatus[] = "/ddsSendCmdStatus";

// Pointer of command status used in the message queue
static commandStatusStructure_t *pMsgQueueCmdStatus;

// Globals accessible to the application code
extern volatile cbuf_handle_t cmdMsgBuffer;
extern volatile int gCommandSourceDDS;
volatile int gCmdDdsServerConnected = ServerStatus_Disconnected;

// Send the message queue of command status to DDS
mqd_t gSendMsgQueueCmdStatusDds = (mqd_t)(-1);

// Receive the message with a timeout value. The received message will be
// written to the memory pointed to by pCmdMsg.
// Return the error status or the received number of bytes.
// If the return value < 0, it means the timeout from poll() or error from
// poll() or recv().
// Note. If we do not set the timeout when using the recv(), the server may not
// be able to close this thread because the recv() is a blocking call.
static int cmdServerDds_recv(struct pollfd *pFds, int timeoutInMs,
                             commandStreamStructure_t *pCmdMsg) {

    int numEvent = poll(pFds, 1, timeoutInMs);
    if (numEvent <= 0) {
        // error (-1) or timeout (0)
        return -1;
    }

    return recv(socketConnect, pCmdMsg, sizeof(commandStreamStructure_t), 0);
}

// Send the last command status in message queue if any.
// Return 0 if sending successfully. Else, -1.
static int cmdServerDds_sendLastCmdStateInMsgQueue(void) {
    int bytesReceived =
        mq_receive(gSendMsgQueueCmdStatusDds, (char *)pMsgQueueCmdStatus,
                   sizeof(commandStatusStructure_t), NULL);

    // Send the message only when there is the available data.
    // Writing to a closed socket will raise SIGPIPE. Check
    // tlmServerGui_checkTel() for the details (references) that how
    // to avoid the termination signal
    int error = 0;
    if (bytesReceived > 0) {
        error = send(socketConnect, pMsgQueueCmdStatus, bytesReceived,
                     MSG_NOSIGNAL);
        if (error < 0) {
            // Return if the connection is broken
            return -1;
        }
    }

    return 0;
}

// Basic close of the connection socket. This function will reset the file
// descriptor of 'pollfd' structure. The server will be put into the
// Disconnected state and wait for the new connection request.
static void cmdServerDds_basicClose(struct pollfd *pFds) {
    printf("DDS CMD socket being reset.\n");
    tcpServer_close(socketConnect);

    socketConnect = 0;
    pFds->fd = 0;

    gCmdDdsServerConnected = ServerStatus_Disconnected;
    printf("The state of DDS command server is disconnected.\n");
}

// Check a DDS command (pCmdMsg) to see if DDS has control and if the command
// has the expected sync pattern. If not, fail the command.
// The status of command will be filled in pCmdStatus if the command is invalid.
// Return true if valid. Else, false.
static bool cmdServerDds_isCmdValid(commandStatusStructure_t *pCmdStatus,
                                    commandStreamStructure_t *pCmdMsg,
                                    int commandSourceDDS) {
    char *reasonCmdFail = "";
    bool isCmdValid = true;

    // Check the command is from DDS or not
    if (isCmdValid && (pCmdMsg->syncpattern != SyncPattern_DDS)) {
        reasonCmdFail = "Invalid sync pattern";
        isCmdValid = false;
    }

    // Check the command source is DDS or not
    if (isCmdValid && (commandSourceDDS != 1)) {
        reasonCmdFail = "Command source is not DDS";
        isCmdValid = false;
    }

    if (!isCmdValid) {
        pCmdStatus->header.frameId = 1;
        pCmdStatus->header.counter = pCmdMsg->counter;
        pCmdStatus->cmdStatus = CmdStatus_NotOK;
        pCmdStatus->duration = 0;
        strncpy(&pCmdStatus->reason[0], reasonCmdFail,
                LENGTH_CMD_STATUS_REASON);
        pCmdStatus->reason[LENGTH_CMD_STATUS_REASON - 1] = '\0';
    }

    return isCmdValid;
}

// Command listener to listen a new command from the DDS TCP/IP client.
// This function listens for a client to connect to the command port and then
// reads commands from the client in a loop.
// Note: The data types of input and output are required for pthread_create().
// The input is ignored.
static void *cmdServerDds_commandListener(void *data) {

    // Wait until begins to listen the command
    while (!isStartListenCmd) {
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

    int timeoutInMs = (timeout <= 0) ? -1 : timeout;

    // Listen the command
    gCmdDdsServerConnected = ServerStatus_Disconnected;
    int error;
    while (isStartListenCmd) {

        // New command message
        commandStreamStructure_t cmdMsg;
        commandStatusStructure_t cmdStatus;

        // State machine to deal with the connection or disconnection of DDS
        switch (gCmdDdsServerConnected) {

        // Look for the connection with client
        case ServerStatus_Disconnected:
            // Wait for the connection request
            socketConnect = tcpServer_accept(socketListen, timeout);
            if (socketConnect == -1) {
                break;
            } else {
                gCmdDdsServerConnected = ServerStatus_Connected;
                printf("The state of DDS command server is connected. socket = "
                       "%d.\n",
                       socketConnect);

                // Make use of poll() to receive the message
                fds.fd = socketConnect;
            }
            break;

        // Connected to DDS command client, look for commands
        case ServerStatus_Connected:

            // Reply the last command status from controller
            error = cmdServerDds_sendLastCmdStateInMsgQueue();

            // We were connected but the connection has been broken
            if (error < 0) {
                cmdServerDds_basicClose(&fds);
                break;
            }

            // Check the new command
            error = cmdServerDds_recv(&fds, timeoutInMs, &cmdMsg);

            // Ignore the timeout or error
            if (error < 0) {
                break;
            }

            // Client closes the connection
            if (error == 0) {
                cmdServerDds_basicClose(&fds);
                break;
            }

            // Check the command is valid or not
            bool isCmdValid =
                cmdServerDds_isCmdValid(&cmdStatus, &cmdMsg, gCommandSourceDDS);

            if (isCmdValid) {
                // Write command to cmdMsgBuffer
                if (circular_buf_put(cmdMsgBuffer, cmdMsg)) {
                    syslog(LOG_NOTICE, "The command message is overwritten in "
                                       "DDS command socket.");
                }
            }

            // Send the NotOK message to client if needed

            // Writing to a closed socket will raise SIGPIPE. Check
            // tlmServerGui_checkTel() for the details (references) that how
            // to avoid the termination signal
            if (!isCmdValid) {
                error = send(socketConnect, &cmdStatus,
                             sizeof(commandStatusStructure_t), MSG_NOSIGNAL);
            } else {
                error = 0;
            }

            // We were connected but the connection has been broken
            if (error < 0) {
                cmdServerDds_basicClose(&fds);
                break;
            }

            break;

        default:
            break;
        }

        // Give other functions a chance to run
        nanosleep(&ts50, NULL);
    }

    mq_close(gSendMsgQueueCmdStatusDds);
    mq_unlink(queueNameCmdStatus);
    g_free(pMsgQueueCmdStatus);

    if (socketConnect > 0) {
        tcpServer_close(socketConnect);
    }

    tcpServer_close(socketListen);
    gCmdDdsServerConnected = ServerStatus_Exit;

    return 0;
}

// Create the message queue (non-blocking).
// Return 0 if success, otherwise, -1.
static int cmdServerDds_createMsgQueue(long maxNumQueue) {

    // Create the message queue
    struct mq_attr qAttrCmdStatus;
    qAttrCmdStatus.mq_flags = O_NONBLOCK;
    qAttrCmdStatus.mq_maxmsg = maxNumQueue;
    qAttrCmdStatus.mq_msgsize =
        (unsigned int)(sizeof(commandStatusStructure_t));
    gSendMsgQueueCmdStatusDds = mq_open(
        queueNameCmdStatus, O_CREAT | O_RDWR | O_NONBLOCK, 0, &qAttrCmdStatus);

    // Create the memory allocation for DDS command status message queue
    if (gSendMsgQueueCmdStatusDds != (mqd_t)(-1)) {
        pMsgQueueCmdStatus = g_new0(commandStatusStructure_t, 2);
    } else {
        perror("Failed to create the DDS command status MsgQueue");
        return -1;
    }

    return 0;
}

void cmdServer_closeDds(void) {
    printf("Closing the DDS command server.\n");

    int error = 0;
    if (isStartListenCmd) {
        isStartListenCmd = false;
        error = pthread_join(threadListenCmd, NULL);
    }

    if (error != 0) {
        printf("Failed the waiting of thread in the DDS command server.\n");
    }
}

int cmdServer_initDds(serverInfo_t *cmdServerDds) {

    // Set the internal timeout value
    timeout = cmdServerDds->timeout;

    // Create the socket to listen the connection request
    // Only allow a single connection
    socketListen =
        tcpServer_open(cmdServerDds->family, cmdServerDds->cmdPort, 1);
    if (socketListen == -1) {
        printf("Failed to create the socket to listen the connection request "
               "in the DDS command server.\n");
        return -1;
    }

    // Wait for the connection to DDS command server
    printf("Waiting for the DDS Cmd connection at port %d.\n",
           cmdServerDds->cmdPort);

    // Create the message queue

    // Note that the value of 4 is a random number. I expect there will be only
    // 1 in the real running.
    // The value here is just by try-and-error. If this value is too big, the
    // kernel will reject it for the memory allocation.
    // It is noted that the kernel will restrict the allocated memory for the
    // message queue. Therefore, this value can not be too big.
    long maxNumQueue = 4;
    int error = cmdServerDds_createMsgQueue(maxNumQueue);
    if (error == -1) {
        tcpServer_close(socketListen);

        return -1;
    }

    // Start task to receive commands
    error = pthread_create(&threadListenCmd, NULL, cmdServerDds_commandListener,
                           NULL);
    struct sched_param param;
    if (error < 0) {
        printf("Failed to Create DDS Cmd Listener Thread.\n");

        g_free(pMsgQueueCmdStatus);
        tcpServer_close(socketListen);

        return -1;
    } else {
        // Set priority of this thread
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if ((error = pthread_setschedparam(threadListenCmd, SCHED_OTHER,
                                           &param)) != 0) {
            printf("Cannot initialize thread priority in the DDS command "
                   "server.\n");

            g_free(pMsgQueueCmdStatus);
            tcpServer_close(socketListen);
            pthread_exit(&threadListenCmd);

            return -1;
        }
        isStartListenCmd = true;
    }

    return 0;
}

int cmdServer_sendCmdStatusToMsgQueue(commandStatusStructure_t *pCmdStatus,
                                      unsigned int counter,
                                      unsigned int cmdStatus, double duration,
                                      const char *reason, mqd_t msgQueue) {

    // Fill the command status
    pCmdStatus->header.frameId = 1;
    pCmdStatus->header.counter = counter;
    pCmdStatus->cmdStatus = cmdStatus;
    pCmdStatus->duration = duration;

    strncpy(&pCmdStatus->reason[0], reason, LENGTH_CMD_STATUS_REASON);
    pCmdStatus->reason[LENGTH_CMD_STATUS_REASON - 1] = '\0';

    // Send the message
    int error = mq_send(msgQueue, (char *)pCmdStatus,
                        sizeof(commandStatusStructure_t), 0);
    if (error < 0) {
        perror("Fail to send the command status");
    }

    return error;
}
