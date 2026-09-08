#include "debug_console.h"
#include "debug_window.h"
#include "network.h"
#include <Ethernet.h>

namespace {
EthernetServer server(2323);
EthernetClient client;
DebugWindow window;
struct Queue {
    uint8_t data[2048];
    size_t head = 0, count = 0;
    void put(uint8_t value) {
        if (count < sizeof(data)) data[(head + count++) % sizeof(data)] = value;
    }
    void clear() { head = count = 0; }
    template<class Sink> void drain(Sink& sink) {
        int room = sink.availableForWrite();
        size_t n = room > 0 ? min(count, static_cast<size_t>(room)) : 0;
        n = min(n, sizeof(data) - head);
        if (n) {
            size_t sent = sink.write(data + head, n);
            head = (head + sent) % sizeof(data); count -= sent;
        }
    }
};
Queue usb, lan;
bool captureRequested = false;
bool listening = false;
IPAddress boundIP;

void command(int value) {
    if (value == 'd') {
        window.enable();
        Debug.println("DEBUG: output enabled until reboot; c = 30-second input capture");
    } else if (value == 'c') {
        window.enable();
        captureRequested = true;
    }
}
}

DebugOutput Debug;

size_t DebugOutput::write(uint8_t value) {
    // Logging never touches Ethernet or USB while a drive output is active.
    if (window.enabled(millis())) { usb.put(value); lan.put(value); }
    return 1;
}

bool debug_console_take_capture() {
    bool requested = captureRequested;
    captureRequested = false;
    return requested;
}

void debug_console_loop() {
    for (unsigned i = 0; i < 32 && Serial.available(); ++i) command(Serial.read());
    if (!network_isready()) {
        if (listening) client.stop();
        listening = false;
    } else {
        if (!listening || boundIP != Ethernet.localIP()) {
            client.stop();
            boundIP = Ethernet.localIP();
            server.begin();
            listening = true;
        }
        EthernetClient incoming = server.accept();
        if (incoming) {
            incoming.setConnectionTimeout(20);
            if (!client || !client.connected()) {
                client.stop();
                client = incoming;
            } else incoming.stop();
        }
        for (unsigned i = 0; i < 32 && client.available(); ++i) command(client.read());
    }
    if (!window.enabled(millis())) {
        usb.clear(); lan.clear();
        return;
    }
    // Best effort: slow or absent readers lose output instead of stalling control.
    if (Serial) usb.drain(Serial);
    else usb.clear();
    if (client.connected()) lan.drain(client);
    else lan.clear();
}
