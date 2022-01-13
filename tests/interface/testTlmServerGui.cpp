#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <mqueue.h>

#include "gtest/gtest.h"

extern "C" {
#include "commandStructure.h"
#include "tcpServer.h"
#include "tlmServer.h"
}

extern volatile int gTlmGuiServerConnected;
extern mqd_t gTlmGuiSendMsgQueue;

// Data structure of the GUI test telemetry
typedef struct __attribute__((__packed__)) _telemetryGuiTestStructure {
    headerStructure_t header;
    double data;
} telemetryGuiTestStructure_t;

struct TelServerGuiTest : testing::Test {

    const char *localhost = "127.0.0.1";
    serverInfo_t serverInfo;

    TelServerGuiTest() {
        serverInfo.tlmPort = 8888;
        serverInfo.timeout = 100;
        serverInfo.family = AF_INET;
        serverInfo.maxNumQueueTel = 10;
    }

    ~TelServerGuiTest() { telServer_closeGui(); }
};

// Start a listening server.
static void *startServer(void *pServerInfo) {

    serverInfo_t serverInfo = *(serverInfo_t *)pServerInfo;

    int status =
        telServer_initGui(&serverInfo, sizeof(telemetryGuiTestStructure_t));
    EXPECT_EQ(0, status);

    return 0;
}

// Start a command client and write a command.
static void *startClient(void *pServerAddr) {

    EXPECT_EQ(ServerStatus_Disconnected, gTlmGuiServerConnected);

    // Do the connection
    struct sockaddr_in serverAddr = *(struct sockaddr_in *)pServerAddr;

    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, gTlmGuiServerConnected);

    // Send a message
    telemetryGuiTestStructure_t tlmSend;
    tlmSend.header.counter = 1;
    tlmSend.data = 1.2;
    mq_send(gTlmGuiSendMsgQueue, (char *)&tlmSend,
            sizeof(telemetryGuiTestStructure_t), 1);
    sleep(1);

    // Check to get the message from socket
    telemetryGuiTestStructure_t tlmRecv;
    int msgSize =
        recv(socketDesc, &tlmRecv, sizeof(telemetryGuiTestStructure_t), 0);

    EXPECT_EQ(tlmSend.header.counter, tlmRecv.header.counter);
    EXPECT_EQ(tlmSend.data, tlmRecv.data);
    EXPECT_EQ(sizeof(telemetryGuiTestStructure_t), msgSize);

    // Close the socket
    close(socketDesc);
    sleep(1);

    // Send the messages to let the server notices the connection is closed
    int counter;
    for (counter = 0; counter < 30; counter++) {
        mq_send(gTlmGuiSendMsgQueue, (char *)&tlmSend,
                sizeof(telemetryGuiTestStructure_t), 1);
        sleep(1);

        if (gTlmGuiServerConnected != ServerStatus_Connected) {
            break;
        }
    }

    EXPECT_EQ(ServerStatus_Exit, gTlmGuiServerConnected);

    // Close the server to release the resource
    telServer_closeGui();
    EXPECT_EQ(ServerStatus_Exit, gTlmGuiServerConnected);

    return 0;
}

TEST_F(TelServerGuiTest, telServerInitGui) {

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
    serverAddr.sin_port = htons(serverInfo.tlmPort);
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
}
