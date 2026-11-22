/* SPDX-License-Identifier: MIT */
#ifndef RTS_AUDIO_H
#define RTS_AUDIO_H

#include "rts/math3d.h"
#include "rts/planet.h"
#include "rts/vfs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RtsAudioChannel {
    RTS_AUD_EXTERIOR_AIRBORNE = 0, /* Silenced in orbital vacuum */
    RTS_AUD_HULL_CONDUCTED = 1,    /* Felt through deck/boots even in vacuum */
    RTS_AUD_RADIO_COMMS = 2,       /* Always clear on suit radio (unaffected by vacuum/muffle) */
    RTS_AUD_INTERIOR_SFX = 3       /* Interior consoles, alarms, hydraulic doors */
} RtsAudioChannel;

typedef struct RtsAudioVoice {
    char asset_path[128];
    RtsAudioChannel channel;
    float gain;
    float pan;
    float lowpass_muffle; /* 0.0 = crisp, 1.0 = heavily muffled */
    bool audible;
} RtsAudioVoice;

typedef struct RtsAudioStats {
    uint32_t voices_submitted;
    uint32_t voices_audible;
    uint32_t voices_silenced_vacuum;
    uint32_t wavs_decoded;
} RtsAudioStats;

typedef struct RtsAudioBackend {
    void *state;
    bool (*init)(void *state, int sample_rate);
    void (*shutdown)(void *state);
    void (*submit_voice)(void *state, const RtsAudioVoice *voice, const int16_t *pcm, size_t sample_count);
    RtsAudioStats (*get_stats)(void *state);
} RtsAudioBackend;

typedef struct RtsWavPcm {
    int sample_rate;
    int channels;
    int16_t *samples;
    size_t sample_count;
} RtsWavPcm;

typedef enum RtsMusicState {
    RTS_MUSIC_SOLARIS_ORBIT = 0,
    RTS_MUSIC_SURFACE_COMMAND = 1,
    RTS_MUSIC_RED_ALERT_BATTLE = 2
} RtsMusicState;

typedef struct RtsAudioEngine {
    RtsAudioBackend backend;
    const RtsVfs *vfs;
    RtsVec3d listener_pos;
    RtsVec3d listener_right;
    double listener_altitude_m;
    float interior_muffle;
    bool initialized;
    RtsMusicState music_state;
    double music_cooldown_s;
    RtsAudioVoice last_voice;
} RtsAudioEngine;

RtsAudioBackend rts_audio_null_create(void);
RtsAudioBackend rts_audio_sdl3_create(void);

bool rts_wav_decode_memory(const uint8_t *bytes, size_t size, RtsWavPcm *out_wav);
void rts_wav_free(RtsWavPcm *wav);

bool rts_audio_init(RtsAudioEngine *ae, RtsAudioBackend backend, const RtsVfs *vfs, int sample_rate);
void rts_audio_shutdown(RtsAudioEngine *ae);
void rts_audio_set_listener(RtsAudioEngine *ae, RtsVec3d world_pos, RtsVec3d right_axis, double altitude_m, float interior_muffle);
RtsAudioVoice rts_audio_play_3d(RtsAudioEngine *ae, const RtsPlanetConfig *planet, const char *wav_vfs_path, RtsVec3d src_pos, RtsAudioChannel channel, float base_gain, float max_dist_m);

/* Milestone 014: Full 27-asset external CC BY 4.0 SFX + adaptive Solaris music helpers */
size_t rts_audio_catalog_count(void);
const char *rts_audio_catalog_path(size_t index);
const char *rts_audio_music_track_path(RtsMusicState state);
bool rts_audio_update_music(RtsAudioEngine *ae, RtsMusicState target_state, double dt_s);
RtsAudioVoice rts_audio_play_event(RtsAudioEngine *ae, const RtsPlanetConfig *planet, const char *event_name, RtsVec3d src_pos, RtsAudioChannel channel);

#endif /* RTS_AUDIO_H */
