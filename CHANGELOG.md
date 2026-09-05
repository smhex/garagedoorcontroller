# Changelog

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
- Temporal filtering of short input transitions remains open. A 439 us `0/0`
  transition was observed at an end position; it does not prove external actuation.
- Software pulse timing does not measure voltage at the XB10 interface.

No release tag has been created yet.
