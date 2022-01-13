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

extern volatile cbuf_handle_t cmdMsgBuffer;

extern volatile int gCommandSourceDDS;
extern volatile int gCmdDdsServerConnected;

extern mqd_t gSendMsgQueueCmdStatusDds;

struct CmdServerDdsTest : testing::Test {

    const char *localhost = "127.0.0.1";
    serverInfo_t serverInfo;

    CmdServerDdsTest() {
        serverInfo.cmdPort = 8888;
        serverInfo.timeout = 100;
        serverInfo.family = AF_INET;

        gCommandSourceDDS = 1;
        cmdMsgBuffer = circular_buf_init(1);
    }

    ~CmdServerDdsTest() {
        cmdServer_closeDds();

        gCommandSourceDDS = 0;
        circular_buf_free(cmdMsgBuffer);
    }
};

// Start a listening server.
static void *startServer(void *pServerInfo) {

    serverInfo_t serverInfo = *(serverInfo_t *)pServerInfo;

    int status = cmdServer_initDds(&serverInfo);
    EXPECT_EQ(0, status);

    return 0;
}

// Start a command client and write commands.
static void *startClient(void *pServerAddr) {

    EXPECT_EQ(ServerStatus_Disconnected, gCmdDdsServerConnected);

    // Do the connection
    struct sockaddr_in serverAddr = *(struct sockaddr_in *)pServerAddr;

    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, gCmdDdsServerConnected);

    // Test the NoAck messages
    commandStreamStructure_t cmdMsg;

    // Write the command and wait for some time to let the command server get
    // the message
    // By adding one to sync pattern, command should fail
    cmdMsg.syncpattern = SyncPattern_DDS + 1;
    cmdMsg.counter = 1;
    cmdMsg.cmd = 0x1;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);
    sleep(1);

    // Check to receive the no acknowledgement from socket
    commandStatusStructure_t cmdStatus;
    int msgSize =
        recv(socketDesc, &cmdStatus, sizeof(commandStatusStructure_t), 0);

    EXPECT_EQ(sizeof(commandStatusStructure_t), msgSize);
    EXPECT_EQ(1, cmdStatus.header.frameId);
    EXPECT_EQ(cmdMsg.counter, cmdStatus.header.counter);
    EXPECT_EQ(CmdStatus_NotOK, cmdStatus.cmdStatus);
    EXPECT_EQ(0, cmdStatus.duration);
    EXPECT_STREQ("Invalid sync pattern", cmdStatus.reason);

    // Simulate the random counter value
    cmdMsg.counter = 4;
    cmdMsg.cmd = 0x1;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);
    sleep(1);

    msgSize = recv(socketDesc, &cmdStatus, sizeof(commandStatusStructure_t), 0);
    EXPECT_EQ(sizeof(commandStatusStructure_t), msgSize);
    EXPECT_EQ(1, cmdStatus.header.frameId);
    EXPECT_EQ(4, cmdStatus.header.counter);
    EXPECT_EQ(CmdStatus_NotOK, cmdStatus.cmdStatus);
    EXPECT_STREQ("Invalid sync pattern", cmdStatus.reason);

    // Close the socket
    close(socketDesc);
    sleep(1);

    EXPECT_EQ(ServerStatus_Disconnected, gCmdDdsServerConnected);

    // Try the connection again
    socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, gCmdDdsServerConnected);

    // Write the command and wait for some time to let the command server get
    // the message
    cmdMsg.syncpattern = SyncPattern_DDS;
    cmdMsg.counter = 1;
    cmdMsg.cmd = 0x1;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);
    sleep(1);

    // Simulate to send the last command status from controller
    cmdServer_sendCmdStatusToMsgQueue(&cmdStatus, cmdMsg.counter, CmdStatus_OK,
                                      1.2, "A reason",
                                      gSendMsgQueueCmdStatusDds);
    sleep(1);

    // Check to receive the command status
    msgSize = recv(socketDesc, &cmdStatus, sizeof(commandStatusStructure_t), 0);
    EXPECT_EQ(sizeof(commandStatusStructure_t), msgSize);
    EXPECT_EQ(1, cmdStatus.header.frameId);
    EXPECT_EQ(cmdMsg.counter, cmdStatus.header.counter);
    EXPECT_EQ(CmdStatus_OK, cmdStatus.cmdStatus);
    EXPECT_DOUBLE_EQ(1.2, cmdStatus.duration);
    EXPECT_STREQ("A reason", cmdStatus.reason);

    // Close the server to release the resource
    // Note that we do not want to close the 'socketDesc' here to simulate the
    // real condition that the sever should be able to close the connection by
    // itself.
    cmdServer_closeDds();
    EXPECT_EQ(ServerStatus_Exit, gCmdDdsServerConnected);

    return 0;
}

TEST_F(CmdServerDdsTest, cmdServerInitDds) {

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
