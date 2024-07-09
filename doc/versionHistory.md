# Version History

0.2.1

- Copy and modify the **ds402.h** and **copley.h** from **ts_rotator_controller**.
- Add the **driveTool.c**.

0.2.0

- Reformat the code.
- Use the `lsstts/rotator_pxi:v0.5` in `Jenkinsfile`.

0.1.9

- Use the **Coverage** plug-in to replace the **Cobertura** plug-in in `Jenkinsfile`.

0.1.8

- Add the `calcTimeDiff()` and `calcTimeLeft()`.

0.1.7

- Fix the possible overflow of `joinStr()`.

0.1.6

- Improve the `logTlm.c` with 2 buffers to avoid the lost of data.

0.1.5

- Add the `logTlm.c`.

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
