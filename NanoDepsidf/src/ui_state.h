#pragma once

#include <stdint.h>

// Cross-core UI state for the Main Screen's live detent readout: control_task.c (Core 0)
// is the single producer (owns knob/button reads per the architecture log), display_task.c
// (Core 1) is the single consumer, polling this on its own redraw timer. Plain atomic --
// the display only ever wants the *latest* value each time it redraws, never a history of
// every intermediate change, so there's nothing to lose by overwriting.
//
// Phase 8: the mock menu's active/selection fields this file used to carry are gone -- the
// real menu (`menu.c`) owns its own state and exposes it via menu_get_render_snapshot()
// instead, mutex-protected rather than atomics (a menu row is formatted text, not a scalar).

void ui_state_init(void);

// Producer side (Core 0)
void ui_state_set_detent(int32_t wrapped_index); // 0..(num_detents-1) -- already wrapped
                                                  // by the caller, this module doesn't
                                                  // know num_detents

// Consumer side (Core 1)
int32_t ui_state_get_detent(void);
