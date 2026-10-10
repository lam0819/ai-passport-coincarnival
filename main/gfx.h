// Band drawing: every draw call clips to the band (a window of the screen)
// being composed.
//
// Two band buffers take turns: one is composed while the panel's DMA is still
// sending the other, so a frame is never drawn into memory that is in flight.
// A band can be narrower than the screen (gfx_band_begin_win): Coin Carnival
// redraws only the machine's field each frame and sends just that window.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "assets.h"
#include "config.h"

// Both byte-swapped for the panel.
uint16_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b);
uint16_t gfx_swap(uint16_t natural_rgb565);

// A full-width band at screen row y0, filled with bg.
void gfx_band_begin(int y0, int rows, uint16_t bg);
// A window x0..x0+w of rows y0..y0+rows (w * rows <= SCREEN_W * BAND_ROWS).
void gfx_band_begin_win(int x0, int w, int y0, int rows, uint16_t bg);
const uint16_t *gfx_band_buf(void);
int gfx_band_x0(void);
int gfx_band_w(void);
int gfx_band_y0(void);
int gfx_band_rows(void);

void gfx_fill(int x, int y, int w, int h, uint16_t color);
// A horizontal run x0..x1-1 on row y.
void gfx_hline(int x0, int x1, int y, uint16_t color);
uint16_t gfx_mix(uint16_t base, uint16_t color, int alpha);
// Mixes color over what is there: alpha 0 (none) .. 256 (solid).
void gfx_blend(int x, int y, int w, int h, uint16_t color, int alpha);
// Filled ellipse centred on (cx, cy); radii up to GFX_MAX_RX.
#define GFX_MAX_RX 24
void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t color);
void gfx_text(const char *text, int x, int y, int scale, uint16_t color);
int gfx_text_w(const char *text, int scale);
void gfx_init(void);
