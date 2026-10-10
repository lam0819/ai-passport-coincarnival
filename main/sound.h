// Chiptune sound: a tiny square/triangle/noise synth on the ES8311 speaker.
//
// One task renders 10 ms at a time: a looping tune (lead + bass) and one
// sound effect on top. Everything is synthesized, so it costs no flash and
// almost no RAM. Without a codec it stays silent and the game plays on.
#pragma once

#include <stdbool.h>

typedef enum {
    SFX_TICK, SFX_SELECT, SFX_PAUSE, SFX_DROP, SFX_LAND, SFX_CLINK, SFX_CHANCE, SFX_REEL,
    SFX_REEL_STOP, SFX_WIN, SFX_PRIZE, SFX_JPC, SFX_LOCKED, SFX_UNLOCK, SFX_COUNT,
} sfx_t;

typedef enum {
    MUSIC_NONE, MUSIC_CARNIVAL, MUSIC_JACKPOT, MUSIC_COUNT,
} music_t;

typedef enum { SOUND_ALL, SOUND_SFX_ONLY, SOUND_OFF, SOUND_MODES } sound_mode_t;

void sound_init(sound_mode_t mode);
void sound_play(sfx_t sfx);
void sound_music(music_t music);   // the jingles play once, the tunes loop
void sound_set_hurry(bool hurry);  // the tune runs faster during a bonus
void sound_set_mode(sound_mode_t mode);
sound_mode_t sound_mode(void);
const char *sound_mode_name(sound_mode_t mode);
