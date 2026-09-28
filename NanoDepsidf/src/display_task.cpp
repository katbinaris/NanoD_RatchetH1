#include "display_task.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "lgfx_config.hpp"
#include "ui_gfx.hpp"
#include "ui_fx.hpp"
#include "ui_screens.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include <string.h>

// Plain-C model layer -- linkage fixed here at the include site so menu.h/ui_state.h
// themselves stay untouched.
extern "C" {
#include "menu.h"
#include "ui_state.h"
#include "icon_store.h"
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

enum View : uint8_t { V_BOOT, V_MAIN, V_ROOT, V_HAPTIC, V_HID, V_BOOTMODE, V_ATTRACT };

static View view_for(const menu_render_snapshot_t &s) {
    switch (s.screen) {
        case MENU_SCREEN_ROOT: return V_ROOT;
        case MENU_SCREEN_HAPTIC: return V_HAPTIC;
        case MENU_SCREEN_HID: return V_HID;
        case MENU_SCREEN_BOOT: return V_BOOTMODE;
        default: return V_MAIN;
    }
}

static inline bool is_settings_view(View v) { return v == V_HAPTIC || v == V_HID || v == V_BOOTMODE; }

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
static int64_t s_last_activity_us = 0;

static menu_render_snapshot_t s_last_snap;
static bool s_have_last = false;
static uint8_t s_last_buttons = 0;
static int32_t s_last_detent = 0;

static uint32_t s_last_save_count = 0;
static int64_t s_toast_until_us = 0;

static haptic_type_t s_last_feel = HAPTIC_TYPE_SAW;
static int s_morph_from = -1;
static int64_t s_morph_start_us = -(1LL << 40);

static menu_hid_type_t s_last_hid = MENU_HID_MOUSE;
static int s_slide_dir = 0;
static int64_t s_slide_start_us = -(1LL << 40);

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
    if (s_list_prev_screen != MENU_SCREEN_ROOT) {
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
            ui::MainInputs in = {
                ui_state_get_usb_serial_active(), menu_get_haptic_sound(), menu_get_hid_type(),
                // APP mode always runs VISCOSE (control_task.c), so that's what the feel line says.
                menu_get_hid_type() == MENU_HID_APP ? HAPTIC_TYPE_VISCOSE : menu_get_haptic_type(),
                ui_state_get_buttons(), s_icon_set ? s_icon : nullptr,
            };
            ui::draw_main(in);
            break;
        }
        case V_ROOT:
            ui::draw_menu_list(snap, list_scroll_now(now));
            break;
        case V_HAPTIC: {
            int64_t m = now - s_morph_start_us;
            bool morphing = m < FEEL_MORPH_MS * 1000LL;
            ui::OrbitInputs in = {
                menu_get_haptic_type(), (uint32_t)(now / 1000),
                morphing ? s_morph_from : -1, morphing ? ease_out3(m / (FEEL_MORPH_MS * 1000.0f)) : 1.0f, blink_on,
            };
            ui::draw_orbit(snap, in);
            break;
        }
        case V_HID: {
            float k = ease_out3((now - s_slide_start_us) / (HID_SLIDE_MS * 1000.0f));
            ui::HidInputs in = {menu_get_hid_type(), (1 - k) * s_slide_dir * HID_SLIDE_PX, blink_on};
            ui::draw_hid(snap, in);
            break;
        }
        case V_BOOTMODE:
            ui::draw_boot_mode(snap, menu_get_boot_mode(), ui_state_get_usb_serial_active(), blink_on);
            break;
        case V_ATTRACT:
            ui::fx_attract((uint32_t)((now - s_attract_start_us) / 1000));
            break;
    }
    if (is_settings_view(v) && now < s_toast_until_us) {
        ui::draw_saved_toast();
    }
}

// Runs one tick. Returns how soon the next one should come.
enum Pace { PACE_IDLE, PACE_LOOP, PACE_FAST };

static Pace update_ui(void) {
    int64_t now = esp_timer_get_time();
    menu_render_snapshot_t snap;
    menu_get_render_snapshot(&snap);
    uint8_t buttons = ui_state_get_buttons();
    int32_t detent = ui_state_get_detent();
    haptic_type_t feel = menu_get_haptic_type();
    menu_hid_type_t hid = menu_get_hid_type();

    bool first = !s_have_last;
    if (first) {
        s_last_snap = snap;
        s_last_save_count = snap.save_count;
        s_last_feel = feel;
        s_last_hid = hid;
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

    bool snapshot_changed = memcmp(&snap, &s_last_snap, sizeof(snap)) != 0;
    bool buttons_changed = buttons != s_last_buttons;
    // A new icon counts as activity so an upload wakes the screen and shows it.
    bool activity = snapshot_changed || buttons_changed || detent != s_last_detent || icon_changed;
    if (activity) s_last_activity_us = now;

    // TEMPORARY DIAGNOSTIC (DEVELOPMENT_PLAN.md Phase 8) -- see menu.h's
    // menu_get_last_input_us() comment.
    if (snapshot_changed) {
        int64_t input_us = menu_get_last_input_us();
        if (input_us > 0) {
            ESP_LOGI(TAG, "menu render latency: %lld ms", (long long)((now - input_us) / 1000));
        }
    }

    if (snap.save_count != s_last_save_count) {
        s_last_save_count = snap.save_count;
        s_toast_until_us = now + TOAST_MS * 1000LL;
    }
    if (feel != s_last_feel) {
        s_morph_from = s_last_feel;
        s_morph_start_us = now;
        s_last_feel = feel;
    }
    if (hid != s_last_hid) {
        s_slide_dir = (((int)hid - (int)s_last_hid + MENU_HID_TYPE_COUNT) % MENU_HID_TYPE_COUNT == 1) ? 1 : -1;
        s_slide_start_us = now;
        s_last_hid = hid;
    }
    if (snap.screen == MENU_SCREEN_ROOT && (snapshot_changed || first || s_list_prev_screen != MENU_SCREEN_ROOT)) {
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
        if (target != V_MAIN) {
            s_attract_on = false;
        } else if (s_attract_on) {
            if (activity) s_attract_on = false;
        } else if (now - s_last_activity_us >= ATTRACT_IDLE_MS * 1000LL) {
            s_attract_on = true;
            s_attract_start_us = now;
        }
        if (s_attract_on) target = V_ATTRACT;
    }
    ui_state_set_screensaver(s_attract_on);

    bool redraw = first || snapshot_changed || buttons_changed || icon_changed;
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
             || (s_view == V_ROOT && s_list_anim)
             || (s_view == V_HAPTIC && now - s_morph_start_us < FEEL_MORPH_MS * 1000LL)
             || (s_view == V_HID && now - s_slide_start_us < HID_SLIDE_MS * 1000LL);
    bool looping = s_booting || s_view == V_ATTRACT
                || (s_view == V_HAPTIC && snap.selected == MENU_HAPTIC_ROW_FEEL);
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
    xTaskCreatePinnedToCore(display_task_fn, "display", 6144, NULL, PRIO_DISPLAY, NULL, CORE_IO);
}
