#pragma once
#include "driveio.h"

enum class DoorState { Unknown, Open, Closed, Opening, Closing };

// End switches cannot distinguish intermediate motion from a stopped door.
// Motion is an assumption after a command, not a measured movement signal.
// Repeated direction pulses have not stopped the tested drive: never infer stop.
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
            // Keep the prior motion assumption and target until an end switch.
            // Still permit the requested pulse without claiming stop or reversal.
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
