// Everything Coin Carnival draws, from rectangles, spans and ellipses: no
// bitmaps. art_machine() paints the whole machine; the band window decides
// which part ends up in the buffer.
#pragma once

#include "machine.h"

typedef struct {
    const char *msg;     // the tray's message line, or NULL for the counters
    int edge_pct;        // 0..100: holding into a side (pause)
    uint32_t frame;
} art_view_t;

void art_init(void);
void art_prepare(const machine_t *m);           // once per frame: draw order
void art_machine(const machine_t *m, const art_view_t *v);
int art_lamp_phase(uint32_t frame);              // the display lamps step at 4 Hz
void art_symbol(int sym, int cx, int cy);
void art_prize(int pid, int cx, int cy, int r, bool known);
void art_coin_icon(int cx, int cy);
