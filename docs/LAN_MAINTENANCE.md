# PlatformIO LAN upload and console

The MKR Zero uses its own SD slot (SPI1, SDCARD_SS_PIN), not the slot on
the Ethernet shield. The card must use FAT16 or FAT32; exFAT is unsupported.
The normal USB upload environments remain available.

## First installation

1. Configure `GDC_UPLOAD_TOKEN` in the ignored `include/config_local.h` with
   32–64 random hexadecimal characters. This checkout has a generated local
   token. The uploader reads this same file; it never prints the token or puts
   it on the command line. Missing/empty tokens disable LAN uploads.
2. Ensure the card has no existing `UPDATE.BIN` in its root directory: the
   Arduino SDU loader processes that filename before application startup.
3. Use PlatformIO **mkrzero-release → Upload** once over USB to install the
   firmware including SDU and the LAN services.
4. Configure the controller address as described below, then use
   **mkrzero-lan → Upload** and **mkrzero-lan → Monitor** in PlatformIO.

The ignored `platformio_local.ini` contains this checkout's controller address.
Both Upload and Monitor in VS Code use it automatically. For another checkout,
create that local file with the following contents, substituting the address:

```ini
[env:mkrzero-lan]
upload_port = 192.0.2.42
```

Keep real connection details in this ignored file, not in `platformio.ini`.
Without the local file, the LAN environment falls back to the `GDC_HOST`
environment variable set before launching VS Code. `monitor_port` follows
`upload_port` automatically. A DHCP reservation is useful when using an IP.

From a PlatformIO PowerShell terminal, the equivalent commands are:

```powershell
pio run -e mkrzero-lan -t upload
pio device monitor -e mkrzero-lan
```

An environment variable set only in a terminal does not change the environment
of an already-running VS Code extension. The local file takes precedence over
the environment variable. The example address is not a default device address.

## Update flow on the HMI

Before deploying a changed firmware image (including test updates), increment
`GDC_FIRMWARE_VERSION` in `include/firmware_version.h` and record the change in
`CHANGELOG.md`. Do not deploy different firmware changes under the same version.
Rebuilding an unchanged image does not increment the version automatically.
This is a release rule, not a device-enforced version comparison.

Version 1.0.4 is the current numbered LAN-maintenance update. The version appears
on the HMI overview, in the boot log, and in the retained JSON field `version`
on the existing `gdc/system/info` topic. A retained old value alone is not proof
of success: wait for the expected new version after the update.

1. Start **Upload** in the LAN environment with the door at a known end position.
2. The display wakes and switches to the overview/start page, headed
   **New firmware**. It shows the staged target version, **INFO: Install** and
   a countdown. The target version is read from the staged firmware and saved beside it; it is also
   available if the dialog is redisplayed after a restart.
3. The complete image is received and verified on the SD card before this dialog
   appears. Release the middle **Info** button if held, then press it within
   60 seconds to activate the staged image.
4. The display shows **Receiving...**, **Verifying...**, then the approval dialog
   and **Restarting...**. Door commands are paused during the transfer. During the
   dialog, a local or remote door command cancels the pending update and removes
   its staged image before the command is performed.
5. SDU flashes the image on restart. Check the new boot timestamp in the console.
   The uploader's success message confirms verified staging, not a successful
   boot of the new application. Reopen the monitor after the reset.

An unanswered dialog leaves the verified staging file on the card but hides the
dialog after 60 seconds. The current firmware keeps running. On the next reboot
the controller validates that staging file again and offers it for approval.
A new upload replaces the staged image and shows a new dialog. A door command
during the dialog removes the staged image and cancels it; after that, a new
upload or a reboot is needed to show the dialog again. A disconnected uploader
or failed validation removes its incomplete staging data. The overview shows an
abort message for five seconds; details are in the PlatformIO upload output.

The updater receives into reserved scratch file `GDCUP.TMP`, flushes it, reads it
back, checks CRC32, size, the MKR Zero SDU prefix and application vectors, then
renames it to non-bootable `GDCUP.BIN`. Only a local Info-button approval renames
that file to `UPDATE.BIN` and resets. A partial transfer never intentionally
creates the boot filename. SDU occupies 16 KiB of sketch flash at
0x2000; application vectors start at 0x6000. All three build environments include
the same loader, so further LAN updates remain possible.

CRC32 detects corruption; it is not a firmware signature. The raw TCP uploader
uses the local token and physical approval, without encryption. Use these LAN
services only within the trusted local network. Power loss while SDU is actually
flashing is not a rollback mechanism; USB recovery may be needed. Do not remove
power or the SD card during an update. USB remains the recovery path.

## Debug console

The ordinary PlatformIO monitor connects to raw TCP port **2323**, using
`socket://ADDRESS:2323`. There is no Telnet negotiation or separate console app.
USB receives the same application log output.

- Logging is enabled for the first 30 seconds of application uptime.
- Afterwards output stops, but both command inputs remain available.
- Lowercase **d** enables logging until reboot (also across TCP reconnects).
- Lowercase **c** runs the previous 30-second input capture; this used to be `d`.
  That diagnostic pauses controller commands and MQTT, and dumps its buffered
  transitions when finished. It is separate from ordinary continuous logging.
- One LAN monitor can be connected at a time. Logs use bounded, best-effort
  queues; slow/absent readers can lose messages. There is no persistent SD log.

This is a log/command console, not a source-level debugger with breakpoints.
The listener does not reset the controller on connection, so connecting after
the boot window requires `d` and does not replay the entire boot log.

## Validation

Hardware test on 2026-09-07: PlatformIO LAN upload from 1.0.0 to 1.0.1
completed after local Info-button approval. The controller staged and verified
the image, restarted, and published a live `gdc/system/info` message with
`version: "1.0.1"`. A separate subscription also confirmed the retained value
as 1.0.1. The operator reported a successful update but read 1.1.0 on the display;
that visual discrepancy is not yet explained (HMI and MQTT use the same version
variable). The fault scenarios listed below are not all hardware-validated.

Automated checks:

```text
python tests/host/run.py --cxx PATH_TO_G++
python -m unittest discover -s tests/host -p test_upload_lan.py
pio run -e mkrzero-release -e mkrzero-debug -e mkrzero-lan
```

Before relying on LAN maintenance, verify on hardware:

- USB bootstrap boots normally with and without the card; GPIO command outputs
  remain inactive during boot and maintenance.
- USB/TCP output expires at 30 seconds; `d` restores it until the next restart.
- A pending upload wakes the HMI. Holding Info beforehand does not approve;
  releasing and pressing approves. Let another request expire without pressing.
- Interrupt a transfer: normal operation resumes and no `UPDATE.BIN` is created.
- Upload a newly built image, observe progress, then confirm its new boot
  timestamp and normal DHCP/MQTT operation after SDU finishes.
- Verify missing card, wrong filesystem, link loss and slow console readers.

References: [PlatformIO TCP monitor](https://docs.platformio.org/en/latest/core/userguide/device/cmd_monitor.html),
[custom upload command](https://docs.platformio.org/en/latest/projectconf/sections/env/options/upload/upload_command.html),
[Arduino SDU example](https://github.com/arduino/ArduinoCore-samd/blob/master/libraries/SDU/examples/Usage/Usage.ino).
