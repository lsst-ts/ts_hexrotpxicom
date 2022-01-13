# Command Status

When the commandable SAL component (CSC) commands the low-level controller, the related command status will be provided to let CSC know the command status.

## Structure of Command and Status

The **commandStatusStructure_t** is the structure of command status.
The status is defined in enum: **CmdStatus**.
They are in [commandStructure.h](../include/interface/commandStructure.h).

After CSC issues a command with **commandStreamStructure_t** structure, it will receive one of two types of status: *CmdStatus_OK* and *CmdStatus_NotOK*.

There is a *header* in **commandStatusStructure_t**.
The *frameId* is always 1.

## Command Acknowledgement

The controller should acknowledge CSC actively for the command status with *CmdStatus_NotOK* if:

1. The command is not in the list of valid commands defined in enum: **CmdType**.
2. The command does not have the correct sync pattern of data distribution system (DDS).
3. The command source of controller is the engineering user interface (EUI) instead of DDS.

## Command Result

The controller will report the result of command execution to the client.
If a command failed in the inspection before execution, the *CmdStatus_NotOK* will reply to the client with the related *counter* and *reason* defined in **commandStatusStructure_t**.

If the command passes the inspection, it will be considered OK and send the *CmdStatus_OK* to CSC.
The *duration* in **commandStatusStructure_t** will be filled with the estimated execution time in second.
The value of 0 means the executation time is much less than 1.

When the command is executed, the related information will pass to the Simulink model to generate the related command to the Copley drives.
Because there is no method to get the command status in the Simulink model, it would be difficult to monitor the command execution in the C wrapper code.
Therefore, the *CmdStatus_OK* might just give the limited information to CSC.
