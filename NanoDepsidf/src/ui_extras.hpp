#pragma once
// This fork's screens (LIGHTS, agent notifications), drawn with the Pixel UI toolkit like
// ui_screens.cpp and kept apart from it so upstream changes merge cleanly.

#include <stdint.h>
extern "C" {
#include "menu.h"
}

namespace ui {

// LIGHTS: one row per field (MENU_LIGHTS_ROW_*), the focused one on an amber bar. The rim of
// the glass mirrors the LED ring as it is right now (`ring`: 60 RGB triples, clockwise from 12
// o'clock as led_task_snapshot() gives them; nullptr = none). `swatch`: the colour in use.
struct LightsInputs {
    uint32_t swatch;
    const uint8_t (*ring)[3];
    bool blink_on;
};
void draw_lights(const menu_render_snapshot_t &snap, const LightsInputs &in);

// An agent notification (notify.h). Takes the whole screen while the menu is closed.
enum { AGENT_CLAUDE = 0, AGENT_CODEX, AGENT_CURSOR, AGENT_OTHER };
struct NotifyInputs {
    int agent;               // AGENT_*: which mark goes on the badge
    const char *source;      // "CLAUDE CODE"
    uint32_t color;          // the agent's colour (RGB888)
    const char *title;       // "RUN"
    const char *body;        // the command or file, wrapped to three lines
    bool ask;                // an approval (ALLOW / DENY), else an attention item (any key)
    const uint32_t *queue;   // colours of everything waiting, this one first
    int waiting;             // how many (>= 1)
    float hold;              // F1 hold progress 0..1 -- a green arc fills the rim
    uint8_t buttons;         // held keys (UI_BTN_*)
    uint32_t t_ms;           // animation clock
};
void draw_notify(const NotifyInputs &in);

} // namespace ui
