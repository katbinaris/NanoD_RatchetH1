#include "foc_math.h"
#include <math.h>

foc_ab_t foc_inverse_park(foc_dq_t dq, float theta_e) {
    float s = sinf(theta_e);
    float c = cosf(theta_e);
    foc_ab_t ab;
    ab.alpha = dq.d * c - dq.q * s;
    ab.beta  = dq.d * s + dq.q * c;
    return ab;
}

foc_abc_t foc_inverse_clarke(foc_ab_t ab) {
    static const float SQRT3_OVER_2 = 0.8660254f;
    foc_abc_t abc;
    abc.a = ab.alpha;
    abc.b = -0.5f * ab.alpha + SQRT3_OVER_2 * ab.beta;
    abc.c = -0.5f * ab.alpha - SQRT3_OVER_2 * ab.beta;
    return abc;
}
