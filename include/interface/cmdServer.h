#ifndef CMDSERVER_H
#define CMDSERVER_H

#include <mqueue.h>

#include "tlmServer.h"

// Initialize the command server of graphical user interface (GUI) with the
// specified server information. This function will block until the connection
// with the client is constructed.
// Return 0 if success, otherwise, return -1.
int cmdServer_initGui(serverInfo_t *cmdServerGui);

// Close the command server and the client connection, if any for the graphical
// user interface (GUI). This is used in the shutdown process.
void cmdServer_closeGui(void);

// Initialize the command server of data distribution system (DDS) with the
// specified server information.
// Return 0 if success, otherwise, return -1.
int cmdServer_initDds(serverInfo_t *cmdServerDds);

// Close the command server and the client connection, if any for the data
// distribution system (DDS). This is used in the shutdown process.
void cmdServer_closeDds(void);

// Sent the command status to message queue with the inputs of counter, command
// status (enum: CmdStatus), estimated duration of command in second, and reason
// if the command failed (put "" if nothing to report). If "reason" is longer
// than the buffer defined in commandStatusStructure_t, it will be truncated to
// fit.
// Return 0 if success, otherwise, return -1.
int cmdServer_sendCmdStatusToMsgQueue(commandStatusStructure_t *pCmdStatus,
                                      unsigned int counter,
                                      unsigned int cmdStatus, double duration,
                                      const char *reason, mqd_t msgQueue);

#endif
