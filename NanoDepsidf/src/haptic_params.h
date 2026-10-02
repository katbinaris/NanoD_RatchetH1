#pragma once

// Phase 8 step 3: haptic detent parameters, live-tunable via the Haptic Configurator menu
// screen (menu.c) and consumed directly by the real-time haptic control loop
// (control_task.c). Pulled into their own shared header instead of living only as
// control_task.c's private #defines (which is where they lived through step 2) because
// menu.c's rotate_*()/default-value code needs the EXACT same bounds -- duplicating them as
// separate literals in two files is exactly the kind of drift this project has already been
// bitten by before (see MS_TO_ITERS()'s history in DEVELOPMENT_PLAN.md), and would be
// actively unsafe here specifically: menu.c's detent-count field used to be a cosmetic
// step-1 placeholder (clamped 0-120, wired to nothing real). Now that it directly drives
// `2*pi/num_detents` in control_task.c's real-time loop, an unclamped or wrongly-bounded
// value -- 0 above all -- is a live division-by-zero/NaN hazard on the actual motor output,
// not just a UI quirk.

#define HAPTIC_NUM_DETENTS_DEFAULT 12u // 30deg spacing
#define HAPTIC_NUM_DETENTS_MIN 3u
#define HAPTIC_NUM_DETENTS_MAX 36u

#define HAPTIC_KP_DEFAULT 6.0f // V/rad -- "sharpness". Likely saturates against the voltage
                                // cap over much of a detent's travel (that's bounded by
                                // MOTOR_MAX_CURRENT_STATIC_A, a separate knob, not this gain)
#define HAPTIC_KP_MIN 0.0f      // 0 = no detents at all, pure Kd -- see HAPTIC_KD_MAX's
                                // "viscous fluid" note
#define HAPTIC_KP_MAX 20.0f

#define HAPTIC_KD_DEFAULT 0.01f // V per (rad/s)
#define HAPTIC_KD_MIN 0.0f
#define HAPTIC_KD_MAX 0.15f // HAPTIC_KP_MIN + Kd near this ceiling gives a distinct "viscous
                             // fluid" knob feel -- pure velocity damping, no positional spring

// SHAPE (percent): bends Saw's straight force line. 0 = straight, as before. Higher values
// soften the pull near a step's centre and move the rise towards the midpoint between two
// steps, where it gets steeper; the force at the midpoint itself stays the same
// (control_task.c). Capped below 100 so the centre always keeps some spring.
#define HAPTIC_SHAPE_DEFAULT 0
#define HAPTIC_SHAPE_MIN 0
#define HAPTIC_SHAPE_MAX 90
#define HAPTIC_SHAPE_STEP 5

// Phase 8 step 4: the three selectable haptic profiles (Haptic Configurator's "Haptic Type"
// field, menu.c) -- shared with control_task.c, which is the one that actually branches on
// this to pick a restoring-force law each tick, and with config_store.c's load-time sanity
// check (previously a separate hand-synced HAPTIC_TYPE_COUNT define there -- now just this
// enum's own count, one less place for the two to drift apart).
typedef enum {
    HAPTIC_TYPE_SAW = 0, // nearest-grid-point linear snap -- the original/default profile
    HAPTIC_TYPE_SINE,    // continuous Vq = -Kp*sin(num_detents*rel) -- a smooth "bump" instead
                         // of a snap; historically rejected as the ONLY profile (steepest
                         // slope at the detent center caused persistent oscillation there,
                         // flattest at the boundary -- backwards for a crisp click) but kept
                         // here as a deliberately different, selectable feel, not a default
    HAPTIC_TYPE_VISCOSE, // Kp forced to 0 regardless of the menu's own Kp field -- pure
                         // velocity damping, no positional spring, no clicks
    HAPTIC_TYPE_COUNT
} haptic_type_t;
