#include "foc_calibration.h"
#include "motor_driver.h"
#include "motor_config.h"
#include "mt6701.h"
#include "foc_math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include <math.h>

#define NVS_NAMESPACE "foc_cal"
#define NVS_KEY       "cal"

static const char *TAG = "foc_cal";

#define ALIGN_SETTLE_MS 1000
#define STEP_SETTLE_MS   500
#define STEP_ELECTRICAL_RAD (float)(M_PI / 4.0) // 45 electrical degrees -- small, unambiguous

// Below this, treat the rotor as not having moved (stuck/cogging/wiring issue) rather than
// trusting a noisy direction reading. ~0.01 rad mechanical is ~45 raw counts -- well above
// the ~4-count noise floor measured during MT6701 bring-up, well below the expected step.
#define MIN_VALID_MECH_DELTA_RAD 0.01f

static void command_dq(float d, float theta_elec) {
    foc_dq_t dq = { .d = d, .q = 0.0f };
    foc_ab_t ab = foc_inverse_park(dq, theta_elec);
    foc_abc_t abc = foc_inverse_clarke(ab);
    motor_driver_set_phase_voltages(abc.a, abc.b, abc.c);
}

// Ramps direct-axis magnitude 0 -> d_target at a fixed electrical angle instead of
// commanding it in one step. A step (especially the direction-detect step, which also
// jumps the commanded angle by 45 electrical degrees) draws an inrush current spike the
// static current cap in motor_config.h can't see -- see MOTOR_VOLTAGE_SLEW_LIMIT_V_PER_S.
#define CAL_RAMP_STEPS 20
#define CAL_RAMP_STEP_MS 10 // 200ms ramp
static void command_dq_ramped(float d_target, float theta_elec) {
    for (int i = 1; i <= CAL_RAMP_STEPS; i++) {
        command_dq(d_target * ((float)i / CAL_RAMP_STEPS), theta_elec);
        vTaskDelay(pdMS_TO_TICKS(CAL_RAMP_STEP_MS));
    }
}

static float raw_to_rad(int32_t raw) {
    return ((float)raw / 16384.0f) * 2.0f * (float)M_PI;
}

foc_calibration_t foc_calibration_run(void) {
    foc_calibration_t cal = { .electrical_offset_rad = 0.0f, .direction = 1.0f, .valid = false };

    // Step 1: align. Direct-axis voltage at a fixed electrical angle pulls the rotor's flux
    // into alignment with that angle (this is what direct/d-axis voltage does physically --
    // unlike quadrature/q-axis, it produces no continuous torque, just a restoring pull
    // toward alignment, which is exactly what's needed to establish a known reference).
    ESP_LOGI(TAG, "calibration: aligning...");
    command_dq_ramped(MOTOR_EFFECTIVE_STATIC_VOLTAGE_LIMIT_V, 0.0f);
    vTaskDelay(pdMS_TO_TICKS(ALIGN_SETTLE_MS));
    int32_t zero_raw = mt6701_read_angle_raw();
    if (zero_raw < 0) {
        ESP_LOGE(TAG, "calibration: sensor read failed during align");
        return cal;
    }

    // Step 2: small step to detect sensor direction relative to commanded electrical
    // rotation. Must be small enough that the mechanical response is unambiguous (no risk
    // of wraparound confusion) -- 45 electrical degrees is a small fraction of a mechanical
    // revolution once divided by pole pairs.
    ESP_LOGI(TAG, "calibration: stepping to detect direction...");
    command_dq_ramped(MOTOR_EFFECTIVE_STATIC_VOLTAGE_LIMIT_V, STEP_ELECTRICAL_RAD);
    vTaskDelay(pdMS_TO_TICKS(STEP_SETTLE_MS));
    int32_t step_raw = mt6701_read_angle_raw();
    if (step_raw < 0) {
        ESP_LOGE(TAG, "calibration: sensor read failed during step");
        return cal;
    }

    int32_t delta_raw = step_raw - zero_raw;
    if (delta_raw > 8192) delta_raw -= 16384;   // unwrap to shortest signed path
    if (delta_raw < -8192) delta_raw += 16384;
    float delta_mech_rad = raw_to_rad(delta_raw);

    if (fabsf(delta_mech_rad) < MIN_VALID_MECH_DELTA_RAD) {
        ESP_LOGE(TAG, "calibration: rotor didn't move during step (delta=%.4f rad) -- "
                       "aborting, not trusting a noisy direction reading",
                 delta_mech_rad);
        return cal;
    }

    cal.direction = (delta_mech_rad > 0.0f) ? 1.0f : -1.0f;
    float zero_mech_rad = raw_to_rad(zero_raw);
    cal.electrical_offset_rad = cal.direction * zero_mech_rad * MOTOR_POLE_PAIRS;
    cal.valid = true;

    ESP_LOGI(TAG, "calibration OK: direction=%.0f, offset=%.4f rad (zero_raw=%ld, "
                   "step_raw=%ld, delta_mech=%.4f rad)",
             cal.direction, cal.electrical_offset_rad, (long)zero_raw, (long)step_raw,
             delta_mech_rad);
    return cal;
}

bool foc_calibration_load(foc_calibration_t *out) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return false; // namespace doesn't exist yet -- normal on first-ever boot
    }
    foc_calibration_t stored;
    size_t len = sizeof(stored);
    esp_err_t err = nvs_get_blob(h, NVS_KEY, &stored, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(stored)) {
        return false;
    }
    // Sanity check before trusting stored data -- guards against a corrupted/partial write
    // rather than silently using garbage to drive the motor.
    if (!stored.valid || (stored.direction != 1.0f && stored.direction != -1.0f) ||
        !isfinite(stored.electrical_offset_rad)) {
        ESP_LOGW(TAG, "stored calibration failed sanity check, ignoring");
        return false;
    }
    *out = stored;
    ESP_LOGI(TAG, "loaded calibration from NVS: direction=%.0f, offset=%.4f rad",
             out->direction, out->electrical_offset_rad);
    return true;
}

void foc_calibration_erase(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return; // nothing stored
    }
    esp_err_t err = nvs_erase_key(h, NVS_KEY);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    ESP_LOGI(TAG, "calibration erased (%s)", esp_err_to_name(err));
}

void foc_calibration_save(const foc_calibration_t *cal) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open (write) failed: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_blob(h, NVS_KEY, cal, sizeof(*cal));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to save calibration to NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "calibration saved to NVS");
    }
}
