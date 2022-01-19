#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <stdbool.h>

// Open a server. This function is reentrant and protocol-independent.
// User needs to specify the protocol family for socket (such as AF_INET), port,
// and the maximum of connection.
// Return the socket descriptor that listens the connection request.
// If error, return -1.
int tcpServer_open(int family, int port, int maxConnection);

// Accept the connection request with timeout millisecond. If timeout <= 0,
// means no timeout. In this case, the function will block forever until the
// acceptance of a connection request.
// Return the connected socket. If fail, return -1.
int tcpServer_accept(int serverSocketDesc, int timeout);

// The connection between the TCP/IP server and client is on or not. The input
// is the socket descriptor of connection between the server and client.
// Return true if the connection is on. Else, false (which means there is an
// error).
// Note: The implementation detail here is to check the state of socket.
// Therefore, this function applies to the socket that listens to the connection
// request as well.
bool tcpServer_isConnected(int socketDesc);

// Close the socket. This function can be used to close the socket that listens
// the connection request or the socket that is connected with the TCP/IP
// client.
void tcpServer_close(int socketDesc);

// Get the socket to do the connection with the specific family.
// This function is to support the test cases only.
// Return an available socket.
int tcpServer_getSocketConnect(int family);

#endif // TCPSERVER_H
