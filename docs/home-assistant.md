# Home Assistant

The controller uses MQTT Discovery. Add Home Assistant's MQTT integration to the
same broker; no YAML entities are required. After the controller connects, it
creates one **Garage Door Controller** device with cover, firmware update,
restart button, ENV measurements and diagnostic entities.

## Per-controller MQTT tree

Each controller uses its configured Ethernet MAC as a topic identifier:

```text
gdc/gdc-aabbccddeeff/
├── availability                 retained: online | offline
├── state                        retained JSON device state
├── system/boot                  retained JSON boot uptime, sent once per connection
├── update/state                 retained JSON update state
└── command/
    ├── door                     open | close | stop
    ├── update                   install
    └── restart                  restart
```

The actual MAC replaces `aabbccddeeff`. Commands must not be retained. The
state includes current and target door state, raw end switches and outputs,
sensor availability and measurements, IP address, last-boot information, packet counters,
firmware/update state, last command source, and the most recent complete opening
and closing travel time. Travel time is published only after a controller-started
trip reaches its target end switch without an intermediate stop.

The discovered **Door status** entity shows **open** or **closed** at an end
switch, **moving** after a controller-started open/close command until an end
switch is reached, **stopped** after the controller sends its stop pulse, or
**unknown** otherwise. It is an inferred travel status; without an
additional motion sensor, an external stop or a blockage between end positions
cannot be detected.

**Last boot** is derived from `system/boot`, which is sent only after MQTT
connects. It therefore stays visible as a relative timestamp without creating
an activity entry for every periodic state update.

## Firmware update

Upload the image from VS Code using **mkrzero-lan → Upload**. Once verification
on SD is complete, choose **Install** on the discovered Firmware entity or send
`install` to `gdc/gdc-<mac>/command/update`. The controller accepts it only at a
known end position. The retained `update/state` document reports installed and
staged versions and installation progress.

## Migration

Verify the automatically discovered device first, update dashboards and
automations, then remove previous manually configured YAML MQTT entities. The
former global `gdc/system/...`, `gdc/control/...` and `gdc/update/...` topics
are no longer published.
