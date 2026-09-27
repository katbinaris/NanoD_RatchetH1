#include "i2s_task.h"
#include "tasks_common.h"
#include "board_pins.h"
#include "audio_trigger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "i2s";

// --- Phase 7: I2S audio (haptic click + startup chime) ---
// NOT a port of legacy_fw's Arduino WAV-playback code. Architecture modeled on a sibling
// project's implementation instead (Lucu-Kind/src/haptics.h+.cpp -- a different device,
// same MAX98357A I2S amp wiring as this board: DOUT/BCLK/LRC = 9/10/11, see board_pins.h).
// See DEVELOPMENT_PLAN.md Phase 7 for the full rationale. Key departures from that
// reference, adapted to ESP-IDF/this project rather than copied:
//   - `driver/i2s_std` (this IDF's current I2S API) instead of the legacy `driver/i2s.h`
//     the reference project uses.
//   - Click triggers arrive via audio_trigger.h's lock-free counter+queue (this project's
//     own module, same pattern as the reference's g_clickTriggerCount) instead of Arduino
//     globals.
//   - This is a first working slice: one click timbre (the reference's simplest "BLIP" --
//     fixed frequency, fixed fast decay, no rate/noise modulation) plus the startup chime.
//     Expanding to more timbres/an out-of-bounds tone is follow-on work, not done here.

#define AUDIO_SAMPLE_RATE_HZ 44100

// Samples per i2s_channel_write() call. Small on purpose (same reasoning as the reference
// project's DMA_BUF_LEN=16): the click-trigger check happens once per sample inside the
// chunk-fill loop below, so a smaller chunk mainly shortens how long the task can be
// blocked inside i2s_channel_write() (a "blind window" during which it can't be doing
// anything else) once the DMA queue is full in steady state, not how quickly a new click
// is *detected* (that's already per-sample). Retune on hardware if underruns show up.
#define AUDIO_CHUNK_LEN 64

// DMA descriptor count/frame size -- originally 8 descriptors (~11.6ms buffered), picked
// without over-tuning ahead of real hardware testing. That's exactly the hardware test this
// comment deferred: Phase 8 menu testing found clicks going weak/glitchy specifically while
// the menu is open, not in normal (single-label) display mode. Root cause: both the click
// and a menu redraw are triggered by the same detent-crossing event, and a menu redraw
// touches up to 4 styled/positioned labels (vs. one text update in normal mode) -- more
// Core 1 SPI/LVGL work landing at the exact instant the I2S task needs to keep feeding its
// DMA buffer on time. Doubled to 16 descriptors (~23.2ms buffered) to give the audio task
// enough slack to ride through that redraw burst without an audible gap -- still short
// enough that the added latency on a click is imperceptible.
#define AUDIO_DMA_DESC_NUM 16
#define AUDIO_DMA_FRAME_NUM AUDIO_CHUNK_LEN

// 256-entry sine table, computed once at startup rather than in the hot per-sample loop --
// same reasoning as the reference project: swaps a transcendental sinf() call for a
// multiply+cast+array-read on every oscillator tick.
#define SINE_LUT_SIZE 256
static int16_t s_sine_lut[SINE_LUT_SIZE];

static void sine_lut_init(void) {
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        s_sine_lut[i] = (int16_t)(sinf((2.0f * (float)M_PI * i) / SINE_LUT_SIZE) * 32767.0f);
    }
}

// phase must be in [0, 2*PI). Returns sin(phase) scaled to roughly -1.0..1.0.
static inline float lut_sine(float phase) {
    uint8_t idx = (uint8_t)(phase * (SINE_LUT_SIZE / (2.0f * (float)M_PI)));
    return s_sine_lut[idx] * (1.0f / 32767.0f);
}

static i2s_chan_handle_t s_tx_chan = NULL;

static void audio_i2s_init(void) {
    sine_lut_init();

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_DMA_FRAME_NUM;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_LRC,
            .dout = PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_chan));
}

static void audio_write_mono_chunk(const int16_t *mono, size_t count) {
    int16_t stereo[AUDIO_CHUNK_LEN * 2];
    for (size_t i = 0; i < count; i++) {
        stereo[i * 2] = mono[i];
        stereo[i * 2 + 1] = mono[i]; // duplicated into both slots -- see reference project's
                                     // SD_MODE comment on why stereo-duplicated-mono avoids
                                     // depending on which physical slot the amp reads
    }
    size_t bytes_written;
    i2s_channel_write(s_tx_chan, stereo, count * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

// Short 3-note arpeggio (C5 E5 G5), one partial each, fast exponential decay. Envelope
// tracked incrementally (per-sample multiply by a precomputed decay factor) instead of
// calling expf() fresh every sample -- the reference project's chime specifically
// documents this as the fix for a progressively-glitching startup tone.
static void play_startup_chime(void) {
    static const float note_freqs_hz[3] = { 523.25f, 659.25f, 783.99f }; // C5 E5 G5
    static const float note_starts_s[3] = { 0.0f, 0.08f, 0.16f };
    const float decay = 25.0f;
    const float decay_mul = expf(-decay / AUDIO_SAMPLE_RATE_HZ);
    const float peak_mix = 1.5f; // realistic ~2-note overlap, not all 3 at full volume at once

    float phase[3] = { 0.0f, 0.0f, 0.0f };
    float envelope[3] = { 0.0f, 0.0f, 0.0f };
    bool active[3] = { false, false, false };

    const float total_duration_s = note_starts_s[2] + 7.0f / decay; // ~-60dB after the last note
    const float fade_out_s = 0.01f;
    uint32_t total_samples = (uint32_t)(AUDIO_SAMPLE_RATE_HZ * total_duration_s);

    int16_t chunk[AUDIO_CHUNK_LEN];
    for (uint32_t sample_index = 0; sample_index < total_samples; ) {
        uint32_t chunk_len = total_samples - sample_index < AUDIO_CHUNK_LEN
                            ? total_samples - sample_index : AUDIO_CHUNK_LEN;
        for (uint32_t i = 0; i < chunk_len; i++) {
            float t = (float)(sample_index + i) / AUDIO_SAMPLE_RATE_HZ;
            float mix = 0.0f;
            for (int n = 0; n < 3; n++) {
                if (t < note_starts_s[n]) continue;
                phase[n] += (2.0f * (float)M_PI * note_freqs_hz[n]) / AUDIO_SAMPLE_RATE_HZ;
                if (phase[n] > 2.0f * (float)M_PI) phase[n] -= 2.0f * (float)M_PI;
                if (!active[n]) {
                    envelope[n] = 1.0f;
                    active[n] = true;
                } else {
                    envelope[n] *= decay_mul;
                }
                mix += lut_sine(phase[n]) * envelope[n];
            }
            float sample = (mix / peak_mix) * 20000.0f;
            float remaining_s = total_duration_s - t;
            if (remaining_s < fade_out_s) {
                sample *= fmaxf(remaining_s, 0.0f) / fade_out_s;
            }
            if (sample > 20000.0f) sample = 20000.0f;
            if (sample < -20000.0f) sample = -20000.0f;
            chunk[i] = (int16_t)sample;
        }
        audio_write_mono_chunk(chunk, chunk_len);
        sample_index += chunk_len;
    }
}

// Click timbre selection -- audibility test. Both simplified from the reference project
// (drops its click-rate/brightness modulation -- this device doesn't track either), and
// both cranked up in amplitude/duration relative to the reference's own defaults so a
// click is unmistakable on hardware before dialing back down.
// Change CLICK_TIMBRE below + reflash to compare; only the active one is compiled in.
#define CLICK_TIMBRE_WOOD_TOCK 0 // fast-decay sine with a downward pitch chirp
#define CLICK_TIMBRE_TICK_THUD 1 // sharp high tick layered with a low body thud
#define CLICK_TIMBRE CLICK_TIMBRE_WOOD_TOCK

#define CLICK_CLIP_LIMIT 32000.0f // headroom under int16 full scale, avoids a hard wrap
                                   // on cast while still allowing audible clipping/
                                   // saturation on the sharpest part of the transient --
                                   // raised from 30000 alongside CLICK_AMPLITUDE below
                                   // (detent click needed to be louder; pushing further
                                   // into clipping/saturation is the deliberate loudness
                                   // lever here, not a bug -- there's no separate hardware/
                                   // amp gain control in this design, see MAX98357A
                                   // SD_MODE comment above)

#if CLICK_TIMBRE == CLICK_TIMBRE_WOOD_TOCK
// Retuned again for an even higher-pitched, shorter "wood click" -- confirmed working
// well on hardware at 900Hz/15ms, then 1200Hz/9ms, now pushed to 2400Hz/5ms. Decay and
// chirp rates scaled up to match (same ~50%-of-duration glide completion point, same
// ~-45dB (e^-5.4) decay by end of window as the earlier versions had).
#define CLICK_DURATION_S 0.005f // short -- see decay/chirp rates below, both sized to fit
#define CLICK_BASE_FREQ_HZ 2400.0f
#define CLICK_DECAY_PER_S 1080.0f // e^(-1080*0.005) ~= 0.0045 -- fully decayed by CLICK_DURATION_S
#define CLICK_CHIRP_RATE 400.0f // downward glide completes in ~1/400s = ~2.5ms, inside the
                                 // 5ms window
#define CLICK_AMPLITUDE 32000.0f // reference default was 12000, raised to 22000 then here
                                  // to 32000 (near int16 full scale) -- the detent click
                                  // needed to be louder
#elif CLICK_TIMBRE == CLICK_TIMBRE_TICK_THUD
#define CLICK_DURATION_S 0.05f // long enough for the ~150/s thud decay to fully ring out
#define CLICK_TICK_FREQ_HZ 1800.0f
#define CLICK_THUD_FREQ_HZ 100.0f // fixed -- reference modulates this by click rate, which
                                   // this device doesn't track
#define CLICK_AMPLITUDE 32000.0f // reference default was 13000, raised to 22000 then here
                                  // to 32000 (near int16 full scale) -- the detent click
                                  // needed to be louder
#endif

// Button-tap "thump" -- a distinct, always-available timbre (not gated by CLICK_TIMBRE
// above, which only selects between alternate DETENT click sounds). Single low-frequency
// partial, fast exponential decay, no chirp -- deliberately lower/duller than the detent
// click so a button tap doesn't sound like a detent crossing.
#define THUMP_DURATION_S 0.04f // long enough for the decay below to fully ring out
#define THUMP_FREQ_HZ 110.0f
#define THUMP_DECAY_PER_S 140.0f // e^(-140*0.04) ~= 0.0033 -- fully decayed by THUMP_DURATION_S
#define THUMP_AMPLITUDE 22000.0f // matches CLICK_AMPLITUDE's post-revert level, not the
                                  // brief 32000 both were tried at

// Once a click starts, another queued one won't cut it off for at least this long -- a
// fast burst of detents plays as several distinct pulses instead of each one instantly
// overwriting the last before it's audible. Matches the reference project's
// MIN_CLICK_RETRIGGER_SEC.
#define MIN_CLICK_RETRIGGER_S 0.004f

static void i2s_task_fn(void *arg) {
    ESP_LOGI(TAG, "i2s task started on core %d, prio %d", xPortGetCoreID(), uxTaskPriorityGet(NULL));

    audio_i2s_init();
    play_startup_chime();

    float click_phase = 1.0f; // 1.0 = idle, matches the reference project's convention
    const float dt_s = 1.0f / AUDIO_SAMPLE_RATE_HZ;
    audio_click_type_t active_click_type = AUDIO_CLICK_NORMAL;
    float active_click_duration_s = CLICK_DURATION_S; // which of CLICK_DURATION_S/
                                                        // THUMP_DURATION_S gates the
                                                        // "click_phase < duration" check
                                                        // below, set per active_click_type
#if CLICK_TIMBRE == CLICK_TIMBRE_WOOD_TOCK
    float click_phase_acc = 0.0f;
#elif CLICK_TIMBRE == CLICK_TIMBRE_TICK_THUD
    float click_phase_tick = 0.0f;
    float click_phase_thud = 0.0f;
#endif
    float thump_phase_acc = 0.0f; // button-thump's own oscillator phase -- separate from
                                   // the detent-click accumulator(s) above since, although
                                   // only one voice ever plays at a time, keeping them
                                   // distinct avoids a stale phase carrying over between
                                   // two different-frequency timbres played back to back

    int16_t chunk[AUDIO_CHUNK_LEN];

    while (1) {
        for (int i = 0; i < AUDIO_CHUNK_LEN; i++) {
            if (click_phase >= MIN_CLICK_RETRIGGER_S) {
                audio_click_type_t type;
                if (audio_trigger_try_consume(&type)) {
                    click_phase = 0.0f;
                    active_click_type = type;
                    active_click_duration_s = (type == AUDIO_CLICK_BUTTON_THUMP)
                                             ? THUMP_DURATION_S : CLICK_DURATION_S;
#if CLICK_TIMBRE == CLICK_TIMBRE_WOOD_TOCK
                    click_phase_acc = 0.0f;
#elif CLICK_TIMBRE == CLICK_TIMBRE_TICK_THUD
                    click_phase_tick = 0.0f;
                    click_phase_thud = 0.0f;
#endif
                    thump_phase_acc = 0.0f;
                    // Debug trigger log (Phase 7 bring-up): confirms a click was actually
                    // consumed/started here on Core 1, independent of whether it's audible
                    // -- separates "not triggering" from "too quiet to notice". Edge-
                    // triggered (once per consumed click, not per sample), but a fast
                    // sustained spin can still fire this often enough to matter for UART
                    // timing (same class of issue as the Phase 2a per-tick-logging
                    // incident, see DEVELOPMENT_PLAN.md) -- remove or rate-limit this once
                    // triggering itself is confirmed working.
                    ESP_LOGI(TAG, "click trigger consumed, type=%d", (int)type);
                }
            }

            float sample = 0.0f;
            if (click_phase < active_click_duration_s) {
                if (active_click_type == AUDIO_CLICK_BUTTON_THUMP) {
                    // Single low partial, fast exponential decay, no chirp -- see
                    // THUMP_* comment above.
                    float envelope = expf(-THUMP_DECAY_PER_S * click_phase);
                    thump_phase_acc += (2.0f * (float)M_PI * THUMP_FREQ_HZ) * dt_s;
                    if (thump_phase_acc > 2.0f * (float)M_PI) thump_phase_acc -= 2.0f * (float)M_PI;
                    sample = lut_sine(thump_phase_acc) * envelope * THUMP_AMPLITUDE;
                } else {
#if CLICK_TIMBRE == CLICK_TIMBRE_WOOD_TOCK
                    // Fast downward pitch glide + exponential decay.
                    float chirp = CLICK_BASE_FREQ_HZ * (1.6f - 0.6f * fminf(1.0f, click_phase * CLICK_CHIRP_RATE));
                    float envelope = expf(-CLICK_DECAY_PER_S * click_phase);
                    click_phase_acc += (2.0f * (float)M_PI * chirp) * dt_s;
                    if (click_phase_acc > 2.0f * (float)M_PI) click_phase_acc -= 2.0f * (float)M_PI;
                    sample = lut_sine(click_phase_acc) * envelope * CLICK_AMPLITUDE;
#elif CLICK_TIMBRE == CLICK_TIMBRE_TICK_THUD
                    // Sharp high tick layered with a low body thud, independent decays.
                    float env_tick = expf(-900.0f * click_phase);
                    float env_thud = expf(-150.0f * click_phase);
                    click_phase_tick += (2.0f * (float)M_PI * CLICK_TICK_FREQ_HZ) * dt_s;
                    click_phase_thud += (2.0f * (float)M_PI * CLICK_THUD_FREQ_HZ) * dt_s;
                    if (click_phase_tick > 2.0f * (float)M_PI) click_phase_tick -= 2.0f * (float)M_PI;
                    if (click_phase_thud > 2.0f * (float)M_PI) click_phase_thud -= 2.0f * (float)M_PI;
                    sample = (lut_sine(click_phase_tick) * env_tick * 0.5f
                            + lut_sine(click_phase_thud) * env_thud * 1.0f) * CLICK_AMPLITUDE;
#endif
                }
                if (sample > CLICK_CLIP_LIMIT) sample = CLICK_CLIP_LIMIT;
                if (sample < -CLICK_CLIP_LIMIT) sample = -CLICK_CLIP_LIMIT;
                click_phase += dt_s;
            }
            chunk[i] = (int16_t)sample;
        }
        audio_write_mono_chunk(chunk, AUDIO_CHUNK_LEN);
    }
}

void i2s_task_start(void) {
    xTaskCreatePinnedToCore(i2s_task_fn, "i2s", 4096, NULL, PRIO_I2S, NULL, CORE_IO);
}
