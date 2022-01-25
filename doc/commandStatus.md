# Command Status

When the commandable SAL component (CSC) commands the low-level controller, the related command status will be provided to let CSC know the command status.

## Structure of Command and Status

The **commandStatusStructure_t** is the structure of command status.
The status is defined in enum: **CmdStatus**.
They are in [commandStructure.h](../include/interface/commandStructure.h).

After CSC issues a command with **commandStreamStructure_t** structure, it will receive one of two types of status: *CmdStatus_OK* and *CmdStatus_NotOK*.

There is a *header* in **commandStatusStructure_t**.
The *frameId* is always 1 (or **FrameId_CmdStatus**).

## Command Acknowledgement

The controller should acknowledge CSC actively for the command status with *CmdStatus_NotOK* if:

1. The command is not in the list of valid commands defined in enum: **CmdType**.
This enum is defined in the `ts_rotator_controller` or `ts_hexapod_controller`.
2. The command does not have the correct commander (enum: **Commander**) of CSC.
3. CSC issues a command when the graphical user interface (GUI) has control.
(The GUI will not see this because the GUI takes control from CSC whenever it issues a command.)

Note: The GUI can always take the control from CSC.

## Command Result

The controller will report the result of command execution to the client.
If a command failed in the inspection before execution, the *CmdStatus_NotOK* will reply to the client with the related *counter* and *reason* defined in **commandStatusStructure_t**.

If the command passes the inspection, it will be considered OK and send the *CmdStatus_OK* to CSC.
The *duration* in **commandStatusStructure_t** will be filled with the estimated execution time in second.
The value of 0 means the executation is done.
For some command such as `track` in the rotator controller, it is considered done when it is acknowledged.

Most commands will get a *CmdStatus_OK* when they have finished.
The commands that take a long time to finish get a *CmdStatus_OK* when they have successfully started.
Those are:

- stop: *CmdStatus_OK* when the enabled substate transitions to STOPPING or STOPPED.
- moving point to point: *CmdStatus_OK* when the enabled substate transitions to MOVING POINT TO POINT.
