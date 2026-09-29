#pragma once
// Command-wheel cards: the 120x64 illustration of what a command does (a mini canvas + a mini
// layers panel), drawn from the profile's scene description (app_profile.h app_scene_t) --
// nothing is stored as pixels. Crisp by rule: whole-pixel rects and 1-bit glyphs only, no
// anti-aliased edges (DEVELOPMENT_PLAN.md "Figma command wheel", preview round 3).

#include <stdint.h>
extern "C" {
#include "app_profiles/app_profile.h"
}

namespace ui {

constexpr int CARD_W = 120;
constexpr int CARD_H = 64;

// The scene's keyframe at t_ms (looping), with the card chrome. scene == nullptr draws the
// "cancel" card.
void draw_card(const app_scene_t *scene, int x, int y, uint32_t t_ms);

// The wheel screen (replaces the Main Screen while the wheel key is held).
struct WheelView {
    const char *ring_name;      // header
    const char *ring_tabs[8];   // short ring names ("BUILD", "ALIGN") -- the ring tabs
    int ring_count, ring;       // tabs + the open one
    int count, entry;           // entries incl. cancel (0) + the chosen one
    const char *name;           // "WRAP IN FRAME" / "CANCEL"
    uint8_t modifier;           // KEYS: KEYBOARD_MODIFIER_* of the shortcut
    const char *key;            // KEYS: its key ("G"); nullptr for ACTIONS / cancel
    bool search;                // ACTIONS: shown as Cmd K SEARCH
    const app_scene_t *scene;   // this entry's card (nullptr = cancel)
    const app_scene_t *prev;    // the card sliding out, or nullptr
    bool prev_valid;            // a slide is in flight (prev may be the cancel card)
    float slide;                // 0..1 slide progress, 1 = at rest
    int slide_dir;              // +1: moved to the next entry (cards move left)
    uint32_t t_ms;              // time since this entry was chosen (card animation clock)
};
void draw_wheel(const WheelView &v);

// Shortcut as small dark keycaps with white legends (modifier glyphs + key), centered.
void draw_chord(uint8_t modifier, const char *key, float cx, int y, const char *tail);

} // namespace ui
