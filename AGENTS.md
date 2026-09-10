# Firmware versions and local settings

- Before deploying changed firmware, give it a new version in
  `include/firmware_version.h` and update `CHANGELOG.md`. This also applies to
  test updates. Do not reuse a version for different deployed firmware changes.
- Keep MQTT, HMI and boot-log versions based on this single version source.
- Keep actual controller hostnames/IP addresses in ignored `platformio_local.ini`
  and credentials/tokens in ignored `include/config_local.h`. Do not put those
  local values in tracked files, examples, logs or commits.
- After a LAN update, verify the expected new version through `gdc/system/info`;
  a successful staging response alone does not confirm boot success.
