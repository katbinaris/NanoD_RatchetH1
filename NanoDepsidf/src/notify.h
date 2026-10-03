#pragma once

#include <stdbool.h>
#include <stdint.h>

// Agent notifications (Claude Code, Codex, Cursor): the host daemon (tools/agents/) posts them
// over the vendor interface (ext_proto.h EXT_CMD_NOTIFY). While one is up -- menu closed -- the
// ring breathes in the agent's colour, the screen shows it, and the keys answer it:
//   ASK  (an approval):  hold F1 = ALLOW, F3 = DENY, F2 / F4 = LATER (answer on the computer)
//   INFO (needs you):    any key = DISMISS
// A decision goes back to the host as an event. The oldest item shows first.

#define NOTIFY_MAX 4
#define NOTIFY_TITLE_MAX 16
#define NOTIFY_BODY_MAX 39
#define NOTIFY_HOLD_MS 700 // F1 must be held this long to allow: no accidental approvals

typedef enum { NOTIFY_SRC_CLAUDE = 0, NOTIFY_SRC_CODEX, NOTIFY_SRC_CURSOR, NOTIFY_SRC_OTHER, NOTIFY_SRC_COUNT } notify_source_t;
typedef enum { NOTIFY_ASK = 0, NOTIFY_INFO, NOTIFY_KIND_COUNT } notify_kind_t;
typedef enum { NOTIFY_ALLOW = 1, NOTIFY_DENY = 2, NOTIFY_LATER = 3, NOTIFY_DISMISS = 4 } notify_decision_t;
#define NOTIFY_FLAG_NUDGE 0x01 // input is needed: a gentle double tap in the knob every few seconds

typedef struct {
    uint16_t id;
    uint8_t source; // notify_source_t
    uint8_t kind;   // notify_kind_t
    uint8_t flags;  // NOTIFY_FLAG_*
    uint32_t color; // RGB888 chosen by the host; 0 = the source's own
    char title[NOTIFY_TITLE_MAX + 1];
    char body[NOTIFY_BODY_MAX + 1];
} notify_item_t;

// --- host side (TinyUSB / usb task, Core 1) ---
void notify_post(const notify_item_t *it); // a new id joins the queue; a known id is updated
void notify_clear(uint16_t id);
void notify_clear_all(void);
bool notify_take_event(uint16_t *id, uint8_t *decision); // decisions waiting to go to the host

// --- control task (Core 0, every tick, CONTROL_HOT) ---
bool notify_active(void);
// The held-key mask (UI_BTN_*) while an item is up and the menu is closed. F1 (allow) must be
// the physical button only: the caller leaves the companion's virtual F1 out.
void notify_keys(uint8_t held, int64_t now_us);
// The nudge: a voltage to add to the motor's q axis this tick (0 = none). `shown`: an item is
// up and the menu is closed. Two soft bursts when an item that needs input appears, then
// again every NOTIFY_NUDGE_PERIOD_MS; quiet while F1 is held.
#define NOTIFY_NUDGE_PERIOD_MS 5000
float notify_nudge_vq(int64_t now_us, bool shown);

// --- display / LEDs (Core 1) ---
// The item on show, how many are waiting (itself included) and how far F1 has been held
// (0..1). False when there's nothing to show.
bool notify_peek(notify_item_t *out, int *count, float *hold);
// The colours of everything waiting, the one on show first; returns how many (<= max).
int notify_colors(uint32_t *out, int max);
uint32_t notify_version(void); // bumps on every change of what's shown
uint32_t notify_color(const notify_item_t *it); // the host's colour, else the source's (RGB888)
const char *notify_source_name(uint8_t source);
