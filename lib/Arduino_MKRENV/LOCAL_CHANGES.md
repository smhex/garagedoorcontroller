# Local Arduino_MKRENV 1.2.1 copy

Source: Arduino's Arduino_MKRENV 1.2.1 distribution, previously installed by
PlatformIO. Original copyright headers and LGPL-2.1 license are retained.

Changes: failed begin does not call end (which shuts down the shared Wire bus).
Temperature/humidity/pressure conversion polling has a 250 ms deadline per wait
and returns NaN on I2C error or timeout. Calibration must be finite and I2C reads
successful before begin succeeds. Failed byte reads are checked before shifting.

The deadline bounds conversion polling, not individual Wire transactions. A
physically stuck shared I2C bus can still affect the display and sensors; this is
not bus fault isolation or a hard real-time guarantee. Other unused library APIs
(UV and explicit end) retain their upstream behavior. Review these changes when
updating the vendored library; do not add a competing registry copy to lib_deps.
