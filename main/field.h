// The coin field: a fixed pool of discs pushed by the plate, in Q4 fixed
// point. No floats, no malloc. Same algorithm as physics() in the prototype:
// position-based, a few relaxation passes of disc-disc overlap per step.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

enum { T_FREE = 0, T_COIN, T_BALL, T_PRIZE };
#define F_TOP 1      // on top of the pusher plate
#define F_STACK 2    // riding on another coin (second layer)
#define NO_SUP 0xff

typedef struct {
    int16_t x, y;      // Q4
    int16_t px, py;    // last step, for riding coins
    uint8_t t, f, sup, pid;
} obj_t;

typedef struct {
    obj_t o[MAX_OBJ];
    uint8_t ord[MAX_OBJ];   // live slots, kept sorted by x between steps
    int n;                  // live count
    int push_y;             // plate face, Q4
    uint16_t push_step;
    int cap;                // pool limit in use: MAX_OBJ, or less if frames run long
} field_t;

// Called for each disc leaving the field: paid (front lip, middle) or not.
typedef void (*field_out_fn)(const obj_t *o, bool paid, void *ctx);

void field_init_tables(void);
void field_clear(field_t *f);
obj_t *field_add(field_t *f, int t, int x, int y, bool top);   // Q4; NULL when full
void field_step(field_t *f, field_out_fn out, void *ctx);
int field_count(const field_t *f, int t);
int field_radius(int t);   // Q4
int field_push_at(int step);   // Q4 plate face at a step of the stroke
// One shared xorshift32, the same generator as the prototype's.
void rng_seed(uint32_t s);
uint32_t rng_next(void);
int rng_range(int n);   // 0..n-1
int16_t isin16(uint16_t phase);   // Q14
