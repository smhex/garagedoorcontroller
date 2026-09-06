#pragma once
#include <stdint.h>

// A restart is armed only after the broker confirms deletion of the retained command.
class MqttRestart {
public:
    void request() { if (!armed) pending = true; }
    bool requested() const { return armed; }
    template <typename Send>
    bool service(uint32_t now, bool ready, Send send) {
        if (!pending || armed || !ready) return false;
        if (attempted && uint32_t(now - lastAttempt) < 10000) return false;
        attempted = true;
        lastAttempt = now;
        if (!send("gdc/system/restart", "", true, 1)) return false;
        pending = false;
        armed = true;
        return true;
    }
private:
    bool pending = false, armed = false, attempted = false;
    uint32_t lastAttempt = 0;
};

template <typename Send>
bool mqtt_counted_send(uint32_t& count, Send send) {
    if (!send()) return false;
    ++count;
    return true;
}
