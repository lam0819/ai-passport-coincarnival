#include "field.h"

#include <math.h>
#include <string.h>

static int16_t s_push[PUSH_STEPS];
static int16_t s_sin[257];
static uint32_t s_rng = 0x2545F491u;

void rng_seed(uint32_t s)
{
    s_rng = s ? s : 1;
}

uint32_t rng_next(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

int rng_range(int n)
{
    return n <= 1 ? 0 : (int)(((uint64_t)rng_next() * (uint32_t)n) >> 32);
}

void field_init_tables(void)
{
    /* once at boot: the plate's cosine stroke and a sine table, so the
     * per-step code is integer only */
    for (int i = 0; i < PUSH_STEPS; i++) {
        double c = 0.5 - 0.5 * cos(2.0 * 3.14159265358979 * i / PUSH_STEPS);
        s_push[i] = (int16_t)lround((PUSH_MIN + (PUSH_MAX - PUSH_MIN) * c) * Q);
    }
    for (int i = 0; i <= 256; i++) {
        s_sin[i] = (int16_t)lround(16384.0 * sin(2.0 * 3.14159265358979 * i / 256));
    }
}

int16_t isin16(uint16_t phase)
{
    int i = phase >> 8, fr = phase & 255;
    return (int16_t)(s_sin[i] + (((s_sin[i + 1] - s_sin[i]) * fr) >> 8));
}

int field_push_at(int step)
{
    return s_push[step % PUSH_STEPS];
}

int field_radius(int t)
{
    return (t == T_COIN ? COIN_R : t == T_BALL ? BALL_R : PRIZE_R) * Q;
}

static int mass(int t)
{
    return t == T_COIN ? MASS_COIN : t == T_BALL ? MASS_BALL : MASS_PRIZE;
}

static uint32_t isqrt32(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) {
        b >>= 2;
    }
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

void field_clear(field_t *f)
{
    int cap = f->cap;
    memset(f, 0, sizeof(*f));
    f->cap = cap > 0 && cap <= MAX_OBJ ? cap : MAX_OBJ;
    f->push_y = s_push[0];
}

obj_t *field_add(field_t *f, int t, int x, int y, bool top)
{
    if (f->n >= (f->cap ? f->cap : MAX_OBJ)) {
        return NULL;
    }
    for (int i = 0; i < MAX_OBJ; i++) {
        obj_t *o = &f->o[i];
        if (o->t != T_FREE) {
            continue;
        }
        o->x = o->px = (int16_t)x;
        o->y = o->py = (int16_t)y;
        o->t = (uint8_t)t;
        o->f = top ? F_TOP : 0;
        o->sup = NO_SUP;
        o->pid = 0;
        f->ord[f->n++] = (uint8_t)i;
        return o;
    }
    return NULL;
}

int field_count(const field_t *f, int t)
{
    int n = 0;
    for (int i = 0; i < f->n; i++) {
        n += f->o[f->ord[i]].t == t;
    }
    return n;
}

static void sort_x(field_t *f)
{
    /* insertion sort: the order barely changes from one pass to the next */
    for (int i = 1; i < f->n; i++) {
        uint8_t k = f->ord[i];
        int16_t kx = f->o[k].x;
        int j = i - 1;
        while (j >= 0 && f->o[f->ord[j]].x > kx) {
            f->ord[j + 1] = f->ord[j];
            j--;
        }
        f->ord[j + 1] = k;
    }
}

enum { G_FIELD, G_TOP, G_STACK };

static inline int group(const obj_t *o)
{
    return (o->f & F_TOP) ? G_TOP : (o->f & F_STACK) ? G_STACK : G_FIELD;
}

static void separate(field_t *f, int g, bool popping)
{
    sort_x(f);
    int pops = 0;
    for (int i = 0; i < f->n; i++) {
        obj_t *a = &f->o[f->ord[i]];
        if (group(a) != g) {
            continue;
        }
        int ra = field_radius(a->t), ma = mass(a->t);
        for (int j = i + 1; j < f->n; j++) {
            obj_t *b = &f->o[f->ord[j]];
            int32_t dx = b->x - a->x;
            if (dx >= ra + MAX_R * Q) {
                break;
            }
            if (group(b) != g) {
                continue;
            }
            int32_t rr = ra + field_radius(b->t);
            if (dx >= rr) {
                continue;
            }
            int32_t dy = b->y - a->y;
            int32_t d2 = dx * dx + dy * dy;
            if (d2 >= rr * rr) {
                continue;
            }
            int32_t d = (int32_t)isqrt32((uint32_t)d2);
            if (d < 1) {
                dx = rng_range(17) - 8;
                dy = 8;
                d = (int32_t)isqrt32((uint32_t)(dx * dx + dy * dy));
            }
            int32_t ov = rr - d;
            if (popping && pops < 3 && a->t == T_COIN && b->t == T_COIN && ov * 100 > STACK_POP_PCT * rr) {
                /* the rear coin rides up onto the front one */
                obj_t *back = a->y < b->y ? a : b, *front = back == a ? b : a;
                back->f |= F_STACK;
                back->sup = (uint8_t)(front - f->o);
                pops++;
                continue;
            }
            int32_t k = ov * STIFF_PCT / 100, mb = mass(b->t), tm = ma + mb;
            int32_t den = d * tm;
            a->x = (int16_t)(a->x - dx * k * mb / den);
            a->y = (int16_t)(a->y - dy * k * mb / den);
            b->x = (int16_t)(b->x + dx * k * ma / den);
            b->y = (int16_t)(b->y + dy * k * ma / den);
        }
    }
}

static void face(field_t *f, bool stacked_too)
{
    for (int i = 0; i < f->n; i++) {
        obj_t *o = &f->o[f->ord[i]];
        if ((o->f & F_TOP) || ((o->f & F_STACK) && !stacked_too)) {
            continue;
        }
        int lim = f->push_y + field_radius(o->t);
        if (o->y < lim) {
            o->y = (int16_t)lim;
        }
    }
}

static bool near_support(const obj_t *o, const obj_t *q)
{
    if (q->t != T_COIN || (q->f & (F_TOP | F_STACK))) {
        return false;
    }
    int32_t dx = q->x - o->x, dy = q->y - o->y, lim = COIN_R * Q * 115 / 100;
    return dx * dx + dy * dy < lim * lim;
}

void field_step(field_t *f, field_out_fn out, void *ctx)
{
    int prev = f->push_y;
    f->push_step = (uint16_t)((f->push_step + 1) % PUSH_STEPS);
    f->push_y = s_push[f->push_step];
    int dp = f->push_y - prev;

    for (int i = 0; i < f->n; i++) {
        obj_t *o = &f->o[f->ord[i]];
        o->px = o->x;
        o->py = o->y;
    }
    /* the plate top rides with the plate; the back wall holds coins back */
    for (int i = 0; i < f->n; i++) {
        obj_t *o = &f->o[f->ord[i]];
        if (o->f & F_TOP) {
            int r = field_radius(o->t);
            o->y = (int16_t)(o->y + dp < r ? r : o->y + dp);
        }
    }
    /* stacked coins ride on their support */
    for (int i = 0; i < f->n; i++) {
        obj_t *o = &f->o[f->ord[i]];
        if (!(o->f & F_STACK)) {
            continue;
        }
        const obj_t *s = o->sup == NO_SUP ? NULL : &f->o[o->sup];
        if (!s || s->t == T_FREE || (s->f & (F_TOP | F_STACK))) {
            o->f &= (uint8_t)~F_STACK;
            o->sup = NO_SUP;
            continue;
        }
        o->x = (int16_t)(o->x + s->x - s->px);
        o->y = (int16_t)(o->y + s->y - s->py);
    }
    for (int it = 0; it < ITER; it++) {
        face(f, true);
        separate(f, G_FIELD, it == ITER - 1);
        separate(f, G_TOP, false);
        for (int i = 0; i < f->n; i++) {
            obj_t *o = &f->o[f->ord[i]];
            if ((o->f & F_TOP) || o->y < WALL_END * Q) {
                int r = field_radius(o->t);
                if (o->x < r) {
                    o->x = (int16_t)r;
                }
                if (o->x > FW * Q - r) {
                    o->x = (int16_t)(FW * Q - r);
                }
            }
        }
    }
    separate(f, G_STACK, false);
    face(f, false);
    /* stacked coins without anything under them drop down */
    for (int i = 0; i < f->n; i++) {
        obj_t *o = &f->o[f->ord[i]];
        if (!(o->f & F_STACK)) {
            continue;
        }
        if (o->sup != NO_SUP && near_support(o, &f->o[o->sup])) {
            continue;
        }
        o->sup = NO_SUP;
        for (int j = 0; j < f->n; j++) {
            if (near_support(o, &f->o[f->ord[j]])) {
                o->sup = f->ord[j];
                break;
            }
        }
        if (o->sup == NO_SUP) {
            o->f &= (uint8_t)~F_STACK;
        }
    }
    /* plate coins past the plate face fall onto the field */
    for (int i = 0; i < f->n; i++) {
        obj_t *o = &f->o[f->ord[i]];
        if ((o->f & F_TOP) && o->y > f->push_y) {
            o->f = 0;
            o->sup = NO_SUP;
            o->y = (int16_t)(f->push_y + field_radius(o->t) * 6 / 10);
        }
    }
    /* front lip (pays in the middle) and side gutters */
    int w = 0;
    for (int i = 0; i < f->n; i++) {
        uint8_t k = f->ord[i];
        obj_t *o = &f->o[k];
        bool gone = false, paid = false;
        if (!(o->f & F_TOP)) {
            if (o->y > FL * Q) {
                gone = true;
                int off = o->x - FW * Q / 2;
                paid = (off < 0 ? -off : off) <= PAY_W * Q / 2;
            } else if ((o->x < 0 || o->x > FW * Q) && o->y >= WALL_END * Q) {
                gone = true;
            }
        }
        if (gone) {
            if (out) {
                out(o, paid, ctx);
            }
            o->t = T_FREE;
        } else {
            f->ord[w++] = k;
        }
    }
    f->n = w;
}
