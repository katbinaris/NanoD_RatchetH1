#include "display_task.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "lgfx_config.hpp"
#include "ui_gfx.hpp"
#include "ui_fx.hpp"
#include "ui_screens.hpp"
#include "ui_extras.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "driver/ledc.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Plain-C model layer -- linkage fixed here at the include site so menu.h/ui_state.h
// themselves stay untouched.
extern "C" {
#include "menu.h"
#include "ui_state.h"
#include "icon_store.h"
#include "app_mode.h"
#include "app_profiles/app_profiles.h"
#include "class/hid/hid.h"
#include "sysmon.h"
#include "notify.h"
#include "led_task.h"
#include "media.h"
#include "agent_board.h"
#include "esp_heap_caps.h"
#include "user_prefs.h"
#include "app_colors.h"
}

static const char *TAG = "display";

// --- Pixel UI (DEVELOPMENT_PLAN.md "Pixel UI") ---
// This file owns *when* things are drawn; ui_screens / ui_fx own *what* (ported from the
// approved browser mockups), ui_gfx the pixel primitives.
//
// Rendering model (unchanged from the LovyanGFX migration): one full-screen 240x240 RGB565
// LGFX_Sprite in internal RAM, fully redrawn and sent with one pushSprite() per frame
// (measured: draw ~1ms, push ~11.8ms). Frames are only produced when needed:
//   - on any change a viewer would see (menu snapshot, held buttons, view change, blink edge,
//     SAVED! on/off);
//   - continuously at ~30fps while something loops (loading screen, attract animation, the
//     FEEL curve when FEEL has focus);
//   - back-to-back (yield only) during short transitions (iris wipe, list scroll, FEEL morph,
//     HID carousel slide) so a 60-320ms move gets every frame it can.
// Everything else stays event-driven at the 30ms poll, as before.
//
// Views: one per menu screen, plus the loading screen and the attract animation. Every view
// change goes through the iris wipe. Attract starts after ATTRACT_IDLE_MS with no knob
// movement or button change on the Main Screen; any input ends it (control_task.c swallows
// the waking button press via ui_state_set_screensaver()).

#define LCD_LEDC_TIMER LEDC_TIMER_0
#define LCD_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_LEDC_FREQ_HZ 5000
#define LCD_BACKLIGHT_DUTY_PERCENT 80 // starting point, not tuned against ambient light yet

#define UI_REDRAW_PERIOD_MS 30 // idle poll
#define UI_ANIM_DELAY_MS 20    // between frames of a looping animation (+~13ms frame = ~30fps)

#define IRIS_MS 320
#define ATTRACT_IDLE_MS 5000 // by request: 5s without touching the knob or buttons
#define TOAST_MS 1300
#define FEEL_MORPH_MS 250
#define HID_SLIDE_MS 160
#define HID_SLIDE_PX 70
#define MENU_LIST_ROW_PITCH_PX 32
#define MENU_LIST_ANIM_MS 60 // hardware-tuned (LVGL era), unchanged

static LGFX s_lcd;
static LGFX_Sprite s_frame(&s_lcd);

// Backlight: plain LEDC, carried over verbatim from the LVGL version -- independent of the
// graphics library, deliberately not moved to LovyanGFX's Light_PWM.
static void backlight_init(void) {
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    timer_cfg.timer_num = LCD_LEDC_TIMER;
    timer_cfg.freq_hz = LCD_LEDC_FREQ_HZ;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num = PIN_LCD_BL;
    channel_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    channel_cfg.channel = LCD_LEDC_CHANNEL;
    channel_cfg.timer_sel = LCD_LEDC_TIMER;
    channel_cfg.duty = 0; // ramped up explicitly after panel init -- avoids a visible flash of
                          // uninitialized panel RAM before the first real frame is drawn
    channel_cfg.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

static void backlight_set_percent(uint32_t percent) {
    uint32_t duty = (255 * percent) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL);
}

// Internal DMA-capable RAM first (fastest push path); PSRAM only as a fallback.
static bool frame_init(void) {
    s_frame.setColorDepth(16);
    s_frame.setPsram(false);
    if (s_frame.createSprite(LCD_WIDTH, LCD_HEIGHT) != nullptr) {
        ESP_LOGI(TAG, "frame sprite in internal RAM");
        return true;
    }
    ESP_LOGW(TAG, "frame sprite: internal RAM allocation failed, trying PSRAM");
    s_frame.setPsram(true);
    return s_frame.createSprite(LCD_WIDTH, LCD_HEIGHT) != nullptr;
}

// --- view state ---

enum View : uint8_t {
    V_BOOT, V_MAIN, V_ROOT, V_HAPTIC, V_HID, V_BOOTMODE, V_APP_PROFILE, V_DISPLAY, V_DEVICE, V_SYSINFO, V_RECAL, V_BINDINGS, V_ATTRACT,
    V_LIGHTS, V_NOTIFY
};

static View view_for(const menu_render_snapshot_t &s) {
    switch (s.screen) {
        case MENU_SCREEN_ROOT: return V_ROOT;
        case MENU_SCREEN_HAPTIC: return V_HAPTIC;
        case MENU_SCREEN_HID: return V_HID;
        case MENU_SCREEN_BOOT: return V_BOOTMODE;
        case MENU_SCREEN_APP_PROFILE: return V_APP_PROFILE;
        case MENU_SCREEN_DISPLAY: return V_DISPLAY;
        case MENU_SCREEN_DEVICE: return V_DEVICE;
        case MENU_SCREEN_SYSINFO: return V_SYSINFO;
        case MENU_SCREEN_RECALIBRATE: return V_RECAL;
        case MENU_SCREEN_BINDINGS: return V_BINDINGS;
        case MENU_SCREEN_LIGHTS: return V_LIGHTS;
        default: return V_MAIN;
    }
}

static inline bool is_settings_view(View v) {
    return v == V_HAPTIC || v == V_HID || v == V_BOOTMODE || v == V_APP_PROFILE || v == V_DISPLAY || v == V_BINDINGS
        || v == V_LIGHTS;
}

// --- MUSIC: the now-playing cover (media.h) ---
// Decoded once per cover into a PSRAM sprite, its lower part darkened under the title, then
// copied into the frame each time the screen is drawn.
static LGFX_Sprite s_cover;
static uint8_t *s_jpeg = nullptr;
static uint32_t s_cover_seen = 0;
static bool s_cover_ok = false;

static void cover_tick(void) {
    uint32_t v = media_cover_version();
    if (v == s_cover_seen) return;
    s_cover_seen = v;
    if (s_jpeg == nullptr) s_jpeg = (uint8_t *)heap_caps_malloc(MEDIA_COVER_MAX, MALLOC_CAP_SPIRAM);
    if (s_cover.getBuffer() == nullptr) {
        s_cover.setColorDepth(16);
        s_cover.setPsram(true);
        s_cover.createSprite(LCD_WIDTH, LCD_HEIGHT);
    }
    uint32_t ver;
    size_t n = s_jpeg ? media_cover_copy(s_jpeg, MEDIA_COVER_MAX, &ver) : 0;
    if (n == 0 || s_cover.getBuffer() == nullptr) {
        s_cover_ok = false;
        return;
    }
    s_cover.fillScreen(TFT_BLACK);
    s_cover_ok = s_cover.drawJpg(s_jpeg, n, 0, 0);
    if (!s_cover_ok) {
        ESP_LOGW(TAG, "cover: JPEG decode failed (%u bytes)", (unsigned)n);
        return;
    }
    // Darken the lower part for the title: a smooth ramp from y 148 to 188, then 24%.
    uint16_t *px = (uint16_t *)s_cover.getBuffer(); // RGB565, byte-swapped (big-endian)
    for (int y = 148; y < LCD_HEIGHT; y++) {
        float t = y >= 188 ? 1.0f : (y - 148) / 40.0f;
        float k = 1.0f - 0.76f * t * t * (3 - 2 * t);
        for (int x = 0; x < LCD_WIDTH; x++) {
            uint16_t v = px[y * LCD_WIDTH + x];
            v = (uint16_t)(v << 8 | v >> 8);
            uint16_t r = (uint16_t)(((v >> 11) & 31) * k), g = (uint16_t)(((v >> 5) & 63) * k), b = (uint16_t)((v & 31) * k);
            v = (uint16_t)(r << 11 | g << 5 | b);
            px[y * LCD_WIDTH + x] = (uint16_t)(v << 8 | v >> 8);
        }
    }
}

static bool profile_is(const char *id) {
    return menu_get_hid_type() == MENU_HID_APP && strcmp(app_profiles_get(menu_get_app_profile())->id, id) == 0;
}

// Overlays on the now-playing screen: the volume while the knob turns, a glyph after a key.
#define NP_VOLUME_MS 1500
#define NP_GLYPH_MS 700
static int64_t s_np_turned_us = -(1LL << 40), s_np_key_us = -(1LL << 40);
static int s_np_glyph = ui::NP_GLYPH_NONE;

// The notification on show, kept for the iris that closes on it after it's been answered.
static notify_item_t s_notice;
static int s_notice_waiting = 0;
static float s_notice_hold = 0;

// The colour the LIGHTS swatch shows: the custom hue, or the active app's (amber without one).
static uint32_t lights_swatch(void) {
    lights_t l;
    lights_get(&l);
    if (l.src == LIGHT_SRC_CUSTOM) return lights_hsv((float)l.hue, l.sat / 100.0f, 1.0f);
    uint32_t acc[3] = {ui::AMBER, ui::AMBER, ui::AMBER};
    if (menu_get_hid_type() == MENU_HID_APP) {
        const app_profile_t *p = app_profiles_get(menu_get_app_profile());
        app_accents(p->icon48, p->plasma_heat, acc);
    }
    return acc[1];
}

// Screens drawn as the scrolling text list.
static inline bool is_list_screen(menu_screen_id_t s) {
    return s == MENU_SCREEN_ROOT || s == MENU_SCREEN_DEVICE;
}

static View s_view = V_BOOT;
static int64_t s_boot_start_us = 0;
static bool s_booting = true;

// Iris wipe: the first half closes on the old view (drawn from the snapshot it was showing),
// the second half opens on the new one.
static bool s_iris_active = false;
static View s_iris_from = V_BOOT;
static int64_t s_iris_start_us = 0;
static menu_render_snapshot_t s_iris_from_snap;

static bool s_attract_on = false;
static int64_t s_attract_start_us = 0;
static uint32_t s_attract_seed = 0; // picks this idle session's random routines
static int64_t s_last_activity_us = 0;

static menu_render_snapshot_t s_last_snap;
static bool s_have_last = false;
static uint8_t s_last_buttons = 0;
static int32_t s_last_detent = 0;

static uint32_t s_last_save_count = 0;
static uint32_t s_last_reset_count = 0;
static bool s_toast_factory = false; // the toast says FACTORY, not SAVED!
static int64_t s_toast_until_us = 0;

static haptic_type_t s_last_feel = HAPTIC_TYPE_SAW;
static int s_morph_from = -1;
static int64_t s_morph_start_us = -(1LL << 40);

static menu_hid_type_t s_last_hid = MENU_HID_APP;
static int s_slide_dir = 0;
static int64_t s_slide_start_us = -(1LL << 40);

// PROFILE screen carousel slide (same timing as the HID one).
#define PROFILE_SLIDE_PX 80
static int32_t s_last_profile = 0;
static int s_profile_slide_dir = 0;
static int64_t s_profile_slide_start_us = -(1LL << 40);

// Top-level list scroll -- same model as before: one animated scroll value that centers the
// selected row, jumping (not sliding) whenever the list is (re)entered.
static float s_list_from = 0, s_list_to = 0;
static int64_t s_list_start_us = 0;
static bool s_list_anim = false;
static menu_screen_id_t s_list_prev_screen = MENU_SCREEN_NONE;

// HID-uploaded icon (RAM only): copied out of icon_store whenever its version moves, so the
// draw path never touches the USB side's buffer.
static uint8_t s_icon[ICON_BYTES];
static bool s_icon_set = false;
static uint32_t s_icon_version = 0;

static bool s_drawn_blink = false;
static bool s_drawn_toast = false;

static float list_scroll_now(int64_t now) {
    if (!s_list_anim) return s_list_to;
    int64_t el = now - s_list_start_us;
    if (el >= MENU_LIST_ANIM_MS * 1000LL) {
        s_list_anim = false;
        return s_list_to;
    }
    return s_list_from + (s_list_to - s_list_from) * (float)el / (MENU_LIST_ANIM_MS * 1000.0f);
}

static void list_retarget(const menu_render_snapshot_t &snap, int64_t now) {
    float target = (snap.selected < 0 ? 0 : snap.selected) * (float)MENU_LIST_ROW_PITCH_PX;
    if (s_list_prev_screen != snap.screen) {
        // Unconditional jump on (re)entry -- never nested under a "target changed" check;
        // that exact guard caused the first-open pile-up bug under LVGL.
        s_list_to = target;
        s_list_anim = false;
    } else if (target != s_list_to) {
        s_list_from = list_scroll_now(now);
        s_list_to = target;
        s_list_start_us = now;
        s_list_anim = true;
    }
}

// --- APP-mode 3D shape (profiles with APP_VISUAL_SHAPE, e.g. Plasticity) ---
// The shape follows the knob's continuous travel (ui_state_get_knob_angle) into whichever
// channel the live action drives: orbit turns it one-for-one, zoom slides the nested copies
// (one doubling per quarter turn), pan scrolls the floor (~380px per turn). Letting go of
// orbit eases it to the nearest 45 + k*90 deg pose -- the clean 2:1 isometric view.
#define SHAPE_SETTLE_MS 220
#define SHAPE_FLASH_MS 260
#define SHAPE_ACTIVE_MS 120     // keep frames coming this long after the knob last moved
#define SHAPE_DEADBAND 20       // knob travel below 0.002 rad is sensor noise, not a move
#define SHAPE_KNOB_SIGN 1       // flip if the shape turns against the knob
static float s_shape_yaw = (float)M_PI / 4, s_shape_zoom = 0, s_shape_pan = 0;
static int32_t s_shape_last_angle = 0;
static bool s_shape_have_angle = false;
static app_fx_t s_shape_last_fx = APP_FX_NONE;
static bool s_settling = false;
static float s_settle_from = 0, s_settle_to = 0;
static int64_t s_settle_start_us = 0;
static int64_t s_flash_until_us = 0;
static int s_flash_slot = APP_SLOT_F3;
static int64_t s_shape_moved_us = -(1LL << 40);
static uint8_t s_shape_prev_buttons = 0;

static const app_profile_t *shape_profile(void) {
    if (menu_get_hid_type() != MENU_HID_APP) return nullptr;
    const app_profile_t *p = app_profiles_get(menu_get_app_profile());
    return p->visual == APP_VISUAL_SHAPE ? p : nullptr;
}

// A slot with no turn action (a TAP, or nothing) leaves the knob's own action live.
static int turn_slot(const app_profile_t *p, int slot) {
    app_action_kind_t k = p->slot[slot].kind;
    return (k == APP_ACT_NONE || k == APP_ACT_TAP) ? APP_SLOT_KNOB : slot;
}

// Every tick. Returns true while the shape needs frames (moving, settling or flashing).
static bool shape_tick(int64_t now, uint8_t buttons) {
    int32_t angle = ui_state_get_knob_angle();
    if (!s_shape_have_angle) {
        s_shape_last_angle = angle;
        s_shape_have_angle = true;
    }
    int32_t diff = angle - s_shape_last_angle;
    float d = 0;
    if (diff >= SHAPE_DEADBAND || diff <= -SHAPE_DEADBAND) {
        d = diff * 1e-4f * SHAPE_KNOB_SIGN;
        s_shape_last_angle = angle;
    }
    uint8_t pressed = buttons & ~s_shape_prev_buttons;
    s_shape_prev_buttons = buttons;
    const app_profile_t *p = shape_profile();
    if (p == nullptr || menu_is_open()) return false;

    app_fx_t fx = p->slot[turn_slot(p, app_mode_live_slot())].fx;
    if (d != 0) {
        s_shape_moved_us = now;
        switch (fx) {
            case APP_FX_ORBIT: s_shape_yaw += d; s_settling = false; break;
            case APP_FX_PAN: s_shape_pan += d * 60.0f; break;
            case APP_FX_ZOOM: s_shape_zoom += d / ((float)M_PI / 2); break;
            default: break;
        }
    }
    if (s_shape_last_fx == APP_FX_ORBIT && fx != APP_FX_ORBIT) {
        const float q = (float)M_PI / 2, rest = (float)M_PI / 4;
        s_settle_from = s_shape_yaw;
        s_settle_to = roundf((s_shape_yaw - rest) / q) * q + rest;
        s_settle_start_us = now;
        s_settling = true;
    }
    s_shape_last_fx = fx;
    // A key's quick-press tap (e.g. F3 = undo while holding it opens the wheel) flashes too.
    static uint32_t s_taps_seen = 0;
    static bool s_taps_have = false;
    int tap_slot;
    uint32_t taps = app_mode_last_tap(&tap_slot);
    if (s_taps_have && taps != s_taps_seen && tap_slot < APP_SLOT_COUNT && p->slot[tap_slot].fx == APP_FX_FLASH) {
        s_flash_until_us = now + SHAPE_FLASH_MS * 1000LL;
        s_flash_slot = tap_slot;
    }
    s_taps_seen = taps;
    s_taps_have = true;
    for (int slot = APP_SLOT_F1; slot < APP_SLOT_COUNT; slot++) {
        static const uint8_t KEY[APP_SLOT_COUNT] = {0, UI_BTN_F1, UI_BTN_F2, UI_BTN_F3, UI_BTN_F4};
        if ((pressed & KEY[slot]) && p->slot[slot].kind == APP_ACT_TAP && p->slot[slot].fx == APP_FX_FLASH) {
            s_flash_until_us = now + SHAPE_FLASH_MS * 1000LL;
            s_flash_slot = slot;
        }
    }
    if (s_settling) {
        float k = (now - s_settle_start_us) / (SHAPE_SETTLE_MS * 1000.0f);
        if (k >= 1) {
            k = 1;
            s_settling = false;
        }
        float e = 1 - (1 - k) * (1 - k) * (1 - k);
        s_shape_yaw = s_settle_from + (s_settle_to - s_settle_from) * e;
    }
    return s_settling || now < s_flash_until_us || now - s_shape_moved_us < SHAPE_ACTIVE_MS * 1000LL;
}

// APP mode: the active profile as the screens see it. `slot` is the live slot.
static ui::AppView app_view(int slot, int64_t now) {
    static const char *const VIA[APP_SLOT_COUNT] = {"KNOB", "F1 + KNOB", "F2 + KNOB", "F3 + KNOB", "F4 + KNOB"};
    static const char *const KEY[APP_SLOT_COUNT] = {"KNOB", "F1", "F2", "F3", "F4"};
    const app_profile_t *p = app_profiles_get(menu_get_app_profile());
    slot = turn_slot(p, slot);
    ui::AppView v = {};
    v.name = p->name;
    v.icon24 = p->icon24;
    for (int i = 0; i < 4; i++) v.legend[i] = p->legend[i];
    v.action = p->slot[slot].label ? p->slot[slot].label : "";
    v.action_via = VIA[slot];
    v.action_key = KEY[slot];
    if (p->visual == APP_VISUAL_SHAPE) {
        static ui::ShapeView sv;
        app_fx_t fx = p->slot[slot].fx;
        sv.shape = (ui::ShapeKind)p->shape;
        sv.style = (ui::ShapeStyle)p->shape_style;
        sv.scene = fx == APP_FX_ORBIT ? ui::SCENE_ORBIT : fx == APP_FX_PAN ? ui::SCENE_PAN : ui::SCENE_ZOOM;
        sv.yaw = s_shape_yaw;
        sv.zoom = s_shape_zoom;
        sv.pan = s_shape_pan;
        if (p->shape_stepped) {
            // Only clean poses: 32 per turn (45 deg is one), zoom in 1/8 doublings, pan in
            // 2px -- no sub-pixel crawl between frames.
            const float step = 2 * (float)M_PI / 32;
            sv.yaw = roundf(sv.yaw / step) * step;
            sv.zoom = roundf(sv.zoom * 8) / 8;
            sv.pan = roundf(sv.pan / 2) * 2;
        }
        sv.flash = now < s_flash_until_us;
        v.shape = &sv;
        if (sv.flash) { // the tap's own key + label under the shape while it flashes
            v.flash = true;
            v.action_key = KEY[s_flash_slot];
            v.action = p->slot[s_flash_slot].label ? p->slot[s_flash_slot].label : "";
        }
    }
    return v;
}

// --- Command wheel (profiles with an APP_ACT_COMMANDS slot, e.g. Figma) ---
// app_mode.c owns the wheel; this side only animates it: the card carousel slides between
// entries (and rings), each card loops its keyframes from when it was chosen, and a command
// that ran echoes on the Main Screen for ECHO_MS.
#define WHEEL_SLIDE_MS 120
#define ECHO_MS 1100
static bool s_wheel_was_open = false;
static int s_wheel_ring = -1, s_wheel_entry = -1;
static int64_t s_wheel_entry_us = 0, s_wheel_slide_us = -(1LL << 40);
static int s_wheel_slide_dir = 1;
static const app_scene_t *s_wheel_prev = nullptr;
static const app_cmd_t *s_wheel_prev_cmd = nullptr; // ...and its command (typed / chord cards)
static bool s_wheel_prev_valid = false;
static uint32_t s_echo_count = 0;
static bool s_echo_have = false;
static int64_t s_echo_start_us = -(1LL << 40);
static int s_echo_ring = 0, s_echo_entry = 0;

static const app_cmd_t *wheel_cmd(const app_profile_t *p, int ring, int entry) {
    if (ring < 0 || ring >= p->ring_count || entry < 1 || entry > p->rings[ring].count) return nullptr;
    return &p->rings[ring].cmds[entry - 1];
}

// Every tick: follow app_mode's wheel. Returns true while the wheel or an echo needs frames.
static bool wheel_tick(int64_t now, bool *changed) {
    const app_profile_t *p = app_profiles_get(menu_get_app_profile());
    int ring, entry;
    bool open = menu_get_hid_type() == MENU_HID_APP && app_mode_wheel(&ring, &entry);
    *changed = open != s_wheel_was_open;
    if (open) {
        if (!s_wheel_was_open) {
            s_wheel_entry_us = now;
            s_wheel_prev_valid = false;
        } else if (ring != s_wheel_ring || entry != s_wheel_entry) {
            const app_cmd_t *old = wheel_cmd(p, s_wheel_ring, s_wheel_entry);
            s_wheel_prev = old ? old->scene : nullptr;
            s_wheel_prev_cmd = old;
            s_wheel_prev_valid = true;
            s_wheel_slide_dir = (ring != s_wheel_ring) ? (ring > s_wheel_ring ? 1 : -1) : (entry > s_wheel_entry ? 1 : -1);
            s_wheel_slide_us = now;
            s_wheel_entry_us = now;
            *changed = true;
        }
        s_wheel_ring = ring;
        s_wheel_entry = entry;
    }
    s_wheel_was_open = open;

    int er, ee;
    uint32_t runs = app_mode_last_run(&er, &ee);
    if (!s_echo_have) {
        s_echo_count = runs;
        s_echo_have = true;
    } else if (runs != s_echo_count) {
        s_echo_count = runs;
        s_echo_ring = er;
        s_echo_entry = ee;
        s_echo_start_us = now;
        *changed = true;
    }
    bool echo = now - s_echo_start_us < ECHO_MS * 1000LL;
    static bool s_echo_was = false;
    if (echo != s_echo_was) *changed = true;
    s_echo_was = echo;
    return open || echo;
}

// A key's name on its keycap ("G", "NUM1", "TAB").
static void key_label(uint8_t k, char *out, size_t n) {
    if (k >= HID_KEY_A && k <= HID_KEY_Z) snprintf(out, n, "%c", 'A' + k - HID_KEY_A);
    else if (k >= HID_KEY_1 && k <= HID_KEY_9) snprintf(out, n, "%c", '1' + k - HID_KEY_1);
    else if (k == HID_KEY_0) snprintf(out, n, "0");
    else if (k >= HID_KEY_KEYPAD_1 && k <= HID_KEY_KEYPAD_9) snprintf(out, n, "NUM%d", k - HID_KEY_KEYPAD_1 + 1);
    else if (k == HID_KEY_TAB) snprintf(out, n, "TAB");
    else if (k == HID_KEY_SLASH) snprintf(out, n, "/");
    else if (k == HID_KEY_PERIOD) snprintf(out, n, ".");
    else if (k == HID_KEY_SPACE) snprintf(out, n, "SPACE");
    else if (k == HID_KEY_ENTER) snprintf(out, n, "ENTER");
    else if (k == HID_KEY_BACKSPACE) snprintf(out, n, "BKSP");
    else if (k == HID_KEY_ESCAPE) snprintf(out, n, "ESC");
    else snprintf(out, n, "?");
}

// How the wheel draws a command that has no scene of its own: a macro that types text as a
// little terminal typing it; a shortcut, or a macro that starts with keys, as its chord on big
// keycaps (a key repeated, like Esc Esc, shows once per press). `key` gets the key's label.
struct CardOf {
    const char *typed;
    bool chord;
    uint8_t mod, repeat;
};
static CardOf card_of(const app_profile_t *p, const app_cmd_t *c, char *key, size_t n) {
    CardOf k = {nullptr, false, 0, 1};
    if (c->kind == APP_CMD_MACRO && c->macro != 0 && c->macro <= p->macro_count) {
        const app_macro_t *m = &p->macros[c->macro - 1];
        if (m->count > 0 && m->steps[0].kind == APP_MSTEP_TEXT) {
            k.typed = m->steps[0].text;
        } else if (m->count > 0 && m->steps[0].kind == APP_MSTEP_KEY) {
            const app_key_t first = m->steps[0].key;
            k.chord = true;
            k.mod = first.modifier;
            key_label(first.keycode, key, n);
            while (k.repeat < m->count && k.repeat < 3 && m->steps[k.repeat].kind == APP_MSTEP_KEY
                   && m->steps[k.repeat].key.keycode == first.keycode && m->steps[k.repeat].key.modifier == first.modifier) {
                k.repeat++;
            }
        }
    } else if (c->kind == APP_CMD_KEYS && c->scene == nullptr) {
        k.chord = true;
        k.mod = c->key.modifier;
        key_label(c->key.keycode, key, n);
    }
    return k;
}

static ui::WheelView wheel_view(int64_t now) {
    static char key[8];
    const app_profile_t *p = app_profiles_get(menu_get_app_profile());
    ui::WheelView v = {};
    const app_ring_t &r = p->rings[s_wheel_ring];
    v.ring_name = r.name;
    v.ring_count = p->ring_count < 8 ? p->ring_count : 8;
    for (int i = 0; i < v.ring_count; i++) v.ring_tabs[i] = p->rings[i].tab;
    v.ring = s_wheel_ring;
    v.count = r.count + 1;
    v.entry = s_wheel_entry;
    const app_cmd_t *c = wheel_cmd(p, s_wheel_ring, s_wheel_entry);
    v.name = c ? c->name : "CANCEL";
    v.scene = c ? c->scene : nullptr;
    if (c) {
        // ACTIONS show the profile's search key (Figma Cmd K, Plasticity F) + SEARCH.
        const app_key_t &k = c->kind == APP_CMD_ACTIONS ? p->search.open : c->key;
        v.search = c->kind == APP_CMD_ACTIONS;
        v.modifier = k.modifier;
        key_label(k.keycode, key, sizeof(key));
        v.key = key;
        CardOf co = card_of(p, c, key, sizeof(key));
        v.typed = co.typed;
        v.chord = co.chord;
        v.chord_repeat = co.repeat;
        if (co.chord) v.modifier = co.mod;
    }
    // The card sliding out, drawn the same way it was drawn in.
    static char prev_key[12];
    const app_cmd_t *pc = s_wheel_prev_valid ? s_wheel_prev_cmd : nullptr;
    if (pc != nullptr) {
        CardOf co = card_of(p, pc, prev_key, sizeof(prev_key));
        v.prev_typed = co.typed;
        v.prev_chord = co.chord;
        v.prev_modifier = co.mod;
        v.prev_key = prev_key;
        v.prev_chord_repeat = co.repeat;
    }
    v.prev = s_wheel_prev;
    v.prev_valid = s_wheel_prev_valid;
    float k = (now - s_wheel_slide_us) / (WHEEL_SLIDE_MS * 1000.0f);
    v.slide = k >= 1 ? 1 : 1 - (1 - k) * (1 - k) * (1 - k);
    v.slide_dir = s_wheel_slide_dir;
    v.t_ms = (uint32_t)((now - s_wheel_entry_us) / 1000);
    return v;
}

// Parameter mode (app_mode.c): the value dial after e.g. FILLET runs from the wheel.
#define PARAM_NUDGE_MS 90
static bool param_view(int64_t now, ui::ParamView *v) {
    if (menu_get_hid_type() != MENU_HID_APP) return false;
    app_param_state_t s;
    if (!app_mode_param(&s)) return false;
    const app_profile_t *p = app_profiles_get(menu_get_app_profile());
    const app_cmd_t *c = wheel_cmd(p, s.ring, s.entry);
    if (c == nullptr || c->param == nullptr) return false;
    const app_param_t *pr = c->param;
    static uint32_t s_bump_seen = 0;
    static int64_t s_bump_at = -(1LL << 40);
    if (s.bump != s_bump_seen) {
        s_bump_seen = s.bump;
        s_bump_at = now;
    }
    *v = {};
    v->name = c->name;
    v->label = (s.value < 0 && pr->label_neg) ? pr->label_neg : pr->label;
    v->value = s.value;
    v->decimals = pr->decimals;
    v->degrees = pr->flags & APP_PARAM_DEG;
    for (int i = 0; i < 3; i++) v->steps[i] = pr->steps[i];
    v->step = s.step;
    v->visual = pr->visual;
    v->modes = pr->modes;
    v->axes = pr->flags & APP_PARAM_AXES;
    v->axis = s.axis;
    v->axis_bits = s.uniform ? 7 : s.plane ? (uint8_t)(7 & ~(1 << s.axis)) : (uint8_t)(1 << s.axis);
    v->f3 = s.f3_ms ? (s.f3_ms - 1) / 600.0f : -1.0f;
    v->nudge = now - s_bump_at < PARAM_NUDGE_MS * 1000LL ? 3 : 0;
    v->field = s.field;
    v->typed = s.typed;
    v->drawn = s.value;
    if (s.field && !s.typed) { // A: only the change is known
        v->value = s.value - pr->start;
        v->label = pr->label;
    }
    return true;
}

static float ease_out3(float k) {
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    return 1 - (1 - k) * (1 - k) * (1 - k);
}

static void draw_view(View v, const menu_render_snapshot_t &snap, int64_t now) {
    bool blink_on = ((now / 1000) % 900) < 600;
    switch (v) {
        case V_BOOT: {
            int64_t e = (now - s_boot_start_us) / 1000;
            ui::fx_boot((uint32_t)(e < ui::BOOT_ANIM_MS ? e : ui::BOOT_ANIM_MS));
            break;
        }
        case V_MAIN: {
            bool app = menu_get_hid_type() == MENU_HID_APP;
            // MUSIC with something playing: the cover takes the whole screen.
            media_track_t trk;
            if (profile_is("music") && media_get_track(&trk)) {
                if (s_cover_ok) s_cover.pushSprite(&s_frame, 0, 0);
                float vk = 1.0f - (now - s_np_turned_us) / (NP_VOLUME_MS * 1000.0f);
                float gk = 1.0f - (now - s_np_key_us) / (NP_GLYPH_MS * 1000.0f);
                ui::draw_now_playing({trk.title, trk.artist, s_cover_ok, app_profiles_get(menu_get_app_profile())->icon48,
                                      trk.palette[0] ? trk.palette[0] : ui::AMBER, trk.playing, trk.volume,
                                      vk > 0 ? (vk > 0.25f ? 1.0f : vk * 4) : 0.0f, s_np_glyph, gk > 0 ? gk : 0.0f});
                break;
            }
            ui::AppView av = app ? app_view(app_mode_live_slot(), now) : ui::AppView{};
            // AGENTS at rest: the dashboard of running agents takes the middle.
            bool board = app && profile_is("agents") && app_mode_live_slot() == APP_SLOT_KNOB && !s_wheel_was_open;
            if (board) av.action = av.action_via = "";
            ui::WheelView wv;
            bool wheel = app && s_wheel_was_open;
            if (wheel) wv = wheel_view(now);
            ui::ParamView pv;
            bool param = app && param_view(now, &pv);
            if (app && now - s_echo_start_us < ECHO_MS * 1000LL) {
                const app_cmd_t *c = wheel_cmd(app_profiles_get(menu_get_app_profile()), s_echo_ring, s_echo_entry);
                if (c) {
                    av.echo = true;
                    av.echo_scene = c->scene;
                    av.echo_name = c->name;
                    av.echo_ms = (uint32_t)((now - s_echo_start_us) / 1000);
                }
            }
            ui::MainInputs in = {
                ui_state_get_usb_serial_active(), menu_get_haptic_sound(), menu_get_hid_type(),
                menu_get_haptic_type(), // the feel line isn't shown in APP mode
                ui_state_get_buttons(), s_icon_set ? s_icon : nullptr, app ? &av : nullptr,
                wheel ? &wv : nullptr, param ? &pv : nullptr,
            };
            ui::draw_main(in);
            if (board && !wheel && !param && !av.echo) {
                agent_row_t rows[AGENT_BOARD_MAX];
                ui::AgentRowView views[AGENT_BOARD_MAX];
                int n = agent_board_get(rows, AGENT_BOARD_MAX);
                for (int i = 0; i < n; i++) {
                    notify_item_t tmp = {};
                    tmp.source = rows[i].source;
                    views[i] = {notify_color(&tmp), rows[i].name, rows[i].state};
                }
                ui::draw_agent_board(views, n, (uint32_t)(now / 1000));
            }
            break;
        }
        case V_ROOT:
        case V_DEVICE:
            ui::draw_menu_list(snap, list_scroll_now(now));
            break;
        case V_HAPTIC: {
            int64_t m = now - s_morph_start_us;
            bool morphing = m < FEEL_MORPH_MS * 1000LL;
            ui::OrbitInputs in = {
                menu_get_haptic_type(), (uint32_t)(now / 1000),
                morphing ? s_morph_from : -1, morphing ? ease_out3(m / (FEEL_MORPH_MS * 1000.0f)) : 1.0f, blink_on,
                menu_haptic_edit_profile() == HAPTIC_PROFILE_SMOOTH ? 0 : (int)HAPTIC_PROFILES[menu_haptic_edit_profile()].detents,
            };
            ui::draw_orbit(snap, in);
            break;
        }
        case V_HID: {
            float k = ease_out3((now - s_slide_start_us) / (HID_SLIDE_MS * 1000.0f));
            const app_profile_t *p = app_profiles_get(menu_get_app_profile());
            ui::HidInputs in = {menu_get_hid_type(), (1 - k) * s_slide_dir * HID_SLIDE_PX, blink_on, p->name, p->icon24};
            ui::draw_hid(snap, in);
            break;
        }
        case V_APP_PROFILE: {
            // The registry's profiles as the screen sees them, fetched every frame: the companion
            // app can add, change or remove one while this screen is up (a replaced profile's
            // memory lives on for a few seconds, far longer than a frame).
            static ui::ProfileItem items[APP_PROFILES_MAX];
            int count = app_profiles_count();
            for (int i = 0; i < count; i++) {
                const app_profile_t *p = app_profiles_get(i);
                items[i] = {p->name, p->icon24, p->icon48, {p->legend[0], p->legend[1], p->legend[2], p->legend[3]}};
            }
            int index = menu_get_app_profile();
            if (index >= count) index = count - 1;
            float k = ease_out3((now - s_profile_slide_start_us) / (HID_SLIDE_MS * 1000.0f));
            ui::ProfileInputs in = {items, count, index, (1 - k) * s_profile_slide_dir * PROFILE_SLIDE_PX, blink_on};
            ui::draw_app_profile(snap, in);
            break;
        }
        case V_BOOTMODE:
            ui::draw_boot_mode(snap, menu_get_boot_mode(), ui_state_get_usb_serial_active(), blink_on);
            break;
        case V_DISPLAY:
            ui::draw_display(snap, (int)menu_get_display_rotation(), blink_on);
            break;
        case V_SYSINFO: {
            sysmon_info_t info;
            sysmon_get(&info);
            ui::draw_sysinfo(snap, info, pd_status_get());
            break;
        }
        case V_RECAL:
            ui::draw_recalibrate(snap);
            break;
        case V_BINDINGS:
            ui::draw_bindings(snap, menu_get_host(), blink_on);
            break;
        case V_LIGHTS: {
            static uint8_t leds[LED_VIEW_COUNT][3];
            led_task_snapshot(leds);
            ui::draw_lights(snap, {lights_swatch(), leds, blink_on});
            break;
        }
        case V_NOTIFY: {
            const notify_item_t &n = s_notice;
            uint32_t queue[NOTIFY_MAX];
            int waiting = notify_colors(queue, NOTIFY_MAX);
            if (waiting < 1) { // the iris closing on an item that's already been answered
                queue[0] = notify_color(&n);
                waiting = 1;
            }
            ui::draw_notify({n.source < NOTIFY_SRC_OTHER ? (int)n.source : ui::AGENT_OTHER, notify_source_name(n.source),
                             notify_color(&n), n.title, n.body, n.kind == NOTIFY_ASK, queue, waiting, s_notice_hold,
                             ui_state_get_buttons(), (uint32_t)(now / 1000)});
            break;
        }
        case V_ATTRACT: {
            // APP mode: the active profile's icon (and colours) instead of the QUADRA wordmark.
            const uint8_t *icon = nullptr;
            const uint32_t *heat = nullptr;
            if (menu_get_hid_type() == MENU_HID_APP) {
                const app_profile_t *p = app_profiles_get(menu_get_app_profile());
                icon = p->icon48;
                if (p->plasma_heat[0] | p->plasma_heat[1] | p->plasma_heat[2]) heat = p->plasma_heat;
            }
            ui::fx_attract((uint32_t)((now - s_attract_start_us) / 1000), icon, heat, s_attract_seed);
            break;
        }
    }
    if (is_settings_view(v) && now < s_toast_until_us) {
        ui::draw_saved_toast(s_toast_factory ? "FACTORY" : "SAVED!");
    }
}

// Runs one tick. Returns how soon the next one should come.
enum Pace { PACE_IDLE, PACE_LOOP, PACE_FAST };

static Pace update_ui(void) {
    int64_t now = esp_timer_get_time();
    menu_render_snapshot_t snap;
    menu_get_render_snapshot(&snap);
    ui::set_saw_shape(menu_get_haptic_shape());
    uint8_t buttons = ui_state_get_buttons();
    int32_t detent = ui_state_get_detent();
    haptic_type_t feel = menu_get_haptic_type();
    menu_hid_type_t hid = menu_get_hid_type();

    // Screen rotation (DISPLAY setting): applied to the panel, so every view -- the sprite is
    // square and always drawn upright -- turns with it. Live while the setting is adjusted.
    static int s_rotation = -1;
    int rotation = (int)menu_get_display_rotation();
    bool rotation_changed = rotation != s_rotation;
    if (rotation_changed) {
        s_lcd.setRotation(rotation & 3);
        s_rotation = rotation;
    }

    bool first = !s_have_last;
    if (first) {
        s_last_snap = snap;
        s_last_save_count = snap.save_count;
        s_last_feel = feel;
        s_last_hid = hid;
        s_last_profile = menu_get_app_profile();
        s_last_buttons = buttons;
        s_last_detent = detent;
        s_last_activity_us = now;
        s_have_last = true;
    }

    bool icon_changed = icon_store_version() != s_icon_version;
    if (icon_changed) {
        s_icon_version = icon_store_version();
        s_icon_set = icon_store_copy(s_icon, sizeof(s_icon));
    }

    // The user's idle word (user_prefs.h), set from the host at any time.
    static uint32_t s_text_version = 0;
    bool text_changed = user_text_version() != s_text_version;
    if (text_changed) {
        s_text_version = user_text_version();
        char word[USER_TEXT_MAX + 1];
        user_text_get(word, sizeof(word));
        ui::fx_set_word(word);
    }
    // An agent notification (notify.h) takes the Main Screen's place and keeps the screen awake.
    static uint32_t s_notice_version = 0;
    notify_item_t ni;
    int nwait;
    float nhold;
    bool notice = notify_peek(&ni, &nwait, &nhold);
    bool notice_changed = notify_version() != s_notice_version || (notice ? nhold : 0) != s_notice_hold;
    s_notice_version = notify_version();
    if (notice) {
        s_notice = ni;
        s_notice_waiting = nwait;
    }
    s_notice_hold = notice ? nhold : 0;

    // MUSIC's now-playing (media.h) and AGENTS' dashboard (agent_board.h), both from the host.
    cover_tick();
    static uint32_t s_media_seen = 0, s_board_seen = 0;
    bool media_changed = media_version() != s_media_seen, board_changed = agent_board_version() != s_board_seen;
    s_media_seen = media_version();
    s_board_seen = agent_board_version();
    media_track_t trk;
    bool music_on = profile_is("music") && media_get_track(&trk);
    bool music_playing = music_on && trk.playing;
    if (music_on && !snap.open) {
        if (detent != s_last_detent) s_np_turned_us = now; // volume ring
        uint8_t pressed = buttons & ~s_last_buttons;
        if (pressed & (UI_BTN_F1 | UI_BTN_F2 | UI_BTN_F3)) {
            s_np_key_us = now;
            s_np_glyph = (pressed & UI_BTN_F1) ? (trk.playing ? ui::NP_GLYPH_PAUSE : ui::NP_GLYPH_PLAY)
                       : (pressed & UI_BTN_F2) ? ui::NP_GLYPH_PREV : ui::NP_GLYPH_NEXT;
        }
    }
    bool np_overlay = music_on && (now - s_np_turned_us < NP_VOLUME_MS * 1000LL || now - s_np_key_us < NP_GLYPH_MS * 1000LL);
    static bool s_np_overlay_was = false;
    bool np_overlay_ended = s_np_overlay_was && !np_overlay; // one more frame, or its last faint one stays up
    s_np_overlay_was = np_overlay;
    bool board_live = false; // a row is animating (WORKING dots, ASKING blink)
    if (profile_is("agents")) {
        agent_row_t rows[AGENT_BOARD_MAX];
        int n = agent_board_get(rows, AGENT_BOARD_MAX);
        for (int i = 0; i < n; i++) board_live |= rows[i].state == AGENT_WORKING || rows[i].state == AGENT_ASKING;
    }

    bool snapshot_changed = memcmp(&snap, &s_last_snap, sizeof(snap)) != 0;
    bool buttons_changed = buttons != s_last_buttons;
    // A new icon counts as activity so an upload wakes the screen and shows it; so does a new
    // track. While music plays, its cover stays up instead of the idle animation.
    bool activity = snapshot_changed || buttons_changed || detent != s_last_detent || icon_changed || notice_changed
                 || (music_on && media_changed);
    if (activity || notice || music_playing) s_last_activity_us = now;

    if (snap.save_count != s_last_save_count) {
        s_last_save_count = snap.save_count;
        s_toast_until_us = now + TOAST_MS * 1000LL;
        s_toast_factory = false;
    }
    if (snap.reset_count != s_last_reset_count) { // F2 held on Haptics: the profile is back to factory
        s_last_reset_count = snap.reset_count;
        s_toast_until_us = now + TOAST_MS * 1000LL;
        s_toast_factory = true;
    }
    if (feel != s_last_feel) {
        s_morph_from = s_last_feel;
        s_morph_start_us = now;
        s_last_feel = feel;
    }
    if (hid != s_last_hid) {
        int step = (menu_hid_type_pos(hid) - menu_hid_type_pos(s_last_hid) + MENU_HID_TYPE_COUNT) % MENU_HID_TYPE_COUNT;
        s_slide_dir = (step == 1) ? 1 : -1; // by display order (APP first)
        s_slide_start_us = now;
        s_last_hid = hid;
    }
    int32_t profile = menu_get_app_profile();
    if (profile != s_last_profile) {
        s_profile_slide_dir = profile > s_last_profile ? 1 : -1;
        s_profile_slide_start_us = now;
        s_last_profile = profile;
    }
    if (is_list_screen(snap.screen) && (snapshot_changed || first || s_list_prev_screen != snap.screen)) {
        list_retarget(snap, now);
    }
    s_list_prev_screen = snap.screen;

    // --- which view should be up ---
    if (s_booting && now - s_boot_start_us >= ui::BOOT_ANIM_MS * 1000LL) {
        s_booting = false;
        s_last_activity_us = now; // idle timer starts once the Main Screen is actually up
    }
    View target;
    if (s_booting) {
        target = V_BOOT;
    } else {
        target = view_for(snap);
        if (notice && target == V_MAIN) target = V_NOTIFY;
        if (target != V_MAIN) {
            s_attract_on = false;
        } else if (s_attract_on) {
            if (activity) s_attract_on = false;
        } else if (now - s_last_activity_us >= ATTRACT_IDLE_MS * 1000LL) {
            s_attract_on = true;
            s_attract_start_us = now;
            s_attract_seed = esp_random();
        }
        if (s_attract_on) target = V_ATTRACT;
    }
    ui_state_set_screensaver(s_attract_on);

    // APP mode's live slot changes a beat after the raw buttons (it's debounced) -- redraw
    // when it does, or the middle would show the previous action.
    static int s_last_app_slot = -1;
    int app_slot = app_mode_live_slot();
    bool app_slot_changed = app_slot != s_last_app_slot;
    s_last_app_slot = app_slot;
    bool shape_live = shape_tick(now, buttons);
    bool wheel_changed = false;
    bool wheel_live = wheel_tick(now, &wheel_changed);
    app_param_state_t pstate;
    bool param_live = menu_get_hid_type() == MENU_HID_APP && app_mode_param(&pstate);
    static bool s_param_was = false;
    if (param_live != s_param_was) wheel_changed = true;
    s_param_was = param_live;
    if (wheel_live || param_live) s_last_activity_us = now; // no screensaver over the wheel, an echo or a value dial
    // SYS INFO follows sysmon's twice-a-second refresh (and the USB power read, which finishes a
    // moment after boot).
    static pd_status_t s_last_power = {};
    static uint32_t s_last_sysmon = 0;
    pd_status_t power = pd_status_get();
    sysmon_info_t sys;
    sysmon_get(&sys);
    bool sys_changed = memcmp(&power, &s_last_power, sizeof(power)) != 0 || sys.version != s_last_sysmon;
    s_last_power = power;
    s_last_sysmon = sys.version;
    bool redraw = first || snapshot_changed || buttons_changed || icon_changed || app_slot_changed || wheel_changed
               || rotation_changed || (sys_changed && s_view == V_SYSINFO) || text_changed || notice_changed
               || media_changed || board_changed || np_overlay_ended;
    if (target != s_view) {
        s_iris_from = s_view;
        s_iris_from_snap = s_last_snap;
        s_iris_start_us = now;
        s_iris_active = true;
        s_view = target;
        redraw = true;
    }

    // --- does this tick need a frame? ---
    float iris_p = 1.0f;
    if (s_iris_active) {
        iris_p = (now - s_iris_start_us) / (IRIS_MS * 1000.0f);
        if (iris_p >= 1.0f) s_iris_active = false; // this frame shows the finished view
        redraw = true;
    }
    bool fast = s_iris_active
             || ((s_view == V_ROOT || s_view == V_DEVICE) && s_list_anim)
             || (s_view == V_HAPTIC && now - s_morph_start_us < FEEL_MORPH_MS * 1000LL)
             || (s_view == V_HID && now - s_slide_start_us < HID_SLIDE_MS * 1000LL)
             || (s_view == V_APP_PROFILE && now - s_profile_slide_start_us < HID_SLIDE_MS * 1000LL)
             || (s_view == V_MAIN && shape_live)
             || (s_view == V_MAIN && now - s_wheel_slide_us < WHEEL_SLIDE_MS * 1000LL);
    bool looping = s_booting || s_view == V_ATTRACT || s_view == V_NOTIFY // the notification breathes
                || s_view == V_LIGHTS // the rim mirrors the animated LED ring
                || (s_view == V_MAIN && (np_overlay || board_live)) // volume ring / key glyph, board dots
                || (s_view == V_MAIN && (wheel_live || param_live)) // card animations, value dial
                || (s_view == V_HAPTIC
                    && (snap.selected == MENU_HAPTIC_ROW_FEEL || snap.selected == MENU_HAPTIC_ROW_SHAPE
                        || snap.selected == MENU_HAPTIC_ROW_STEPS));
    bool blink_on = ((now / 1000) % 900) < 600;
    bool blink_edge = is_settings_view(s_view) && snap.dirty && blink_on != s_drawn_blink;
    bool toast_on = is_settings_view(s_view) && now < s_toast_until_us;
    redraw = redraw || fast || looping || blink_edge || toast_on != s_drawn_toast;

    if (redraw) {
        s_frame.fillScreen(ui::BLACK);
        if (s_iris_active) {
            bool closing = iris_p < 0.5f;
            draw_view(closing ? s_iris_from : s_view, closing ? s_iris_from_snap : snap, now);
            ui::iris_mask(closing ? 126.0f * (1 - 2 * iris_p) : 126.0f * (2 * iris_p - 1));
        } else {
            draw_view(s_view, snap, now);
        }
        s_frame.pushSprite(0, 0);
        s_drawn_blink = blink_on;
        s_drawn_toast = toast_on;
    }

    s_last_snap = snap;
    s_last_buttons = buttons;
    s_last_detent = detent;
    return fast ? PACE_FAST : looping ? PACE_LOOP : PACE_IDLE;
}

static void display_task_fn(void *arg) {
    ESP_LOGI(TAG, "display task started on core %d, prio %d", xPortGetCoreID(), (int)uxTaskPriorityGet(NULL));

    backlight_init();

    if (!s_lcd.init()) {
        ESP_LOGE(TAG, "LovyanGFX init failed -- display will stay dark");
        vTaskDelete(NULL);
        return;
    }
    s_lcd.fillScreen(TFT_BLACK);
    if (!frame_init()) {
        ESP_LOGE(TAG, "frame sprite allocation failed -- display will stay dark");
        vTaskDelete(NULL);
        return;
    }
    ui::bind(&s_frame);
    ui::fx_init();
    char word[USER_TEXT_MAX + 1];
    user_text_get(word, sizeof(word));
    ui::fx_set_word(word); // the loading screen already assembles the user's word

    s_boot_start_us = esp_timer_get_time();
    update_ui(); // first (black) frame of the loading screen before the backlight comes up
    backlight_set_percent(LCD_BACKLIGHT_DUTY_PERCENT);
    ESP_LOGI(TAG, "display init done");

    while (1) {
        switch (update_ui()) {
            case PACE_FAST:
                // Short transition in flight: next frame right away. The 100Hz tick makes
                // even vTaskDelay(1) cost up to 10ms/frame; a yield still round-robins with
                // the same-priority I2S task, and lower-priority tasks wait at most one
                // transition (IRIS_MS).
                taskYIELD();
                break;
            case PACE_LOOP:
                vTaskDelay(pdMS_TO_TICKS(UI_ANIM_DELAY_MS));
                break;
            default:
                vTaskDelay(pdMS_TO_TICKS(UI_REDRAW_PERIOD_MS));
                break;
        }
    }
}

void display_task_start(void) {
    // 6KB (was 4KB): the Pixel UI draw path is deeper (screen -> toolkit -> text lambdas) and
    // the last measured high-water mark at 4KB was ~2.2KB free before it existed.
    // 8KB: + the now-playing cover's JPEG decode (its work pool is on the heap, its state isn't).
    xTaskCreatePinnedToCore(display_task_fn, "display", 8192, NULL, PRIO_DISPLAY, NULL, CORE_IO);
}
