// Host harness for Coin Carnival: runs the firmware's own field / machine /
// art / gfx code on the PC. Checks the rules, measures the long-run payout,
// estimates the per-frame cost, and renders screenshots into docs/.
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "art.h"
#include "gfx.h"
#include "machine.h"
#include "sound.h"

/* sound: silent on the host */
void sound_init(sound_mode_t m) { (void)m; }
void sound_play(sfx_t s) { (void)s; }
void sound_music(music_t m) { (void)m; }
void sound_set_hurry(bool h) { (void)h; }
void sound_set_mode(sound_mode_t m) { (void)m; }
sound_mode_t sound_mode(void) { return SOUND_ALL; }
const char *sound_mode_name(sound_mode_t m) { (void)m; return "SOUND: ON"; }

extern long g_gfx_px;
static machine_t M;
static int s_fail, s_ok;
static const char *s_out;

#define CHECK(cond, ...) do { if (cond) { s_ok++; printf("ok   "); } else { s_fail++; printf("FAIL "); } \
    printf(__VA_ARGS__); printf("\n"); } while (0)

static void steps(int n, int key)
{
    for (int i = 0; i < n; i++) {
        machine_input(&M, key, false, false);
        machine_step(&M);
    }
}

static void press_ok(void)
{
    machine_input(&M, KEY_OK, true, false);
    machine_step(&M);
    machine_input(&M, KEY_NONE, false, false);
    machine_step(&M);
}

static void calm(void)
{
    M.slot_q = 0;
    M.slot.on = M.wheel.on = M.jpc.on = false;
    M.wheel.queued = 0;
    M.shower = 0;
    M.n_drops = 0;
    M.n_ball_wait = 0;
}

static int total_coins(void)
{
    return M.bank + machine_coins_inside(&M);
}

/* ---- screenshots ---- */
static uint16_t s_fb[SCREEN_W * SCREEN_H];

static void shot(const char *name)
{
    art_view_t v = { NULL, 0, M.steps };
    art_prepare(&M);
    for (int y = 0; y < SCREEN_H; y += BAND_ROWS) {
        gfx_band_begin(y, BAND_ROWS, 0);
        art_machine(&M, &v);
        memcpy(&s_fb[y * SCREEN_W], gfx_band_buf(), SCREEN_W * BAND_ROWS * 2);
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.rgb565", s_out, name);
    FILE *f = fopen(path, "wb");
    fwrite(s_fb, 2, SCREEN_W * SCREEN_H, f);
    fclose(f);
}

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

int main(int argc, char **argv)
{
    s_out = argc > 1 ? argv[1] : ".";
    field_init_tables();
    art_init();
    rng_seed(42);
    machine_init(&M);

    /* --- a fresh machine */
    CHECK(field_count(&M.field, T_COIN) >= 75, "fresh machine: %d coins on the field", field_count(&M.field, T_COIN));
    CHECK(M.bank == START_BANK, "starts with %d virtual coins", START_BANK);
    CHECK(field_count(&M.field, T_BALL) == 2 && field_count(&M.field, T_PRIZE) == 2, "2 bonus balls and 2 prizes on the field");
    CHECK(M.st.won == 0 && M.st.lost == 0, "settling the new field pays nothing");
    shot("play");

    /* --- controls */
    int b0 = M.bank;
    press_ok();
    CHECK(M.bank == b0 - 1 && M.n_drops == 1, "tap OK drops one coin");
    steps(DROP_STEPS, KEY_NONE);
    CHECK(M.n_drops == 0, "it reaches the plate in %d ms", DROP_STEPS * 1000 / FPS);
    b0 = M.bank;
    machine_input(&M, KEY_OK, true, false);
    machine_step(&M);
    steps(89, KEY_OK);
    int fired = b0 - M.bank;
    CHECK(fired >= 10 && fired <= 12, "hold OK 3 s: rapid fire, %d coins (4/s after 0.3 s)", fired);
    steps(30, KEY_NONE);
    int a0 = M.aim;
    steps(3, KEY_UP);
    int fine = a0 - M.aim;
    steps(10, KEY_UP);
    CHECK(fine > 0 && fine * 3 < (a0 - M.aim - fine) * 1, "UP moves the dropper left; the first 0.15 s is fine aim (%d vs %d Q4)", fine, a0 - M.aim - fine);
    int held = 0;
    while (M.aim > COIN_R * Q) {
        machine_input(&M, KEY_UP, false, false);
    }
    while (!machine_input(&M, KEY_UP, false, false) && held < 200) {
        held++;
    }
    CHECK(held >= EDGE_HOLD_STEPS - 3 && held < EDGE_HOLD_STEPS, "holding into the left side %.1f s opens the pause menu", EDGE_HOLD_STEPS / (double)FPS);
    a0 = M.aim;
    machine_input(&M, KEY_DOWN, false, true);
    CHECK(M.aim < a0 || a0 <= COIN_R * Q, "swapped keys: DOWN moves left");
    machine_input(&M, KEY_NONE, false, false);

    /* --- ledger, payout, physics over a long autoplay */
    {
        rng_seed(7);
        machine_init(&M);
        M.bank = 100000;
        int start = total_coins();
        int max_n = 0;
        stats_t s0 = M.st;
        double t0 = now_us();
        int aim_t = 100;
        for (int i = 0; i < FPS * 60 * 60; i++) {   /* one hour of rapid play at random aims */
            if (i % 30 == 0) {
                aim_t = 15 + rng_range(170);
            }
            int d = aim_t * Q - M.aim;
            M.aim += d > 64 ? 64 : d < -64 ? -64 : d;
            if (i % 8 == 0) {
                machine_drop(&M);
            }
            machine_step(&M);
            if (M.field.n > max_n) {
                max_n = M.field.n;
            }
        }
        double us = (now_us() - t0) / (FPS * 60 * 60);
        stats_t s = M.st;
        double dropped = s.dropped - s0.dropped, won = s.won - s0.won, lost = s.lost - s0.lost, minted = s.minted - s0.minted;
        int end = total_coins() + (int)(s.lost - s0.lost);
        CHECK(end == start + (int)(minted + s.gifts - s0.gifts), "coin ledger balances over an hour (%d in, %d out)", start, end);
        CHECK(dropped > 13000, "one hour of autoplay: %.0f coins dropped", dropped);
        CHECK(won / dropped > 0.25 && won / dropped < 0.55, "front lip pays %.0f%% of coins dropped; gutters and corner holes take %.0f%%",
              100 * won / dropped, 100 * lost / dropped);
        double rtp = (won + minted) / dropped;
        CHECK(rtp > 0.4 && rtp < 0.6, "long-run payout incl. slot / wheel / jackpot: %.1f%% of coins dropped (target ~50%%)", 100 * rtp);
        CHECK(s.slot - s0.slot > 300 && s.wheel - s0.wheel > 30 && s.jpc - s0.jpc > 3 && s.prizes - s0.prizes > 3,
              "bonuses fire: %u slot spins, %u wheel spins (%u balls), %u jackpot chances (%u jackpots), %u prizes",
              s.slot - s0.slot, s.wheel - s0.wheel, s.balls_off - s0.balls_off, s.jpc - s0.jpc, s.jpwin - s0.jpwin, s.prizes - s0.prizes);
        CHECK(max_n <= MAX_OBJ, "pool never exceeds %d objects (peak %d)", MAX_OBJ, max_n);
        /* overlaps after a step */
        double worst = 0;
        int pairs = 0;
        for (int i = 0; i < M.field.n; i++) {
            for (int j = i + 1; j < M.field.n; j++) {
                const obj_t *a = &M.field.o[M.field.ord[i]], *b = &M.field.o[M.field.ord[j]];
                if ((a->f ^ b->f) & (F_TOP | F_STACK) || (a->f & F_STACK)) {
                    continue;
                }
                long dx = a->x - b->x, dy = a->y - b->y, rr = field_radius(a->t) + field_radius(b->t);
                if (dx * dx + dy * dy < rr * rr) {
                    double ov = (rr - __builtin_sqrt((double)(dx * dx + dy * dy))) / Q;
                    pairs += ov > 1.0;
                    if (ov > worst) {
                        worst = ov;
                    }
                }
            }
        }
        CHECK(pairs < M.field.n / 10 && worst < COIN_R, "discs stay apart: %d overlaps deeper than 1 unit among %d, deepest %.1f", pairs, M.field.n, worst);
        printf("info host physics %.1f us per step (%d objects)\n", us, M.field.n);
        printf("info payout per seed:");
        for (int sd = 1; sd <= 5; sd++) {
            rng_seed((uint32_t)sd * 977);
            machine_init(&M);
            M.bank = 100000;
            stats_t q = M.st;
            for (int i = 0; i < FPS * 60 * 20; i++) {
                if (i % 30 == 0) {
                    aim_t = 15 + rng_range(170);
                }
                int d2 = aim_t * Q - M.aim;
                M.aim += d2 > 64 ? 64 : d2 < -64 ? -64 : d2;
                if (i % 8 == 0) {
                    machine_drop(&M);
                }
                machine_step(&M);
            }
            printf(" %.0f%%", 100.0 * ((M.st.won - q.won) + (M.st.minted - q.minted)) / (M.st.dropped - q.dropped));
        }
        printf(" (20 min each)\n");
        /* aiming at the gate vs random */
        rng_seed(99);
        machine_init(&M);
        M.bank = 100000;
        stats_t q = M.st;
        for (int i = 0; i < FPS * 60 * 20; i++) {
            /* lead the gate: aim where it will be in 7 steps */
            uint16_t ph = M.gate_phase;
            M.gate_phase = (uint16_t)(ph + g_themes[M.theme].gate_phase * GATE_CHECK_STEPS);
            int target = machine_gate_x(&M);
            M.gate_phase = ph;
            int d2 = target - M.aim;
            M.aim += d2 > 64 ? 64 : d2 < -64 ? -64 : d2;
            if (i % 8 == 0) {
                machine_drop(&M);
            }
            machine_step(&M);
        }
        printf("info a skilled player who leads the CHANCE gate gets %.0f%% (%u slot spins in 20 min)\n",
               100.0 * ((M.st.won - q.won) + (M.st.minted - q.minted)) / (M.st.dropped - q.dropped), M.st.slot - q.slot);
    }

    /* --- jackpot */
    rng_seed(5);
    machine_init(&M);
    calm();
    M.jp_x10 = 4567;
    int mint0 = M.st.minted;
    M.jpc.on = true;
    M.jpc.t = 0;
    M.jpc.pocket = 3;
    steps(JPC_STEPS, KEY_NONE);
    CHECK(M.st.minted - mint0 == 456 && machine_jp(&M) == g_themes[0].jp, "jackpot pocket pays the pool (456) and resets it to %d", g_themes[0].jp);
    CHECK(M.shower <= SHOWER_MAX, "a big win rains at most %d coins onto the plate, the rest goes to the bank", SHOWER_MAX);
    M.jpc.on = true;
    M.jpc.t = 0;
    M.jpc.pocket = 0;
    calm();
    M.jpc.on = true;
    mint0 = M.st.minted;
    steps(JPC_STEPS, KEY_NONE);
    CHECK(M.st.minted - mint0 == 20, "the 20 pocket pays 20");

    /* --- never stuck */
    calm();
    {   /* nothing left to push off: an empty field */
        int cap = M.field.cap;
        field_clear(&M.field);
        M.field.cap = cap;
    }
    M.bank = 0;
    M.regen_t = 0;
    M.rescue_t = 0;
    steps(RESCUE_STEPS, KEY_NONE);
    CHECK(M.bank == RESCUE, "an empty bank with nothing in play: +%d rescue after %d s", RESCUE, RESCUE_STEPS / FPS);
    calm();
    M.bank = 10;
    M.regen_t = 0;
    steps(REGEN_STEPS * 2, KEY_NONE);
    CHECK(M.bank >= 12, "under %d coins the bank refills 1 per %d s", REGEN_CAP, REGEN_STEPS / FPS);
    calm();
    M.bank = 500;
    M.play_t = 0;
    uint32_t pr = M.st.play_rewards;
    for (int i = 0; i < PLAY_REWARD_STEPS + 30; i++) {
        machine_input(&M, i % 150 == 0 ? KEY_DOWN : KEY_NONE, false, false);
        machine_step(&M);
    }
    CHECK(M.st.play_rewards == pr + 1, "every %d min of active play gives +%d coins", PLAY_REWARD_STEPS / FPS / 60, PLAY_REWARD);
    M.play_t = 0;
    steps(PLAY_REWARD_STEPS + 30 * 30, KEY_NONE);
    CHECK(M.st.play_rewards == pr + 1, "idle time does not count (no key for %d s)", ACTIVE_STEPS / FPS);

    /* --- prizes, sets, themes */
    rng_seed(11);
    machine_init(&M);
    for (int i = 0; i < 5; i++) {
        machine_collect(&M, i);
    }
    int t0 = M.tickets;
    machine_collect(&M, 5);
    CHECK(M.tickets - t0 == 105 && (M.sets & 1), "a prize is +5 tickets; the 6th of a set adds +100");
    machine_collect(&M, 5);
    CHECK(M.tickets - t0 == 110, "a set pays once");
    machine_set_theme(&M, 1);
    steps(60, KEY_NONE);
    int balls = field_count(&M.field, T_BALL) + M.n_ball_wait;
    for (int i = 0; i < M.n_drops; i++) {
        balls += M.drops[i].t == T_BALL;
    }
    CHECK(balls == 3 && M.jp_x10 >= 3000, "CANDY FACTORY runs 3 bonus balls");

    /* --- save / load */
    rng_seed(3);
    machine_init(&M);
    M.bank = 1234;
    M.tickets = 77;
    M.coll[8] = 2;
    steps(300, KEY_NONE);
    static prog_blob_t pb;
    static field_blob_t fb;
    machine_save_prog(&M, &pb);
    machine_save_field(&M, &fb);
    int coins = field_count(&M.field, T_COIN), bank = pb.bank, n = M.field.n;
    static machine_t M2;
    memset(&M2, 0, sizeof(M2));
    machine_init(&M2);
    CHECK(machine_load_prog(&M2, &pb) && machine_load_field(&M2, &fb), "progress and field load back");
    CHECK(M2.bank == bank && M2.tickets == 77 && M2.coll[8] == 2 && M2.field.n == n && field_count(&M2.field, T_COIN) == coins,
          "after a reboot: same bank, tickets, prizes and %d objects in place", n);
    size_t fsz = offsetof(field_blob_t, o) + (size_t)fb.n * sizeof(fb.o[0]);
    CHECK(sizeof(prog_blob_t) < 128 && fsz <= 8 + MAX_OBJ * 6, "NVS: prog %zu B + field %zu B (namespace 'pusher')", sizeof(prog_blob_t), fsz);
    pb.ver = 99;
    CHECK(!machine_load_prog(&M2, &pb), "a blob from another version is ignored");

    /* --- fallback cap */
    rng_seed(4);
    machine_init(&M);
    M.field.cap = 120;
    M.bank = 5000;
    for (int i = 0; i < FPS * 120; i++) {
        if (i % 4 == 0) {
            machine_drop(&M);
        }
        machine_step(&M);
    }
    CHECK(M.field.n <= 120, "coin pool fallback: capped at 120 objects holds (%d)", M.field.n);

    /* --- cost per frame: pixels written for the field window */
    rng_seed(8);
    machine_init(&M);
    M.bank = 5000;
    for (int i = 0; i < 600; i++) {
        if (i % 8 == 0) {
            machine_drop(&M);
        }
        machine_step(&M);
    }
    art_view_t v = { NULL, 0, 0 };
    art_prepare(&M);
    g_gfx_px = 0;
    int w = FIELD_X1 - FIELD_X0, rows_max = SCREEN_W * BAND_ROWS / w, bands = 0;
    for (int y = FIELD_Y0; y < FIELD_Y1; y += rows_max) {
        int rows = FIELD_Y1 - y < rows_max ? FIELD_Y1 - y : rows_max;
        gfx_band_begin_win(FIELD_X0, w, y, rows, 0);
        art_machine(&M, &v);
        bands++;
    }
    long field_bytes = (long)w * (FIELD_Y1 - FIELD_Y0) * 2;
    printf("info play frame: field window %dx%d in %d bands = %ld KB over SPI (%.1f ms at 40 MHz), %ld px written (%d objects)\n",
           w, FIELD_Y1 - FIELD_Y0, bands, field_bytes / 1024, field_bytes * 8 / 40e3, g_gfx_px, M.field.n);
    printf("info full screen would be %d KB (%.1f ms); the HUD/display strip (%d KB) and tray (%d KB) go only when they change\n",
           SCREEN_W * SCREEN_H * 2 / 1024, SCREEN_W * SCREEN_H * 16 / 40e3, SCREEN_W * FIELD_Y0 * 2 / 1024, SCREEN_W * (SCREEN_H - FIELD_Y1) * 2 / 1024);
    CHECK(field_bytes * 8 / 40e3 < 20.0, "per-frame SPI time leaves room for 30 fps");
    printf("info RAM: machine_t %zu B, field blob %zu B, band buffers %d B\n", sizeof(machine_t), sizeof(field_blob_t),
           2 * SCREEN_W * BAND_ROWS * 2);

    /* --- screenshots of the bonuses and themes */
    rng_seed(21);
    machine_init(&M);
    M.bank = 500;
    steps(40, KEY_NONE);
    M.slot_q = 2;
    steps(20, KEY_NONE);
    shot("slot");
    calm();
    M.wheel.on = true;
    M.wheel.t = 40;
    M.wheel.seg = 3;
    M.wheel.from = 0;
    machine_popup(&M, "STAR: SPIN THE WHEEL!", PX(0x7f, 0xd8, 0xff));
    shot("wheel");
    M.wheel.on = false;
    M.jpc.on = true;
    M.jpc.t = 50;
    M.jpc.pocket = 3;
    M.jp_x10 = 3456;
    shot("jackpot");
    M.jpc.on = false;
    for (int t = 1; t < NTHEMES; t++) {
        M.owned |= 1 << t;
        machine_set_theme(&M, t);
        machine_new_field(&M);
        steps(60, KEY_NONE);
        char nm[16];
        snprintf(nm, sizeof(nm), "theme%d", t);
        shot(nm);
    }

    printf(s_fail ? "%d checks, %d FAILED\n" : "%d checks, all passed\n", s_ok + s_fail, s_fail);
    return s_fail ? 1 : 0;
}
