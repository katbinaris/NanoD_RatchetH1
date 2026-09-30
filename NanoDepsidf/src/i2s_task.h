#pragma once
#include <stdint.h>

void i2s_task_start(void);

// Times the I2S DMA ran out of fresh samples (the task didn't write in time, so a buffer
// played twice -- an audible gap or stutter). Counts from boot. Any core.
uint32_t i2s_task_gap_count(void);
