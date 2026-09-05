# Optional sensor hardware checks

1. With the ENV shield fitted, upload this branch. Expect `SENSORS: shield ready`,
   DHCP/MQTT connection, normal local/remote door operation, and sensor snapshots
   approximately every ten seconds. Compare temperature/humidity/pressure/light
   against the previous firmware and check the OLED sensor page.
2. Subscribe to `gdc/system/sensors/status`: valid values report retained
   `available`. Until a valid snapshot exists, or after a measurement fails, the
   status is `unavailable` and no numeric snapshot is sent to `gdc/system/sensors`.
   Existing MQTT consumers must use the new status topic to reject cached values.
3. If practical, power off the controller and remove only the ENV shield while
   keeping Ethernet/HMI connected. Power on: expect the unavailable log, normal
   boot and working display/buttons/MQTT. Initialization retries every 30 seconds;
   there must be no repeating watchdog reboot caused by the missing shield.
4. Power off before refitting the shield, then boot and verify sensor reporting
   returns. Do not unplug stacked boards while powered. Runtime retry and timeout
   paths are covered by host fault injection; hardware hot-plug is not required.

Sampling is every ten seconds, with the first read due after initialization.
Each conversion wait is limited to 250 ms; temperature/humidity each have a busy
wait and a data-ready wait. Reads stop at the first non-finite result. Invalid
snapshots are discarded together; old data becomes unavailable after 30 seconds.
The light floor of 0.0001 lux is retained for HomeKit. Numeric JSON format and
units are unchanged; the OLED displays unavailable instead of invalid values.

The locally maintained Arduino_MKRENV library preserves the shared Wire bus when
initialization fails. Core Wire operations themselves are not given a new timeout:
a physically stuck bus can still affect HMI and sensor calls. The watchdog remains
the fallback for such stalls. No claim of hard real-time I2C operation is made.
