#include "audio_trigger.h"
#include "tasks_common.h"
#include <stdatomic.h>

#define AUDIO_CLICK_QUEUE_SIZE 16

// Written by the producer (audio_trigger_click, Core 0) before the release-ordered
// increment below publishes it; read by the consumer (audio_trigger_try_consume, Core 1)
// only after an acquire-ordered load observes that increment. That ordering is what makes
// the plain (non-atomic) array access below safe across cores -- by the time the consumer
// sees the incremented count, this write is guaranteed visible to it too.
static audio_click_type_t s_type_queue[AUDIO_CLICK_QUEUE_SIZE];

static _Atomic uint32_t s_trigger_count = 0;

// Consumer-owned bookkeeping (Core 1 only) -- no atomics needed, single consumer.
static uint32_t s_last_seen_count = 0;
static uint32_t s_pending = 0;
static uint32_t s_consumed_index = 0;

void audio_trigger_init(void) {
    atomic_store_explicit(&s_trigger_count, 0, memory_order_relaxed);
    s_last_seen_count = 0;
    s_pending = 0;
    s_consumed_index = 0;
}

void CONTROL_HOT audio_trigger_click(audio_click_type_t type) {
    uint32_t write_index = atomic_load_explicit(&s_trigger_count, memory_order_relaxed);
    s_type_queue[write_index % AUDIO_CLICK_QUEUE_SIZE] = type;
    atomic_store_explicit(&s_trigger_count, write_index + 1, memory_order_release);
}

bool audio_trigger_try_consume(audio_click_type_t *out_type) {
    uint32_t now_count = atomic_load_explicit(&s_trigger_count, memory_order_acquire);
    if (now_count != s_last_seen_count) {
        s_pending += (now_count - s_last_seen_count);
        s_last_seen_count = now_count;
    }
    if (s_pending == 0) {
        return false;
    }
    *out_type = s_type_queue[s_consumed_index % AUDIO_CLICK_QUEUE_SIZE];
    s_consumed_index++;
    s_pending--;
    return true;
}
