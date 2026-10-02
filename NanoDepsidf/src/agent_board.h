#pragma once

#include <stdbool.h>
#include <stdint.h>

// The AGENTS profile's dashboard: the agent sessions the host daemon knows about (from the
// Claude Code / Codex / Cursor hooks) and what each is doing. Pushed whole by the host
// (ext_proto.h EXT_CMD_AGENTS), shown by the display task.

#define AGENT_BOARD_MAX 4
#define AGENT_NAME_MAX 12

typedef enum { AGENT_IDLE = 0, AGENT_WORKING, AGENT_YOUR_TURN, AGENT_ASKING, AGENT_STATE_COUNT } agent_state_t;

typedef struct {
    uint8_t source; // notify_source_t
    uint8_t state;  // agent_state_t
    char name[AGENT_NAME_MAX + 1]; // the project folder
} agent_row_t;

void agent_board_set(const agent_row_t *rows, int n); // TinyUSB task
int agent_board_get(agent_row_t *out, int max);       // display task
uint32_t agent_board_version(void);
