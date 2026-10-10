// Coin Carnival: title, play, pause, prize counter, collection; saving;
// the frame loop with partial redraw.
//
// Drawing: only what changed goes to the panel. In play the field window
// (FIELD_X0..X1 x FIELD_Y0..Y1) is sent every frame; the HUD/display strip
// and the tray only when something on them changed. Menus redraw on input.
#include "game.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "art.h"
#include "boot.h"
#include "display.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx.h"
#include "input.h"
#include "machine.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sound.h"

static const char *TAG = "carnival";

typedef enum { ST_TITLE, ST_PLAY, ST_PAUSE, ST_COUNTER, ST_COLL } state_t;

static machine_t s_m;
static union {
    prog_blob_t prog;
    field_blob_t field;
} s_blob;
static prog_blob_t s_saved_prog;   /* to skip writes when nothing changed */
static nvs_handle_t s_nvs;
static bool s_nvs_ok;
static state_t s_state;
static int s_sel;
static bool s_armed;          /* reset progress: OK twice */
static bool s_swap;
static bool s_assistant;
static bool s_redraw = true;
static bool s_dim, s_screen_off;
static char s_msg[32];
static int s_msg_t;
static uint32_t s_frame;
static int s_prog_t, s_field_t;
static uint32_t s_sig_top, s_sig_tray;
/* frame stats */
static int64_t s_stat_t0;
static int s_stat_frames, s_stat_late;
static int64_t s_stat_render_us, s_stat_render_max, s_stat_step_us;

static void msg(const char *t)
{
    snprintf(s_msg, sizeof(s_msg), "%s", t);
    s_msg_t = 45;
    s_redraw = true;
}

/* ---- saving (NVS namespace 'pusher', shared partition: small blobs) ------ */

static void save_prog(void)
{
    if (!s_nvs_ok) {
        return;
    }
    machine_save_prog(&s_m, &s_blob.prog);
    s_blob.prog.sound = (uint8_t)sound_mode();
    s_blob.prog.swap = s_swap;
    if (memcmp(&s_blob.prog, &s_saved_prog, sizeof(s_saved_prog)) == 0) {
        return;   /* nothing new: spare the flash */
    }
    if (nvs_set_blob(s_nvs, "prog", &s_blob.prog, sizeof(prog_blob_t)) == ESP_OK) {
        nvs_commit(s_nvs);
        s_saved_prog = s_blob.prog;
    }
}

static void save_all(void)
{
    save_prog();
    if (!s_nvs_ok) {
        return;
    }
    machine_save_field(&s_m, &s_blob.field);
    size_t n = offsetof(field_blob_t, o) + (size_t)s_blob.field.n * sizeof(s_blob.field.o[0]);
    nvs_set_blob(s_nvs, "field", &s_blob.field, n);
    nvs_commit(s_nvs);
    s_field_t = 0;
}

static void load_all(void)
{
    machine_init(&s_m);
    if (nvs_open("pusher", NVS_READWRITE, &s_nvs) != ESP_OK) {
        return;
    }
    s_nvs_ok = true;
    size_t n = sizeof(prog_blob_t);
    if (nvs_get_blob(s_nvs, "prog", &s_blob.prog, &n) != ESP_OK || n != sizeof(prog_blob_t)) {
        return;
    }
    prog_blob_t p = s_blob.prog;
    if (!machine_load_prog(&s_m, &p)) {
        return;
    }
    s_saved_prog = p;
    s_swap = p.swap;
    n = sizeof(field_blob_t);
    memset(&s_blob.field, 0, sizeof(s_blob.field));
    if (nvs_get_blob(s_nvs, "field", &s_blob.field, &n) != ESP_OK || !machine_load_field(&s_m, &s_blob.field)) {
        machine_new_field(&s_m);   /* keep the progress, rebuild the machine */
    }
    machine_set_theme(&s_m, s_m.theme);
}

static sound_mode_t sound_mode_saved(void)
{
    return s_saved_prog.ver == PROG_VER && s_saved_prog.sound < SOUND_MODES ? (sound_mode_t)s_saved_prog.sound : SOUND_ALL;
}

/* ---- menus ------------------------------------------------------------------ */

enum { M_PLAY, M_COUNTER, M_COLL, M_SOUND, M_KEYS, M_RESET, M_ASSIST, M_RESUME, M_TITLE, M_THEME0, M_BACK = M_THEME0 + NTHEMES };

static int menu_items(int *ids)
{
    int n = 0;
    switch (s_state) {
    case ST_TITLE:
        ids[n++] = M_PLAY;
        ids[n++] = M_COUNTER;
        ids[n++] = M_COLL;
        ids[n++] = M_SOUND;
        ids[n++] = M_KEYS;
        ids[n++] = M_RESET;
        if (s_assistant) {
            ids[n++] = M_ASSIST;
        }
        break;
    case ST_PAUSE:
        ids[n++] = M_RESUME;
        ids[n++] = M_TITLE;
        ids[n++] = M_SOUND;
        if (s_assistant) {
            ids[n++] = M_ASSIST;
        }
        break;
    case ST_COUNTER:
        for (int i = 0; i < NTHEMES; i++) {
            ids[n++] = M_THEME0 + i;
        }
        ids[n++] = M_BACK;
        break;
    default:
        ids[n++] = M_BACK;
        break;
    }
    return n;
}

static void item_label(int id, char *buf, size_t n)
{
    switch (id) {
    case M_PLAY: snprintf(buf, n, "PLAY"); break;
    case M_COUNTER: snprintf(buf, n, "PRIZE COUNTER"); break;
    case M_COLL: snprintf(buf, n, "COLLECTION"); break;
    case M_SOUND: snprintf(buf, n, "%s", sound_mode_name(sound_mode())); break;
    case M_KEYS: snprintf(buf, n, "KEYS: %s", s_swap ? "UP=RIGHT" : "UP=LEFT"); break;
    case M_RESET: snprintf(buf, n, "%s", s_armed ? "RESET? OK AGAIN" : "RESET PROGRESS"); break;
    case M_ASSIST: snprintf(buf, n, "BACK TO ASSISTANT"); break;
    case M_RESUME: snprintf(buf, n, "RESUME"); break;
    case M_TITLE: snprintf(buf, n, "TITLE"); break;
    case M_BACK: snprintf(buf, n, "BACK"); break;
    default: {
        int t = id - M_THEME0;
        if (s_m.owned & (1 << t)) {
            snprintf(buf, n, "%s  %s", g_themes[t].en, t == s_m.theme ? "PLAYING" : "OWNED");
        } else {
            snprintf(buf, n, "%s  %dT", g_themes[t].en, g_themes[t].cost);
        }
    }
    }
}

static void set_state(state_t st, int sel)
{
    s_state = st;
    s_sel = sel;
    s_armed = false;
    s_redraw = true;
}

static void choose(int id)
{
    switch (id) {
    case M_PLAY:
    case M_RESUME:
        set_state(ST_PLAY, 0);
        s_m.hold_t = s_m.ok_t = s_m.edge_t = 0;
        s_m.rapid_acc = 0;
        break;
    case M_TITLE:
        save_all();
        set_state(ST_TITLE, 0);
        break;
    case M_COUNTER:
        set_state(ST_COUNTER, s_m.theme);
        break;
    case M_COLL:
        set_state(ST_COLL, 0);
        break;
    case M_BACK:
        set_state(ST_TITLE, id == M_BACK && s_state == ST_COUNTER ? 1 : 2);
        break;
    case M_SOUND:
        sound_set_mode((sound_mode_t)((sound_mode() + 1) % SOUND_MODES));
        sound_music(sound_mode() == SOUND_ALL ? MUSIC_CARNIVAL : MUSIC_NONE);
        save_prog();
        break;
    case M_KEYS:
        s_swap = !s_swap;
        save_prog();
        break;
    case M_RESET:
        if (!s_armed) {
            s_armed = true;
            return;
        }
        s_armed = false;
        {
            machine_init(&s_m);
            save_all();
            msg("PROGRESS RESET");
        }
        break;
    case M_ASSIST:
        save_all();
        boot_to_assistant();
        msg("ASSISTANT NOT FOUND");
        break;
    default: {
        int t = id - M_THEME0;
        char b[32];
        if (s_m.owned & (1 << t)) {
            if (t != s_m.theme) {
                machine_set_theme(&s_m, t);
                save_all();
            }
            snprintf(b, sizeof(b), "NOW PLAYING: %s", g_themes[t].en);
            sound_play(SFX_SELECT);
        } else if (s_m.tickets >= g_themes[t].cost) {
            s_m.tickets -= g_themes[t].cost;
            s_m.owned |= (uint8_t)(1 << t);
            machine_set_theme(&s_m, t);
            save_all();
            snprintf(b, sizeof(b), "UNLOCKED %s!", g_themes[t].en);
            sound_play(SFX_UNLOCK);
        } else {
            snprintf(b, sizeof(b), "NEED %ld MORE TICKETS", (long)(g_themes[t].cost - s_m.tickets));
            sound_play(SFX_LOCKED);
        }
        msg(b);
    }
    }
    s_redraw = true;
}

static void update_menu(const input_state_t *in)
{
    int ids[12], n = menu_items(ids);
    if (in->up_pressed) {
        s_sel = (s_sel + n - 1) % n;
        s_armed = false;
        sound_play(SFX_TICK);
    }
    if (in->down_pressed) {
        s_sel = (s_sel + 1) % n;
        s_armed = false;
        sound_play(SFX_TICK);
    }
    if (s_sel >= n) {
        s_sel = 0;
    }
    if (in->ok_pressed) {
        sound_play(SFX_SELECT);
        choose(ids[s_sel]);
    }
}

/* ---- drawing ------------------------------------------------------------------ */

#define PANEL PX(0x08, 0x08, 0x14)
#define GOLDC PX(0xff, 0xd2, 0x3a)
#define WHITE PX(0xff, 0xff, 0xff)
#define GREY PX(0x99, 0x99, 0xa8)

static void text_c(const char *t, int cx, int y, int scale, uint16_t c)
{
    gfx_text(t, cx - gfx_text_w(t, scale) / 2, y, scale, c);
}

static void draw_items(int y)
{
    int ids[12], n = menu_items(ids);
    char b[40];
    for (int i = 0; i < n; i++) {
        item_label(ids[i], b, sizeof(b));
        if (i == s_sel) {
            gfx_fill(60, y + i * 16 - 4, 200, 15, PX(0x40, 0x30, 0x10));
            gfx_fill(60, y + i * 16 - 4, 3, 15, GOLDC);
        }
        text_c(b, 160, y + i * 16, 1, i == s_sel ? GOLDC : WHITE);
    }
}

static void draw_overlay(void)
{
    char b[48];
    if (s_state == ST_PLAY) {
        return;
    }
    gfx_blend(0, 0, SCREEN_W, SCREEN_H, PANEL, 200);
    switch (s_state) {
    case ST_TITLE:
        text_c("COIN CARNIVAL", 161, 23, 3, PX(0x80, 0x40, 0x10));
        text_c("COIN CARNIVAL", 160, 22, 3, GOLDC);
        for (int i = 0; i < 9; i++) {
            art_coin_icon(64 + i * 24, 54 + (i % 2) * 2);
        }
        text_c("A PUSHER ARCADE - VIRTUAL COINS ONLY", 160, 66, 1, GREY);
        draw_items(86);
        snprintf(b, sizeof(b), "COINS %ld   TICKETS %ld", (long)s_m.bank, (long)s_m.tickets);
        text_c(b, 160, 204, 1, PX(0xff, 0x9a, 0xd5));
        text_c("UP/DOWN AIM  OK DROP  HOLD OK RAPID", 160, 220, 1, GREY);
        break;
    case ST_PAUSE:
        text_c("PAUSED", 160, 40, 3, WHITE);
        draw_items(90);
        text_c("HOLD UP OR DOWN AT A SIDE TO PAUSE", 160, 200, 1, GREY);
        break;
    case ST_COUNTER:
        text_c("PRIZE COUNTER", 160, 22, 2, GOLDC);
        snprintf(b, sizeof(b), "YOU HAVE %ld TICKETS", (long)s_m.tickets);
        text_c(b, 160, 46, 1, PX(0xff, 0x9a, 0xd5));
        text_c("TRADE TICKETS FOR NEW MACHINES", 160, 60, 1, GREY);
        draw_items(86);
        break;
    case ST_COLL: {
        text_c("COLLECTION", 160, 14, 2, GOLDC);
        int got = 0;
        for (int t = 0; t < NTHEMES; t++) {
            int y = 50 + t * 38;
            gfx_text(g_themes[t].en, 12, y - 4, 1, (s_m.owned & (1 << t)) ? WHITE : GREY);
            if (s_m.sets & (1 << t)) {
                gfx_text("SET!", 12, y + 6, 1, GOLDC);
            }
            for (int i = 0; i < 6; i++) {
                int pid = t * 6 + i, cx = 118 + i * 34;
                art_prize(pid, cx, y, 10, s_m.coll[pid] > 0);
                if (s_m.coll[pid]) {
                    got++;
                    snprintf(b, sizeof(b), "%d", s_m.coll[pid]);
                    text_c(b, cx, y + 10, 1, WHITE);
                }
            }
        }
        snprintf(b, sizeof(b), "%d / %d PRIZES   FULL SET +100T", got, NPRIZE);
        text_c(b, 160, 202, 1, GREY);
        draw_items(220);
        break;
    }
    default:
        break;
    }
    if (s_msg_t > 0 && s_state != ST_PLAY) {
        gfx_fill(40, 186, 240, 13, PX(0x30, 0x20, 0x40));
        text_c(s_msg, 160, 189, 1, WHITE);
    }
}

static void view(art_view_t *v)
{
    v->frame = s_frame;
    v->msg = s_m.nocoin_t ? "NO COINS - REFILL SOON" : (s_msg_t > 0 && s_state == ST_PLAY ? s_msg : NULL);
    v->edge_pct = s_m.edge_t > 6 ? s_m.edge_t * 100 / EDGE_HOLD_STEPS : 0;
}

/* compose one window of the screen and send it, band by band */
static void paint(int x0, int x1, int y0, int y1, bool overlay)
{
    art_view_t v;
    view(&v);
    int w = x1 - x0, rows_max = SCREEN_W * BAND_ROWS / w;
    for (int y = y0; y < y1; y += rows_max) {
        int rows = y1 - y < rows_max ? y1 - y : rows_max;
        gfx_band_begin_win(x0, w, y, rows, 0);
        art_machine(&s_m, &v);
        if (overlay) {
            draw_overlay();
        }
        display_send_rect(x0, y, w, gfx_band_buf(), rows);
    }
}

static uint32_t mixsig(uint32_t h, uint32_t v)
{
    return (h ^ v) * 16777619u;
}

static void render(void)
{
    art_prepare(&s_m);
    if (s_redraw) {
        s_redraw = false;
        paint(0, SCREEN_W, 0, SCREEN_H, true);
        s_sig_top = s_sig_tray = 0;
        return;
    }
    if (s_state != ST_PLAY) {
        return;   /* menus are still: nothing to send */
    }
    /* top strip: HUD + display */
    uint32_t h = 2166136261u;
    h = mixsig(h, (uint32_t)s_m.bank);
    h = mixsig(h, (uint32_t)s_m.tickets);
    h = mixsig(h, (uint32_t)machine_jp(&s_m));
    h = mixsig(h, (uint32_t)s_m.slot_q);
    h = mixsig(h, (uint32_t)art_lamp_phase(s_frame));
    h = mixsig(h, s_m.slot.on | (s_m.wheel.on << 1) | (s_m.jpc.on << 2));
    if (s_m.slot.on || s_m.wheel.on || s_m.jpc.on) {
        h = mixsig(h, s_frame);   /* animating */
    }
    if (h != s_sig_top) {
        s_sig_top = h;
        paint(0, SCREEN_W, 0, FIELD_Y0, false);
    }
    paint(FIELD_X0, FIELD_X1, FIELD_Y0, FIELD_Y1, false);
    art_view_t v;
    view(&v);
    h = 2166136261u;
    h = mixsig(h, s_m.st.won);
    h = mixsig(h, s_m.st.dropped);
    h = mixsig(h, s_m.tray_flash > 0);
    h = mixsig(h, (uint32_t)v.edge_pct);
    h = mixsig(h, v.msg ? (uint32_t)(uintptr_t)v.msg + (uint32_t)v.msg[0] : 0);
    if (h != s_sig_tray) {
        s_sig_tray = h;
        paint(0, SCREEN_W, FIELD_Y1, SCREEN_H, false);
    }
}

/* ---- power: dim the menus, then switch the panel off on the title -------- */

static bool power_update(const input_state_t *in)
{
    if (in->any_pressed && (s_dim || s_screen_off)) {
        if (s_screen_off) {
            display_power(true);
            s_screen_off = false;
            s_redraw = true;
        }
        display_set_backlight(100);
        s_dim = false;
        return false;   /* the waking press does nothing else */
    }
    if (s_state == ST_PLAY) {
        return true;
    }
    float idle = input_idle_s();
    if (!s_dim && idle > DIM_AFTER_S) {
        display_set_backlight(DIM_PCT);
        s_dim = true;
    }
    if (!s_screen_off && s_state == ST_TITLE && idle > SCREEN_OFF_AFTER_S) {
        display_power(false);
        s_screen_off = true;
    }
    return true;
}

/* ---- loop ------------------------------------------------------------------- */

static void step(const input_state_t *in)
{
    s_frame++;
    if (s_msg_t > 0 && --s_msg_t == 0 && s_state != ST_PLAY) {
        s_redraw = true;
    }
    if (s_state != ST_PLAY) {
        if (in->any_pressed) {
            s_redraw = true;
        }
        update_menu(in);
        return;
    }
    int key = in->up ? KEY_UP : in->down ? KEY_DOWN : in->ok ? KEY_OK : KEY_NONE;
    if (machine_input(&s_m, key, in->ok_pressed, s_swap)) {
        sound_play(SFX_PAUSE);
        save_all();
        set_state(ST_PAUSE, 0);
        return;
    }
    int64_t t0 = esp_timer_get_time();
    machine_step(&s_m);
    s_stat_step_us += esp_timer_get_time() - t0;
    sound_set_hurry(s_m.slot.on || s_m.wheel.on || s_m.jpc.on);
    if (++s_prog_t >= SAVE_PROG_STEPS) {
        s_prog_t = 0;
        save_prog();
    }
    if (++s_field_t >= SAVE_FIELD_STEPS) {
        save_all();
    }
}

void game_run(void)
{
    field_init_tables();
    art_init();
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        /* Never erase here: the assistant's settings live in the same NVS. */
        ESP_LOGW(TAG, "nvs init: %s; progress not kept", esp_err_to_name(err));
        machine_init(&s_m);
    } else {
        load_all();
    }
    rng_seed((uint32_t)esp_timer_get_time() ^ 0x9e3779b9u);
    sound_init(sound_mode_saved());
    sound_music(sound_mode() == SOUND_ALL ? MUSIC_CARNIVAL : MUSIC_NONE);
    s_assistant = boot_assistant_available();
    ESP_LOGI(TAG, "bank %ld, tickets %ld, %d objects, machine %u bytes, assistant %s, free heap %u",
             (long)s_m.bank, (long)s_m.tickets, s_m.field.n, (unsigned)sizeof(machine_t),
             s_assistant ? "found" : "missing", (unsigned)esp_get_free_heap_size());
    set_state(ST_TITLE, 0);

    /* Fixed timestep: the machine always advances one 30 fps step, so the
     * stroke, the drops and the payout do not depend on how long a frame took.
     * If drawing falls behind, up to MAX_CATCHUP steps run before one draw. */
    int64_t next = esp_timer_get_time();
    s_stat_t0 = next;
    for (;;) {
        input_state_t in = input_take();
        if (!power_update(&in)) {
            memset(&in, 0, sizeof(in));
        }
        int64_t now = esp_timer_get_time();
        int steps = 1 + (int)((now - next) / FRAME_US);
        if (steps > MAX_CATCHUP) {
            steps = MAX_CATCHUP;
        }
        for (int i = 0; i < steps; i++) {
            step(&in);
            /* edges belong to the first step; later ones see only held keys */
            in.up_pressed = in.down_pressed = in.ok_pressed = false;
            in.up_released = in.down_released = in.ok_released = false;
            in.any_pressed = false;
        }
        if (!s_screen_off) {
            int64_t t0 = esp_timer_get_time();
            render();
            int64_t us = esp_timer_get_time() - t0;
            s_stat_render_us += us;
            if (us > s_stat_render_max) {
                s_stat_render_max = us;
            }
        }
        s_stat_frames++;
        next += s_screen_off ? 100000 : FRAME_US;   /* 30 fps; 10 Hz key polling when off */
        while (esp_timer_get_time() < next) {
            vTaskDelay(1);
        }
        if (esp_timer_get_time() - next > 100000) {
            next = esp_timer_get_time();   /* fell far behind; resync */
            s_stat_late++;
        }
        if (now - s_stat_t0 >= 10000000) {   /* every 10 s: frame rate, cost, heap */
            int fr = s_stat_frames ? s_stat_frames : 1;
            int step_avg = (int)(s_stat_step_us / fr), render_avg = (int)(s_stat_render_us / fr);
            ESP_LOGI(TAG, "fps %.1f, late %d, step avg %d us, render avg %d us max %d us, objs %d/%d, free heap %u, min %u",
                     s_stat_frames * 1e6 / (double)(now - s_stat_t0), s_stat_late, step_avg, render_avg,
                     (int)s_stat_render_max, s_m.field.n, s_m.field.cap, (unsigned)esp_get_free_heap_size(),
                     (unsigned)esp_get_minimum_free_heap_size());
            /* coin pool fallback: if play frames run long, cap the pool lower
             * (new coins from rewards go straight to the bank instead) */
            if (s_state == ST_PLAY && step_avg + render_avg > FRAME_US - 3000 && s_m.field.cap > 120) {
                s_m.field.cap = 120;
                ESP_LOGW(TAG, "frames run long: coin pool capped at 120");
            }
            s_stat_t0 = now;
            s_stat_frames = s_stat_late = 0;
            s_stat_render_us = s_stat_render_max = s_stat_step_us = 0;
        }
    }
}
