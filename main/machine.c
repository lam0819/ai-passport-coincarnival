#include "machine.h"

#include <stdio.h>
#include <string.h>

#include "sound.h"

#define C3(r, g, b) PX(r, g, b)
const theme_t g_themes[NTHEMES] = {
    { "HARBOUR NIGHT", 0, 2, 100, GATE_PHASE_STEP, 300,
      C3(0x0f, 0x1a, 0x33), C3(0x24, 0x34, 0x5e), C3(0x24, 0x46, 0x7a), C3(0x8c, 0x96, 0xad), C3(0xff, 0xcc, 0x33), C3(0xff, 0x5a, 0x7a),
      { { "FISHBALLS", C3(0xe8, 0xa2, 0x3c) }, { "EGG WAFFLE", C3(0xf2, 0xd1, 0x6b) }, { "STRIPED BAG", C3(0xd2, 0x3b, 0x3b) },
        { "TRAM", C3(0x2f, 0x9e, 0x4f) }, { "FERRY", C3(0x3c, 0x7f, 0xd2) }, { "NEON SIGN", C3(0xff, 0x4f, 0xd8) } } },
    { "CANDY FACTORY", 150, 3, 100, GATE_PHASE_STEP, 300,
      C3(0x3a, 0x18, 0x38), C3(0x6b, 0x2a, 0x5e), C3(0xa2, 0x4d, 0x84), C3(0xf3, 0xc1, 0xd8), C3(0xff, 0xe0, 0x66), C3(0x6c, 0xf0, 0xff),
      { { "LOLLIPOP", C3(0xff, 0x6f, 0x91) }, { "MARSHMALLOW", C3(0xff, 0xf0, 0xf5) }, { "CHOCOLATE", C3(0x7b, 0x4a, 0x2e) },
        { "JELLY", C3(0x7c, 0xdb, 0x5a) }, { "MINT", C3(0x7f, 0xe3, 0xd3) }, { "GOLD CHOC", C3(0xff, 0xd2, 0x3a) } } },
    { "SPACE STATION", 400, 2, 150, GATE_PHASE_STEP * 17 / 10, 400,
      C3(0x05, 0x06, 0x0f), C3(0x1d, 0x24, 0x40), C3(0x2c, 0x35, 0x66), C3(0xa8, 0xb3, 0xd6), C3(0x7f, 0xd8, 0xff), C3(0xb3, 0xff, 0x5a),
      { { "ROCKET", C3(0xe8, 0xe8, 0xf0) }, { "SATELLITE", C3(0xc0, 0xc8, 0xd8) }, { "ASTRONAUT", C3(0xff, 0xff, 0xff) },
        { "ALIEN", C3(0x79, 0xe0, 0x6b) }, { "MOON ROCK", C3(0xb0, 0xa8, 0x90) }, { "RINGED PLANET", C3(0xe8, 0xb8, 0x60) } } },
    { "DRAGON PALACE", 900, 2, 100, GATE_PHASE_STEP * 12 / 10, 600,
      C3(0x03, 0x20, 0x2a), C3(0x0d, 0x4a, 0x52), C3(0x12, 0x70, 0x7a), C3(0xd9, 0xc3, 0x8a), C3(0xff, 0xd2, 0x3a), C3(0xff, 0x6b, 0x3d),
      { { "PEARL", C3(0xf4, 0xf0, 0xe8) }, { "CORAL", C3(0xff, 0x7b, 0x6b) }, { "JADE CHARM", C3(0x5a, 0xd1, 0x9a) },
        { "GOLDFISH", C3(0xff, 0x9a, 0x2e) }, { "DRAGON ORB", C3(0xff, 0x40, 0x40) }, { "TREASURE BOX", C3(0xc8, 0x96, 0x2e) } } },
};

/* slot: one weighted roll decides the spin (weights x2 of the prototype's) */
static const struct { uint8_t w; int8_t sym; reward_t r; } SLOT_TABLE[] = {
    { 30, SYM_CHERRY, { 2, 0, 0, 0 } }, { 14, SYM_BELL, { 5, 0, 0, 0 } }, { 8, SYM_STAR, { 0, 0, 1, 0 } },
    { 3, SYM_SEVEN, { 0, 0, 0, 1 } }, { 12, SYM_BAG, { 0, 10, 0, 0 } }, { 133, -1, { 0, 0, 0, 0 } },
};
const reward_t g_wheel[NWHEEL] = {
    { 3, 0, 0, 0 }, { 0, 5, 0, 0 }, { 4, 0, 0, 0 }, { 0, 10, 0, 0 }, { 2, 0, 0, 0 }, { 0, 15, 0, 0 },
    { 6, 0, 0, 0 }, { 0, 5, 0, 0 }, { 4, 0, 0, 0 }, { 0, 20, 0, 0 }, { 10, 0, 0, 0 }, { 0, 5, 0, 0 },
};
const struct pocket g_pockets[NPOCKET] = { { 20, 0, 45 }, { 30, 0, 30 }, { 50, 0, 18 }, { 0, 1, 7 } };

#define GOLD PX(0xff, 0xd2, 0x3a)
#define PINK PX(0xff, 0x9a, 0xd5)
#define SKY PX(0x7f, 0xd8, 0xff)
#define RED PX(0xff, 0x40, 0x40)

void machine_popup(machine_t *m, const char *text, uint16_t col)
{
    if (m->settling) {
        return;
    }
    memmove(&m->pop[0], &m->pop[1], sizeof(popup_t) * (MAX_POPUPS - 1));
    popup_t *p = &m->pop[MAX_POPUPS - 1];
    snprintf(p->text, sizeof(p->text), "%s", text);
    p->col = col;
    p->t = 36;   /* 1.2 s */
}

int machine_jp(const machine_t *m)
{
    return m->jp_x10 / 10;
}

int machine_gate_x(const machine_t *m)
{
    return FW * Q / 2 + (int)(((int32_t)(FW / 2 - 18) * Q * isin16(m->gate_phase)) >> 14);
}

int machine_gate_w(const machine_t *m)
{
    return GATE_W * Q * g_themes[m->theme].gate_pct / 100;
}

int machine_coins_inside(const machine_t *m)
{
    int n = field_count(&m->field, T_COIN) + m->shower;
    for (int i = 0; i < m->n_drops; i++) {
        n += m->drops[i].t == T_COIN;
    }
    return n;
}

static void push_drop(machine_t *m, int t, int x, int age, int pid, bool checked)
{
    if (m->n_drops >= MAX_DROPS) {
        if (t == T_COIN) {
            m->bank++;
            m->st.overflow++;
        } else if (t == T_BALL && m->n_ball_wait < MAX_BALLWAIT) {
            m->ball_wait[m->n_ball_wait++] = 30;
        } else if (t == T_PRIZE) {
            m->prize_wait = 90;
        }
        return;
    }
    drop_t *d = &m->drops[m->n_drops++];
    d->x = (int16_t)x;
    d->age = (uint8_t)age;
    d->t = (uint8_t)t;
    d->pid = (uint8_t)pid;
    d->checked = checked;
}

static void ball_back(machine_t *m, int steps)
{
    if (m->n_ball_wait < MAX_BALLWAIT) {
        m->ball_wait[m->n_ball_wait++] = (uint8_t)steps;
    }
}

static void spawn_prize(machine_t *m, int x, int y)
{
    int pid = m->theme * 6 + rng_range(6);
    if (y >= 0) {
        obj_t *o = field_add(&m->field, T_PRIZE, x, y, false);
        if (o) {
            o->pid = (uint8_t)pid;
            return;
        }
    }
    /* from the hopper onto the plate */
    push_drop(m, T_PRIZE, COIN_R * Q + rng_range((FW - 2 * COIN_R) * Q), 0, pid, true);
}

void machine_mint(machine_t *m, int n)
{
    /* reward coins: a shower onto the plate, the rest straight to the bank */
    m->st.minted += n;
    int room = m->field.cap - 20 - m->field.n - m->n_drops - m->shower;
    if (room < 0) {
        room = 0;
    }
    int to_field = n < SHOWER_MAX ? n : SHOWER_MAX;
    if (to_field > room) {
        to_field = room;
    }
    m->shower += to_field;
    m->bank += n - to_field;
    m->st.overflow += n - to_field;
}

void machine_collect(machine_t *m, int pid)
{
    if (m->coll[pid] < 255) {
        m->coll[pid]++;
    }
    m->st.prizes++;
    m->tickets += 5;
    const theme_t *th = &g_themes[pid / 6];
    char buf[32];
    snprintf(buf, sizeof(buf), "%s +5T", th->prizes[pid % 6].en);
    machine_popup(m, buf, th->prizes[pid % 6].col);
    sound_play(SFX_PRIZE);
    int t = pid / 6;
    bool all = true;
    for (int i = 0; i < 6; i++) {
        all = all && m->coll[t * 6 + i] > 0;
    }
    if (all && !(m->sets & (1 << t))) {
        m->sets |= (uint8_t)(1 << t);
        m->tickets += 100;
        machine_popup(m, "SET COMPLETE +100T", GOLD);
    }
    if (m->prize_wait < PRIZE_BACK_WON) {
        m->prize_wait = PRIZE_BACK_WON;
    }
}

static void start_wheel(machine_t *m)
{
    m->st.wheel++;
    if (m->wheel.on) {
        m->wheel.queued++;
        return;
    }
    m->wheel.on = true;
    m->wheel.t = 0;
    m->wheel.from = m->last_wheel_seg;
    m->wheel.seg = (uint8_t)rng_range(NWHEEL);
}

static void start_jpc(machine_t *m)
{
    if (m->jpc.on) {
        return;
    }
    int tot = 0;
    for (int i = 0; i < NPOCKET; i++) {
        tot += g_pockets[i].w;
    }
    int r = rng_range(tot), p = 0;
    while (r >= g_pockets[p].w) {
        r -= g_pockets[p++].w;
    }
    m->jpc.on = true;
    m->jpc.t = 0;
    m->jpc.pocket = (uint8_t)p;
    m->st.jpc++;
    sound_play(SFX_JPC);
}

static void award(machine_t *m, const reward_t *r, const char *src)
{
    char buf[32];
    if (r->coins) {
        machine_mint(m, r->coins);
        snprintf(buf, sizeof(buf), "%s +%d COINS", src, r->coins);
        machine_popup(m, buf, GOLD);
        sound_play(SFX_WIN);
    }
    if (r->tickets) {
        m->tickets += r->tickets;
        snprintf(buf, sizeof(buf), "%s +%d TICKETS", src, r->tickets);
        machine_popup(m, buf, PINK);
        sound_play(SFX_WIN);
    }
    if (r->wheel) {
        machine_popup(m, "STAR: SPIN THE WHEEL!", SKY);
        start_wheel(m);
    }
    if (r->jpc) {
        start_jpc(m);
    }
}

static void start_slot(machine_t *m)
{
    int tot = 0, n = (int)(sizeof(SLOT_TABLE) / sizeof(SLOT_TABLE[0]));
    for (int i = 0; i < n; i++) {
        tot += SLOT_TABLE[i].w;
    }
    int r = rng_range(tot), e = 0;
    while (r >= SLOT_TABLE[e].w) {
        r -= SLOT_TABLE[e++].w;
    }
    if (SLOT_TABLE[e].sym >= 0) {
        m->slot.reels[0] = m->slot.reels[1] = m->slot.reels[2] = (uint8_t)SLOT_TABLE[e].sym;
    } else {
        for (int i = 0; i < 3; i++) {
            m->slot.reels[i] = (uint8_t)rng_range(NSYM);
        }
        if (m->slot.reels[0] == m->slot.reels[1] && m->slot.reels[1] == m->slot.reels[2]) {
            m->slot.reels[2] = (uint8_t)((m->slot.reels[2] + 1) % NSYM);
        }
    }
    m->slot.r = SLOT_TABLE[e].r;
    m->slot.on = true;
    m->slot.t = 0;
    m->st.slot++;
    sound_play(SFX_REEL);
}

/* a disc left the field */
static void on_out(const obj_t *o, bool paid, void *ctx)
{
    machine_t *m = ctx;
    if (m->settling) {
        return;
    }
    if (paid) {
        if (o->t == T_COIN) {
            m->bank++;
            m->st.won++;
            m->tray_flash = 9;
            sound_play(SFX_CLINK);
        } else if (o->t == T_BALL) {
            m->st.balls_off++;
            start_wheel(m);
            ball_back(m, BALL_BACK_WON);
        } else {
            machine_collect(m, o->pid);
        }
    } else {
        if (o->t == T_COIN) {
            m->st.lost++;
        } else if (o->t == T_BALL) {
            ball_back(m, BALL_BACK_LOST);   /* balls run in a closed loop */
        } else if (m->prize_wait < PRIZE_BACK_LOST) {
            m->prize_wait = PRIZE_BACK_LOST;
        }
    }
}

static void land(machine_t *m, const drop_t *d)
{
    int r = COIN_R * Q, x = d->x;
    if (x < r) {
        x = r;
    }
    if (x > FW * Q - r) {
        x = FW * Q - r;
    }
    int span = m->field.push_y - 2 * r;
    int y = d->t == T_COIN ? r + rng_range(span > Q ? span : Q) : BALL_R * Q;   /* balls and prizes enter at the back */
    obj_t *o = field_add(&m->field, d->t, x, y, true);
    if (!o) {   /* pool full: a coin goes back to the bank, a ball or prize waits */
        if (d->t == T_COIN) {
            m->bank++;
            m->st.overflow++;
        } else if (d->t == T_BALL) {
            ball_back(m, 30);
        } else if (m->prize_wait < 90) {
            m->prize_wait = 90;
        }
        return;
    }
    o->pid = d->pid;
    if (d->t == T_COIN && !d->checked) {
        sound_play(SFX_LAND);
    }
}

bool machine_drop(machine_t *m)
{
    if (m->bank <= 0) {
        m->nocoin_t = 30;
        return false;
    }
    m->bank--;
    m->st.dropped++;
    m->jp_x10 += JP_STEP_X10;
    push_drop(m, T_COIN, m->aim + rng_range(2 * JITTER * Q + 1) - JITTER * Q, 0, 0, false);
    sound_play(SFX_DROP);
    return true;
}

bool machine_input(machine_t *m, int key, bool ok_edge, bool swap)
{
    m->idle_t = key ? 0 : (uint16_t)(m->idle_t < 60000 ? m->idle_t + 1 : 60000);
    bool pause = false;
    if (key == KEY_UP || key == KEY_DOWN) {
        if (m->hold_t < 255) {
            m->hold_t++;
        }
        int dir = key == KEY_UP ? -1 : 1;
        if (swap) {
            dir = -dir;
        }
        int sp = m->hold_t <= AIM_SLOW_STEPS ? AIM_STEP * 35 / 100 : AIM_STEP;
        m->aim += dir * sp;
        int lo = COIN_R * Q, hi = (FW - COIN_R) * Q;
        bool lim = m->aim <= lo || m->aim >= hi;
        m->aim = m->aim < lo ? lo : m->aim > hi ? hi : m->aim;
        if (lim) {
            if (++m->edge_t >= EDGE_HOLD_STEPS) {
                m->edge_t = 0;
                pause = true;
            }
        } else {
            m->edge_t = 0;
        }
    } else {
        m->hold_t = 0;
        m->edge_t = 0;
    }
    if (key == KEY_OK) {
        if (ok_edge) {
            machine_drop(m);
        }
        if (m->ok_t < 255) {
            m->ok_t++;
        }
        if (m->ok_t > RAPID_AFTER_STEPS) {
            m->rapid_acc += 2;
            while (m->rapid_acc >= RAPID_HALF_STEPS) {
                m->rapid_acc -= RAPID_HALF_STEPS;
                machine_drop(m);
            }
        }
    } else {
        m->ok_t = 0;
        m->rapid_acc = 0;
    }
    if (m->nocoin_t) {
        m->nocoin_t--;
    }
    return pause;
}

static int balls_total(const machine_t *m)
{
    int n = field_count(&m->field, T_BALL) + m->n_ball_wait;
    for (int i = 0; i < m->n_drops; i++) {
        n += m->drops[i].t == T_BALL;
    }
    return n;
}

void machine_set_theme(machine_t *m, int t)
{
    m->theme = (uint8_t)t;
    int base = g_themes[t].jp * 10;
    if (m->jp_x10 < base) {
        m->jp_x10 = base;
    }
    for (int have = balls_total(m); have < g_themes[t].balls; have++) {
        ball_back(m, 15);
    }
}

void machine_new_field(machine_t *m)
{
    field_clear(&m->field);
    m->n_drops = 0;
    m->shower = m->shower_t = 0;
    m->n_ball_wait = 0;
    m->slot_q = 0;
    m->slot.on = m->wheel.on = m->jpc.on = false;
    m->prize_wait = 0;
    /* hex-packed rows in front of the plate, a few gaps, a handful on the plate */
    int n = 0;
    for (int row = 0; n < START_FIELD; row++) {
        int y = (PUSH_MIN + COIN_R) * Q + row * COIN_R * Q * 175 / 100;
        if (y > FL * Q - COIN_R * Q * 12 / 10) {
            break;
        }
        for (int x = COIN_R * Q + (row % 2) * COIN_R * Q; x <= (FW - COIN_R) * Q && n < START_FIELD;
             x += COIN_R * Q * 205 / 100) {
            if (rng_range(100) < 12) {
                continue;
            }
            field_add(&m->field, T_COIN, x + rng_range(Q) - Q / 2, y + rng_range(Q) - Q / 2, false);
            n++;
        }
    }
    for (int i = 0; i < START_PLATE; i++) {
        field_add(&m->field, T_COIN, COIN_R * Q + rng_range((FW - 2 * COIN_R) * Q),
                  COIN_R * Q + rng_range((PUSH_MIN - 2 * COIN_R) * Q), true);
    }
    for (int i = 0; i < g_themes[m->theme].balls; i++) {
        field_add(&m->field, T_BALL, (40 + i * 60) * Q + rng_range(20 * Q), (FL - 40) * Q - rng_range(20 * Q), false);
    }
    for (int i = 0; i < 2; i++) {
        spawn_prize(m, (40 + i * 110) * Q, (FL - 60) * Q);
    }
    /* settle without paying anything out */
    m->settling = true;
    for (int i = 0; i < 90; i++) {
        field_step(&m->field, on_out, m);
    }
    m->settling = false;
}

void machine_init(machine_t *m)
{
    memset(m, 0, sizeof(*m));
    m->bank = START_BANK;
    m->owned = 1;
    m->jp_x10 = g_themes[0].jp * 10;
    m->aim = FW * Q / 2;
    for (int i = 0; i < 3; i++) {
        m->last_reels[i] = (uint8_t)(i * 2);
    }
    machine_new_field(m);
}

void machine_step(machine_t *m)
{
    const theme_t *th = &g_themes[m->theme];
    m->steps++;
    m->gate_phase = (uint16_t)(m->gate_phase + th->gate_phase);
    int gx = machine_gate_x(m), gw = machine_gate_w(m);
    for (int i = 0; i < m->n_drops; i++) {
        drop_t *d = &m->drops[i];
        d->age++;
        if (!d->checked && d->age >= GATE_CHECK_STEPS) {
            d->checked = 1;
            int off = d->x - gx;
            if ((off < 0 ? -off : off) < gw / 2) {
                if (m->slot_q < SLOT_Q) {
                    m->slot_q++;
                }
                sound_play(SFX_CHANCE);
            }
        }
    }
    int w = 0;
    for (int i = 0; i < m->n_drops; i++) {
        if (m->drops[i].age >= DROP_STEPS) {
            drop_t d = m->drops[i];
            land(m, &d);
        } else {
            m->drops[w++] = m->drops[i];
        }
    }
    m->n_drops = w;
    if (m->shower > 0) {
        if (++m->shower_t >= SHOWER_EVERY) {
            m->shower_t = 0;
            m->shower--;
            push_drop(m, T_COIN, COIN_R * Q + rng_range((FW - 2 * COIN_R) * Q), DROP_STEPS * 3 / 10, 0, true);
        }
    } else {
        m->shower_t = 0;
    }
    for (int i = m->n_ball_wait - 1; i >= 0; i--) {
        if (m->ball_wait[i] > 0) {
            m->ball_wait[i]--;
        }
        if (m->ball_wait[i] == 0 && !m->wheel.on) {
            m->ball_wait[i] = m->ball_wait[--m->n_ball_wait];
            push_drop(m, T_BALL, 30 * Q + rng_range((FW - 60) * Q), 0, 0, true);
        }
    }
    if (m->prize_wait > 0 && --m->prize_wait == 0 && field_count(&m->field, T_PRIZE) < 2) {
        spawn_prize(m, 0, -1);
    }
    /* slot */
    if (!m->slot.on && m->slot_q > 0) {
        m->slot_q--;
        start_slot(m);
    }
    if (m->slot.on) {
        m->slot.t++;
        if (m->slot.t == 27 || m->slot.t == 39 || m->slot.t == 51) {
            sound_play(SFX_REEL_STOP);
        }
        if (m->slot.t >= SLOT_STEPS) {
            m->slot.on = false;
            memcpy(m->last_reels, m->slot.reels, 3);
            award(m, &m->slot.r, "SLOT");
        }
    }
    if (m->wheel.on) {
        m->wheel.t++;
        if (m->wheel.t >= WHEEL_STEPS) {
            m->wheel.on = false;
            m->last_wheel_seg = m->wheel.seg;
            award(m, &g_wheel[m->wheel.seg], "WHEEL");
            if (m->wheel.queued) {
                uint8_t q = m->wheel.queued;
                m->st.wheel--;   /* counted when it was queued */
                start_wheel(m);
                m->wheel.queued = (uint8_t)(q - 1);
            }
        }
    }
    if (m->jpc.on) {
        m->jpc.t++;
        if (m->jpc.t >= JPC_STEPS) {
            m->jpc.on = false;
            const struct pocket *p = &g_pockets[m->jpc.pocket];
            char buf[32];
            if (p->jp) {
                int n = machine_jp(m);
                m->st.jpwin++;
                machine_mint(m, n);
                snprintf(buf, sizeof(buf), "JACKPOT +%d!!", n);
                machine_popup(m, buf, RED);
                m->jp_x10 = th->jp * 10;
                sound_music(MUSIC_JACKPOT);
            } else {
                machine_mint(m, p->coins);
                snprintf(buf, sizeof(buf), "JP CHANCE +%d", p->coins);
                machine_popup(m, buf, GOLD);
                sound_play(SFX_WIN);
            }
        }
    }
    field_step(&m->field, on_out, m);
    /* never stuck: a slow refill while low, a rescue when all has run dry,
     * and a reward for every 5 minutes of active play */
    if (m->bank < REGEN_CAP) {
        if (++m->regen_t >= REGEN_STEPS) {
            m->regen_t = 0;
            m->bank++;
            m->st.gifts++;
        }
    } else {
        m->regen_t = 0;
    }
    if (m->bank == 0 && m->n_drops == 0 && m->shower == 0 && !m->slot.on && !m->wheel.on && !m->jpc.on) {
        if (++m->rescue_t >= RESCUE_STEPS) {
            m->rescue_t = 0;
            m->bank += RESCUE;
            m->st.gifts += RESCUE;
            machine_popup(m, "RESCUE +20 COINS", SKY);
        }
    } else {
        m->rescue_t = 0;
    }
    if (m->idle_t < ACTIVE_STEPS) {
        if (++m->play_t >= PLAY_REWARD_STEPS) {
            m->play_t = 0;
            m->bank += PLAY_REWARD;
            m->st.gifts += PLAY_REWARD;
            m->st.play_rewards++;
            machine_popup(m, "PLAY REWARD +30 COINS", SKY);
            sound_play(SFX_WIN);
        }
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (m->pop[i].t) {
            m->pop[i].t--;
        }
    }
    if (m->tray_flash) {
        m->tray_flash--;
    }
}

/* ---- saving -------------------------------------------------------------- */

void machine_save_prog(const machine_t *m, prog_blob_t *b)
{
    memset(b, 0, sizeof(*b));
    b->ver = PROG_VER;
    b->theme = m->theme;
    b->owned = m->owned;
    b->sets = m->sets;
    /* coins in flight or due go back to the bank, so nothing is lost on a
     * power cut */
    b->bank = m->bank + m->shower;
    for (int i = 0; i < m->n_drops; i++) {
        b->bank += m->drops[i].t == T_COIN;
    }
    b->tickets = m->tickets;
    b->jp_x10 = m->jp_x10;
    b->regen_t = m->regen_t;
    b->play_t = m->play_t;
    memcpy(b->coll, m->coll, NPRIZE);
    b->dropped = m->st.dropped;
    b->won = m->st.won;
    b->minted = m->st.minted;
}

bool machine_load_prog(machine_t *m, const prog_blob_t *b)
{
    if (b->ver != PROG_VER || b->theme >= NTHEMES || !(b->owned & 1) || b->bank < 0 || b->tickets < 0) {
        return false;
    }
    m->theme = b->theme;
    m->owned = b->owned;
    m->sets = b->sets;
    m->bank = b->bank;
    m->tickets = b->tickets;
    m->jp_x10 = b->jp_x10;
    m->regen_t = b->regen_t;
    m->play_t = b->play_t;
    memcpy(m->coll, b->coll, NPRIZE);
    m->st.dropped = b->dropped;
    m->st.won = b->won;
    m->st.minted = b->minted;
    return true;
}

void machine_save_field(const machine_t *m, field_blob_t *b)
{
    memset(b, 0, sizeof(*b));
    b->ver = FIELD_VER;
    b->push_step = m->field.push_step;
    b->prize_wait = m->prize_wait;
    int n = 0;
    for (int i = 0; i < m->field.n; i++) {
        const obj_t *o = &m->field.o[m->field.ord[i]];
        b->o[n].x = o->x;
        b->o[n].y = o->y;
        b->o[n].tf = (uint8_t)(o->t | ((o->f & F_TOP) << 4));   /* stacking resettles */
        b->o[n].pid = o->pid;
        n++;
    }
    b->n = (uint8_t)n;
    b->nballs = (uint8_t)(m->n_ball_wait + m->wheel.queued);
    for (int i = 0; i < m->n_drops; i++) {
        b->nballs += m->drops[i].t == T_BALL;
    }
}

bool machine_load_field(machine_t *m, const field_blob_t *b)
{
    if (b->ver != FIELD_VER || b->n > MAX_OBJ) {
        return false;
    }
    field_clear(&m->field);
    m->field.push_step = (uint16_t)(b->push_step % PUSH_STEPS);
    m->field.push_y = field_push_at(m->field.push_step);
    for (int i = 0; i < b->n; i++) {
        int t = b->o[i].tf & 15;
        if (t < T_COIN || t > T_PRIZE) {
            return false;
        }
        obj_t *o = field_add(&m->field, t, b->o[i].x, b->o[i].y, (b->o[i].tf >> 4) & F_TOP);
        o->pid = b->o[i].pid < NPRIZE ? b->o[i].pid : 0;
    }
    m->n_drops = 0;
    m->n_ball_wait = 0;
    for (int i = 0; i < b->nballs && i < MAX_BALLWAIT; i++) {
        ball_back(m, 15);
    }
    m->prize_wait = b->prize_wait;
    if (field_count(&m->field, T_PRIZE) == 0 && m->prize_wait == 0) {
        m->prize_wait = 30;
    }
    return true;
}
