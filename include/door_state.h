#pragma once
#include <Arduino.h>
#include "driveio.h"

enum class DoorState { Unknown, Open, Closed, Opening, Closing, Stopped };

// End switches cannot distinguish intermediate motion from a stopped door.
// Motion and an implicit stop are controller-owned assumptions, not measured
// movement signals.
class DoorStateTracker {
public:
    DoorState state = DoorState::Unknown;
    int target = 0;

    void observe(int input, bool commandPulseActive = false) {
        // The command can affect the shared bus status while its output is held.
        // Do not consume these samples, including the last sample before release.
        if (commandPulseActive) return;
        if (input == observed) return;
        observed = input;
        if (input == departureInput) return;
        departureInput = -1;
        if (input == DOORSTATUSOPEN) {
            if (travelActive && target == DOORCOMMANDOPEN)
                lastOpenTravel = millis() - travelStartedAt;
            travelPending = travelActive = false;
            state = DoorState::Open;
            target = DOORCOMMANDOPEN;
        } else if (input == DOORSTATUSCLOSED) {
            if (travelActive && target == DOORCOMMANDCLOSE)
                lastCloseTravel = millis() - travelStartedAt;
            travelPending = travelActive = false;
            state = DoorState::Closed;
            target = DOORCOMMANDCLOSE;
        } else if (state == DoorState::Open || state == DoorState::Closed) {
            // Movement initiated outside this controller has no known direction.
            state = DoorState::Unknown;
        }
    }

    bool command(int direction) {
        if (direction != DOORCOMMANDOPEN && direction != DOORCOMMANDCLOSE) return false;
        if ((state == DoorState::Open && direction == DOORCOMMANDOPEN) ||
            (state == DoorState::Closed && direction == DOORCOMMANDCLOSE)) return false;
        travelEligible = state == DoorState::Open || state == DoorState::Closed;
        travelPending = true;
        travelActive = false;
        departureInput = state == DoorState::Open ? DOORSTATUSOPEN :
                         state == DoorState::Closed ? DOORSTATUSCLOSED : -1;
        target = direction;
        state = direction == DOORCOMMANDOPEN ? DoorState::Opening : DoorState::Closing;
        return true;
    }

    bool isMoving() const {
        return state == DoorState::Opening || state == DoorState::Closing;
    }

    // The drive stops when either command input is pulsed during travel. Reuse
    // the output that initiated the current direction, regardless of the newly
    // requested direction, so a second command is always an implicit stop.
    int stop() {
        if (!isMoving()) return 0;
        const int stopDirection = target;
        travelPending = travelActive = false;
        state = DoorState::Stopped;
        return stopDirection;
    }

    void travelStarted(int direction, uint32_t startedAt) {
        if (!travelPending || direction != target) return;
        travelPending = false;
        travelActive = travelEligible;
        travelStartedAt = startedAt;
    }

    uint32_t lastOpenTravelMs() const { return lastOpenTravel; }
    uint32_t lastCloseTravelMs() const { return lastCloseTravel; }

private:
    int observed = -1;
    int departureInput = -1;
    bool travelPending = false;
    bool travelActive = false;
    bool travelEligible = false;
    uint32_t travelStartedAt = 0;
    uint32_t lastOpenTravel = 0;
    uint32_t lastCloseTravel = 0;
};

extern DoorStateTracker doorState;
