#pragma once
#include <Arduino.h>
void lan_update_init();
void lan_update_loop();
bool lan_update_busy();
bool lan_update_approval_pending();
void lan_update_cancel_for_command();
bool lan_update_display(String* lines);
