#include "display_task.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "lgfx_config.hpp"
#include "fonts/ui_font_silkscreen.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// Plain-C model layer -- linkage fixed here at the include site so menu.h/ui_state.h
// themselves stay untouched.
extern "C" {
#include "menu.h"
#include "ui_state.h"
}

static const char *TAG = "display";

// LovyanGFX display task: Main Screen + animated menu list (migrated from LVGL -- see
// DEVELOPMENT_PLAN.md "Display stack migration"). Every layout constant below is carried over
// from the LVGL implementation (git history: src/display_task.c).
//
// Rendering model: one full-screen 240x240 RGB565 LGFX_Sprite in internal RAM. A frame is
// always drawn in full (fillScreen + everything) and sent with one pushSprite() -- no
// dirty-rect tracking. A frame is only produced when something a viewer would see changed
// (menu snapshot / detent diff, same as the LVGL version's update_ui() skip) or while a
// scroll animation is in flight. Measured on hardware (checkpoint 2): draw ~1.1ms, push
// ~11.8ms -- so while animating the loop only yields instead of sleeping (see
// display_task_fn()), otherwise a 60ms animation would get just 2 frames at the 30ms poll.
//
// Orientation/colors: lgfx_config.hpp (offset_rotation=3, BGR, invert) -- confirmed on
// hardware at checkpoint 1.

#define LCD_LEDC_TIMER LEDC_TIMER_0
#define LCD_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_LEDC_FREQ_HZ 5000
#define LCD_BACKLIGHT_DUTY_PERCENT 80 // starting point, not tuned against ambient light yet

#define UI_REDRAW_PERIOD_MS 30

#define SCREEN_CX (LCD_WIDTH / 2)
#define SCREEN_CY (LCD_HEIGHT / 2)

// Main Screen layout (LVGL: detent label LV_ALIGN_CENTER 0,-10; cheat tags LV_ALIGN_CENTER
// (i-1)*72, 24 with pad_hor 7 / pad_ver 3 / radius 6 / 1px border).
#define MAIN_DETENT_Y_OFS -10
#define CHEAT_X_SPACING 72
#define CHEAT_Y_OFS 24
#define CHEAT_PAD_HOR 7
#define CHEAT_PAD_VER 3
#define CHEAT_RADIUS 6

// Menu list -- text scrolls past one FIXED highlight (never the other way round). Every row
// idx sits at slot idx*MENU_LIST_ROW_PITCH_PX in an imaginary vertical list and is drawn at
// slot - scroll relative to the screen center; `scroll` is one animated value that centers
// the selected row (target = selected*PITCH -- at idx 0 that's 0, so item 0 centers with
// nothing above it, like a real picker). Values carried over from the LVGL version, all
// tuned on hardware there.
#define MENU_LIST_VISIBLE_ROWS 5   // what fits the round panel's safe area at this font size
#define MENU_LIST_WIDTH 200
#define MENU_LIST_ROW_PITCH_PX 32  // Silkscreen 16 line height (18) + 14 inter-row spacing
#define MENU_LIST_ROW_HEIGHT_PX 28 // highlight height, a little under the pitch
#define MENU_LIST_ANIM_MS 60       // tuned on hardware under LVGL
#define MENU_LIST_ROW_RADIUS 8
// Visible band = the LVGL version's clipping container (MENU_LIST_VISIBLE_ROWS rows tall,
// MENU_LIST_WIDTH wide, centered). A sprite has no implicit clip container, so this is
// applied explicitly with setClipRect() while drawing rows.
#define MENU_LIST_BAND_H (MENU_LIST_VISIBLE_ROWS * MENU_LIST_ROW_PITCH_PX)

// Colors as uint32_t RGB888 -- LovyanGFX reads a uint32_t color argument as RGB888, but a
// plain int literal as RGB565, so these must stay explicitly typed.
static constexpr uint32_t COLOR_BLACK = 0x000000u;
static constexpr uint32_t COLOR_WHITE = 0xFFFFFFu;
static constexpr uint32_t MENU_SELECTED_BG_COLOR = 0xFFC94Du; // amber, UI preview mockup
static constexpr uint32_t MENU_CHEAT_TEXT_COLOR = 0xCFCFCFu;

// LVGL drew the cheat-tag pills as translucent white (bg opa 20/255 ~8%, border opa 46/255
// ~18%). LovyanGFX has no alpha compositing against a live background, but the background
// here is always solid black, and white-over-black at opacity a is exactly grey level a --
// so these flat colors are the same pixels LVGL produced, not an approximation.
static constexpr uint32_t grey(uint8_t level) { return ((uint32_t)level << 16) | ((uint32_t)level << 8) | level; }
static constexpr uint32_t MENU_CHEAT_BG_COLOR = grey(20);
static constexpr uint32_t MENU_CHEAT_BORDER_COLOR = grey(46);
// Rows exactly MENU_LIST_VISIBLE_ROWS/2 ranks from the selected one: white text at LVGL opa
// 100/255 (~40%) over black -- same exact-flattening reasoning as the pills.
static constexpr uint32_t MENU_LIST_DIM_TEXT_COLOR = grey(100);

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

static void draw_main_screen(int32_t detent) {
    char buf[24];
    snprintf(buf, sizeof(buf), "Detent: %" PRId32, detent);
    s_frame.setFont(&ui_font_silkscreen_16_regular);
    s_frame.setTextDatum(textdatum_t::middle_center);
    s_frame.setTextColor(COLOR_WHITE);
    s_frame.drawString(buf, SCREEN_CX, SCREEN_CY + MAIN_DETENT_Y_OFS);

    // Button-legend cheat sheet (F4/F3/F1 per the fixed menu button roles).
    static const char *cheat_text[3] = { "F4:Menu", "F3:Back", "F1:Select" };
    s_frame.setFont(&ui_font_silkscreen_8_regular);
    const int32_t pill_h = s_frame.fontHeight() + 2 * CHEAT_PAD_VER;
    for (int i = 0; i < 3; i++) {
        const int32_t cx = SCREEN_CX + (i - 1) * CHEAT_X_SPACING;
        const int32_t cy = SCREEN_CY + CHEAT_Y_OFS;
        const int32_t pill_w = s_frame.textWidth(cheat_text[i]) + 2 * CHEAT_PAD_HOR;
        const int32_t x = cx - pill_w / 2;
        const int32_t y = cy - pill_h / 2;
        s_frame.fillRoundRect(x, y, pill_w, pill_h, CHEAT_RADIUS, MENU_CHEAT_BG_COLOR);
        s_frame.drawRoundRect(x, y, pill_w, pill_h, CHEAT_RADIUS, MENU_CHEAT_BORDER_COLOR);
        s_frame.setTextColor(MENU_CHEAT_TEXT_COLOR);
        s_frame.drawString(cheat_text[i], cx, cy);
    }
}

// Scroll animation state. Timing is absolute wall-clock interpolation, not a per-tick step:
// progress = (now - start) / duration, recomputed from esp_timer every frame, so frame rate
// and task scheduling jitter change only how many frames are shown, never the duration.
// Linear, matching lv_anim's default path the LVGL version used.
static int32_t s_scroll_from = 0;
static int32_t s_scroll_to = 0; // committed target -- selected*PITCH
static int64_t s_anim_start_us = 0;
static bool s_anim_active = false;
static int s_menu_selected = 0;
static bool s_menu_content_valid = false; // false until the menu has been opened -- forces
                                           // the first open to jump, not animate
static char s_last_menu_title[MENU_TITLE_LEN] = ""; // screen-change detector: menu.c gives
                                                      // every screen a distinct title

// Current scroll position; also retires the animation once it has run its full duration.
static int32_t menu_scroll_now(int64_t now_us) {
    if (!s_anim_active) {
        return s_scroll_to;
    }
    int64_t elapsed = now_us - s_anim_start_us;
    int64_t duration = (int64_t)MENU_LIST_ANIM_MS * 1000;
    if (elapsed >= duration) {
        s_anim_active = false;
        return s_scroll_to;
    }
    if (elapsed < 0) {
        elapsed = 0;
    }
    return s_scroll_from + (int32_t)(((int64_t)(s_scroll_to - s_scroll_from) * elapsed) / duration);
}

// Called only when the snapshot changed while the menu is open.
static void menu_list_retarget(const menu_render_snapshot_t *snap, int64_t now_us) {
    int selected = 0;
    for (int i = 0; i < snap->row_count; i++) {
        if (snap->rows[i].selected) {
            selected = i;
            break;
        }
    }
    s_menu_selected = selected;
    int32_t target = selected * MENU_LIST_ROW_PITCH_PX;

    bool screen_changed = !s_menu_content_valid || strcmp(snap->title, s_last_menu_title) != 0;
    snprintf(s_last_menu_title, sizeof(s_last_menu_title), "%s", snap->title);
    s_menu_content_valid = true;

    if (screen_changed) {
        // Unconditional jump -- deliberately NOT nested under a `target != current` check:
        // under LVGL exactly that guard made the first open (target 0 == stale 0) skip the
        // layout and pile every row on top of each other (DEVELOPMENT_PLAN.md Phase 8).
        s_scroll_to = target;
        s_anim_active = false;
    } else if (target != s_scroll_to) {
        // Starts from where the list currently IS (mid-animation included), not from the
        // previous target -- under LVGL a re-target mid-slide snapped to the old target first.
        s_scroll_from = menu_scroll_now(now_us);
        s_scroll_to = target;
        s_anim_start_us = now_us;
        s_anim_active = true;
    }
}

static void draw_menu_list(const menu_render_snapshot_t *snap, int32_t scroll) {
    // Fixed highlight first, so it sits behind the row text. Never moves.
    s_frame.fillRoundRect(SCREEN_CX - MENU_LIST_WIDTH / 2, SCREEN_CY - MENU_LIST_ROW_HEIGHT_PX / 2,
                          MENU_LIST_WIDTH, MENU_LIST_ROW_HEIGHT_PX, MENU_LIST_ROW_RADIUS,
                          MENU_SELECTED_BG_COLOR);

    const int32_t band_top = SCREEN_CY - MENU_LIST_BAND_H / 2;
    s_frame.setClipRect(SCREEN_CX - MENU_LIST_WIDTH / 2, band_top, MENU_LIST_WIDTH, MENU_LIST_BAND_H);
    s_frame.setFont(&ui_font_silkscreen_16_regular); // regular for every row, selected
                                                     // included -- bold-selected was tried
                                                     // twice and rejected on hardware
    s_frame.setTextDatum(textdatum_t::middle_center);
    const int32_t half_line = s_frame.fontHeight() / 2;

    // Fixed rank-distance dim: the two rows exactly VISIBLE_ROWS/2 from the selected one are
    // grey regardless of whether more content exists past that edge (the conditional
    // version was rejected on hardware) -> grey/white/black-on-amber/white/grey.
    const int top_idx = s_menu_selected - MENU_LIST_VISIBLE_ROWS / 2;
    const int bottom_idx = s_menu_selected + MENU_LIST_VISIBLE_ROWS / 2;

    char buf[MENU_LABEL_TEXT_LEN + MENU_VALUE_TEXT_LEN + 4];
    for (int i = 0; i < snap->row_count; i++) {
        const int32_t row_cy = SCREEN_CY + i * MENU_LIST_ROW_PITCH_PX - scroll;
        if (row_cy + half_line < band_top || row_cy - half_line >= band_top + MENU_LIST_BAND_H) {
            continue; // fully outside the visible band
        }
        const menu_render_row_t *r = &snap->rows[i];
        if (r->value[0] != '\0') {
            snprintf(buf, sizeof(buf), "%s  %s", r->label, r->value);
        } else {
            snprintf(buf, sizeof(buf), "%s", r->label);
        }
        uint32_t color = COLOR_WHITE;
        if (r->selected) {
            color = COLOR_BLACK;
        } else if (i == top_idx || i == bottom_idx) {
            color = MENU_LIST_DIM_TEXT_COLOR;
        }
        s_frame.setTextColor(color);
        s_frame.drawString(buf, SCREEN_CX, row_cy);
    }
    s_frame.clearClipRect();
}

// Last-rendered state -- an unchanged tick with no animation running skips drawing and
// pushing entirely.
static menu_render_snapshot_t s_last_snapshot;
static bool s_last_snapshot_valid = false;
static int32_t s_last_detent = INT32_MIN;

// Returns true while a scroll animation still needs more frames.
static bool update_ui(void) {
    menu_render_snapshot_t snap;
    menu_get_render_snapshot(&snap);
    int32_t detent = ui_state_get_detent();
    int64_t now_us = esp_timer_get_time();

    bool snapshot_changed = !s_last_snapshot_valid || memcmp(&snap, &s_last_snapshot, sizeof(snap)) != 0;
    bool detent_changed = (detent != s_last_detent);
    bool was_animating = s_anim_active;
    if (!snapshot_changed && !(detent_changed && !snap.open) && !was_animating) {
        return false;
    }

    // TEMPORARY DIAGNOSTIC (DEVELOPMENT_PLAN.md Phase 8) -- see menu.h's
    // menu_get_last_input_us() comment.
    if (snapshot_changed) {
        int64_t input_us = menu_get_last_input_us();
        if (input_us > 0) {
            ESP_LOGI(TAG, "menu render latency: %lld ms", (long long)((esp_timer_get_time() - input_us) / 1000));
        }
    }

    s_last_snapshot = snap;
    s_last_snapshot_valid = true;
    s_last_detent = detent;

    if (snap.open) {
        if (snapshot_changed) {
            menu_list_retarget(&snap, now_us);
        }
    } else {
        s_menu_content_valid = false; // next open jumps fresh instead of sliding from a
        s_anim_active = false;        // stale scroll position
    }
    int32_t scroll = menu_scroll_now(now_us);

    s_frame.fillScreen(COLOR_BLACK);
    if (snap.open) {
        draw_menu_list(&snap, scroll);
    } else {
        draw_main_screen(detent);
    }
    s_frame.pushSprite(0, 0);

    return s_anim_active;
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
    update_ui(); // first real frame before the backlight comes up
    backlight_set_percent(LCD_BACKLIGHT_DUTY_PERCENT);
    ESP_LOGI(TAG, "display init done");

    while (1) {
        if (update_ui()) {
            // Animation in flight: next frame right away. The 100Hz tick makes even
            // vTaskDelay(1) cost up to 10ms/frame; a yield still round-robins with the
            // same-priority I2S task, and lower-priority tasks wait at most one animation
            // (MENU_LIST_ANIM_MS).
            taskYIELD();
        } else {
            vTaskDelay(pdMS_TO_TICKS(UI_REDRAW_PERIOD_MS));
        }
    }
}

void display_task_start(void) {
    xTaskCreatePinnedToCore(display_task_fn, "display", 4096, NULL, PRIO_DISPLAY, NULL, CORE_IO);
}
