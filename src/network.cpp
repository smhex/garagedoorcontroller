#include <Ethernet.h>
#include "config.h"
#include "driveio.h"
#include "network.h"

namespace {
const unsigned long retryInterval_ms = 10000;
const unsigned long dhcpTimeout_ms = 2000;
const unsigned long dhcpResponseTimeout_ms = 500;
bool ready = false;
unsigned long lastAttempt_ms = 0;

void acquireLease()
{
    Serial.println("NET: Requesting DHCP lease");
    ready = Ethernet.begin(mac, dhcpTimeout_ms, dhcpResponseTimeout_ms) == 1;
    lastAttempt_ms = millis();
    if (ready) {
        Serial.print("NET: DHCP address: ");
        Serial.println(Ethernet.localIP());
    } else {
        Serial.println("NET: DHCP failed; retry in 10 seconds");
    }
}
}

void network_init()
{
    acquireLease();
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("ERROR: Ethernet shield was not found");
    }
}

bool network_isready()
{
    return ready && Ethernet.linkStatus() != LinkOFF;
}

void network_loop()
{
    // DHCP can block. Never extend a running drive command pulse.
    if (driveio_doorcommandactive()) return;

    if (Ethernet.linkStatus() == LinkOFF) {
        if (ready) ethClient.stop();
        ready = false;
        return;
    }
    if (!ready) {
        if (millis() - lastAttempt_ms >= retryInterval_ms) acquireLease();
        return;
    }

    IPAddress previousIP = Ethernet.localIP();
    int result = Ethernet.maintain();
    if (result == 1 || result == 3) {
        // Drop stale connections and acquire a fresh lease on the next retry.
        Serial.println("NET: DHCP renewal failed; reconnecting");
        ethClient.stop();
        ready = false;
        lastAttempt_ms = millis();
    } else if (result == 2 || result == 4) {
        Serial.print("NET: DHCP lease renewed: ");
        Serial.println(Ethernet.localIP());
        if (Ethernet.localIP() != previousIP) ethClient.stop();
    }
}
