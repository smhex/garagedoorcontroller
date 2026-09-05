#pragma once

// Called with the sampled button level. A held button produces one command.
class ButtonEdge {
public:
    bool update(bool pressed) {
        const bool rising = pressed && !wasPressed;
        wasPressed = pressed;
        return rising;
    }
private:
    bool wasPressed = false;
};
