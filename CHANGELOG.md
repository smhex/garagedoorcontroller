# Changelog

## 1.1.8 - 2026-09-12

- Keep the OLED display active for 60 seconds after an accepted local or MQTT
  door command, showing the overview page and the current door movement.

## 1.1.7 - 2026-09-11

- Fix Ethernet initialization before the hardware check. Version 1.1.6 checked
  the uninitialized cached chip type, preventing DHCP, MQTT and the LAN updater
  from starting.

## 1.1.6 - 2026-09-11

- Show the MQTT connection phase and failure source on the HMI, including DNS,
  TCP, broker/authentication and subscription errors.
- Show detailed MQTT-library diagnostics on the HMI, including broker CONNACK
  rejection reasons, protocol, read/write, timeout, PONG and packet errors.
- Treat an unknown Ethernet link state as unavailable, display it explicitly,
  and avoid DHCP attempts when the Ethernet hardware is absent.

## 1.1.5 - 2026-09-11

- Optimize the HMI navigation around a diagnostic overview with firmware, door,
  Ethernet and MQTT status.
- Replace the separate MQTT page with a combined network page and remove the
  HMI LED debug page from the normal navigation cycle.
- Move author and firmware-update information to the system page and use clear
  textual connection and door states instead of raw boolean values.

## 1.1.3 - 2026-09-11

- Use `remote` consistently for the last command source in Home Assistant MQTT
  Discovery and command reception, matching the door command state feedback.
- Recommend PlatformIO's official VS Code extension and flag the incompatible
  PIOArduino extension as unwanted.

## 1.1.2 - 2026-09-10

- Rename the Home Assistant **Torstatus** entity to **Door status** and use the
  English states `open`, `closed`, `moving`, `stopped` and `unknown`.

## 1.1.1 - 2026-09-10

- Publish last-boot information once per MQTT connection so Home Assistant no
  longer records a changing boot timestamp for every periodic state update.
- Add the Home Assistant **Torstatus** entity with the values **Endlage
  erreicht**, **In Bewegung**, **Angehalten** and **Unbekannt**.

## 1.1.0 - 2026-09-09

### Breaking change

- Replace the former shared `gdc/system/...`, `gdc/control/...` and
  `gdc/update/...` MQTT topics with a MAC-address-specific controller tree:
  `gdc/gdc-<mac-without-colons>/...`. Existing Homebridge mappings and manual
  Home Assistant YAML configuration must be migrated.

### Added

- Add Home Assistant MQTT Discovery for the garage door, firmware update,
  restart button, availability and diagnostic entities.
- Publish the controller's availability using retained MQTT birth/last-will
  messages, and provide last opening/closing travel times as diagnostics.
- Support installing a staged firmware update through MQTT when the door is in
  a safe end position, while retaining the local HMI flow.

### Changed

- Round environmental diagnostic values for Home Assistant and show travel
  times as whole seconds.

## 1.0.10 - 2026-09-09

- Display the last opening and closing travel times in Home Assistant as whole
  seconds.

## 1.0.9 - 2026-09-09

- Refine Home Assistant MQTT Discovery presentation: rounded environmental
  measurements, a last-boot timestamp, a compact MAC-based device identity and
  reliable ENV-shield and firmware-update state reporting.

## 1.0.8 - 2026-09-08

- Replace global MQTT topics with a controller-specific MAC topic tree and
  Home Assistant MQTT discovery for the door, update, diagnostics and sensors.
- Report MKR Zero hardware identity and SAMD21 serial number, plus completed
  opening and closing travel times.

## 1.0.7 - 2026-09-08

- Stop staged-version detection at the terminating byte of its firmware marker
  so unrelated binary strings cannot be appended to the version shown on the
  HMI or in Home Assistant.

## 1.0.6 - 2026-09-08

- Announce verified staged firmware through Home Assistant MQTT discovery. The
  update can be installed with Home Assistant or by holding the local Info
  button on the new firmware-update HMI page.
- Keep staged firmware across door commands and restarts. Installation checks
  the controller's current door state and refuses unsafe requests without
  removing the staged image.

## 1.0.5 - 2026-09-08

- Remove the temporary 30-second input capture from console command `c`.
- Extend automatic USB/TCP logging after startup to 60 seconds. When it ends,
  the console explains that command `d` enables logging again until reboot.

## 1.0.4 - 2026-09-07

- Show the staged firmware version in the English local-update dialog. Preserve
  its version metadata on the SD card so it remains visible after reboot.
- Keep the LAN upload protocol compatible with the already installed 1.0.1
  updater, allowing this update to be installed without a USB bootstrap.

## 1.0.3 - 2026-09-07

## 1.0.2 - 2026-09-07

- Stage LAN uploads before showing the local approval dialog. A verified staged
  image survives an unanswered dialog and is offered again after reboot.
- A local or remote door command while the dialog is shown cancels the dialog
  and removes the staged image. A new upload or controller restart is then
  required before an update can be approved.

## 1.0.1 - 2026-09-07

- Identify this firmware as 1.0.1 in the HMI, boot log and the retained
  `gdc/system/info` MQTT payload (`version`). Keep the version in
  `include/firmware_version.h`; increment it before each changed firmware upload.

- Add PlatformIO LAN firmware staging on the MKR Zero SD card, with a local
  Info-button approval and HMI progress before rebooting into Arduino SDU.
- Add a TCP console on port 2323. USB/TCP logs stop after 30 seconds; `d`
  enables output until reboot. Move the existing input capture command to `c`.

- A second open or close command while the controller reports `opening` or
  `closing` is now an implicit stop. It re-pulses the output that started the
  movement and publishes `stopped` after the pulse.

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
