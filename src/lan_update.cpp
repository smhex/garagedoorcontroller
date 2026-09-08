#include <Arduino.h>
#include <Ethernet.h>
#include <SdFat.h>
#include <SDU.h>
#include "config.h"
#include "debug_console.h"
#include "door_state.h"
#include "driveio.h"
#include "firmware_image.h"
#include "lan_update.h"
#include "hmi.h"
#include "local_approval.h"
#include <WDTZero.h>
#include "mqtt.h"
#include "network.h"
#if __has_include("config_local.h")
#include "config_local.h"
#endif
#ifndef GDC_UPLOAD_TOKEN
#define GDC_UPLOAD_TOKEN ""
#endif

// Referencing this symbol ensures PlatformIO extracts the SDU object from its
// archive. The MKR linker puts its 16 KiB updater before the application vectors.
extern unsigned char sduBoot[0x4000];
extern WDTZero watchdog;

namespace {
EthernetServer server(65280);
EthernetClient client;
SdFat32 sd;
File32 file;
enum class Phase { Idle, Header, Receive, Verify, BootVerify, Reboot };
Phase phase = Phase::Idle;
bool listening = false;
bool sdReady = false;
IPAddress boundIP;
char header[160];
size_t headerSize = 0;
uint32_t size = 0, expectedCrc = 0, position = 0, crc = 0xffffffff;
uint32_t started = 0, lastActivity = 0;
const char temporary[] = "GDCUP.TMP";
const char staged[] = "GDCUP.BIN";
const char versionFile[] = "GDCUP.VER";
char stagedVersion[16] = "unknown";
const char versionMarker[] = "GDC-FW:";
uint8_t versionMarkerIndex = 0;
uint8_t candidateLength = 0;
LocalApproval approval;
bool approvalPending = false;
bool approvalDismissed = false;
bool failed = false;
uint32_t failedAt = 0;

void fail(const char* message) {
    if (client.connected()) { client.print("ERROR "); client.println(message); }
    file.close();
    if (sdReady && phase != Phase::BootVerify) sd.remove(temporary);
    client.stop();
    phase = Phase::Idle;
    failed = true;
    failedAt = millis();
    Debug.print("OTA: rejected/aborted: "); Debug.println(message);
}

void beginApproval() {
    approval.begin(hmi_button_sample());
    approvalPending = true;
    approvalDismissed = false;
    started = millis();
    Debug.println("OTA: staged image awaits local INFO button approval");
}

bool validVersion(const char* value) {
    if (!*value || strlen(value) >= sizeof(stagedVersion)) return false;
    for (const char* p = value; *p; ++p) {
        if (!((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || (*p >= 'a' && *p <= 'z'))) return false;
    }
    return true;
}

void resetVersionDetection() {
    versionMarkerIndex = candidateLength = 0;
    strcpy(stagedVersion, "unknown");
}

void detectVersion(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        const char value = static_cast<char>(data[i]);
        if (versionMarkerIndex < sizeof(versionMarker) - 1) {
            if (value == versionMarker[versionMarkerIndex]) ++versionMarkerIndex;
            else versionMarkerIndex = value == versionMarker[0] ? 1 : 0;
        } else if (candidateLength < sizeof(stagedVersion) - 1 &&
                   ((value >= '0' && value <= '9') || value == '.' || value == '-' || (value >= 'a' && value <= 'z'))) {
            stagedVersion[candidateLength++] = value;
            stagedVersion[candidateLength] = 0;
        }
    }
}

bool saveStagedVersion() {
    File32 version;
    if (!version.open(versionFile, O_RDWR | O_CREAT | O_TRUNC)) return false;
    const size_t length = strlen(stagedVersion);
    const bool saved = version.write(stagedVersion, length) == length && version.sync();
    version.close();
    return saved;
}

void loadStagedVersion() {
    strcpy(stagedVersion, "unknown");
    File32 version;
    if (!version.open(versionFile, O_RDONLY)) return;
    const int count = version.read(stagedVersion, sizeof(stagedVersion) - 1);
    version.close();
    if (count <= 0) { strcpy(stagedVersion, "unknown"); return; }
    stagedVersion[count] = 0;
    if (!validVersion(stagedVersion)) strcpy(stagedVersion, "unknown");
}

void acceptHeader() {
    char token[65], extra;
    unsigned long length, checksum;
    if (sscanf(header, "GDC1 %64s %lu %lx %c", token, &length, &checksum, &extra) != 3 ||
        strcmp(token, GDC_UPLOAD_TOKEN) != 0) {
        fail("invalid request or token"); return;
    }
    if (!FirmwareImage::validSize(length)) { fail("invalid image size"); return; }
    if (doorState.state != DoorState::Open && doorState.state != DoorState::Closed) {
        fail("door must be at a known end position"); return;
    }
    if (driveio_doorcommandactive() || mqtt_isrestartrequested()) { fail("controller busy"); return; }
    size = length; expectedCrc = checksum;
    failed = false;
    started = lastActivity = millis();
    phase = Phase::Receive;
    Debug.println("OTA: receiving image before local approval");
}

void startReceive() {
    if (doorState.state != DoorState::Open && doorState.state != DoorState::Closed) {
        fail("door left end position"); return;
    }
    sdReady = sd.begin(SdSpiConfig(SDCARD_SS_PIN, SHARED_SPI, SD_SCK_MHZ(12), &SPI1));
    if (!sdReady) { fail("SD unavailable; FAT16/FAT32 required in MKR Zero slot"); return; }
    if (sd.exists("UPDATE.BIN")) { fail("UPDATE.BIN already exists"); return; }
    if (!file.open(temporary, O_RDWR | O_CREAT | O_TRUNC)) { fail("cannot create staging file"); return; }
    position = 0; crc = 0xffffffff;
    resetVersionDetection();
    client.println("READY");
}
}

bool lan_update_busy() {
    return phase == Phase::Receive || phase == Phase::Verify || phase == Phase::BootVerify || phase == Phase::Reboot;
}

bool lan_update_approval_pending() { return approvalPending; }

void lan_update_cancel_for_command() {
    if (!approvalPending) return;
    approvalPending = false;
    approvalDismissed = false;
    if (sdReady) sd.remove(staged);
    if (sdReady) sd.remove(versionFile);
    Debug.println("OTA: local/remote door command cancelled pending update and removed staged image");
}

bool lan_update_display(String* lines) {
    if (failed && millis() - failedAt >= 5000) failed = false;
    if (!lan_update_busy() && !approvalPending && !failed) return false;
    if (failed) {
        lines[0] = "Update cancelled";
        lines[1] = "See PlatformIO log";
        lines[2] = "Firmware unchanged";
        lines[3] = "";
        return true;
    }
    if (approvalPending) {
        lines[0] = "New firmware";
        lines[1] = "Version " + String(stagedVersion);
        lines[2] = "INFO: Install";
        lines[3] = String(60 - min(60ul, (millis() - started) / 1000)) + " s - drive cancels";
    } else {
        lines[0] = "Firmware update";
        lines[1] = "Version " + String(stagedVersion);
        lines[2] = phase == Phase::Receive ? "Receiving..." :
                   phase == Phase::Verify || phase == Phase::BootVerify ? "Verifying..." : "Restarting...";
        lines[3] = String(size ? (position * 100ul / size) : 0) + " %";
    }
    return true;
}

void lan_update_init() {
    // Reference SDU even when no upload token is configured.
    Debug.print("OTA: SDU boot prefix: "); Debug.println(sduBoot[0], HEX);
    Debug.print("OTA: firmware marker: "); Debug.println(firmwareVersionMarker);
    if (!strlen(GDC_UPLOAD_TOKEN)) { Debug.println("OTA: disabled; configure GDC_UPLOAD_TOKEN"); return; }
    sdReady = sd.begin(SdSpiConfig(SDCARD_SS_PIN, SHARED_SPI, SD_SCK_MHZ(12), &SPI1));
    if (!sdReady) { Debug.println("OTA: SD unavailable at boot"); return; }
    if (!sd.exists(staged)) { Debug.println("OTA: upload service on TCP 65280"); return; }
    loadStagedVersion();
    if (!file.open(staged, O_RDONLY)) { Debug.println("OTA: cannot open staged image"); return; }
    size = file.fileSize();
    if (!FirmwareImage::validSize(size)) { file.close(); sd.remove(staged); Debug.println("OTA: removed invalid staged image"); return; }
    position = 0;
    phase = Phase::BootVerify;
    Debug.println("OTA: validating staged image found on SD");
}

void lan_update_loop() {
    if (!strlen(GDC_UPLOAD_TOKEN)) return;
    if (phase == Phase::Reboot) {
        if (millis() - lastActivity >= 1000) {
            watchdog.setup(WDT_OFF);
            NVIC_SystemReset();
        }
        return;
    }
    if (approvalPending) {
        if (millis() - started >= 60000) {
            approvalPending = false;
            sd.remove(versionFile);
            approvalDismissed = true;
            Debug.println("OTA: local approval timed out; staged image remains until reboot or replacement");
        } else if (approval.update(hmi_button_sample(), hmi_info_pressed())) {
            if (!sd.rename(staged, "UPDATE.BIN")) { fail("cannot activate staged image"); return; }
            approvalPending = false;
            Debug.println("OTA: locally approved; rebooting into SDU");
            phase = Phase::Reboot; lastActivity = millis();
        }
    }
    if (phase != Phase::BootVerify && (!network_isready() || (listening && boundIP != Ethernet.localIP()))) {
        if (phase != Phase::Idle) fail("network changed");
        listening = false;
        return;
    }
    if (!listening) {
        boundIP = Ethernet.localIP(); server.begin(); listening = true;
    }
    if (phase == Phase::Idle) {
        client = server.accept();
        if (!client) return;
        client.setConnectionTimeout(20);
        started = lastActivity = millis(); headerSize = 0; phase = Phase::Header;
    }
    if (millis() - lastActivity > 10000 || millis() - started > 180000) {
        fail("timeout"); return;
    }
    if (phase != Phase::Verify && phase != Phase::BootVerify && !client.connected() && !client.available()) {
        fail("disconnected"); return;
    }
    if (phase == Phase::Header) {
        for (unsigned i = 0; i < 160 && client.available(); ++i) {
            int ch = client.read(); lastActivity = millis();
            if (ch == '\n') { header[headerSize] = 0; acceptHeader(); startReceive(); return; }
            if (headerSize == sizeof(header) - 1) { fail("header too long"); return; }
            header[headerSize++] = char(ch);
        }
        return;
    }
    uint8_t block[512];
    if (phase == Phase::Receive) {
        int available = client.available();
        if (available <= 0) return;
        size_t n = min(sizeof(block), static_cast<size_t>(available));
        n = min(n, static_cast<size_t>(size - position));
        int received = client.read(block, n);
        if (received <= 0) return;
        if (file.write(block, received) != static_cast<size_t>(received)) { fail("SD write failed"); return; }
        position += received; lastActivity = millis();
        if (position == size) {
            if (!file.sync() || !file.seekSet(0)) { fail("SD sync failed"); return; }
            phase = Phase::Verify; position = 0;
        }
    } else if (phase == Phase::Verify || phase == Phase::BootVerify) {
        size_t n = min(sizeof(block), static_cast<size_t>(size - position));
        if (file.read(block, n) != static_cast<int>(n)) { fail("SD readback failed"); return; }
        if (position < FirmwareImage::bootSize && memcmp(block, sduBoot + position, n)) {
            if (phase == Phase::BootVerify) { file.close(); sd.remove(staged); sd.remove(versionFile); phase = Phase::Idle; Debug.println("OTA: removed staged image with wrong SDU prefix"); return; }
            fail("wrong SDU boot image; build for MKR Zero with SDU"); return;
        }
        if (position == FirmwareImage::bootSize && !FirmwareImage::validVectors(block, size)) {
            if (phase == Phase::BootVerify) { file.close(); sd.remove(staged); sd.remove(versionFile); phase = Phase::Idle; Debug.println("OTA: removed staged image with invalid vectors"); return; }
            fail("invalid application vectors"); return;
        }
        if (phase == Phase::Verify || phase == Phase::BootVerify) detectVersion(block, n);
        crc = FirmwareImage::crc32(crc, block, n); position += n; lastActivity = millis();
        if (position == size) {
            if (phase == Phase::Verify && (crc ^ 0xffffffffu) != expectedCrc) { fail("CRC mismatch"); return; }
            if (!validVersion(stagedVersion)) strcpy(stagedVersion, "unknown");
            file.close();
            if (phase == Phase::BootVerify) {
                phase = Phase::Idle;
                beginApproval();
            } else {
                if (sd.exists(staged)) sd.remove(staged);
                if (sd.exists(versionFile)) sd.remove(versionFile);
                if (!sd.rename(temporary, staged)) { fail("cannot publish staged image"); return; }
                if (!saveStagedVersion()) { sd.remove(staged); fail("cannot save staged version"); return; }
                client.println("STAGED awaiting local approval");
                client.stop();
                phase = Phase::Idle;
                beginApproval();
            }
        }
    }
}
