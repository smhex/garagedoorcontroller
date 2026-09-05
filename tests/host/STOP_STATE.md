# Stop state hardware checks

This PR follows command-safety (#29). A new command during known motion pulses
the requested output and sets current state to `stopped`, regardless of direction.
The next command selects the new direction. The target stays at the previous
direction on stop; HomeKit has no stopped target. Both LEDs stop blinking on stop.
Current/target state is published after the pulse and restored on MQTT reconnect.

Check with local buttons and MQTT, waiting for each 500 ms pulse to finish:

1. From closed, open: expect `opening`. Send open again: expect physical stop and
   `stopped`. Send close: expect `closing`, then `closed` at the end switch.
2. From closed, open then close during motion: expect stop, not reversal. A further
   close starts closing. Repeat from open while closing, using both stop directions.
3. Hold a local door button for more than two seconds: only one command should be
   generated. Release it before issuing the next command.
4. While stopped between end switches, disconnect and reconnect Ethernet. Expect
   `stopped` to be republished. Check Homebridge shows stopped.
5. Issue open when already open (and close when closed): no pulse or false motion.
6. Verify both end positions still override inferred motion/stop and both LEDs
   indicate the correct end position.

Limits: the two status inputs do not distinguish motion from a stop between end
positions. Stops initiated through another remote, obstruction, or drive failure
cannot be detected reliably. After boot between end positions or untracked motion,
the current state is `unknown`; the first direction command assumes the drive was
stationary. Only test the inferred stop behavior after starting from a known end
position. Homebridge's sample five-state mapping does not represent `unknown`.
This PR does not change electrical polarity or fix sensor-induced pulse delays.
