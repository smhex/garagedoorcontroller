# Changelog

## 1.0.0 - release

This release provides a hardware-validated Ethernet/MQTT garage-door controller
with local buttons, MQTT control, end-position reporting and optional
environmental telemetry.

### Highlights

- DHCP Ethernet with retry/recovery, MQTT reconnect and retained controller
  information containing application, firmware version, author and current IP.
- Safe, non-blocking 500 ms drive pulses. Competing or repeated commands cannot
  overlap or extend an active pulse; serial output reports the measured software
  pulse duration.
- Correct MQTT state sequence for local and remote travel: `opening`/`closing`
  is retained until the corresponding end position is observed.
- A 20 ms D1/D3 input filter rejects short Systembus transients while preserving
  raw serial diagnostics. Values caused by the controller's own command pulse
  are not interpreted as an end position.
- Optional ENV shield operation: unavailable or invalid readings do not block
  door control, and `gdc/system/sensors/status` reports availability.
- Safe operation across the 32-bit `millis()` rollover and confirmed retained
  MQTT restart cleanup before watchdog reboot.

### Validation

Host regression tests and the `mkrzero-release` firmware build pass. Hardware
tests covered DHCP/MQTT reconnect, local and MQTT opening/closing, both end
positions, sensor reporting, retained restart, and the filtered input sequence.

### Known limitations

- Stop control is not implemented. On the tested drive, Arduino direction
  pulses did not stop movement, and D1/D3 cannot distinguish movement from a
  stop in an intermediate position.
- External movement and stops between end positions cannot be assigned a
  reliable direction; the state can remain `unknown` until an end position.
- Pulse durations are software timing measurements, not voltage measurements at
  the XB10 interface.

## 0.1.10

- Keep the controller operating when environmental sensors are absent or return
  invalid values. Sensor conversion waits are bounded, measurements run every
  ten seconds, and `gdc/system/sensors/status` reports availability (#31).
- Confirm retained MQTT restart-command cleanup before arming the watchdog reboot
  and record failed publication attempts accurately (#32).
- Make display, LED and periodic timers safe across the 32-bit `millis()`
  wraparound after about 49.7 days (#33).

Validation: sensor, MQTT restart, timing and previous regression tests pass. The
installed ENV shield, display, local/MQTT door control, end positions, sensor
reporting and a retained MQTT restart were verified on hardware. `release/0.1.9`
remains the latest release branch; this version is currently on `dev`.

## 0.1.9 — release preparation

- Bound incoming MQTT diagnostics and prevent overlapping drive command pulses (#29).
- Time pulses from output activation, defer blocking work during pulses, and log
  software pulse duration after release (#30).
- Preserve end-position tracking across command-window input changes; repeat
  current/target state on MQTT reconnect. Repeated commands do not imply a stop
  or reversal. Local held door buttons produce only one command.
- Add buffered input diagnostics (send `d` in the serial monitor for a 30-second
  capture), host regression tests, and CI for PRs/dev/release branches.

Validation: local and MQTT travel, end positions, repeated commands, ignored
commands at an already reached target, DHCP/MQTT reconnect and three-second local
button holds in both directions were verified on hardware. Recorded software
pulses were approximately 502–503 ms. Host tests and firmware compilation passed.

Known limitations:
- Arduino direction pulses did not stop the tested drive. Stop functionality is
  not implemented; the firmware does not infer `stopped` from these pulses.
- External movement/stops cannot reliably be distinguished between end positions.
  Inferred motion can remain stale; external departure from a known end position
  is reported as `unknown`, which the sample Homebridge mapping does not represent.
- Software pulse timing does not measure voltage at the XB10 interface.

No release tag has been created yet.
