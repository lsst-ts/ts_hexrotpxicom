#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <syslog.h>

#include "gtest/gtest.h"

extern "C" {
#include "tcpServer.h"
}

struct TcpServerTest : testing::Test {

    const char *localhost = "127.0.0.1";
    int family;
    int port;
    int maxConnection;
    int socketListen;

    TcpServerTest() {
        family = AF_INET;
        port = 8888;
        maxConnection = 1;
        socketListen = -1;

        openlog("TcpServer", LOG_CONS, LOG_SYSLOG);
    }

    ~TcpServerTest() {
        if (socketListen >= 0) {
            tcpServer_close(socketListen);
        }

        closelog();
    }
};

// Start a listening server and write a message.
// Return the pointer of written message for the following test. The caller
// should free the memory of message.
static void *startServer(void *pSocketListen) {

    int socketListen = *(int *)pSocketListen;

    int socketConnect;
    if ((socketConnect = tcpServer_accept(socketListen, 10000)) == -1) {
        printf("Failed to start the TCP/IP server\n");
        exit(1);
    }

    // Check the socket
    printf("The connected socket is %d.\n", socketConnect);
    EXPECT_TRUE(tcpServer_isConnected(socketConnect));

    // Write a message
    int *pMsg = (int *)malloc(sizeof(int));
    *pMsg = 999;
    send(socketConnect, pMsg, sizeof(int), 0);

    // Close the connection
    tcpServer_close(socketConnect);
    EXPECT_FALSE(tcpServer_isConnected(socketConnect));

    return (void *)pMsg;
}

// Start a listening server with a different port from client for the test.
static void *startServerWrongPort(void *pSocketListen) {

    int socketListen = *(int *)pSocketListen;

    int socketConnect;
    if ((socketConnect = tcpServer_accept(socketListen, 5000)) == -1) {
        printf("Failed to get a connection request before timeout.\n");
    }

    // Close the connection
    tcpServer_close(socketConnect);

    return 0;
}

// Start a client and read a message.
// Return the pointer of received message for the following test. The caller
// should free the memory of message.
static void *startClient(void *pServerAddr) {

    struct sockaddr_in serverAddr = *(struct sockaddr_in *)pServerAddr;

    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);
    if (connect(socketDesc, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1) {
        printf("Connection Failed.\n");
        exit(1);
    }

    // Read the message
    int *pMsg = (int *)malloc(sizeof(int));
    recv(socketDesc, pMsg, sizeof(int), 0);

    printf("TCP/IP client is done.\n");
    return (void *)pMsg;
}

// Get the server address with the specified family, port, and host.
static struct sockaddr_in getServerAddr(int family, int port,
                                        const char *host) {
    // Set up the server address
    // Only use 's_addr' field based on:
    // https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = family;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(host);

    return serverAddr;
}

TEST_F(TcpServerTest, tcpServerOpen) {
    socketListen = tcpServer_open(family, port, maxConnection);

    EXPECT_NE(-1, socketListen);
}

TEST_F(TcpServerTest, tcpServerIsConnected) {

    // No connection
    bool isConnected = tcpServer_isConnected(99);
    EXPECT_FALSE(isConnected);

    // There is the connection
    socketListen = tcpServer_open(family, port, maxConnection);

    isConnected = tcpServer_isConnected(socketListen);
    EXPECT_TRUE(isConnected);
}

TEST_F(TcpServerTest, tcpServerAccept) {

    pthread_t threadServer, threadClient;

    // Run the TCP/IP server
    socketListen = tcpServer_open(family, port, maxConnection);
    int error =
        pthread_create(&threadServer, NULL, startServer, (void *)&socketListen);
    if (error) {
        perror("Thread of TCP/IP server creation failed");
        exit(1);
    }

    // Sleep 2 seconds to simulate the real case
    sleep(2);

    // Run the TCP/IP client
    struct sockaddr_in serverAddr = getServerAddr(family, port, localhost);
    error =
        pthread_create(&threadClient, NULL, startClient, (void *)&serverAddr);
    if (error) {
        perror("Thread of TCP/IP client creation failed");
        exit(1);
    }

    // Wait the threads
    void *pMsgServer, *pMsgClient;
    pthread_join(threadServer, &pMsgServer);
    pthread_join(threadClient, &pMsgClient);
    printf("Finish the waiting of threads.\n");

    // Check the send/recv message
    EXPECT_EQ(*(int *)pMsgServer, *(int *)pMsgClient);

    // Free the memories
    free(pMsgServer);
    pMsgServer = NULL;

    free(pMsgClient);
    pMsgClient = NULL;
}

TEST_F(TcpServerTest, tcpServerAcceptTimeout) {
    socketListen = tcpServer_open(family, port, maxConnection);

    int socketConnect = tcpServer_accept(socketListen, 2000);
    EXPECT_EQ(-1, socketConnect);
}

TEST_F(TcpServerTest, tcpServerAcceptWrongPort) {

    pthread_t threadServer;

    // Run the TCP/IP server
    socketListen = tcpServer_open(family, port, maxConnection);
    int error = pthread_create(&threadServer, NULL, startServerWrongPort,
                               (void *)&socketListen);
    if (error) {
        perror("Thread of TCP/IP server creation failed");
        exit(1);
    }

    // Sleep 2 seconds to simulate the real case
    sleep(2);

    // Do the connection request from a different port: "port + 1"
    struct sockaddr_in serverAddr = getServerAddr(family, port + 1, localhost);
    int socketDesc = tcpServer_getSocketConnect(serverAddr.sin_family);

    int status =
        connect(socketDesc, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    EXPECT_EQ(-1, status);

    // Wait the thread
    pthread_join(threadServer, NULL);
}
