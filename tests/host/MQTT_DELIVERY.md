# MQTT delivery and restart checks

This PR is based on sensor PR #31. Normal operation with the fitted ENV shield,
display and local/MQTT controls was confirmed by the user. Shield removal is not
possible on this installation; missing-hardware and recovery tests remain host
fault-injection tests, not hardware verification.

## Expected behavior

- All outgoing publish calls check the library result. `Publish sent (QoS0)`
  means a successful library send, not confirmed broker/subscriber delivery.
  Failed sends do not increment the unsigned sent counter. Failure logs include
  topic and library error without printing credentials.
- Current/target state and initial system info remain pending on send failure;
  reconnect republishes state/info. Uptime/status repeat every second and sensor
  snapshots/status on the existing ten-second schedule. Command-source events
  are best effort and are not replayed after a failure.
- A `true` on `gdc/system/restart` schedules deletion outside the callback.
  An empty retained QoS1 publish deletes the broker's saved restart command.
  The watchdog reboot is armed only after the library receives PUBACK. Failure
  keeps the controller running and retries after ten seconds when connected.
  Duplicate requests do not bypass the retry interval. Empty echoes are ignored.
- Once reboot is armed, new Arduino drive commands and outgoing telemetry are
  suppressed until the existing watchdog restart (about 16 seconds).

## Hardware checks

1. Upload, verify DHCP/MQTT, sensors, end positions and normal local/remote control.
2. At a stationary end position, publish retained `true` to `gdc/system/restart`.
   Expect `Retained restart command cleared (PUBACK); watchdog restart armed`,
   one reboot and a normal reconnect. Verify the retained restart message is gone
   with a fresh subscription. Wait at least a minute: no repeated reboot.
3. Repeat with non-retained `true`: the controller should also reboot once.
4. Disconnect/reconnect Ethernet at a stationary end position. Check state and
   system info return after reconnect and sensor status/data resume.
5. If using an isolated broker test configuration, simulate a failed deletion or
   missing ACK. The controller must not intentionally reboot until deletion is
   acknowledged. This failure path is covered by host policy tests if impractical
   on hardware. Do not change production broker ACLs just for this test.

QoS1 PUBACK confirms protocol receipt, not disk durability or subscriber delivery.
Host tests exercise the production retry/counting policy with a simulated sender;
actual broker ACK behavior must be verified with the hardware test above.
