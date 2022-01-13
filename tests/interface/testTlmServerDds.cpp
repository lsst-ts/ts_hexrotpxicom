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

extern volatile int gTlmDdsServerConnected;
extern mqd_t gTlmDdsSendMsgQueue;

// Data structure of the DDS test telemetry, which has a bigger size
typedef struct __attribute__((__packed__)) _telemetryDdsTestBigStructure {
    headerStructure_t header;
    double dataA;
    double dataB;
} telemetryDdsTestBigStructure_t;

// Data structure of the DDS test telemetry, which has a smaller size
typedef struct __attribute__((__packed__)) _telemetryDdsTestSmallStructure {
    headerStructure_t header;
    double data;
} telemetryDdsTestSmallStructure_t;

struct TelServerDdsTest : testing::Test {

    const char *localhost = "127.0.0.1";
    serverInfo_t serverInfo;

    TelServerDdsTest() {
        serverInfo.tlmPort = 8888;
        serverInfo.timeout = 100;
        serverInfo.family = AF_INET;
        serverInfo.maxNumQueueTel = 10;
    }

    ~TelServerDdsTest() { telServer_closeDds(); }
};

// Start a listening server.
static void *startServer(void *pServerInfo) {

    serverInfo_t serverInfo = *(serverInfo_t *)pServerInfo;

    int status =
        telServer_initDds(&serverInfo, sizeof(telemetryDdsTestBigStructure_t));
    EXPECT_EQ(0, status);

    return 0;
}

// Start a command client and write a command.
static void *startClient(void *pServerAddr) {

    EXPECT_EQ(ServerStatus_Disconnected, gTlmDdsServerConnected);

    // Do the connection
    struct sockaddr_in serverAddr = *(struct sockaddr_in *)pServerAddr;

    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, gTlmDdsServerConnected);

    // Send a message
    telemetryDdsTestBigStructure_t tlmSendBig;
    tlmSendBig.header.counter = 1;
    tlmSendBig.dataA = 1.2;
    tlmSendBig.dataB = 1.3;
    mq_send(gTlmDdsSendMsgQueue, (char *)&tlmSendBig,
            sizeof(telemetryDdsTestBigStructure_t), 1);
    sleep(1);

    // Check to get the message from socket
    telemetryDdsTestBigStructure_t tlmRecvBig;
    int msgSize = recv(socketDesc, &tlmRecvBig,
                       sizeof(telemetryDdsTestBigStructure_t), 0);

    EXPECT_EQ(sizeof(telemetryDdsTestBigStructure_t), msgSize);
    EXPECT_EQ(tlmSendBig.header.counter, tlmRecvBig.header.counter);
    EXPECT_EQ(tlmSendBig.dataA, tlmRecvBig.dataA);
    EXPECT_EQ(tlmSendBig.dataB, tlmRecvBig.dataB);

    // Test the disconnection
    close(socketDesc);
    sleep(1);

    // Send the messages to let the server notices the connection is closed
    int counter;
    for (counter = 0; counter < 30; counter++) {
        mq_send(gTlmDdsSendMsgQueue, (char *)&tlmSendBig,
                sizeof(telemetryDdsTestBigStructure_t), 1);
        sleep(1);

        if (gTlmDdsServerConnected != ServerStatus_Connected) {
            break;
        }
    }

    EXPECT_EQ(ServerStatus_Disconnected, gTlmDdsServerConnected);

    // Do the connection again
    socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);

    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, gTlmDdsServerConnected);

    // Send the message again with a smaller size
    telemetryDdsTestSmallStructure_t tlmSendSmall;
    tlmSendSmall.header.counter = 2;
    tlmSendSmall.data = 2.2;
    mq_send(gTlmDdsSendMsgQueue, (char *)&tlmSendSmall,
            sizeof(telemetryDdsTestSmallStructure_t), 1);

    // Check to get the message from socket
    telemetryDdsTestSmallStructure_t tlmRecvSmall;
    msgSize = recv(socketDesc, &tlmRecvSmall,
                   sizeof(telemetryDdsTestSmallStructure_t), 0);

    EXPECT_EQ(sizeof(telemetryDdsTestSmallStructure_t), msgSize);
    EXPECT_EQ(tlmSendSmall.header.counter, tlmRecvSmall.header.counter);
    EXPECT_EQ(tlmSendSmall.data, tlmRecvSmall.data);

    // Close the server to release the resource
    // Note that we do not want to close the 'socketDesc' here to simulate the
    // real condition that the sever should be able to close the connection by
    // itself.
    telServer_closeDds();
    EXPECT_EQ(ServerStatus_Exit, gTlmDdsServerConnected);

    return 0;
}

TEST_F(TelServerDdsTest, telServerInitDds) {

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
