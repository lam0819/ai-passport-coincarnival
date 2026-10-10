// The machine's rules: dropping, the CHANCE gate, slot, wheel, jackpot,
// prizes, payouts and the never-stuck refills. Pure logic (host-testable),
// the same rules as updatePlay() in the prototype.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "field.h"

typedef struct {
    const char *en;
    uint16_t cost;            // tickets
    uint8_t balls;            // bonus balls in the machine
    uint8_t gate_pct;         // CHANCE gate width, % of GATE_W
    uint16_t gate_phase;      // gate speed
    uint16_t jp;              // jackpot base
    uint16_t sky, wall, field, plate, accent, lamp;
    struct { const char *en; uint16_t col; } prizes[6];
} theme_t;
extern const theme_t g_themes[NTHEMES];

enum { SYM_CHERRY, SYM_BELL, SYM_STAR, SYM_SEVEN, SYM_BAG, NSYM };
typedef struct { uint8_t coins, tickets, wheel, jpc; } reward_t;
#define NWHEEL 12
extern const reward_t g_wheel[NWHEEL];
#define NPOCKET 4
extern const struct pocket { uint8_t coins, jp, w; } g_pockets[NPOCKET];

typedef struct { int16_t x; uint8_t age, t, pid, checked; } drop_t;
typedef struct { char text[32]; uint16_t col; uint8_t t; } popup_t;

typedef struct {
    uint32_t dropped, won, lost, minted, gifts, play_rewards, slot, wheel, jpc, jpwin, prizes,
        balls_off, overflow;
} stats_t;

typedef struct {
    /* progress (saved in the 'prog' blob) */
    int32_t bank, tickets, jp_x10;
    uint8_t theme, owned, sets;
    uint8_t coll[NPRIZE];
    uint16_t regen_t, play_t;
    stats_t st;
    /* the machine (saved in the 'field' blob) */
    field_t field;
    /* live only */
    int aim;                    // Q4
    uint16_t gate_phase;
    drop_t drops[MAX_DROPS];
    int n_drops;
    int shower, shower_t;
    int slot_q;
    struct { bool on; uint8_t t, reels[3]; reward_t r; } slot;
    struct { bool on; uint8_t t, seg, queued, from; } wheel;
    struct { bool on; uint8_t t, pocket; } jpc;
    uint8_t ball_wait[MAX_BALLWAIT];
    int n_ball_wait;
    uint16_t prize_wait;
    uint16_t rescue_t, idle_t;
    popup_t pop[MAX_POPUPS];
    uint8_t tray_flash;
    uint8_t last_wheel_seg;     // for the display between spins
    uint8_t last_reels[3];
    uint32_t steps;
    bool settling;
    /* controls */
    uint8_t hold_t, ok_t, rapid_acc, edge_t;
    uint8_t nocoin_t;           // "NO COINS" shown in the tray
} machine_t;

enum { KEY_NONE, KEY_UP, KEY_DOWN, KEY_OK };

void machine_init(machine_t *m);           // fresh progress + a fresh field
void machine_new_field(machine_t *m);
void machine_step(machine_t *m);           // one 30 fps step of play
bool machine_drop(machine_t *m);           // one coin from the bank at the aim
// One step of play controls. key: the one key held now (the ladder reads one);
// ok_edge: OK went down this step. UP/DOWN aim (slow at first for fine aim),
// OK drops, held OK rapid-fires; returns true when a side has been held
// for 1.5 s (open the pause menu).
bool machine_input(machine_t *m, int key, bool ok_edge, bool swap);
int machine_gate_x(const machine_t *m);    // Q4
int machine_gate_w(const machine_t *m);    // Q4
int machine_coins_inside(const machine_t *m);
void machine_collect(machine_t *m, int pid);
void machine_mint(machine_t *m, int n);
void machine_set_theme(machine_t *m, int t);   // also keeps the balls right
int machine_jp(const machine_t *m);
void machine_popup(machine_t *m, const char *text, uint16_t col);

/* NVS blobs: small and versioned. */
#define PROG_VER 1
typedef struct {
    uint8_t ver, theme, owned, sets;
    int32_t bank, tickets, jp_x10;
    uint16_t regen_t, play_t;
    uint8_t coll[NPRIZE];
    uint8_t sound, swap, pad[2];
    uint32_t dropped, won, minted;
} prog_blob_t;
#define FIELD_VER 1
typedef struct {
    uint8_t ver, n, nballs, pad;
    uint16_t push_step, prize_wait;
    struct { int16_t x, y; uint8_t tf, pid; } o[MAX_OBJ];
} field_blob_t;
void machine_save_prog(const machine_t *m, prog_blob_t *b);
bool machine_load_prog(machine_t *m, const prog_blob_t *b);
void machine_save_field(const machine_t *m, field_blob_t *b);
bool machine_load_field(machine_t *m, const field_blob_t *b);
