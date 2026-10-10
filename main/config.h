// All tunables in one place, one for one with the C object in
// prototype/index.html. Times are in 30 fps steps; field positions are Q4
// fixed point (1/16 of a field unit; one unit is a pixel at the front lip).
#pragma once

#include <stdint.h>

// --- screen
#define SCREEN_W 320
#define SCREEN_H 240
#define BAND_ROWS 40                 // 2 * 320*40*2 = 50 KB of band buffers, nothing else
#define FPS 30
#define FRAME_US (1000000 / FPS)
#define MAX_CATCHUP 3

// --- field (units; the C side keeps them as Q4)
#define Q 16
#define FW 200                       // width
#define FL 150                       // depth: back wall y=0 .. front lip y=FL
#define COIN_R 8
#define BALL_R 12
#define PRIZE_R 11
#define MAX_R BALL_R
#define PUSH_MIN 34                  // pusher plate face travels PUSH_MIN..PUSH_MAX
#define PUSH_MAX 66
#define PUSH_STEPS 78                // 2.6 s per stroke
#define WALL_END 70                  // side walls end here; past it the sides are gutters
#define PAY_W 96                     // the front lip pays in its middle; the corners are lose holes
#ifndef MAX_OBJ
#define MAX_OBJ 160                  // fixed pool; build with -DMAX_OBJ=120 if a C3 frame runs long
#endif
#define START_FIELD 96
#define START_PLATE 14
#define START_BANK 100

// --- physics
#define ITER 3
#define STIFF_PCT 55
#define STACK_POP_PCT 42
#define MASS_COIN 1
#define MASS_BALL 5
#define MASS_PRIZE 6

// --- dropping
#define DROP_STEPS 14                // 0.45 s from the slot to the plate
#define GATE_CHECK_STEPS 7           // the CHANCE gate is halfway down
#define RAPID_AFTER_STEPS 9          // hold OK 0.3 s: rapid fire
#define RAPID_HALF_STEPS 15          // ...one coin per 7.5 steps = 4/s
#define AIM_STEP (120 * Q / FPS)     // 120 units/s
#define AIM_SLOW_STEPS 5             // the first 0.15 s of a hold moves at 35 %: fine aim
#define JITTER 6
#define GATE_W 16
#define GATE_PHASE_STEP 452          // sin(1.3 t): 65536 per turn, per step at gate speed 1.0
#define EDGE_HOLD_STEPS 45           // hold into a side 1.5 s: pause
#define SHOWER_EVERY 2               // reward coins rain onto the plate, one per 2 steps
#define SHOWER_MAX 30
#define SLOT_Q 4                     // pending slot spins (保留)

// --- bonuses
#define SLOT_STEPS 60
#define WHEEL_STEPS 78
#define JPC_STEPS 96
#define BALL_BACK_WON 75             // a ball pushed off returns after 2.5 s
#define BALL_BACK_LOST 45
#define PRIZE_BACK_LOST 240
#define PRIZE_BACK_WON 180
#define JP_STEP_X10 1                // the jackpot grows 0.1 coin per coin dropped

// --- never stuck
#define REGEN_STEPS (30 * FPS)       // +1 coin per 30 s while the bank is under REGEN_CAP
#define REGEN_CAP 50
#define RESCUE 20                    // empty everything for 8 s: +20
#define RESCUE_STEPS (8 * FPS)
#define PLAY_REWARD_STEPS (300 * FPS)  // every 5 min of active play: +30
#define PLAY_REWARD 30
#define ACTIVE_STEPS (20 * FPS)      // "active" = a key within the last 20 s

// --- saving, power
#define SAVE_PROG_STEPS (10 * FPS)   // the small progress blob
#define SAVE_FIELD_STEPS (120 * FPS) // the field layout; also on pause / title / back
#define DIM_AFTER_S 30.0f
#define DIM_PCT 12
#define SCREEN_OFF_AFTER_S 180.0f

// --- small fixed tables
#define MAX_DROPS 40
#define MAX_POPUPS 4
#define MAX_BALLWAIT 6
#define NTHEMES 4
#define NPRIZE (NTHEMES * 6)

// --- screen layout
#define HUD_H 14
#define DISP_Y0 14
#define DISP_Y1 50
#define FIELD_Y0 50                  // region redrawn every frame in play
#define FIELD_Y1 208
#define FIELD_X0 16
#define FIELD_X1 304
#define FY0 78                       // back of the field on screen
#define FY1 204                      // front lip on screen

// --- colours: byte-swapped RGB565 as compile-time constants
#define RGB565_(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define PX(r, g, b) ((uint16_t)(((RGB565_(r, g, b) >> 8) | (RGB565_(r, g, b) << 8)) & 0xffff))
