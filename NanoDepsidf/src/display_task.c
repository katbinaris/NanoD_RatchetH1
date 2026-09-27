#include "display_task.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui_state.h"
#include "menu.h"
#include "fonts/ui_font_silkscreen.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "display";

// --- Phase 4: Display -- first bring-up slice ---
// Panel + backlight + esp_lvgl_port init, then one static label as a smoke test -- same
// "prove the primitive works" pattern every other phase in this plan used first (Phase 1's
// dummy HID message, Phase 3's HID self-test, Phase 7's BLIP click) before designing real
// screen content. The actual screen content decision (port legacy_fw's SquareLine screens,
// which are profile/MIDI-oriented, vs. a new minimal status UI for this keyboard/mouse-
// focused device) is explicitly NOT made here -- see DEVELOPMENT_PLAN.md Phase 4's "UI
// scope decision", still pending.
//
// Own SPI bus (SPI3_HOST), deliberately separate from MT6701's SPI2_HOST (mt6701.c) --
// two independent peripherals, no sharing/contention between sensor reads and display
// updates.
//
// Buffer choice: a partial (not full-frame) internal-RAM buffer, matching legacy's own
// "1/10-screen partial buffer" approach mentioned in the architecture log, rather than the
// full-frame PSRAM buffer alternative also mentioned there -- simpler and lower-risk for a
// first bring-up (no PSRAM/DMA cache-coherency subtleties to get right blind); revisit if
// partial-buffer refresh rate turns out to be a real limitation once real screen content
// exists.

#define LCD_SPI_HOST SPI3_HOST
#define LCD_LEDC_TIMER LEDC_TIMER_0
#define LCD_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_LEDC_FREQ_HZ 5000
#define LCD_BACKLIGHT_DUTY_PERCENT 80 // starting point, not tuned against ambient light yet

// Rows per partial LVGL draw buffer. Was 40 (1/6 screen) double-buffered; reverted to 24
// (1/10 screen, 240*240/10) single-buffered by request, to directly replicate legacy_fw's
// exact known-smooth configuration (`legacy_fw/src/lcd_thread.cpp`, LVGL8/TFT_eSPI) as a
// test -- the priority-equalization and roller-animation-duration fixes (DEVELOPMENT_PLAN.md
// Phase 8) didn't resolve the reported laggy animation on hardware, so this isolates buffer
// configuration as its own variable rather than assuming the double-buffer switch (done
// earlier to fix a different, already-confirmed bug: blocky redraw artifacts) was correct
// for this symptom too. 24 rows x 240 cols x 2 bytes (RGB565) = 11520 bytes/buffer.
#define LCD_DRAW_BUFFER_ROWS 24

// First-hardware-test findings: background rendered dark purple instead of the expected
// light theme background, with visible artifacts, and content appeared mirrored. Both are
// hardware-orientation/format facts about this specific panel, not code bugs -- same
// category as MOTOR_POLE_PAIRS/HID_WHEEL_SIGN elsewhere in this project, empirically
// determined per-board, not derivable from the datasheet alone. Best first guesses below,
// not yet confirmed on hardware:
//   - LCD_SWAP_BYTES: `esp_lcd_panel_io_spi` sends RGB565 as ESP32 stores it (little-
//     endian in memory); most SPI TFT controllers, GC9A01 included, expect the 16-bit
//     color word big-endian (high byte first) on the wire. Left false, a symmetric color
//     (pure white/black, where both bytes are equal) renders correctly by coincidence,
//     but anything else -- including LVGL9's default light-theme background, which isn't
//     symmetric -- comes out as the wrong hue. This is the leading suspect for the
//     dark-purple/artifact symptom.
//   - LCD_MIRROR_X: the most common fix for a GC9A01 module appearing horizontally
//     mirrored -- this specific panel's FPC mounting orientation on the NanoFOC_D board
//     vs. the vendor component's default MADCTL setting.
// If swap_bytes alone doesn't clear up the artifacts, LCD_SPI_FREQUENCY_HZ
// (board_pins.h, currently 80MHz) is worth trying lower next -- bit errors from a marginal
// SPI clock over this board's actual trace length would look like exactly this kind of
// color speckling too.
// LCD_SWAP_BYTES itself (the underlying hardware fact -- this panel wants RGB565
// big-endian on the wire, ESP32 stores it little-endian) is unchanged and still real. WHERE
// the swap happens changed: see the .color_format/.flags.swap_bytes comment below -- moved
// from a separate lv_draw_sw_rgb565_swap() post-process pass over every flush's dirty area
// to LVGL's native RGB565_SWAPPED render format, found while investigating the laggy-roller
// performance issue (DEVELOPMENT_PLAN.md Phase 8). This constant is kept only as the
// documented hardware fact/history; it no longer feeds `.flags.swap_bytes` directly.
#define LCD_SWAP_BYTES true
#define LCD_MIRROR_X true
#define LCD_MIRROR_Y false
#define LCD_SWAP_XY false

// Confirmed on hardware: black background, white text, correct orientation -- the guesses
// above were right on the first try.
//
// **That confirmation was at LV_DISPLAY_ROTATION_0 (no sw_rotate active) -- doesn't hold
// once rotation is layered on top.** With LCD_ROTATION=270 (see below), overall
// placement/quadrant is correct (270 vs. 90 was the right call, left alone), but glyph
// content still read mirrored on X -- and flipping this LCD_MIRROR_X define made NO
// visible difference either way, which is itself the actual clue.
//
// Traced into `esp_lvgl_port`'s source (esp_lvgl_port_disp.c,
// lvgl_port_disp_rotation_update()): with flags.sw_rotate=true (below), that function
// returns immediately, at every call site (init AND every later rotation change) --
// `esp_lcd_panel_mirror()`/`swap_xy()` are the ONLY things that ever send MADCTL to the
// panel, and in sw_rotate mode they are simply never called, full stop. LCD_MIRROR_X/_Y
// here are consequently dead configuration in this mode -- not "wrong value", but wired to
// nothing -- which is exactly consistent with toggling it doing nothing on hardware. The
// panel has been sitting at its raw, uncompensated default orientation (mirrored on X, per
// this board's FPC mount, same fact the original smoke test discovered) the whole time
// LVGL's software rotation was layered on top of it.
//
// Fix: send the mirror compensation directly via `esp_lcd_panel_mirror()` ourselves right
// after panel init (see panel_and_lvgl_init() below), bypassing esp_lvgl_port's rotation
// plumbing entirely for this -- restores the needed hardware compensation as a one-time,
// independent call, without touching LCD_ROTATION or the sw_rotate mechanism at all.

// Requested on-top-of-that rotation: 90 degrees CCW. Layered as LVGL *software* rotation
// (flags.sw_rotate + lv_display_set_rotation() below) rather than folding it into the
// swap_xy/mirror_x/mirror_y hardware combination above -- those three booleans only cover
// 8 combinations and re-deriving which one adds exactly "90 CCW" on top of the
// already-confirmed-correct base orientation would be exactly the same kind of
// per-board guesswork that took two tries to get right above. Software rotation is
// decoupled from that -- it rotates LVGL's own rendering, independent of the hardware
// orientation already proven correct.
//
// Direction picked by tracing LVGL's own coordinate transform
// (lv_display_rotate_point() in lv_display.c), not assumed from the enum name:
// LV_DISPLAY_ROTATION_90 maps content (x=0,y=0) -> physical (ver_res-1, 0) and content
// (x=0, y=ver_res-1) -> physical (0, 0), i.e. the content's TOP edge ends up on the
// physical RIGHT edge and its LEFT edge ends up on the physical TOP edge -- that is a
// 90-degree CLOCKWISE rotation of the displayed content. First guess (270, for CCW) was
// backwards per direct hardware feedback -- actual requirement is 90 CW, so this is just
// LV_DISPLAY_ROTATION_90 directly.
//
// **Revised after hardware testing showed the result mirrored, not just rotated.** Traced
// to esp_lvgl_port itself: with flags.sw_rotate=true (used here),
// lvgl_port_disp_rotation_update() (esp_lvgl_port_disp.c) returns immediately for every
// rotation angle without ever touching MADCTL -- so at the time this was first tried, the
// LCD_MIRROR_X hardware compensation above was never actually being sent at all (see that
// #define's comment -- fixed separately, by a direct esp_lcd_panel_mirror() call in
// panel_and_lvgl_init() below). This rotation angle was picked against that then-broken,
// effectively-unmirrored baseline, so it doesn't carry over now that the real mirror
// compensation is actually active -- one genuine hardware reflection changes which sw
// rotation angle reads as "90 CW" (reflection+rotation flips handedness: with the real
// mirror now applied, 270 came out upside-down, i.e. exactly the 180-degree-opposite
// mistake from picking the angle for the wrong -- unmirrored -- baseline). Reverted to the
// original LV_DISPLAY_ROTATION_90, correct given the mirror fix, UNDER SW_ROTATE.
//
// **Switched to HARDWARE rotation (sw_rotate=false below), by request, to eliminate the
// lv_draw_sw_rotate() per-flush software pixel-transpose found while investigating the
// laggy-roller performance issue (DEVELOPMENT_PLAN.md Phase 8) -- a real, previously-
// unaccounted-for CPU cost on every single frame.** This changes the actual mechanism:
// lvgl_port_disp_rotation_update() is no longer a no-op (that early-return was specifically
// gated on sw_rotate) -- it now COMPOSES the LCD_MIRROR_X/Y/LCD_SWAP_XY base above with
// whatever LV_DISPLAY_ROTATION_* is requested, via its own dihedral-group math, and sends
// the RESULT to MADCTL directly -- a fundamentally different composition than the software
// path's "rotate the pixel buffer, hardware orientation stays fixed" model. The manual
// esp_lcd_panel_mirror()/swap_xy() calls this file used to make directly (see that
// #define's comment) are REMOVED below -- lvgl_port_disp_rotation_update() owns this now,
// and calling both would just have the second one silently overwrite the first.
//
// Consequence: LV_DISPLAY_ROTATION_90 being correct under sw_rotate does NOT mean it's
// still correct here -- it's a different formula operating on the same base config, and
// this is genuinely untested. Left at _90 as the starting guess (simplest, smallest diff)
// but treat this exactly like every other per-board orientation fact in this project
// (LCD_MIRROR_X included) -- confirm empirically on hardware, flip to _270 first if wrong,
// same "one variable at a time" pattern used throughout.
#define LCD_ROTATION LV_DISPLAY_ROTATION_90

// Hardware feedback: background rendered white where black was expected -- a clean
// inversion (0x0000 <-> 0xFFFF), not garbled colors. This can't be a swap_bytes issue
// (LCD_SWAP_BYTES above) -- byte-swapping is a no-op on both 0x0000 and 0xFFFF, since
// both bytes are already equal in each. A clean full-color invert is what
// `esp_lcd_panel_invert_color()` (sends the panel controller's INVON/INVOFF command) is
// specifically for -- a real, common GC9A01 hardware fact (this panel's polarity wiring),
// independent of anything on the software/LVGL side.
#define LCD_INVERT_COLOR true

// Phase 8: real config menu (menu.c) replaces the old Phase 4 mock (three static labels).
// Two mutually-exclusive views on one screen, switched by visibility flag rather than by
// swapping LVGL screens (simpler for just two views): Main Screen (live detent readout +
// a button-legend cheat sheet) when the menu is closed, a data-driven scrollable row list
// when it's open. This file only ever reads menu.c's render snapshot -- it owns no menu
// state itself, mirroring how it already only reads ui_state.h for the detent value.
// Lowered from an original 150ms once real menu navigation made that period feel sluggish
// -- 150ms was fine for a passive number readout but is a very noticeable input-to-screen
// lag for something you're actively turning/pressing through. Safe to drop this far now
// that update_ui() below diffs against the last-rendered state and skips all LVGL work on
// a tick where nothing changed -- the extra poll frequency is nearly free, not extra redraw
// cost.
#define UI_REDRAW_PERIOD_MS 30

// Menu rows render as a fixed set of plain lv_labels plus one small sliding "highlight"
// rect, replacing an lv_roller. Reason for the switch (DEVELOPMENT_PLAN.md Phase 8): sysmon
// (LVGL's own perf monitor) measured a near-constant ~33-65ms "render" cost on every redraw
// no matter what else was tuned (mask on/off, rounded/square corners) -- reading lv_roller.c
// directly showed why: it does FOUR full text-shape/draw passes over the whole option list
// on every single redraw, unconditionally, in both NORMAL and INFINITE modes (an "above
// selection" clip, a "below selection" clip, a full re-measure of the selected line's size,
// and a full redraw of it) -- baked into the widget itself, no style/Kconfig escape hatch.
// This design instead animates only one small rect per frame; the row labels' text is only
// touched when content actually changes (already gated by update_ui()'s snapshot diff).
//
// This also leans on a real fact about this app's content, not a general-purpose virtualized
// list: MENU_MAX_VISIBLE_ITEMS is 8, but no screen in menu.c actually has more than 6 items
// (Haptic Configurator), and MENU_LIST_VISIBLE_ROWS below is 5 -- so 3 of 4 screens never
// need to scroll at all, and the one screen that does only overflows by exactly one row. The
// common case (moving the highlight among already-visible rows) gets real animation; the
// rare one-row-overflow case gets an instant window-shift rather than a general recycled-
// scroll implementation, which would be a lot of machinery for a case that barely occurs.
// Side benefit: window recompute is always instant regardless of direction, which also
// removes the old roller's noted wraparound quirk (NORMAL mode's "real scroll back through
// the list" on wrap, logged earlier in this phase).
#define MENU_LIST_VISIBLE_ROWS 5 // unchanged from the roller's visible-row count -- still
                                  // what fits this round panel's safe area at this font size
#define MENU_LIST_WIDTH 200 // unchanged from the roller's width
#define MENU_LIST_ROW_PITCH_PX 32 // vertical distance between adjacent row slots -- Silkscreen
                                   // 16px's line_height (18) + the extra inter-row spacing
                                   // (14) already tuned in by request last round, carried
                                   // over so row density looks the same as before
#define MENU_LIST_ROW_HEIGHT_PX 28 // highlight rect height -- a little under the pitch so it
                                    // reads as a pill with breathing room between rows,
                                    // matching the cheat-tag pill styling used elsewhere
#define MENU_LIST_ANIM_MS 120 // carried over from the roller's already-tuned value (raised
                               // from 60 once the real esp_lvgl_port task_max_sleep_ms bug,
                               // not this widget, was found to be the actual "laggy" cause)
#define MENU_LIST_ROW_RADIUS 8 // rounded corners were a real, measured render-time cost on
                                // the roller (forces LVGL's anti-aliased "complex" draw path,
                                // redone every redraw as part of its 4x-per-redraw text
                                // shaping). Here the highlight is one small standalone rect,
                                // repositioned only on an actual selection change rather than
                                // every redraw -- expected to be cheap again, but genuinely
                                // unverified until checked with sysmon post-implementation.
                                // Flip to 0 if it turns out to matter.
#define MENU_SELECTED_BG_COLOR 0xFFC94D // same amber used in the UI preview mockup
#define MENU_UNSELECTED_TEXT_COLOR 0xFFFFFF
#define MENU_LIST_DIM_TEXT_OPA 100 // ~40% -- top/bottom "more items" hint: dim the edge-most
                                    // visible row instead of the canvas/bitmap-mask fade this
                                    // replaces (that mask was a real, measured render cost for
                                    // a case that, per the comment above, now only ever
                                    // affects one screen by one row)
#define MENU_CHEAT_TEXT_COLOR 0xCFCFCF
#define MENU_CHEAT_BG_OPA 20   // ~8%, matches the mockup's rgba(255,255,255,0.08) tag fill
#define MENU_CHEAT_BORDER_OPA 46 // ~18%, matches the mockup's rgba(255,255,255,0.18) tag border

static lv_obj_t *s_detent_label = NULL;
static lv_obj_t *s_cheat_labels[3];
static lv_obj_t *s_menu_rows[MENU_LIST_VISIBLE_ROWS];
static lv_obj_t *s_menu_highlight = NULL;
static int s_highlight_slot = -1; // last slot the highlight was moved to/toward -- lets
                                   // update_ui() skip restarting an already-running or
                                   // already-completed move to the same target

static void backlight_init(void) {
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LCD_LEDC_TIMER,
        .freq_hz = LCD_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .gpio_num = PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CHANNEL,
        .timer_sel = LCD_LEDC_TIMER,
        .duty = 0, // ramped up explicitly below, after panel init -- avoids a visible
                   // flash of uninitialized framebuffer content before the first real
                   // frame is drawn
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

static void backlight_set_percent(uint32_t percent) {
    uint32_t duty = (255 * percent) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CHANNEL);
}

static lv_display_t *panel_and_lvgl_init(void) {
    spi_bus_config_t bus_cfg = GC9A01_PANEL_BUS_SPI_CONFIG(PIN_LCD_SCLK, PIN_LCD_MOSI,
                                                            LCD_WIDTH * LCD_DRAW_BUFFER_ROWS * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = GC9A01_PANEL_IO_SPI_CONFIG(PIN_LCD_CS, PIN_LCD_DC, NULL, NULL);
    io_cfg.pclk_hz = LCD_SPI_FREQUENCY_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    // Hardware feedback: a genuinely asymmetric color (amber, high-red/low-blue) rendered
    // as blue -- a clean R/B channel swap. Same bug *class* as the earlier LCD_SWAP_BYTES
    // mirror issue (Phase 4) -- black/white are invariant under this swap too, so the
    // original smoke test (which only ever showed black/white/amber-as-text-color, never a
    // filled asymmetric-color background) never actually exercised this setting either.
    // Both esp_lcd_gc9a01's own test app and esp_lvgl_port's example use BGR for this exact
    // panel family -- RGB was the wrong first guess.
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
        .vendor_config = NULL, // default GC9A01 init sequence
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_cfg, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, LCD_INVERT_COLOR));
    // No manual esp_lcd_panel_mirror()/swap_xy() call here anymore -- now that
    // flags.sw_rotate=false (below), esp_lvgl_port's own lvgl_port_disp_rotation_update()
    // is alive again and owns sending MADCTL, composing the .rotation struct field below
    // with the requested LCD_ROTATION angle itself. Calling both would just have this one
    // get silently overwritten the moment lv_display_set_rotation() runs. See LCD_ROTATION's
    // comment above for the full mechanism change.
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = PRIO_DISPLAY,
        .task_stack = 7168,
        .task_affinity = CORE_IO,
        // Was 500 -- this is the actual root cause of the "laggy/freezes-then-jumps" menu
        // animation (see the lvgl_port_task_wake() comment in update_ui() for the full
        // mechanism and hardware-measured proof). The explicit wake call is the real fix;
        // this is lowered too as a defense-in-depth bound on worst-case latency in case a
        // future code path changes LVGL objects without remembering to call it.
        .task_max_sleep_ms = 50,
        .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
        .timer_period_ms = 5,
    };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_WIDTH * LCD_DRAW_BUFFER_ROWS,
        // Reverted to false (was true): by request, testing legacy_fw's exact known-smooth
        // configuration -- single buffer, 1/10 screen rows -- since the priority/animation
        // fixes above didn't resolve the reported laggy roller animation on hardware. That
        // alone didn't fix it either (confirmed on hardware), so re-enabling double-buffer
        // on top of the 24-row size instead of matching legacy exactly -- isolates buffer
        // *count* from buffer *size* as two separate variables rather than only ever
        // testing them bundled together.
        .double_buffer = true,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .monochrome = false,
        // RGB565_SWAPPED (not plain RGB565): LVGL's software renderer produces already
        // byte-swapped pixels natively as part of normal drawing (confirmed available --
        // CONFIG_LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=y in this build) instead of the port
        // running a separate lv_draw_sw_rgb565_swap() pass over the whole dirty area on
        // every single flush (see .flags.swap_bytes below) -- found while investigating the
        // laggy-roller performance issue (DEVELOPMENT_PLAN.md Phase 8). The hardware fact
        // this addresses (panel wants big-endian RGB565 on the wire) is unchanged; only
        // where the swap happens does.
        .color_format = LV_COLOR_FORMAT_RGB565_SWAPPED,
        .rotation = {
            // Alive again now that sw_rotate=false below -- this is the base orientation
            // lvgl_port_disp_rotation_update() composes with LCD_ROTATION at runtime.
            .swap_xy = LCD_SWAP_XY,
            .mirror_x = LCD_MIRROR_X,
            .mirror_y = LCD_MIRROR_Y,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = false, // was LCD_SWAP_BYTES (true) -- no longer needed now that
                                  // .color_format above is RGB565_SWAPPED; this flag would
                                  // just add a second, redundant swap pass on top of the
                                  // native one (double-swapping back to the wrong order)
            .sw_rotate = false, // see LCD_ROTATION comment above -- switched from true to
                                 // eliminate the per-flush lv_draw_sw_rotate() software
                                 // pixel-transpose cost
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (disp != NULL && lvgl_port_lock(0)) {
        lv_display_set_rotation(disp, LCD_ROTATION);
        lvgl_port_unlock();
    }
    return disp;
}

// Builds both views once at init; visibility is toggled per-redraw rather than
// creating/destroying objects each time the menu opens/closes.
static void build_ui(lv_display_t *disp) {
    lv_obj_t *screen = lv_display_get_screen_active(disp); // LVGL9 name -- the older
                                                            // lv_disp_get_scr_act() doesn't
                                                            // exist in this version
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);

    s_detent_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_detent_label, lv_color_white(), LV_PART_MAIN);
    // Silkscreen here too, by request ("all fonts ... even main screen") -- was the theme
    // default (lv_font_montserrat_14, no explicit font set at all) until now.
    lv_obj_set_style_text_font(s_detent_label, &ui_font_silkscreen_16_regular, LV_PART_MAIN);
    lv_label_set_text(s_detent_label, "Detent: 0");
    lv_obj_align(s_detent_label, LV_ALIGN_CENTER, 0, -10);

    // Compact button-legend cheat sheet, Main Screen only -- F4/F3/F1 per the fixed menu
    // button roles (DEVELOPMENT_PLAN.md Phase 8 / Architecture decisions log). Small font
    // (Montserrat 12, sdkconfig.defaults) so three tags plus the detent readout above both
    // stay inside the round panel's circular safe area. Styled as a subtle pill (faint
    // white fill + thin border, matching the UI preview mockup's tag treatment) rather than
    // bare text, so it reads as a distinct "legend" rather than competing with the readout.
    static const char *cheat_text[3] = { "F4:Menu", "F3:Back", "F1:Select" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *tag = lv_label_create(screen);
        lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(tag, lv_color_hex(MENU_CHEAT_TEXT_COLOR), LV_PART_MAIN);
        lv_obj_set_style_bg_color(tag, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(tag, MENU_CHEAT_BG_OPA, LV_PART_MAIN);
        lv_obj_set_style_border_color(tag, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_border_opa(tag, MENU_CHEAT_BORDER_OPA, LV_PART_MAIN);
        lv_obj_set_style_border_width(tag, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(tag, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(tag, 7, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(tag, 3, LV_PART_MAIN);
        lv_label_set_text(tag, cheat_text[i]);
        lv_obj_align(tag, LV_ALIGN_CENTER, (i - 1) * 72, 24);
        s_cheat_labels[i] = tag;
    }

    // Menu list -- fixed row slots + one sliding highlight rect (see the MENU_LIST_* comment
    // above for why this replaced an lv_roller). Highlight created first so it paints behind
    // the row labels (z-order = creation order here, no explicit move-to-background needed).
    lv_obj_t *highlight = lv_obj_create(screen);
    lv_obj_remove_style_all(highlight); // drop the theme's default border/padding/scrollbar --
                                         // this is just a colored rect, not an interactive obj
    lv_obj_set_size(highlight, MENU_LIST_WIDTH, MENU_LIST_ROW_HEIGHT_PX);
    lv_obj_set_style_bg_color(highlight, lv_color_hex(MENU_SELECTED_BG_COLOR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(highlight, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(highlight, MENU_LIST_ROW_RADIUS, LV_PART_MAIN);
    lv_obj_align(highlight, LV_ALIGN_CENTER, 0, 0); // real position set on first menu-open in
                                                     // update_ui() -- hidden until then anyway
    lv_obj_set_hidden(highlight, true);
    s_menu_highlight = highlight;

    for (int i = 0; i < MENU_LIST_VISIBLE_ROWS; i++) {
        lv_obj_t *row = lv_label_create(screen);
        lv_obj_set_width(row, MENU_LIST_WIDTH);
        // Silkscreen, in place of the default-theme Montserrat -- a pixel-art bitmap font
        // (ui_font_silkscreen.h), 16px/1bpp monochrome. Regular everywhere, including the
        // selected row (an earlier pass used a bold weight there; LVGL ships no bold variant
        // of anything built-in, so that needed a real second font asset -- reverted by
        // request in favor of all-regular throughout).
        lv_obj_set_style_text_font(row, &ui_font_silkscreen_16_regular, LV_PART_MAIN);
        lv_obj_set_style_text_align(row, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_color(row, lv_color_hex(MENU_UNSELECTED_TEXT_COLOR), LV_PART_MAIN);
        lv_obj_align(row, LV_ALIGN_CENTER, 0, (i - MENU_LIST_VISIBLE_ROWS / 2) * MENU_LIST_ROW_PITCH_PX);
        lv_obj_set_hidden(row, true);
        s_menu_rows[i] = row;
    }
}

// Last-rendered state, compared against on each call so an unchanged tick can skip all
// LVGL work entirely -- this is what makes UI_REDRAW_PERIOD_MS=30 above cheap rather than
// 5x the redraw cost of the original 150ms: between an actual button press/detent crossing,
// menu.c's snapshot is byte-for-byte identical call to call, so most ticks now do a memcmp
// and return, not a full re-layout of every visible label.
static menu_render_snapshot_t s_last_snapshot;
static bool s_last_snapshot_valid = false;
static int32_t s_last_detent = INT32_MIN;

// Called periodically from the task loop (see UI_REDRAW_PERIOD_MS) -- reads the latest
// cross-core UI state (ui_state.h) and menu snapshot (menu.c), and only touches LVGL
// objects when something actually changed since the last call.
static void update_ui(void) {
    menu_render_snapshot_t snap;
    menu_get_render_snapshot(&snap);
    int32_t detent = ui_state_get_detent();

    bool snapshot_changed = !s_last_snapshot_valid || memcmp(&snap, &s_last_snapshot, sizeof(snap)) != 0;
    bool detent_changed = (detent != s_last_detent);
    // Closed->open or open->closed also needs a redraw even if, say, the detent value
    // happens to be unchanged -- covered by snapshot_changed already (snap.open flips).
    if (!snapshot_changed && !(detent_changed && !snap.open)) {
        return; // nothing a viewer would see has changed -- skip the lock and all LVGL calls
    }

    // TEMPORARY DIAGNOSTIC (DEVELOPMENT_PLAN.md Phase 8) -- see menu.h's
    // menu_get_last_input_us() comment. Only fires on an actual detected menu change, on
    // Core 1, which has no real-time deadline -- safe, unlike logging from control_task.c's
    // loop would be.
    if (snapshot_changed) {
        int64_t input_us = menu_get_last_input_us();
        if (input_us > 0) {
            ESP_LOGI(TAG, "menu render latency: %lld ms", (long long)((esp_timer_get_time() - input_us) / 1000));
        }
    }

    if (!lvgl_port_lock(0)) {
        return;
    }

    s_last_snapshot = snap;
    s_last_snapshot_valid = true;
    s_last_detent = detent;

    if (snap.open) {
        lv_obj_set_hidden(s_detent_label, true);
        for (int i = 0; i < 3; i++) {
            lv_obj_set_hidden(s_cheat_labels[i], true);
        }

        // Find the selected row's rank, then compute which window of MENU_LIST_VISIBLE_ROWS
        // rows to show, centered on it and clamped so the window never runs past either end
        // of the list. See the MENU_LIST_* comment above build_ui() for why a fixed window
        // like this is enough (no screen in menu.c overflows by more than one row).
        int selected = 0;
        for (int i = 0; i < snap.row_count; i++) {
            if (snap.rows[i].selected) {
                selected = i;
                break;
            }
        }
        int window_start = selected - MENU_LIST_VISIBLE_ROWS / 2;
        int max_start = snap.row_count - MENU_LIST_VISIBLE_ROWS;
        if (max_start < 0) {
            max_start = 0;
        }
        if (window_start < 0) {
            window_start = 0;
        } else if (window_start > max_start) {
            window_start = max_start;
        }
        int selected_slot = selected - window_start;
        bool more_above = (window_start > 0);
        bool more_below = (window_start + MENU_LIST_VISIBLE_ROWS < snap.row_count);

        for (int i = 0; i < MENU_LIST_VISIBLE_ROWS; i++) {
            int idx = window_start + i;
            lv_obj_t *row = s_menu_rows[i];
            if (idx >= snap.row_count) {
                lv_obj_set_hidden(row, true);
                continue;
            }
            lv_obj_set_hidden(row, false);
            const menu_render_row_t *r = &snap.rows[idx];
            if (r->value[0] != '\0') {
                lv_label_set_text_fmt(row, "%s  %s", r->label, r->value);
            } else {
                lv_label_set_text(row, r->label);
            }
            lv_obj_set_style_text_color(row,
                (i == selected_slot) ? lv_color_black() : lv_color_hex(MENU_UNSELECTED_TEXT_COLOR),
                LV_PART_MAIN);
            // Top/bottom "more items" hint -- dim the edge-most visible row instead of the
            // canvas/bitmap-mask fade the roller version used (see MENU_LIST_DIM_TEXT_OPA).
            lv_opa_t opa = LV_OPA_COVER;
            if ((i == 0 && more_above) || (i == MENU_LIST_VISIBLE_ROWS - 1 && more_below)) {
                opa = MENU_LIST_DIM_TEXT_OPA;
            }
            lv_obj_set_style_text_opa(row, opa, LV_PART_MAIN);
        }

        lv_obj_set_hidden(s_menu_highlight, false);
        int32_t target_y = (selected_slot - MENU_LIST_VISIBLE_ROWS / 2) * MENU_LIST_ROW_PITCH_PX;
        if (selected_slot != s_highlight_slot) {
            if (s_highlight_slot < 0) {
                // Menu just opened (or was fully closed) -- jump straight to the right slot
                // instead of animating in from an arbitrary leftover position.
                lv_obj_set_y(s_menu_highlight, target_y);
            } else {
                // First hand-written lv_anim in this codebase -- everything before this
                // animated via a widget's own built-in behavior (e.g. the roller's
                // LV_ANIM_ON). Animates only this one small rect's y position; the row
                // labels above are never touched per-frame, which is the whole point --
                // see the MENU_LIST_* comment above build_ui() for the cost this avoids.
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, s_menu_highlight);
                lv_anim_set_values(&a, lv_obj_get_y_aligned(s_menu_highlight), target_y);
                lv_anim_set_duration(&a, MENU_LIST_ANIM_MS);
                lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
                lv_anim_start(&a);
            }
            s_highlight_slot = selected_slot;
        }
    } else {
        for (int i = 0; i < MENU_LIST_VISIBLE_ROWS; i++) {
            lv_obj_set_hidden(s_menu_rows[i], true);
        }
        lv_obj_set_hidden(s_menu_highlight, true);
        s_highlight_slot = -1; // forces a fresh jump, not an animated slide, next time the
                                // menu opens rather than sliding in from a stale position
        lv_obj_set_hidden(s_detent_label, false);
        for (int i = 0; i < 3; i++) {
            lv_obj_set_hidden(s_cheat_labels[i], false);
        }
        lv_label_set_text_fmt(s_detent_label, "Detent: %" PRId32, detent);
    }

    lvgl_port_unlock();

    // FOUND THE REAL BUG (DEVELOPMENT_PLAN.md Phase 8): esp_lvgl_port runs its own
    // internal task ("taskLVGL", separate from this "display" task) that calls
    // lv_timer_handler() and then sleeps in xEventGroupWaitBits() for up to
    // task_max_sleep_ms (500ms, set in panel_and_lvgl_init() below) whenever nothing was
    // pending last time it checked. We call LVGL APIs (lv_label_set_text_fmt(),
    // lv_anim_start(), etc.) directly from THIS task, above -- but never told the port's own task that new work
    // exists, so it had no way to wake up early. It would only notice on its own next
    // 500ms timeout, then process everything at once -- measured on hardware as a ~549ms
    // gap before a flush, then several fast catch-up flushes. Confirmed by direct
    // measurement (a temporary flush-interval diagnostic in esp_lvgl_port_disp.c), not
    // guessed. This is almost certainly the real cause of the whole "laggy/freezes-then-
    // jumps" symptom -- everything else tried before this (CPU freq, compiler
    // optimization, buffer size/count, sw_rotate, byte-swap format, task priority,
    // animation duration) was a real fix for a real (if minor, by comparison) cost, but
    // none of them could have fixed a task that was simply asleep the whole time.
    // LVGL_PORT_EVENT_USER exists specifically for this: an external task notifying the
    // port's task that something changed, distinct from pretending to be a touch/encoder
    // event -- wakes it immediately instead of waiting for its own timeout.
    lvgl_port_task_wake(LVGL_PORT_EVENT_USER, NULL);
}

static void display_task_fn(void *arg) {
    ESP_LOGI(TAG, "display task started on core %d, prio %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));

    backlight_init();

    lv_display_t *disp = panel_and_lvgl_init();
    if (disp == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed -- display will stay dark");
    } else {
        if (lvgl_port_lock(0)) {
            build_ui(disp);
            lvgl_port_unlock();
        }
        backlight_set_percent(LCD_BACKLIGHT_DUTY_PERCENT);
        ESP_LOGI(TAG, "display init done");
    }

    while (1) {
        if (disp != NULL) {
            update_ui();
        }
        vTaskDelay(pdMS_TO_TICKS(UI_REDRAW_PERIOD_MS));
    }
}

void display_task_start(void) {
    xTaskCreatePinnedToCore(display_task_fn, "display", 4096, NULL, PRIO_DISPLAY, NULL, CORE_IO);
}
