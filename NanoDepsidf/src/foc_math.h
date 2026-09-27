#pragma once

// Plain-float Clarke/Park inverse transforms. Logic matches Espressif's validated
// mcpwm_foc_svpwm_open_loop example (esp_foc.c), reimplemented in float instead of
// fixed-point IQ math -- ESP32-S3 has a hardware FPU and this isn't a hot-path bottleneck
// yet, so the extra IQmath component dependency isn't worth adding for Phase 2a bring-up.

typedef struct { float d, q; } foc_dq_t;
typedef struct { float alpha, beta; } foc_ab_t;
typedef struct { float a, b, c; } foc_abc_t;

// (d,q) rotating frame -> (alpha,beta) stationary frame, at electrical angle theta_e (rad).
foc_ab_t foc_inverse_park(foc_dq_t dq, float theta_e);

// (alpha,beta) -> three-phase (a,b,c), same units as input.
foc_abc_t foc_inverse_clarke(foc_ab_t ab);
