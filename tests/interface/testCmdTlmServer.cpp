#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "gtest/gtest.h"

extern "C" {
#include "circular_buffer.h"
#include "tcpServer.h"
#include "cmdTlmServer.h"
}

volatile cbuf_handle_t cmdMsgBuffer;

typedef struct _serverData {
    serverInfo_t *pServerInfo;
    struct sockaddr_in serverAddr;
} serverData_t;

// Start a command client and write commands.
static void *startClient(void *pServerData) {
    // Remap the input data
    serverData_t serverData = *(serverData_t *)pServerData;
    serverInfo_t *pServerInfo = serverData.pServerInfo;
    struct sockaddr_in serverAddr = serverData.serverAddr;

    // Do the connection
    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, pServerInfo->serverStatus);

    // Test the NotOK messages
    commandStreamStructure_t cmdMsg;

    // Write the command and wait for some time to let the command server get
    // the message
    // By adding one to the maximum of enum 'Commander', command should fail
    cmdMsg.commander = Commander_CSC + 1;
    cmdMsg.counter = 2;
    cmdMsg.cmd = 1;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);

    // Check to receive the NotOK from socket
    commandStatusStructure_t cmdStatus;
    int msgSize =
        recv(socketDesc, &cmdStatus, sizeof(commandStatusStructure_t), 0);

    EXPECT_EQ(sizeof(commandStatusStructure_t), msgSize);
    EXPECT_EQ(FrameId_CmdStatus, cmdStatus.header.frameId);
    EXPECT_EQ(cmdMsg.counter, cmdStatus.header.counter);
    EXPECT_EQ(CmdStatus_NotOK, cmdStatus.cmdStatus);
    EXPECT_DOUBLE_EQ(0, cmdStatus.duration);
    EXPECT_STREQ("Invalid commander", cmdStatus.reason);

    // Get the NotOK if not the commander
    cmdMsg.commander = Commander_CSC;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);

    msgSize = recv(socketDesc, &cmdStatus, sizeof(commandStatusStructure_t), 0);

    EXPECT_EQ(sizeof(commandStatusStructure_t), msgSize);
    EXPECT_EQ(CmdStatus_NotOK, cmdStatus.cmdStatus);
    EXPECT_STREQ("Is not the commander", cmdStatus.reason);

    // GUI can always give a command
    cmdMsg.commander = Commander_GUI;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);

    // Close the socket
    tcpServer_close(socketDesc);
    sleep(1);

    EXPECT_EQ(ServerStatus_Disconnected, pServerInfo->serverStatus);

    // Try the connection again
    socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    sleep(1);
    EXPECT_EQ(ServerStatus_Connected, pServerInfo->serverStatus);

    // Put the server into the commander and write a command
    pServerInfo->isCommander = true;

    cmdMsg.commander = Commander_CSC;
    send(socketDesc, &cmdMsg, sizeof(cmdMsg), 0);

    // Simulate to send the last command status from controller
    char *reason = "A reason";
    double duration = 1.2;
    cmdTlmServer_sendCmdStatusToMsgQueue(pServerInfo, cmdMsg.counter,
                                         CmdStatus_OK, duration, reason);

    // Check to receive the command status
    msgSize = recv(socketDesc, &cmdStatus, sizeof(commandStatusStructure_t), 0);

    EXPECT_EQ(sizeof(commandStatusStructure_t), msgSize);
    EXPECT_EQ(FrameId_CmdStatus, cmdStatus.header.frameId);
    EXPECT_EQ(cmdMsg.counter, cmdStatus.header.counter);
    EXPECT_EQ(CmdStatus_OK, cmdStatus.cmdStatus);
    EXPECT_DOUBLE_EQ(duration, cmdStatus.duration);
    EXPECT_STREQ(reason, cmdStatus.reason);

    // Close the server to release the resource
    // Note that we do not want to close the 'socketDesc' first to simulate the
    // real condition that the sever should be able to close the connection by
    // itself.
    cmdTlmServer_close(pServerInfo);
    EXPECT_EQ(ServerStatus_Exit, pServerInfo->serverStatus);

    // Close the connection in client to release the resource
    tcpServer_close(socketDesc);
    return 0;
}

struct CmdTlmServerTest : testing::Test {

    const char *localhost = "127.0.0.1";

    char *name = "cmdTlm";
    int timeout = 100;
    unsigned int sizeMsgTlm = 10;
    int port = 8888;
    long maxNumQueueTlm = 10;

    serverInfo_t serverInfo;

    CmdTlmServerTest() {
        cmdMsgBuffer = circular_buf_init(2);

        openlog("CmdTlmServer", LOG_CONS, LOG_SYSLOG);
    }

    ~CmdTlmServerTest() {
        cmdTlmServer_close(&serverInfo);
        circular_buf_free(cmdMsgBuffer);

        closelog();
    }
};

TEST_F(CmdTlmServerTest, init) {
    int status = cmdTlmServer_init(&serverInfo, name, timeout, sizeMsgTlm, port,
                                   maxNumQueueTlm, cmdMsgBuffer);

    EXPECT_EQ(0, status);

    EXPECT_STREQ(name, serverInfo.pName);
    EXPECT_EQ(timeout, serverInfo.timeout);
    EXPECT_EQ(sizeMsgTlm, serverInfo.sizeMsgTlm);

    EXPECT_NE(-1, serverInfo.socketListen);
    EXPECT_EQ(-1, serverInfo.socketConnect);

    EXPECT_FALSE(serverInfo.isReady);
    EXPECT_EQ(ServerStatus_Disconnected, serverInfo.serverStatus);

    EXPECT_STREQ("/queueCmdStatus8888", serverInfo.pQueueNameCmdStatus);
    EXPECT_STREQ("/queueTlm8888", serverInfo.pQueueNameTlm);

    EXPECT_NE(nullptr, serverInfo.pMsgCmdStatus);
    EXPECT_NE(nullptr, serverInfo.pMsgTlm);

    EXPECT_FALSE(serverInfo.isCommander);
}

TEST_F(CmdTlmServerTest, initTimeoutZero) {
    timeout = 0;
    int status = cmdTlmServer_init(&serverInfo, name, timeout, sizeMsgTlm, port,
                                   maxNumQueueTlm, cmdMsgBuffer);

    EXPECT_EQ(0, status);

    EXPECT_EQ(-1, serverInfo.timeout);
}

TEST_F(CmdTlmServerTest, sendCmdStatusToMsgQueue) {
    cmdTlmServer_init(&serverInfo, name, timeout, sizeMsgTlm, port,
                      maxNumQueueTlm, cmdMsgBuffer);

    // Write a message of command status
    unsigned int counter = 3;
    unsigned int cmdStatus = CmdStatus_OK;
    double duration = 1.2;
    char *reason = "This is a reason";
    int status = cmdTlmServer_sendCmdStatusToMsgQueue(
        &serverInfo, counter, cmdStatus, duration, reason);

    EXPECT_EQ(0, status);

    // Receive the message of command status
    size_t sizeCmdStatus = sizeof(commandStatusStructure_t);
    commandStatusStructure_t cmdStatusRecv;
    int bytesReceived = mq_receive(serverInfo.msgQueueCmdStatus,
                                   (char *)&cmdStatusRecv, sizeCmdStatus, NULL);

    EXPECT_EQ(sizeCmdStatus, bytesReceived);

    EXPECT_EQ(FrameId_CmdStatus, cmdStatusRecv.header.frameId);
    EXPECT_EQ(counter, cmdStatusRecv.header.counter);
    EXPECT_EQ(cmdStatus, cmdStatusRecv.cmdStatus);
    EXPECT_DOUBLE_EQ(duration, cmdStatusRecv.duration);
    EXPECT_STREQ(reason, cmdStatusRecv.reason);
}

TEST_F(CmdTlmServerTest, runInNewThread) {

    EXPECT_EQ(0, circular_buf_size(cmdMsgBuffer));

    // Run the server
    cmdTlmServer_init(&serverInfo, name, timeout, sizeMsgTlm, port,
                      maxNumQueueTlm, cmdMsgBuffer);

    int status = cmdTlmServer_runInNewThread(&serverInfo);
    EXPECT_EQ(0, status);

    // Set up the server address
    // We only need to set the 's_addr' of 'sin_addr' based on:
    // https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(localhost);

    // Run the client in a new thread to simulate the use of code in controller
    serverData_t serverData;
    serverData.pServerInfo = &serverInfo;
    serverData.serverAddr = serverAddr;

    pthread_t threadClient;
    int error =
        pthread_create(&threadClient, NULL, startClient, (void *)&serverData);
    if (error != 0) {
        perror("Thread of TCP/IP client creation failed");
        exit(1);
    }

    // Wait the thread
    pthread_join(threadClient, NULL);
    printf("Finish the waiting of client thread.\n");

    // Check there should be two new commands in the buffer now
    EXPECT_EQ(2, circular_buf_size(cmdMsgBuffer));
}
