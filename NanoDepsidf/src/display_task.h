#pragma once
// Defined in display_task.cpp (C++, LovyanGFX), called from plain-C main.c.
#ifdef __cplusplus
extern "C" {
#endif

// The 115 KB frame, from internal RAM while there's still a block that size: call before the
// USB and WiFi tasks start. display_task_start() allocates it itself if this wasn't called.
void display_frame_reserve(void);
void display_task_start(void);

#ifdef __cplusplus
}
#endif
