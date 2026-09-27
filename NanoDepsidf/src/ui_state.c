#include "ui_state.h"
#include <stdatomic.h>

static _Atomic int32_t s_detent = 0;

void ui_state_init(void) {
    atomic_store_explicit(&s_detent, 0, memory_order_relaxed);
}

void ui_state_set_detent(int32_t wrapped_index) {
    atomic_store_explicit(&s_detent, wrapped_index, memory_order_relaxed);
}

int32_t ui_state_get_detent(void) {
    return atomic_load_explicit(&s_detent, memory_order_relaxed);
}
