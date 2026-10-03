#include "agent_board.h"
#include "freertos/FreeRTOS.h"
#include <stdatomic.h>
#include <string.h>

static agent_row_t s_rows[AGENT_BOARD_MAX];
static int s_n = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static _Atomic uint32_t s_version = 0;

void agent_board_set(const agent_row_t *rows, int n) {
    if (n < 0) n = 0;
    if (n > AGENT_BOARD_MAX) n = AGENT_BOARD_MAX;
    portENTER_CRITICAL(&s_mux);
    memcpy(s_rows, rows, (size_t)n * sizeof(rows[0]));
    for (int i = 0; i < n; i++) s_rows[i].name[AGENT_NAME_MAX] = '\0';
    s_n = n;
    portEXIT_CRITICAL(&s_mux);
    atomic_fetch_add(&s_version, 1);
}

int agent_board_get(agent_row_t *out, int max) {
    portENTER_CRITICAL(&s_mux);
    int n = s_n < max ? s_n : max;
    memcpy(out, s_rows, (size_t)n * sizeof(out[0]));
    portEXIT_CRITICAL(&s_mux);
    return n;
}

uint32_t agent_board_version(void) { return atomic_load(&s_version); }
