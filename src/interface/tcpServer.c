#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <poll.h>

#include <tcpServer.h>

int tcpServer_open(int family, int port, int maxConnection) {
    // Modified from the chapter 11.4.7 in Computer Systems, A Programmer’s
    // Perspective

    // Get the service name from the port number
    char serviceName[10];
    sprintf(serviceName, "%d", port);

    // Get a list of potential server addresses
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

    struct addrinfo *listPotentialAddr;
    getaddrinfo(NULL, &serviceName[0], &hints, &listPotentialAddr);

    // Walk the list for one that we can successfully connect to
    int socketOptionValue = 1;
    int socketDesc;
    struct addrinfo *potentialAddr;
    for (potentialAddr = listPotentialAddr; potentialAddr;
         potentialAddr = potentialAddr->ai_next) {
        // Create a socket descriptor
        if ((socketDesc =
                 socket(potentialAddr->ai_family, potentialAddr->ai_socktype,
                        potentialAddr->ai_protocol)) < 0) {
            // Socket failed, try the next
            continue;
        }

        // Eliminates "Address already in use" error from bind
        setsockopt(socketDesc, SOL_SOCKET, SO_REUSEADDR,
                   (void *)&socketOptionValue, sizeof(int));

        // Bind the descriptor to the address
        if (bind(socketDesc, potentialAddr->ai_addr,
                 potentialAddr->ai_addrlen) == 0) {
            // Success
            break;
        }

        //  Bind failed, try the next
        close(socketDesc);
    }

    // Clean up
    freeaddrinfo(listPotentialAddr);

    // No address worked
    if (!potentialAddr) {
        return -1;
    }

    // Make it a listening socket ready to accept connection requests
    if (listen(socketDesc, maxConnection) == -1) {
        perror("Listen failed");
        close(socketDesc);
        return -1;
    }

    return socketDesc;
}

int tcpServer_accept(int serverSocketDesc, int timeout) {

    // Client address
    struct sockaddr_in client;
    int addressLen = sizeof(client);

    // Get the timeout in milisecond
    int timeoutInMs = (timeout <= 0) ? -1 : timeout;

    // Check the events by poll
    struct pollfd fds;
    fds.fd = serverSocketDesc;
    fds.events = POLLIN | POLLOUT;

    int socketConnect = -1;
    int numEvent = poll(&fds, 1, timeoutInMs);
    switch (numEvent) {
    // Timeout
    case 0:
        break;
    // Error
    case -1:
        perror("Error in waiting the connection request");
        break;
    // Get the connection request
    default:
        socketConnect = accept(serverSocketDesc, (struct sockaddr *)&client,
                               (socklen_t *)&addressLen);
        break;
    }

    // Fail to have a connected socket
    if ((socketConnect < 0) || (numEvent <= 0)) {
        return -1;
    }

    // Print the connected IP
    char *ip = inet_ntoa(client.sin_addr);
    printf("Get the connection from: %s.\n", ip);

    return socketConnect;
}

bool tcpServer_isConnected(int socketDesc) {

    // This follows:
    // https://stackoverflow.com/questions/4142012/how-to-find-the-socket-connection-state-in-c

    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(socketDesc, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        perror("Cannot get the socket status");
        return false;
    }

    if (error != 0) {
        printf("Socket error: %s\n", strerror(error));
        return false;
    }

    return true;
}

void tcpServer_close(int socketDesc) {
    if (close(socketDesc) == -1) {
        perror("Failed to close the socket");
    }
}

int tcpServer_getSocketConnect(int family) {
    int socketDesc;
    if ((socketDesc = socket(family, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error.\n");
        exit(1);
    }

    return socketDesc;
}
