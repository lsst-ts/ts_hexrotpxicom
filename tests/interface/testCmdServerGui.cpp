#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "gtest/gtest.h"

extern "C" {
#include "circular_buffer.h"
#include "tcpServer.h"
#include "cmdServer.h"
}

volatile cbuf_handle_t cmdMsgBuffer;

extern volatile int gCommandSourceDDS;
extern volatile int gCmdGuiServerConnected;

struct CmdServerGuiTest : testing::Test {

    const char *localhost = "127.0.0.1";
    serverInfo_t serverInfo;

    CmdServerGuiTest() {
        serverInfo.cmdPort = 8888;
        serverInfo.family = AF_INET;

        gCommandSourceDDS = 1;
        cmdMsgBuffer = circular_buf_init(1);
    }

    ~CmdServerGuiTest() {
        cmdServer_closeGui();

        gCommandSourceDDS = 0;
        circular_buf_free(cmdMsgBuffer);
    }
};

// Start a listening server.
static void *startServer(void *pServerInfo) {

    serverInfo_t serverInfo = *(serverInfo_t *)pServerInfo;
    cmdServer_initGui(&serverInfo);

    return 0;
}

// Start a command client and write a command.
static void *startClient(void *pServerAddr) {

    EXPECT_EQ(ServerStatus_Disconnected, gCmdGuiServerConnected);

    // Do the connection
    struct sockaddr_in serverAddr = *(struct sockaddr_in *)pServerAddr;

    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, gCmdGuiServerConnected);

    // Write the command and wait for some time to let the command server get
    // the message
    commandStreamStructure_t cmdMsg;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);
    sleep(1);

    // The above command will let the GUI take the control back
    EXPECT_EQ(0, gCommandSourceDDS);

    // Close the socket
    close(socketDesc);

    // Close the server to release the resource
    cmdServer_closeGui();
    EXPECT_EQ(ServerStatus_Exit, gCmdGuiServerConnected);

    return 0;
}

TEST_F(CmdServerGuiTest, cmdServerInitGui) {

    EXPECT_EQ(0, circular_buf_size(cmdMsgBuffer));

    pthread_t threadServer, threadClient;

    // Run the TCP/IP server and sleep 2 seconds to simulate the real case
    int error =
        pthread_create(&threadServer, NULL, startServer, (void *)&serverInfo);
    if (error) {
        perror("Thread of TCP/IP server creation failed");
        exit(1);
    }

    sleep(2);

    // Set up the server address
    // We only need to set the 's_addr' of 'sin_addr' based on:
    // https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = serverInfo.family;
    serverAddr.sin_port = htons(serverInfo.cmdPort);
    serverAddr.sin_addr.s_addr = inet_addr(localhost);

    // Run the TCP/IP client
    error =
        pthread_create(&threadClient, NULL, startClient, (void *)&serverAddr);
    if (error) {
        perror("Thread of TCP/IP client creation failed");
        exit(1);
    }

    // Wait the threads
    pthread_join(threadServer, NULL);
    pthread_join(threadClient, NULL);
    printf("Finish the waiting of threads.\n");

    EXPECT_EQ(1, circular_buf_size(cmdMsgBuffer));
}
