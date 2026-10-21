#pragma once
// Controller configuration (F1 > Controls, docs/CONTROLS.md): which device drives each
// GameCube port, per-device binding profiles for every GameCube input, stick/trigger
// shaping and rumble. Saved in settings.ini; applied to Aurora's PAD live.
#include <dolphin/types.h>

union SDL_Event;
struct PADStatus;

namespace recomp::controls {

// F1 menu tab (host thread). Returns true when a setting changed (the menu saves on close).
bool drawTab();
void selectSubTab(const char* name);  // "Ports", "Bindings" or "Analog": selected on the next drawTab() (test aids)
// Host event loop, before the menu's own hotkeys. True: the event belongs to a "press a
// button to bind" capture and must not reach F1/Esc or other hotkeys.
bool handleEvent(const SDL_Event& ev);

// hle_pad.cpp, around Aurora's PADRead: beforeRead assigns devices to ports (hot-plug)
// and pushes changed bindings to Aurora; afterRead applies dead zones / sensitivity and
// records the values for the menu's input test.
void beforeRead();
void afterRead(PADStatus* status);
// PADControlMotor with the port's rumble setting applied.
void controlMotor(u32 chan, u32 cmd);

// Buttons pressed from outside the controller (the AYN Thor companion screen's touch buttons,
// docs/ANDROID.md): OR'd into port 1's buttons for the next `reads` PAD reads. Any thread.
void pressButtons(u16 mask, int reads);

}  // namespace recomp::controls
