#include "art.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"

#define BLACK PX(0, 0, 0)
#define WHITE PX(0xff, 0xff, 0xff)
#define GOLD PX(0xf2, 0xc2, 0x30)
#define GOLD_D PX(0xe0, 0xa8, 0x20)
#define GOLD_S PX(0x8a, 0x5a, 0x12)
#define GOLD_H PX(0xff, 0xf2, 0xb0)
#define BALL PX(0xe8, 0x3a, 0x5a)
#define BALL_H PX(0xff, 0xd0, 0xdc)
#define HOLE PX(0x05, 0x05, 0x0a)
#define DARK PX(0x1a, 0x1a, 0x28)
#define SHADOW PX(0x0a, 0x0a, 0x12)

static uint8_t s_order[MAX_OBJ];
static int s_norder;

void art_init(void)
{
    gfx_init();
}

/* --- projection: the field seen from the front and above. Units are Q4. */
static inline int sc256(int yq)
{
    return 184 + 118 * yq / (FL * Q);
}

static inline int sxq(int xq, int yq)
{
    return 160 + (xq - FW * Q / 2) * sc256(yq) / (256 * Q);
}

static inline int syq(int yq)
{
    return FY0 + yq * (FY1 - FY0) / (FL * Q);
}

/* a four-sided shape with a flat top and bottom */
static void quad(int y0, int xl0, int xr0, int y1, int xl1, int xr1, uint16_t c)
{
    if (y1 <= y0) {
        return;
    }
    int a = y0 < gfx_band_y0() ? gfx_band_y0() : y0;
    int b = y1 > gfx_band_y0() + gfx_band_rows() ? gfx_band_y0() + gfx_band_rows() : y1;
    for (int y = a; y < b; y++) {
        int k = y - y0, h = y1 - y0;
        gfx_hline(xl0 + (xl1 - xl0) * k / h, xr0 + (xr1 - xr0) * k / h, y, c);
    }
}

static void text_c(const char *t, int cx, int y, int scale, uint16_t c)
{
    gfx_text(t, cx - gfx_text_w(t, scale) / 2, y, scale, c);
}

static void text_shadow_c(const char *t, int cx, int y, uint16_t c)
{
    int x = cx - gfx_text_w(t, 1) / 2;
    gfx_text(t, x + 1, y + 1, 1, BLACK);
    gfx_text(t, x, y, 1, c);
}

int art_lamp_phase(uint32_t frame)
{
    return (int)((frame / 8) % 3);
}

void art_coin_icon(int cx, int cy)
{
    gfx_ellipse(cx, cy + 1, 5, 3, GOLD_S);
    gfx_ellipse(cx, cy, 5, 3, GOLD);
    gfx_ellipse(cx, cy, 3, 2, GOLD_D);
}

static void coin(int X, int Y, int s)
{
    int rx = (COIN_R * s + 128) >> 8, ry = (rx + 1) / 2;
    gfx_ellipse(X, Y + ((s * 16 / 10 + 128) >> 8), rx, ry, GOLD_S);
    gfx_ellipse(X, Y, rx, ry, GOLD);
    gfx_ellipse(X, Y, rx * 7 / 10, ry * 7 / 10, GOLD_D);
    gfx_ellipse(X - rx * 3 / 10, Y - ry * 35 / 100, rx * 3 / 10, 1, GOLD_H);
}

static void obj(const obj_t *o, int lift)
{
    int s = sc256(o->y), X = sxq(o->x, o->y), Y = syq(o->y) - lift;
    if (o->t == T_COIN) {
        coin(X, Y - ((o->f & F_STACK) ? (s * 32 / 10) >> 8 : 0), s);
        return;
    }
    int r = (field_radius(o->t) / Q * s + 128) >> 8;
    if (o->t == T_BALL) {
        gfx_ellipse(X, Y + r * 35 / 100, r, r * 45 / 100, SHADOW);
        gfx_ellipse(X, Y - r * 55 / 100, r, r, BALL);
        gfx_fill(X - r + 1, Y - r * 70 / 100, 2 * r - 1, r * 32 / 100, WHITE);
        gfx_ellipse(X - r * 35 / 100, Y - r, r * 3 / 10, r / 4, BALL_H);
        return;
    }
    const theme_t *th = &g_themes[o->pid / 6];
    gfx_ellipse(X, Y + r * 3 / 10, r, r * 45 / 100, SHADOW);
    gfx_ellipse(X, Y - r / 2, r, r * 9 / 10, th->prizes[o->pid % 6].col);
    gfx_fill(X - r + 1, Y - r * 55 / 100, 2 * r - 1, 2, WHITE);
    gfx_ellipse(X - r * 35 / 100, Y - r * 9 / 10, r * 28 / 100, r * 22 / 100, WHITE);
}

void art_prepare(const machine_t *m)
{
    /* field discs back to front (stacked ones half a coin later); insertion
     * sort, since the order barely changes between frames */
    const field_t *f = &m->field;
    int n = 0;
    for (int i = 0; i < f->n; i++) {
        if (!(f->o[f->ord[i]].f & F_TOP)) {
            s_order[n++] = f->ord[i];
        }
    }
    for (int i = 1; i < n; i++) {
        uint8_t k = s_order[i];
        int ky = f->o[k].y + ((f->o[k].f & F_STACK) ? COIN_R * Q / 2 : 0);
        int j = i - 1;
        while (j >= 0) {
            const obj_t *p = &f->o[s_order[j]];
            if (p->y + ((p->f & F_STACK) ? COIN_R * Q / 2 : 0) <= ky) {
                break;
            }
            s_order[j + 1] = s_order[j];
            j--;
        }
        s_order[j + 1] = k;
    }
    s_norder = n;
}

/* --- symbols for the slot and the collection ----------------------------- */

void art_symbol(int sym, int cx, int cy)
{
    switch (sym) {
    case SYM_CHERRY:
        gfx_fill(cx - 1, cy - 9, 2, 6, PX(0x3a, 0x9a, 0x3a));
        gfx_fill(cx - 4, cy - 4, 2, 4, PX(0x3a, 0x9a, 0x3a));
        gfx_fill(cx + 2, cy - 4, 2, 3, PX(0x3a, 0x9a, 0x3a));
        gfx_fill(cx - 4, cy - 5, 8, 2, PX(0x3a, 0x9a, 0x3a));
        gfx_ellipse(cx - 4, cy + 3, 4, 4, PX(0xe0, 0x20, 0x30));
        gfx_ellipse(cx + 4, cy + 2, 4, 4, PX(0xe0, 0x20, 0x30));
        gfx_fill(cx - 6, cy + 1, 2, 2, WHITE);
        break;
    case SYM_BELL:
        gfx_ellipse(cx, cy - 3, 6, 6, PX(0xff, 0xd2, 0x3a));
        quad(cy - 3, cx - 6, cx + 7, cy + 6, cx - 9, cx + 10, PX(0xff, 0xd2, 0x3a));
        gfx_fill(cx - 9, cy + 6, 19, 2, PX(0xc8, 0x96, 0x2e));
        gfx_ellipse(cx, cy + 8, 2, 2, PX(0xc8, 0x96, 0x2e));
        gfx_fill(cx - 3, cy - 6, 2, 6, PX(0xff, 0xf2, 0xb0));
        break;
    case SYM_STAR:
        /* a twinkle: a diamond with long rays */
        for (int d = 0; d <= 6; d++) {
            gfx_hline(cx - 6 + d, cx + 7 - d, cy - d, PX(0x7f, 0xd8, 0xff));
            gfx_hline(cx - 6 + d, cx + 7 - d, cy + d, PX(0x7f, 0xd8, 0xff));
        }
        gfx_fill(cx, cy - 10, 1, 21, PX(0x7f, 0xd8, 0xff));
        gfx_fill(cx - 10, cy, 21, 1, PX(0x7f, 0xd8, 0xff));
        gfx_ellipse(cx, cy, 2, 2, WHITE);
        break;
    case SYM_SEVEN:
        gfx_text("7", cx - 6, cy - 9, 3, PX(0x80, 0x10, 0x10));
        gfx_text("7", cx - 7, cy - 10, 3, PX(0xff, 0x40, 0x40));
        break;
    default:
        gfx_ellipse(cx, cy + 3, 8, 7, PX(0xb0, 0x7a, 0x3a));
        gfx_fill(cx - 3, cy - 7, 6, 4, PX(0x8a, 0x5a, 0x22));
        gfx_text("$", cx - 2, cy, 1, PX(0xff, 0xe0, 0x66));
        break;
    }
}

void art_prize(int pid, int cx, int cy, int r, bool known)
{
    uint16_t c = known ? g_themes[pid / 6].prizes[pid % 6].col : PX(0x30, 0x30, 0x40);
    gfx_ellipse(cx, cy + r * 3 / 10, r, r * 45 / 100, SHADOW);
    gfx_ellipse(cx, cy - r / 4, r, r * 9 / 10, c);
    if (known) {
        gfx_fill(cx - r + 1, cy - r * 3 / 10, 2 * r - 1, 2, WHITE);
        gfx_ellipse(cx - r * 35 / 100, cy - r * 6 / 10, r * 28 / 100, r * 22 / 100, WHITE);
    } else {
        gfx_text("?", cx - 2, cy - 5, 1, PX(0x80, 0x80, 0x90));
    }
}

/* --- the top display: slot, wheel or jackpot chance ---------------------- */

static void draw_slot(const machine_t *m, uint32_t frame)
{
    static const uint8_t STOP[3] = { 27, 39, 51 };
    for (int i = 0; i < 3; i++) {
        int cx = 110 + i * 50, cy = (DISP_Y0 + DISP_Y1) / 2;
        gfx_fill(cx - 17, cy - 15, 34, 30, PX(0xf4, 0xf0, 0xe8));
        gfx_fill(cx - 17, cy - 15, 34, 3, PX(0xc0, 0xb8, 0xa8));
        gfx_fill(cx - 17, cy + 12, 34, 3, PX(0xc0, 0xb8, 0xa8));
        int sym;
        if (m->slot.on && m->slot.t < STOP[i]) {
            sym = (int)((frame / 2 + (uint32_t)i * 2) % NSYM);
            int off = (int)((frame * 7) % 10) - 5;
            art_symbol(sym, cx, cy + off);
            gfx_blend(cx - 17, cy - 12, 34, 24, PX(0xf4, 0xf0, 0xe8), 110);   /* motion blur */
        } else {
            sym = m->slot.on ? m->slot.reels[i] : m->last_reels[i];
            art_symbol(sym, cx, cy);
        }
    }
    char q[8];
    snprintf(q, sizeof(q), "x%d", m->slot_q);
    if (m->slot_q) {
        gfx_text(q, 248, DISP_Y0 + 14, 1, PX(0xff, 0xd2, 0x3a));
    }
    gfx_text("SLOT", 52, DISP_Y0 + 14, 1, PX(0xa0, 0xa8, 0xc8));
}

static int wheel_pos64(const machine_t *m)
{
    if (!m->wheel.on) {
        return m->last_wheel_seg * 64;
    }
    int dist = 2 * NWHEEL + ((m->wheel.seg - m->wheel.from + NWHEEL) % NWHEEL);
    int k = m->wheel.t * 256 / 66;
    if (k > 256) {
        k = 256;
    }
    int inv = 256 - k;
    int e = 256 - inv * inv / 256 * inv / 256;   /* ease out, cubic */
    return m->wheel.from * 64 + dist * 64 * e / 256;
}

static void draw_wheel(const machine_t *m)
{
    /* the roulette seen edge-on: a strip of 12 segments slides under a pointer */
    int pos = wheel_pos64(m), base = pos / 64, frac = pos % 64;
    int cy = (DISP_Y0 + DISP_Y1) / 2 + 3;
    for (int i = -3; i <= 3; i++) {
        int seg = ((base + i) % NWHEEL + NWHEEL) % NWHEEL;
        int x = 160 + i * 40 - frac * 40 / 64 - 19;
        const reward_t *r = &g_wheel[seg];
        uint16_t c = r->coins ? PX(0xff, 0xd2, 0x3a) : PX(0xff, 0x9a, 0xd5);
        if (seg % 2) {
            c = gfx_mix(c, BLACK, 50);
        }
        int xa = x < 64 ? 64 : x, xb = x + 38 > 256 ? 256 : x + 38;
        if (xb <= xa) {
            continue;
        }
        gfx_fill(xa, cy - 12, xb - xa, 24, c);
        char t[8];
        snprintf(t, sizeof(t), "%d%s", r->coins ? r->coins : r->tickets, r->coins ? "C" : "T");
        if (xa == x && xb == x + 38) {
            text_c(t, x + 19, cy - 3, 1, PX(0x30, 0x10, 0x20));
        }
    }
    for (int d = 0; d < 5; d++) {
        gfx_hline(160 - 5 + d, 160 + 6 - d, cy - 16 + d, WHITE);
    }
    text_c("WHEEL", 160, DISP_Y0 + 1, 1, WHITE);
    bool done = m->wheel.on && m->wheel.t >= 66 && (m->wheel.t / 4) % 2;
    if (done) {
        gfx_fill(140, cy - 13, 40, 2, WHITE);
        gfx_fill(140, cy + 11, 40, 2, WHITE);
    }
}

static void draw_jpc(const machine_t *m)
{
    gfx_fill(42, DISP_Y0 + 2, 236, DISP_Y1 - DISP_Y0 - 4, PX(0x22, 0, 0));
    text_c("JACKPOT CHANCE", 160, DISP_Y0 + 4, 1, PX(0xff, 0xd2, 0x3a));
    int t = m->jpc.t;
    for (int i = 0; i < NPOCKET; i++) {
        int x = 88 + i * 48;
        bool lit = i == m->jpc.pocket && t > 78 && (t / 4) % 2;
        gfx_fill(x - 18, DISP_Y1 - 14, 36, 11, lit ? PX(0xff, 0xd2, 0x3a) : PX(0x55, 0x55, 0x22));
        char s[6];
        snprintf(s, sizeof(s), "%d", g_pockets[i].coins);
        text_c(g_pockets[i].jp ? "JP" : s, x, DISP_Y1 - 12, 1, lit ? BLACK : WHITE);
    }
    int k = t < 78 ? t * 256 / 78 : 256, tx = 88 + m->jpc.pocket * 48;
    int bx = tx;
    if (t < 78) {
        /* the ball swings across the funnel, then settles in its pocket */
        int sw = isin16((uint16_t)(k * 3 * 256)) ;   /* three swings */
        bx = 160 + (sw * 70 >> 14) * (256 - k * 6 / 10) / 256 * (256 - k) / 256 + (tx - 160) * k / 256;
    }
    gfx_ellipse(bx, DISP_Y1 - 22 + k * 4 / 256, 4, 4, BALL);
}

/* --- the whole machine ---------------------------------------------------- */

void art_machine(const machine_t *m, const art_view_t *v)
{
    const theme_t *th = &g_themes[m->theme];
    const field_t *f = &m->field;
    int y0 = gfx_band_y0(), y1 = y0 + gfx_band_rows();
    gfx_fill(0, y0, SCREEN_W, y1 - y0, th->sky);

    if (y0 < DISP_Y1) {
        /* HUD */
        char buf[32];
        gfx_fill(0, 0, SCREEN_W, HUD_H, gfx_mix(th->sky, BLACK, 140));
        art_coin_icon(9, 6);
        snprintf(buf, sizeof(buf), "%ld", (long)m->bank);
        gfx_text(buf, 18, 3, 1, PX(0xff, 0xd2, 0x3a));
        snprintf(buf, sizeof(buf), "JP %05d", machine_jp(m));
        text_c(buf, 160, 3, 1, PX(0xff, 0x60, 0x60));
        snprintf(buf, sizeof(buf), "T %ld", (long)m->tickets);
        gfx_text(buf, 316 - gfx_text_w(buf, 1), 3, 1, PX(0xff, 0x9a, 0xd5));
        /* display */
        gfx_fill(40, DISP_Y0, 240, DISP_Y1 - DISP_Y0, PX(0x0b, 0x0b, 0x14));
        gfx_fill(42, DISP_Y0 + 2, 236, DISP_Y1 - DISP_Y0 - 4, th->wall);
        int ph = art_lamp_phase(v->frame);
        for (int i = 0; i < 12; i++) {
            gfx_fill(44 + i * 20, DISP_Y0, 6, 2, (ph + i) % 3 ? th->lamp : PX(0x33, 0x33, 0x33));
        }
        if (m->jpc.on) {
            draw_jpc(m);
        } else if (m->wheel.on) {
            draw_wheel(m);
        } else {
            draw_slot(m, v->frame);
        }
    }

    if (y1 > FIELD_Y0 && y0 < FIELD_Y1) {
        int l0 = sxq(0, 0), r0 = sxq(FW * Q, 0), lw = sxq(0, WALL_END * Q), rw = sxq(FW * Q, WALL_END * Q);
        int lf = sxq(0, FL * Q), rf = sxq(FW * Q, FL * Q), wy = syq(WALL_END * Q);
        /* back wall with the CHANCE gate and the pending-spin lamps */
        gfx_fill(l0 - 6, FY0 - 16, r0 - l0 + 13, 16, DARK);
        int gx = sxq(machine_gate_x(m), 0), gw = machine_gate_w(m) * sc256(0) / (256 * Q);
        gfx_fill(gx - gw / 2, FY0 - 12, gw, 7, BLACK);
        gfx_fill(gx - gw / 2, FY0 - 13, gw, 1, th->accent);
        gfx_fill(gx - 1, FY0 - 10, 3, 3, th->accent);
        for (int i = 0; i < SLOT_Q; i++) {
            gfx_ellipse(r0 - 6 - i * 7, FY0 - 4, 2, 2, i < m->slot_q ? PX(0xff, 0xd2, 0x3a) : PX(0x44, 0x44, 0x44));
        }
        gfx_text("CHANCE", l0, FY0 - 12, 1, gfx_mix(th->accent, DARK, 120));
        /* floor, walls, gutters */
        quad(FY0, l0, r0 + 1, FY1, lf, rf + 1, th->field);
        for (int yy = 10; yy < FL; yy += 20) {
            int yq = yy * Q;
            gfx_hline(sxq(0, yq), sxq(FW * Q, yq), syq(yq), gfx_mix(th->field, WHITE, 14));
        }
        quad(FY0, l0 - 7, l0, wy, lw - 9, lw, th->wall);
        quad(FY0, r0 + 1, r0 + 8, wy, rw + 1, rw + 10, th->wall);
        quad(wy, lw - 9, lw, FY1, lf - 12, lf, HOLE);
        quad(wy, rw + 1, rw + 10, FY1, rf + 1, rf + 13, HOLE);
        /* pusher plate: top + face */
        int py = f->push_y, ps = sc256(py), lift = 6 * ps / 256;
        int pyy = syq(py) - lift;
        quad(FY0 - 6, l0, r0 + 1, pyy, sxq(0, py), sxq(FW * Q, py) + 1, th->plate);
        for (int i = 0; i < f->n; i++) {
            const obj_t *o = &f->o[f->ord[i]];
            if (o->f & F_TOP) {
                obj(o, 6 * sc256(o->y) / 256);
            }
        }
        gfx_fill(sxq(0, py), pyy, sxq(FW * Q, py) - sxq(0, py) + 1, lift + 1, PX(0x4a, 0x50, 0x68));
        gfx_fill(sxq(0, py), pyy, sxq(FW * Q, py) - sxq(0, py) + 1, 1, gfx_mix(th->plate, WHITE, 128));
        /* field discs back to front */
        for (int i = 0; i < s_norder; i++) {
            obj(&f->o[s_order[i]], 0);
        }
        /* front lip: pays in the middle, lose holes at the corners */
        gfx_fill(lf - 12, FY1, rf - lf + 25, 3, HOLE);
        int pl = sxq((FW / 2 - PAY_W / 2) * Q, FL * Q), pr = sxq((FW / 2 + PAY_W / 2) * Q, FL * Q);
        gfx_fill(pl, FY1, pr - pl, 3, th->accent);
        /* coins in flight and the dropper */
        int ax = sxq(m->aim, 0);
        for (int i = 0; i < m->n_drops; i++) {
            const drop_t *d = &m->drops[i];
            int k = d->age * 256 / DROP_STEPS, y = 60 + k * (FY0 - 66) / 256;
            int X = sxq(d->x, 0);
            if (!d->checked || d->age < GATE_CHECK_STEPS) {
                X += (ax - X) * (256 - k) / 256;   /* leaves the slot at the aim */
            }
            if (d->t == T_COIN) {
                gfx_ellipse(X, y, 4, 2, GOLD);
                gfx_ellipse(X, y + 1, 4, 1, GOLD_S);
            } else {
                gfx_ellipse(X, y, 6, 5, d->t == T_BALL ? BALL : PX(0xff, 0xd0, 0xf0));
            }
        }
        gfx_fill(ax - 7, 56, 14, 5, PX(0xcc, 0xcc, 0xcc));
        gfx_fill(ax - 3, 61, 6, 3, PX(0x99, 0x99, 0x99));
        for (int d = 0; d < 4; d++) {
            gfx_hline(ax - 3 + d, ax + 4 - d, 52 + d, th->accent);
        }
        /* popups rise over the field */
        int py0 = 112;
        for (int i = 0; i < MAX_POPUPS; i++) {
            const popup_t *p = &m->pop[i];
            if (p->t) {
                text_shadow_c(p->text, 160, py0 - (36 - p->t) * 14 / 36, p->col);
                py0 += 12;
            }
        }
    }

    if (y1 > FIELD_Y1 - 1) {
        /* payout tray */
        gfx_fill(18, FY1 + 3, 284, SCREEN_H - FY1 - 3, PX(0x11, 0x11, 0x11));
        gfx_fill(22, FY1 + 6, 276, 26, m->tray_flash ? PX(0x3a, 0x30, 0x10) : PX(0x1c, 0x1c, 0x24));
        char buf[40];
        if (v->msg && v->msg[0]) {
            text_c(v->msg, 160, FY1 + 10, 1, WHITE);
        } else {
            snprintf(buf, sizeof(buf), "WON %lu   DROP %lu", (unsigned long)m->st.won, (unsigned long)m->st.dropped);
            text_c(buf, 160, FY1 + 10, 1, PX(0x99, 0x99, 0x99));
        }
        if (v->edge_pct > 0) {
            gfx_fill(110, FY1 + 22, 100, 5, PX(0x33, 0x33, 0x33));
            gfx_fill(110, FY1 + 22, v->edge_pct, 5, WHITE);
        }
    }
}
