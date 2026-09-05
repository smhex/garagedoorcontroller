# Command safety regression tests

Run `python tests/host/run.py` with a host C++ compiler on PATH, or pass
`--cxx /path/to/g++`. Tests compile the production `src/driveio.cpp` against
simulated GPIO/time and exercise the production MQTT log formatter. No controller
is connected or flashed. The tests check both command directions, competing and
duplicate commands, pulse completion, invalid commands, and log buffer boundaries.

These tests do not prove electrical pulse timing, MQTT delivery, or HMI behavior.
For the first PR, verify on hardware:

1. Build and upload `mkrzero-release`; check DHCP and MQTT reconnect.
2. Check one local and one remote command in each direction.
3. With the drive command wires disconnected and a logic analyzer on D0/D2,
   issue a local opposite command during a pulse. Confirm the other output stays
   low, the original pulse is not restarted, and a fresh command works after it.
4. Confirm an ignored command logs `drive pulse active` without changing the LEDs
   or publishing a new target state. MQTT processing pauses during pulses, so a
   remote command arriving during a pulse can be processed after it ends.
5. Send a non-retained invalid payload of 120 `x` characters to each command topic
   (`gdc/control/setnewdoorstate` and `gdc/system/restart`). Confirm truncated
   invalid diagnostics, no motion/restart, and continued MQTT operation.

Do not send `true` to the restart topic for the invalid-message test.
