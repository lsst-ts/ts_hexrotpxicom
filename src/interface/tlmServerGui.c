#include <pthread.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <mqueue.h>

#include "tcpServer.h"
#include "tlmServer.h"

// Socket to listen the connection request
static int socketListen = 0;

// Socket to connect to the TCP/IP client
static int socketConnect = 0;

// Timeout in millisecond
static int timeout = 0;

// Thread to write the telemetry
static pthread_t threadWriteTel;

// Is started to write the telemetry or not
// This value is set to be true when the thread is ready. False when the
// software is ready to close the server.
static bool isStartWriteTel = false;

// Name of telemetry queue
static const char queueNameTel[] = "/tlmSendMsgQ";

// Pointer of telemetry message
static gpointer *pMsgTel;

// Size of the telemetry message in byte. This will be set at
// telServer_initGui().
static unsigned int sizeMsgTel = 0;

// Globals accessible to the application code
mqd_t gTlmGuiSendMsgQueue = (mqd_t)(-1);
volatile int gTlmGuiServerConnected = ServerStatus_Disconnected;

// Check the telemetry message and write to the TCP/IP client.
// It will allow the GUI to connect. Does not attempt to re-connect
// if the connection is lost.
// Note: The data types of input and output are required for pthread_create().
// The input is ignored.
static void *tlmServerGui_checkTel(void *data) {

    // Wait until begins to write the telemetry
    while (!isStartWriteTel) {
        sleep(1);
    }

    // Loop delay time
    // 20 milliseconds (= 50 Hz)
    struct timespec ts20;
    ts20.tv_nsec = 20000000;
    ts20.tv_sec = 0;

    int optVal = 1;
    int error;
    int bytesReceived;
    while (isStartWriteTel && (gTlmGuiServerConnected != ServerStatus_Exit)) {

        // Loop to wait for message queue and then send it on the socket
        switch (gTlmGuiServerConnected) {

        // Look for connection to TLM server
        case ServerStatus_Disconnected:
            // Wait for the connection request with timeout
            socketConnect = tcpServer_accept(socketListen, timeout);
            if (socketConnect == -1) {
                break;
            }

            // Set the option of TCP_NODELAY
            error = setsockopt(socketConnect, IPPROTO_TCP, TCP_NODELAY, &optVal,
                               sizeof(optVal));
            if (error == -1) {
                perror("Failed to set the TCP_NODELAY in connected socket "
                       "of GUI telemetry server");

                tcpServer_close(socketConnect);
                socketConnect = 0;
                break;
            }

            printf("The state of GUI telemetry server is connected. socket = "
                   "%d.\n",
                   socketConnect);

            // Change to the connected state
            gTlmGuiServerConnected = ServerStatus_Connected;

            break;

        // Write the telemetry
        case ServerStatus_Connected:
            // Receive msgQ
            bytesReceived = mq_receive(gTlmGuiSendMsgQueue, (char *)pMsgTel,
                                       sizeMsgTel, NULL);

            // Send the message only when there is the available data.
            // Writing to a closed socket will raise SIGPIPE.
            // For the refereces of this and how to avoid, follow:
            // https://newbedev.com/how-to-prevent-sigpipes-or-handle-them-properly
            // https://stackoverflow.com/questions/26752649/so-nosigpipe-was-not-declared
            // https://stackoverflow.com/questions/19172804/crash-when-sending-data-without-connection-via-socket-in-linux?rq=1
            if (bytesReceived > 0) {
                error =
                    send(socketConnect, pMsgTel, bytesReceived, MSG_NOSIGNAL);

                // We were connected but the connection has been broken
                if (error < 0) {
                    printf("GUI telemetry socket being closed.\n");

                    tcpServer_close(socketConnect);
                    socketConnect = 0;

                    gTlmGuiServerConnected = ServerStatus_Exit;
                    break;
                }
            }
            break;

        default:
            break;
        }

        // Give other functions a chance to run
        nanosleep(&ts20, NULL);
    }

    mq_close(gTlmGuiSendMsgQueue);
    mq_unlink(queueNameTel);
    g_free(pMsgTel);

    if (socketConnect > 0) {
        tcpServer_close(socketConnect);
    }
    tcpServer_close(socketListen);

    gTlmGuiServerConnected = ServerStatus_Exit;

    return 0;
}

void telServer_closeGui(void) {
    printf("Closing the GUI telemetry server.\n");

    int error = 0;
    if (isStartWriteTel) {
        isStartWriteTel = false;
        error = pthread_join(threadWriteTel, NULL);
    }

    if (error != 0) {
        printf("Failed the waiting of thread in the GUI telemetry server.\n");
    }
}

// Create the message queue (non-blocking).
// Return 0 if success, otherwise, -1.
static int telServerGui_createMsgQueue(long maxNumQueue) {
    struct mq_attr qAttrTel;
    qAttrTel.mq_flags = O_NONBLOCK;
    qAttrTel.mq_maxmsg = maxNumQueue;
    qAttrTel.mq_msgsize = (long)sizeMsgTel;
    gTlmGuiSendMsgQueue =
        mq_open(queueNameTel, O_CREAT | O_RDWR | O_NONBLOCK, 0, &qAttrTel);

    // Create the memory allocation for GUI telemetry message queue
    if (gTlmGuiSendMsgQueue != (mqd_t)(-1)) {
        pMsgTel = g_malloc0_n((gsize)sizeMsgTel, 2);
    } else {
        perror("Failed to create the GUI TLM MsgQueue");
        return -1;
    }

    return 0;
}

int telServer_initGui(serverInfo_t *telServerGui, unsigned int sizeTel) {

    // Set the internal timeout value
    timeout = telServerGui->timeout;

    // Set the size of telemetry message
    sizeMsgTel = sizeTel;

    // Create the socket to listen the connection request
    // Only allow a single connection
    socketListen =
        tcpServer_open(telServerGui->family, telServerGui->tlmPort, 1);
    if (socketListen == -1) {
        printf("Failed to create the socket to listen the connection request "
               "in the GUI telemetry server.\n");
        return -1;
    }

    // Wait for the connection to GUI telemetry server
    printf("Waiting for the GUI telemetry connection at port %d.\n",
           telServerGui->tlmPort);

    // Create Msg Queue
    int error = telServerGui_createMsgQueue(telServerGui->maxNumQueueTel);
    if (error == -1) {
        tcpServer_close(socketListen);

        return -1;
    }

    // Start task to write telemetry
    error = pthread_create(&threadWriteTel, NULL, tlmServerGui_checkTel, NULL);
    struct sched_param param;
    if (error < 0) {
        printf("Failed to Create GUI TEL Listener Thread.\n");

        g_free(pMsgTel);
        tcpServer_close(socketListen);

        return -1;
    } else {
        // Set priority of this thread
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if ((error = pthread_setschedparam(threadWriteTel, SCHED_OTHER,
                                           &param)) != 0) {
            printf("Can't init GUI TLM thread priority.\n");

            g_free(pMsgTel);
            tcpServer_close(socketListen);
            pthread_exit(&threadWriteTel);

            return -1;
        }
        isStartWriteTel = true;
    }

    return 0;
}
