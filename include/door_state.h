#pragma once
#include "driveio.h"

enum class DoorState { Unknown, Open, Closed, Opening, Closing, Stopped };

// End switches cannot distinguish intermediate motion from a stopped door.
// Motion and stops are inferred only from commands sent by this controller.
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
        if (input == DOORSTATUSOPEN) {
            state = DoorState::Open;
            target = DOORCOMMANDOPEN;
        } else if (input == DOORSTATUSCLOSED) {
            state = DoorState::Closed;
            target = DOORCOMMANDCLOSE;
        } else if (state == DoorState::Open || state == DoorState::Closed) {
            // Movement initiated outside this controller has no known direction.
            state = DoorState::Unknown;
        }
    }

    bool command(int direction) {
        if (direction != DOORCOMMANDOPEN && direction != DOORCOMMANDCLOSE) return false;
        if (state == DoorState::Opening || state == DoorState::Closing) {
            state = DoorState::Stopped;
            // HomeKit has no stopped target: preserve the previous target.
        } else {
            if ((state == DoorState::Open && direction == DOORCOMMANDOPEN) ||
                (state == DoorState::Closed && direction == DOORCOMMANDCLOSE)) return false;
            target = direction;
            state = direction == DOORCOMMANDOPEN ? DoorState::Opening : DoorState::Closing;
        }
        return true;
    }

private:
    int observed = -1;
};

extern DoorStateTracker doorState;
