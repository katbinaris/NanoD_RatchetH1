#include "motor_driver.h"
#include "board_pins.h"
#include "motor_config.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdint.h>

static const char *TAG = "motor_driver";

// Carrier frequency = resolution_hz / period_ticks (in UP_DOWN mode, one full up+down cycle
// takes exactly period_ticks ticks). Originally 10MHz/1000=10kHz, matching Espressif's
// mcpwm_foc_svpwm_open_loop example -- but 10kHz is right in the audible range and produced
// a persistent high-pitched whine from the motor windings. Was raised to 32MHz/32kHz
// (above human hearing, see git history/DEVELOPMENT_PLAN.md for the derivation) -- but that
// caused a real brownout under a tight 5V/500mA USB port supply at the time: confirmed by
// reverting to 10kHz alone (no other change) and the brownout stopped, even though modeled
// average motor current was already nowhere near the port's 500mA limit. Conclusion then:
// the 32kHz carrier's switching-current spikes (not average current) were tripping the
// port's overcurrent protection.
//
// RE-TESTED and RAISED BACK to 32kHz: clean on hardware this time, no brownout. Current
// draw context has changed a lot since the original incident (0.5A static cap now vs.
// whatever was in effect then, haptic-mode usage patterns instead of the original
// sustained PD-hold test, etc.), so this isn't necessarily proof the underlying physical
// risk (switching-current spikes on a weak supply) is gone -- it's supply-dependent and
// was only ever confirmed on one specific tight 5V/500mA USB port. Worth re-confirming on
// that same weak supply specifically if it's still around, rather than assuming today's
// clean result generalizes to every supply.
#define MCPWM_RESOLUTION_HZ 32000000 // 32MHz -> 32kHz carrier (see above)
#define MCPWM_PERIOD_TICKS  1000

// ACTUAL VALID COMPARE RANGE, confirmed by reading esp_driver_mcpwm's source directly
// (not assumed) after "compare value out of range" kept recurring despite a duty clamp
// that should have prevented it: mcpwm_timer.c halves period_ticks into peak_ticks for
// MCPWM_TIMER_COUNT_MODE_UP_DOWN ("in symmetric mode, peak_ticks = period_ticks / 2"), and
// mcpwm_cmpr.c's mcpwm_comparator_set_compare_value() validates against peak_ticks, not
// period_ticks. Every prior "out of range" incident across this bring-up was this same
// factor-of-2 mistake -- compare values were being scaled against MCPWM_PERIOD_TICKS (1000)
// when the real ceiling in this count mode is always half that.
#define MCPWM_PEAK_TICKS (MCPWM_PERIOD_TICKS / 2)

static mcpwm_timer_handle_t s_timer;
static mcpwm_oper_handle_t s_oper[3];
static mcpwm_cmpr_handle_t s_cmpr[3];
static mcpwm_gen_handle_t s_gen[3];

static const int s_in_pins[3] = { PIN_IN_U, PIN_IN_V, PIN_IN_W };
static const int s_en_pins[3] = { PIN_EN_U, PIN_EN_V, PIN_EN_W };

void motor_driver_enable(bool enable) {
    for (int i = 0; i < 3; i++) {
        gpio_set_level(s_en_pins[i], enable ? 1 : 0);
    }
}

esp_err_t motor_driver_init(void) {
    // NOTE on EN/FAULT: the STSPIN233's EN pins are technically combined EN/FAULT pins --
    // the chip can pull one low internally on a real overcurrent/short/thermal fault, and
    // ST's reference design pairs this with an external RC network for auto-retry timing.
    // An earlier version of this code switched these to open-drain + readable to make use
    // of that. Reverted: legacy Arduino/SimpleFOC firmware drove these same pins as plain
    // push-pull outputs (never reading them) and successfully spun this exact motor/board,
    // and with the open-drain change the fault-read was permanently stuck low, unchanging,
    // across a 10s test with zero commanded voltage -- consistent with a floating input
    // (no pull-up to release to) rather than a real fault signal. This board most likely
    // doesn't implement the external RC network the fault-reporting feature needs, so
    // there's no reliable way to read fault state here -- back to plain push-pull, matching
    // what's proven to work, relying on the voltage/current limits in motor_config.h and
    // the wall-clock hard timeout in control_task.c for safety instead.
    uint64_t en_mask = 0;
    for (int i = 0; i < 3; i++) {
        en_mask |= (1ULL << s_en_pins[i]);
    }
    gpio_config_t en_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = en_mask,
    };
    ESP_ERROR_CHECK(gpio_config(&en_cfg));
    motor_driver_enable(false); // stay disabled until PWM outputs are at a known-safe state

    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = MCPWM_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN, // center-aligned, lower switching noise
        .period_ticks = MCPWM_PERIOD_TICKS,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &s_timer));

    for (int i = 0; i < 3; i++) {
        mcpwm_operator_config_t oper_config = { .group_id = 0 };
        ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &s_oper[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(s_oper[i], s_timer));

        mcpwm_comparator_config_t cmpr_config = { .flags.update_cmp_on_tez = true };
        ESP_ERROR_CHECK(mcpwm_new_comparator(s_oper[i], &cmpr_config, &s_cmpr[i]));
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(s_cmpr[i], 0));

        mcpwm_generator_config_t gen_config = { .gen_gpio_num = s_in_pins[i] };
        ESP_ERROR_CHECK(mcpwm_new_generator(s_oper[i], &gen_config, &s_gen[i]));

        // Single generator per phase -- same edge convention (UP+compare->LOW,
        // DOWN+compare->HIGH) as Espressif's validated reference example's high-side
        // generator, just without a paired complementary/dead-time generator.
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(s_gen[i],
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, s_cmpr[i], MCPWM_GEN_ACTION_LOW)));
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(s_gen[i],
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN, s_cmpr[i], MCPWM_GEN_ACTION_HIGH)));
    }

    ESP_ERROR_CHECK(mcpwm_timer_enable(s_timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "motor driver initialized (PWM running, EN outputs disabled)");
    return ESP_OK;
}

// Clamp with margin, not exactly [0,1] -- a correctly-derived command can still touch the
// exact duty=0 or duty=1 boundary at sinusoid peaks (e.g. cos(0)=1 precisely), and that
// exact boundary value was rejected as "out of range" by mcpwm during first bring-up (see
// motor_config.h). Structural fix: never let the compare value get near either edge,
// regardless of what voltage upstream code requests.
#define DUTY_MARGIN 0.01f

static uint32_t voltage_to_compare(float phase_volts) {
    // This board's driver switches each phase between 0V and the supply rail, so duty=0.5
    // means 0V average -- duty fraction is referenced to a virtual neutral at Vbus/2.
    float duty = (phase_volts / MOTOR_MAX_VOLTAGE_V) + 0.5f;
    if (duty < DUTY_MARGIN) duty = DUTY_MARGIN;
    if (duty > 1.0f - DUTY_MARGIN) duty = 1.0f - DUTY_MARGIN;
    return (uint32_t)(duty * MCPWM_PEAK_TICKS); // NOT MCPWM_PERIOD_TICKS -- see comment above
}

void motor_driver_set_phase_voltages(float ua, float ub, float uc) {
    mcpwm_comparator_set_compare_value(s_cmpr[0], voltage_to_compare(ua));
    mcpwm_comparator_set_compare_value(s_cmpr[1], voltage_to_compare(ub));
    mcpwm_comparator_set_compare_value(s_cmpr[2], voltage_to_compare(uc));
}
