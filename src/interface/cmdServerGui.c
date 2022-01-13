#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/socket.h>
#include <stdbool.h>

#include "circular_buffer.h"
#include "tcpServer.h"
#include "tlmServer.h"
#include "cmdServer.h"

// Socket to listen the connection request
static int socketListen = 0;

// Socket to connect to the TCP/IP client
static int socketConnect = 0;

// Thread to listen the command
static pthread_t threadListenCmd;

// Is started to listen the command or not
// This value is set to be true when the thread is ready. False when the
// software is ready to close the server.
static bool isStartListenCmd = false;

// Globals accessible to the application code
extern volatile cbuf_handle_t cmdMsgBuffer;
volatile int gCmdGuiServerConnected = ServerStatus_Disconnected;
volatile int gCommandSourceDDS = 0;

// Command listener to listen a new command from the GUI TCP/IP client.
// This function listens for a client to connect to the command port and then
// reads commands from the client in a loop.
// Note: The data types of input and output are required for pthread_create().
// The input is ignored.
static void *cmdServerGui_commandListener(void *data) {

    // Wait until begins to listen the command
    while (!isStartListenCmd) {
        sleep(1);
    }

    // Loop delay time
    // 50 milliseconds (= 20 Hz)
    struct timespec ts50;
    ts50.tv_nsec = 50000000;
    ts50.tv_sec = 0;

    int error;
    while (isStartListenCmd &&
           (gCmdGuiServerConnected == ServerStatus_Connected)) {

        // New command message
        commandStreamStructure_t cmdMsg;

        // Do not use the poll() here as in cmdServerDds.c.
        // This is to let the GUI command client to decide to close the program
        // or not (recv() is a blocking call)
        error = recv(socketConnect, &cmdMsg, sizeof(cmdMsg), 0);
        if (error < 0) {
            printf("Bad cmd msg from GUI.\n");
            continue;
        } else if (error == 0) {
            printf("GUI has disconnected the CMD Socket. Existing.\n");
            gCmdGuiServerConnected = ServerStatus_Exit;
        } else {
            // IF DDS was in control, a cmd from GUI will revert to GUI
            // control
            if (gCommandSourceDDS) {
                printf("GUI takes the control back. Command source = GUI.\n");
                gCommandSourceDDS = 0;
            }

            if (!gCommandSourceDDS) {
                printf("Received %d bytes : 0x%04X, counter=0x%08X, "
                       "cmd=%08X ,parameter1 = %f, parameter2 = %f \n",
                       error, cmdMsg.syncpattern, cmdMsg.counter, cmdMsg.cmd,
                       cmdMsg.param1, cmdMsg.param2);

                // Write command to cmdMsgBuffer
                if (circular_buf_put(cmdMsgBuffer, cmdMsg)) {
                    syslog(LOG_NOTICE, "The command message is overwritten "
                                       "in the GUI command server.");
                }
            }
        }

        // Give other functions a chance to run
        nanosleep(&ts50, NULL);
    }

    if (socketConnect > 0) {
        tcpServer_close(socketConnect);
    }

    tcpServer_close(socketListen);
    gCmdGuiServerConnected = ServerStatus_Exit;

    return 0;
}

void cmdServer_closeGui(void) {
    printf("Closing the GUI command server.\n");

    int error = 0;
    if (isStartListenCmd) {
        isStartListenCmd = false;
        error = pthread_join(threadListenCmd, NULL);
    }

    if (error != 0) {
        printf("Failed the waiting of thread in the GUI command server.\n");
    }
}

int cmdServer_initGui(serverInfo_t *cmdServerGui) {

    // Create the socket to listen the connection request
    // Only allow a single connection
    socketListen =
        tcpServer_open(cmdServerGui->family, cmdServerGui->cmdPort, 1);
    if (socketListen == -1) {
        printf("Failed to create the socket to listen the connection request "
               "in the GUI command server.\n");
        return -1;
    }

    // Wait for the connection to GUI command server
    printf("Waiting for the GUI Cmd connection at port %d.\n",
           cmdServerGui->cmdPort);

    // Intentionally to wait forever until the connection request is accepted
    socketConnect = tcpServer_accept(socketListen, -1);
    if (socketConnect == -1) {
        printf("Failed to create the socket to connect to the TCP/IP client in "
               "the GUI command server.\n");

        tcpServer_close(socketListen);

        return -1;
    }

    // Start task to receive commands
    int error = pthread_create(&threadListenCmd, NULL,
                               cmdServerGui_commandListener, NULL);
    struct sched_param param;
    if (error < 0) {
        printf("Failed to Create the GUI Cmd Listener Thread.\n");

        tcpServer_close(socketConnect);
        tcpServer_close(socketListen);

        return -1;
    } else {
        // Set priority of this thread
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if ((error = pthread_setschedparam(threadListenCmd, SCHED_OTHER,
                                           &param)) != 0) {
            printf("Cannot initialize thread priority in the GUI command "
                   "server.\n");

            tcpServer_close(socketConnect);
            tcpServer_close(socketListen);
            pthread_exit(&threadListenCmd);

            return -1;
        }
        isStartListenCmd = true;
        gCmdGuiServerConnected = ServerStatus_Connected;

        printf("Connection is on in the GUI command server.\n");
    }

    return 0;
}
