# Version History

0.1.4

- Change the telemetry loop to be 50 Hz instead of 20 Hz.
This is to deal with the observed time delay on summit.

0.1.3

- Copy the `testCircularBufferThread.cpp` from `ts_rotator_controller`.

0.1.2

- Support the telemetry in `cmdTlmServer.c`.

0.1.1

- Replace the `printf()` with `syslog()` in `tcpServer.c`, `circular_buffer.c`, and `configPxi.c`.
- Add the `cmdTlmServer.c`.
This tries to unify the command server codes of GUI and CSC in `ts_rotator_controller` and `ts_hexapod_controller`.
The support of telemetry part will be done in DM-33310, which will test the use of single socket for command and telemetry.

0.1.0

- Initial migration of the common codes from `ts_rotator_controller`.
