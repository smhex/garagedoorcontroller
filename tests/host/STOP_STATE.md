# Drive timing and end-position checks

During inferred motion, a second local or MQTT direction command is an implicit
stop: it pulses the output that started the current movement, regardless of the
newly requested direction. The controller then reports `stopped`. A following
command starts the requested direction normally. Current/target state is
published after the pulse and restored on MQTT reconnect.
Input samples taken during the controller's own command pulse are ignored by the
state tracker, including the sample read just before the output is released.
After release, a persistent end position is accepted on the next input sample.
This avoids interpreting command-coupled bus levels as an actual end position.
All decoded input states must also remain stable for 20 ms before they reach the
door-state tracker or publish an external command source. Raw D1/D3 readings
remain unfiltered in the serial diagnostic output.

Check with local buttons and MQTT, waiting for each 500 ms pulse to finish:

1. From closed, open: expect `opening`, followed by `open` at the end switch.
2. Repeat a same/opposite direction command during inferred motion: expect a
   `DOORSTOP` log, a `stopped` publication, and a pulse on the output used for
   the original movement. Verify the physical stop separately.
   Repeat from open while closing. Actual motion must be observed separately.
3. Hold a local door button for more than two seconds: only one command should be
   generated. Release it before issuing the next command.
4. Reconnect Ethernet at an end position: the current end position is republished.
5. Issue open when already open (and close when closed): no pulse or false motion.
6. Verify both end positions still override inferred motion and both LEDs
   indicate the correct end position.

Limits: the two status inputs do not distinguish motion from a stop between end
positions. Stops initiated through another remote, obstruction, or drive failure
cannot be detected reliably. After boot between end positions or untracked motion,
the current state is `unknown`; the first direction command assumes the drive was
stationary. External stops can leave an inferred moving state stale until an end
position is reached. Homebridge's sample five-state mapping does not represent `unknown`.
Electrical polarity is unchanged. During an active pulse the main loop services
GPIO timing and the watchdog only; sensor, HMI, MQTT and serial output wait for
release. Timing starts when the Arduino output is actually set HIGH. This remains
software timing, not a measurement of the XB10 voltage or a hard real-time guarantee.

Diagnostics after upload:
- `IO: pulse complete Arduino D0 HIGH duration=500 ms (software timing)` identifies
  the opening output; D2 identifies the closing output.
- `IO: t=... ms raw=... pulse=...` records input transitions. `pulse=1` samples
  were taken during the command window and are ignored by the state tracker.
  At most 16 transitions are buffered during a pulse; overflow is reported.
- Leave at least one second between commands, and capture the
  command, pulse report, input transitions and physical motion together.

Observed with the original remote: closed D1/D3=0/1, travel=1/1, open=1/0.
A 439 us 0/0 transient was captured at the open end position; a single 0/0 sample
is not reliable proof of external actuation. The 20 ms filter rejects this known
transient. The controller can only report stops it initiated itself; external
stops remain indistinguishable from an intermediate moving state.
