#include "app_colors.h"
#include <stddef.h>

#define ICON 48
#define AMBER_RGB 0xFFC94Du

static float luma(uint32_t c) {
    return 0.30f * ((c >> 16) & 0xFF) + 0.59f * ((c >> 8) & 0xFF) + 0.11f * (c & 0xFF);
}

// The icon's three most common colours, dark to bright, exactly as sampled. Pixels are binned
// coarsely (3 bits per channel) so the shading of one colour counts as one; near-black
// (outlines) is skipped.
void app_accents(const uint8_t *icon, const uint32_t *heat, uint32_t out[3]) {
    if (heat && (heat[0] | heat[1] | heat[2])) {
        for (int i = 0; i < 3; i++) out[i] = heat[i];
        return;
    }
    for (int i = 0; i < 3; i++) out[i] = AMBER_RGB;
    if (icon == NULL) return;
    struct Bin { uint16_t key; uint16_t n; uint32_t r, g, b; } bins[32]; // pixel art has few colours
    int nb = 0;
    for (int i = 0; i < ICON * ICON; i++) {
        uint16_t v = (uint16_t)(icon[i * 2] << 8 | icon[i * 2 + 1]);
        uint32_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
        if (r + g + b < 90) continue; // black / outline
        uint16_t key = (uint16_t)((r >> 5) << 6 | (g >> 5) << 3 | (b >> 5));
        int k = 0;
        while (k < nb && bins[k].key != key) k++;
        if (k == nb) {
            if (nb == 32) continue; // overflow pixels are simply not counted
            bins[nb].key = key; bins[nb].n = 0; bins[nb].r = bins[nb].g = bins[nb].b = 0;
            nb++;
        }
        bins[k].n++; bins[k].r += r; bins[k].g += g; bins[k].b += b;
    }
    uint32_t pick[3];
    int np = 0;
    for (; np < 3; np++) {
        int best = -1;
        for (int k = 0; k < nb; k++)
            if (bins[k].n && (best < 0 || bins[k].n > bins[best].n)) best = k;
        if (best < 0) break;
        pick[np] = (bins[best].r / bins[best].n) << 16 | (bins[best].g / bins[best].n) << 8 | (bins[best].b / bins[best].n);
        bins[best].n = 0;
    }
    if (np == 0) return;
    for (; np < 3; np++) pick[np] = pick[np - 1]; // fewer colours: repeat the last
    for (int i = 1; i < 3; i++)
        for (int j = i; j > 0 && luma(pick[j]) < luma(pick[j - 1]); j--) { uint32_t t = pick[j]; pick[j] = pick[j - 1]; pick[j - 1] = t; }
    for (int i = 0; i < 3; i++) out[i] = pick[i];
}
