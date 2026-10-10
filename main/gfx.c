#include "gfx.h"

#include <string.h>

#ifdef GFX_COUNT_PIXELS
long g_gfx_px;   /* host harness: pixels written, for a cost estimate */
#define COUNT(n) (g_gfx_px += (n))
#else
#define COUNT(n) ((void)0)
#endif

static uint16_t s_bufs[2][SCREEN_W * BAND_ROWS];
static uint16_t *s_buf = s_bufs[0];
static int s_next;
static int s_x0, s_w = SCREEN_W, s_y0, s_rows;
/* half-widths of every ellipse row, filled once: [rx][ry][row] */
#define TAB_RX 12
#define TAB_RY 7
static uint8_t s_hw[TAB_RX + 1][TAB_RY + 1][TAB_RY + 1];   /* coins: 832 bytes */

static int hw_calc(int rx, int ry, int dy)
{
    /* rx * sqrt(1 - (dy/ry)^2), sampled at the row centre, integer only */
    int num = ry * ry * 4 - (2 * dy) * (2 * dy);
    if (num <= 0) {
        return 0;
    }
    int t = rx * rx * num / (ry * ry * 4), v = 0, b = 1 << 10;
    while (b) {
        if ((v + b) * (v + b) <= t) {
            v += b;
        }
        b >>= 1;
    }
    return v;
}

uint16_t gfx_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t v = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return gfx_swap(v);
}

uint16_t gfx_swap(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}

void gfx_init(void)
{
    for (int rx = 1; rx <= TAB_RX; rx++) {
        for (int ry = 1; ry <= TAB_RY; ry++) {
            for (int dy = 0; dy <= ry; dy++) {
                s_hw[rx][ry][dy] = (uint8_t)hw_calc(rx, ry, dy);
            }
        }
    }
}

void gfx_band_begin_win(int x0, int w, int y0, int rows, uint16_t bg)
{
    /* display_send_rect waits for the previous transfer before starting the
     * next, so the buffer two bands back is always free by now. */
    s_buf = s_bufs[s_next];
    s_next ^= 1;
    s_x0 = x0;
    s_w = w;
    s_y0 = y0;
    s_rows = rows;
    COUNT(w * rows);
    for (int i = 0; i < w * rows; i++) {
        s_buf[i] = bg;
    }
}

void gfx_band_begin(int y0, int rows, uint16_t bg)
{
    gfx_band_begin_win(0, SCREEN_W, y0, rows, bg);
}

const uint16_t *gfx_band_buf(void) { return s_buf; }
int gfx_band_x0(void) { return s_x0; }
int gfx_band_w(void) { return s_w; }
int gfx_band_y0(void) { return s_y0; }
int gfx_band_rows(void) { return s_rows; }

static bool clip(int *x, int *y, int *w, int *h)
{
    int rx = *x - s_x0, ry = *y - s_y0;
    if (ry < 0) {
        *h += ry;
        ry = 0;
    }
    if (ry + *h > s_rows) {
        *h = s_rows - ry;
    }
    if (rx < 0) {
        *w += rx;
        rx = 0;
    }
    if (rx + *w > s_w) {
        *w = s_w - rx;
    }
    *x = rx;
    *y = ry;
    return *w > 0 && *h > 0;
}

void gfx_fill(int x, int y, int w, int h, uint16_t color)
{
    if (!clip(&x, &y, &w, &h)) {
        return;
    }
    COUNT(w * h);
    for (int yy = 0; yy < h; yy++) {
        uint16_t *row = &s_buf[(y + yy) * s_w + x];
        for (int xx = 0; xx < w; xx++) {
            row[xx] = color;
        }
    }
}

void gfx_hline(int x0, int x1, int y, uint16_t color)
{
    int ry = y - s_y0;
    if (ry < 0 || ry >= s_rows) {
        return;
    }
    x0 -= s_x0;
    x1 -= s_x0;
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > s_w) {
        x1 = s_w;
    }
    COUNT(x1 > x0 ? x1 - x0 : 0);
    uint16_t *row = &s_buf[ry * s_w];
    for (int x = x0; x < x1; x++) {
        row[x] = color;
    }
}

uint16_t gfx_mix(uint16_t base, uint16_t color, int alpha)
{
    uint16_t v = gfx_swap(base), c = gfx_swap(color);
    int ia = 256 - alpha;
    int r = ((v >> 11) * ia + (c >> 11) * alpha) >> 8;
    int g = (((v >> 5) & 63) * ia + ((c >> 5) & 63) * alpha) >> 8;
    int b = ((v & 31) * ia + (c & 31) * alpha) >> 8;
    return gfx_swap((uint16_t)((r << 11) | (g << 5) | b));
}

void gfx_blend(int x, int y, int w, int h, uint16_t color, int alpha)
{
    if (alpha <= 0) {
        return;
    }
    if (alpha >= 256) {
        gfx_fill(x, y, w, h, color);
        return;
    }
    if (!clip(&x, &y, &w, &h)) {
        return;
    }
    uint16_t c = gfx_swap(color);
    int cr = c >> 11, cg = (c >> 5) & 63, cb = c & 31;
    int ia = 256 - alpha;
    COUNT(w * h * 4);   /* a blend costs about four fills */
    for (int yy = 0; yy < h; yy++) {
        uint16_t *row = &s_buf[(y + yy) * s_w + x];
        for (int xx = 0; xx < w; xx++) {
            uint16_t v = gfx_swap(row[xx]);
            int r = ((v >> 11) * ia + cr * alpha) >> 8;
            int g = (((v >> 5) & 63) * ia + cg * alpha) >> 8;
            int b = ((v & 31) * ia + cb * alpha) >> 8;
            row[xx] = gfx_swap((uint16_t)((r << 11) | (g << 5) | b));
        }
    }
}

void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t color)
{
    if (rx < 1) {
        rx = 1;
    }
    if (rx > GFX_MAX_RX) {
        rx = GFX_MAX_RX;
    }
    if (ry < 1) {
        ry = 1;
    }
    if (ry > GFX_MAX_RX) {
        ry = GFX_MAX_RX;
    }
    if (cy + ry < s_y0 || cy - ry >= s_y0 + s_rows) {
        return;
    }
    for (int dy = -ry; dy <= ry; dy++) {
        int ady = dy < 0 ? -dy : dy;
        int hw = (rx <= TAB_RX && ry <= TAB_RY) ? s_hw[rx][ry][ady] : hw_calc(rx, ry, ady);
        if (hw > 0 || dy == 0) {
            gfx_hline(cx - hw, cx + hw + 1, cy + dy, color);
        }
    }
}

static int glyph_index(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');   /* the font is capitals only */
    }
    const char *p = strchr(g_font_order, ch);
    return p ? (int)(p - g_font_order) : -1;
}

void gfx_text(const char *text, int x, int y, int scale, uint16_t color)
{
    if (y + 7 * scale <= s_y0 || y >= s_y0 + s_rows) {
        return;   /* nothing of this line in the band */
    }
    for (; *text; text++) {
        int gi = glyph_index(*text);
        if (gi < 0) {
            gi = glyph_index('?');
        }
        const uint8_t *glyph = g_font[gi];
        for (int row = 0; row < 7; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 5; col++) {
                if (bits & (1u << col)) {
                    gfx_fill(x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

int gfx_text_w(const char *text, int scale)
{
    int n = 0;
    while (text[n]) {
        n++;
    }
    return n ? n * 6 * scale - scale : 0;
}
