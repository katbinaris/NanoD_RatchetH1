#pragma once

// Motor + safety config for the NanoFOC_D BLDC knob.
//
// RESOLVED (was an open concern after the second bring-up attempt reset repeatedly):
// measured phase-to-phase (line-to-line) resistance directly with a multimeter = 5.29ohm,
// essentially identical to the legacy "5.3" constant (BLDCMotor(7, 5.3) in
// legacy_fw/src/foc_thread.cpp) -- confirming that value was actually line-to-line, but had
// been used as per-phase resistance in the current-limit math (in both the legacy firmware
// and this port) ever since. True per-phase (phase-to-neutral) resistance for a wye-wound
// motor is half the line-to-line value. This was almost certainly the direct cause of the
// second attempt's brownout resets: at the 2.5V topology-clamped command, the wrong (2x too
// large) resistance made the math think current was a safe 2.5/5.3=0.47A when it was
// actually closer to 2.5/2.65=0.94A -- roughly double the intended 0.5A cap, under sustained
// near-locked-rotor load (the knob wasn't rotating, so this was the worst-case condition
// for the whole 8-second test, not a brief transient).
#define MOTOR_PHASE_TO_PHASE_RESISTANCE_OHM 5.29f
#define MOTOR_PHASE_RESISTANCE_OHM (MOTOR_PHASE_TO_PHASE_RESISTANCE_OHM / 2.0f)

// Pole pairs: legacy firmware assumed 7 (BLDCMotor(7, 5.3)). Was changed to 4 (8 physical
// poles) based on a user magnet count, which turned out to be a stator coil count (12), not
// a rotor pole count -- a different physical quantity. RESOLVED via the unconfounded
// pole-pair diagnostic (BTN_A+BTN_D): command a known, fixed electrical Hz open-loop and
// measure actual mechanical Hz from the MT6701 directly (pole_pairs = electrical_Hz /
// measured_mechanical_Hz), which sidesteps the RPM-labeling confound of the earlier
// stall-based comparison (see git history / DEVELOPMENT_PLAN.md for that reasoning). Result:
// -13.462 rad over 5.0s at 3.00 Hz electrical -> 0.4285 Hz mechanical -> exactly 7.00 pole
// pairs, a clean integer result. Confirms the legacy value was correct all along, and is
// consistent with a 12N14P topology (12 stator coils / 14 rotor poles), a very common
// gimbal-motor arrangement -- more so than the 12N8P (4 pole pairs) guess.
//
// IMPORTANT: any cached NVS calibration was computed under the wrong (4) pole-pair
// assumption and is now invalid (the electrical<->mechanical angle relationship it encodes
// no longer matches reality) -- force a fresh calibration (BTN_A+BTN_B) after this change,
// don't rely on the cached one.
#define MOTOR_POLE_PAIRS 7

// --- Bring-up safety limits (deliberately conservative -- raise only once verified) ---

// Supply/output voltage cap.
#define MOTOR_MAX_VOLTAGE_V 5.0f

// Current cap. This board has NO current sensing (see DEVELOPMENT_PLAN.md decisions log),
// so this can't be a closed-loop limit -- it's enforced as a static Ohm's-law voltage clamp
// instead (the same approach SimpleFOC uses when no current sensor is configured). This is
// a steady-state/locked-rotor approximation that ignores inductance/back-EMF dynamics AND
// inrush at enable-time, which this Ohm's-law model cannot capture at all -- not a substitute
// for real current sensing. Was temporarily lowered to 0.15A while debugging a string of
// incidents that turned out to be a combination of the wrong phase resistance (now
// corrected above) and an unrelated MCPWM peak_ticks scaling bug (see motor_driver.c) --
// with both root causes actually fixed and a clean run confirmed at 0.15A, restored to the
// originally-intended 0.5A.
//
// CORRECTION to an earlier conclusion in this file's history: a brownout under a tight
// 5V/500mA USB port was initially (wrongly) attributed ENTIRELY to the 32kHz MCPWM carrier,
// with current level cleared as "never actually the problem" based on a single confounded
// comparison (both carrier freq AND current cap changed together across the tests run at
// that time). That conclusion was premature: restoring to 0.5A at the *already-fixed*
// 10kHz carrier brought the brownout straight back (now even during calibration, not just
// PD hold), proving current level independently matters too -- it isn't just carrier
// frequency. Both contribute to the same hard, shared ~500mA ceiling on this power source.
//
// Given that, split into two levels instead of one, since they have different risk
// profiles against an inverse-time-tripping USB port limit: a CONTINUOUS cap for anything
// sustained (the PD hold can run for up to 15s straight) and a higher PEAK cap for brief,
// bounded operations only (calibration's align/step, ~1.5s total). 0.2A continuous is the
// last value directly confirmed brownout-free under sustained PD hold (at 10kHz). 0.3A
// peak is an untested bisection point between that confirmed-safe 0.2A and the
// confirmed-unsafe 0.5A -- needs verification, raise/lower from here based on evidence, not
// assumption.
//
// RESOLVED: tested on a beefier PD-capable supply (this board has an onboard PD chip with
// its own NVS-baked profile, not yet driven by firmware -- but it negotiates independently)
// -- no brownout at all, confirming the plain 5V/500mA USB port really was the hard ceiling,
// not a firmware bug. But torque was still weak at these same conservative levels, which
// makes sense: they were bisected specifically against the *weak* port's budget, not this
// one. Also explains why open-loop rotation felt strong at this same cap while closed-loop
// holding didn't: current = (V-back_EMF)/R -- while rotating, back-EMF opposes the applied
// voltage, so real current stays below this Ohm's-law worst-case model; while holding
// (or during calibration's static align/step), back-EMF is ~zero, so real current sits at
// the full modeled worst case, i.e. holding is inherently the more current-hungry case for
// the same nominal cap. Not a firmware inconsistency -- physically expected.
//
// CONFIRMED DECISIVELY: reconstructed the open-loop rotation test as a separate diagnostic
// mode (BTN_C) and ran it on the *same* laptop supply that browns out the closed-loop
// static-hold test, at the *same* 0.5A cap and 10kHz carrier -- open-loop rotation was
// strong (held firmly by hand) with zero brownouts, while closed-loop static holding still
// browned out. Same supply, same cap, same carrier, opposite outcome: this conclusively
// confirms the axis that matters is STATIC (zero/near-zero speed, no back-EMF) vs ROTATING
// (back-EMF assists), not "how long" an operation runs -- the earlier CONTINUOUS/PEAK
// naming modeled the wrong axis (calibration and PD hold are BOTH effectively static from
// the current model's perspective, despite one being brief and one sustained). Renamed and
// re-split accordingly: STATIC cap (calibration's align/step AND the PD hold, both static-
// current scenarios) kept at the last confirmed-safe static level, 0.2A; ROTATING cap
// (the open-loop diagnostic, and any future velocity-mode rotation) raised to 0.5A, now
// directly confirmed strong and brownout-free on the weakest supply tested.
//
// CONFIRMED via live vq monitoring during a single-position hold test: idle ~0.045V,
// pinned at exactly 0.530V on any real push -- an exact match to 0.2A x 2.645ohm = 0.529V.
// Directly proves vq saturates against this current cap, not PD gains (Kp/Kd only affect
// how fast the ceiling is reached, not the ceiling itself). Bisecting upward from here --
// this single-position test is gentler than the multi-target tour that browned out at
// 0.5A (no instant 90deg target jumps), so it may tolerate more than 0.2A.
//
// 0.35A tested clean: vq pinned at exactly 0.926V as predicted (0.35 x 2.645), no
// brownout, divergence-abort correctly fired on a hard push past ~86 deg (expected safety
// behavior, not a bug). Continuing the bisection upward -- testing the full 0.5A rotating-
// level cap next, since this gentler single-position test hasn't hit it yet.
//
// LOWERED back to 0.3A: 0.5A tested clean for gentle single-position holding, but the
// 30-target tour with Kp raised to 5.0 (to overcome static-friction breakaway on small
// steps) commands much closer to the full ceiling much more often/aggressively -- 0.3A
// keeps meaningful headroom under the PD-driven peak commands specifically, rather than
// relying on the single-position test's gentler duty cycle to stay safe.
//
// RAISED back to 0.5A: the Kp=5.0 concern above no longer applies -- that boost turned out
// to be compensating for the MOTOR_POLE_PAIRS bug (4 instead of 7) and a control-law
// direction-sign bug, both now fixed; the bench tour is back to the gentler Kp=1.5 and
// completed all 30 targets cleanly. Also directly requested to raise the haptic detent
// demo's peak torque (0.3A was identified as the dominant reason clicks felt soft/damped
// compared to legacy_fw's real closed-loop current control) -- this is a re-raise to the
// already-once-validated-safe level, not a new untested one, but still worth watching for
// brownout on hardware given the haptic demo's different (faster slew, live-adjustable
// gains) usage pattern than what was originally tested at 0.5A.
//
// RAISED to ~0.76A: requested bump of the effective vq clamp from ~1.32V to 2.0V for a
// stronger click punch -- this is beyond the previously-validated 0.5A/1.3V range, so
// watch for brownout/coil heating on hardware.
#define MOTOR_MAX_CURRENT_STATIC_A 0.756f
#define MOTOR_MAX_CURRENT_ROTATING_A 0.756f
#define MOTOR_STATIC_CURRENT_DERIVED_VOLTAGE_LIMIT_V (MOTOR_MAX_CURRENT_STATIC_A * MOTOR_PHASE_RESISTANCE_OHM)
#define MOTOR_ROTATING_CURRENT_DERIVED_VOLTAGE_LIMIT_V (MOTOR_MAX_CURRENT_ROTATING_A * MOTOR_PHASE_RESISTANCE_OHM)

// Bounds dV/dt on commanded phase voltage, independent of the steady-state current cap
// above. The Ohm's-law model above cannot see inrush -- a step change in commanded
// voltage/angle (e.g. calibration's direct-axis step, or a closed-loop target change)
// draws a brief current spike well above the steady-state estimate while the field/rotor
// catch up, which is consistent with the "rotor moves, then brownout" symptom on the new,
// tighter supply. Used by foc_calibration.c (ramped align/step) and control_task.c
// (CL_MAX_VQ_STEP_V) rather than commanding a full step in one tick.
#define MOTOR_VOLTAGE_SLEW_LIMIT_V_PER_S 8.0f

// Topology-imposed ceiling. This board's driver is a single-supply, half-bridge-per-phase
// scheme (duty centered at 50% = 0V), so a phase can only linearly swing +/-Vbus/2 without
// clipping -- NOT the full Vbus. Missing this caused a real bug during first bring-up: a
// 2.65V command clipped against this 2.5V ceiling, mcpwm rejected the out-of-range compare
// value, and the resulting per-tick error logging (over UART, ~7ms/line) starved the
// scheduler badly enough to trip the idle-task watchdog and desync the control loop from
// wall-clock time by ~18s -- the motor sat energized in a near-static, distorted field
// instead of rotating (matches the observed symptom: noise + held torque, no rotation).
#define MOTOR_HALF_BUS_LIMIT_V (MOTOR_MAX_VOLTAGE_V / 2.0f)

// The actual voltage clamps control code must respect -- don't use MOTOR_MAX_VOLTAGE_V or
// the current-derived limits directly, always go through these. Two variants: STATIC for
// anything at zero/near-zero rotor speed (foc_calibration.c's align/step, control_task.c's
// PD hold), ROTATING for anything with the field continuously advancing (the open-loop
// diagnostic) -- see the current-cap comment above for why these must differ.
#define MOTOR_EFFECTIVE_STATIC_VOLTAGE_LIMIT_V \
    (MOTOR_STATIC_CURRENT_DERIVED_VOLTAGE_LIMIT_V < MOTOR_HALF_BUS_LIMIT_V \
        ? (MOTOR_STATIC_CURRENT_DERIVED_VOLTAGE_LIMIT_V < MOTOR_MAX_VOLTAGE_V ? MOTOR_STATIC_CURRENT_DERIVED_VOLTAGE_LIMIT_V : MOTOR_MAX_VOLTAGE_V) \
        : MOTOR_HALF_BUS_LIMIT_V)
#define MOTOR_EFFECTIVE_ROTATING_VOLTAGE_LIMIT_V \
    (MOTOR_ROTATING_CURRENT_DERIVED_VOLTAGE_LIMIT_V < MOTOR_HALF_BUS_LIMIT_V \
        ? (MOTOR_ROTATING_CURRENT_DERIVED_VOLTAGE_LIMIT_V < MOTOR_MAX_VOLTAGE_V ? MOTOR_ROTATING_CURRENT_DERIVED_VOLTAGE_LIMIT_V : MOTOR_MAX_VOLTAGE_V) \
        : MOTOR_HALF_BUS_LIMIT_V)
