#pragma once
#include <Arduino.h>
void lan_update_init();
void lan_update_loop();
bool lan_update_busy();
bool lan_update_installing();
bool lan_update_available();
bool lan_update_state_changed();
String lan_update_version();
bool lan_update_can_install();
const char* lan_update_install_block_reason();
void lan_update_request_install(const char* source);
void lan_update_note_door_command(const String& source);
bool lan_update_display(String* lines);
