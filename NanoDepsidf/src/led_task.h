#pragma once
#include <stdint.h>

void led_task_start(void);

// What the LEDs show right now, for the companion app: ring positions 0-59 clockwise from
// 12 o'clock in the screen's frame (the ring's own index order and screen rotation undone),
// then the keys' LEDs, F1's two first. RGB as sent to the strips (power-limited, dim).
#define LED_VIEW_COUNT 68
void led_task_snapshot(uint8_t out[LED_VIEW_COUNT][3]);
