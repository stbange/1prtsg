/* SPDX-License-Identifier: MIT */
#include "rts/audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct NullAudioState {
    RtsAudioStats stats;
} NullAudioState;

static bool null_aud_init(void *state, int sample_rate) {
    (void)state;
    return sample_rate > 0;
}

static void null_aud_shutdown(void *state) {
    free(state);
}

static void null_aud_submit(void *state, const RtsAudioVoice *voice, const int16_t *pcm, size_t sample_count) {
    (void)pcm;
    NullAudioState *s = (NullAudioState *)state;
    s->stats.voices_submitted++;
    if (sample_count > 0) {
        s->stats.wavs_decoded++;
    }
    if (voice->audible) {
        s->stats.voices_audible++;
    } else if (voice->channel == RTS_AUD_EXTERIOR_AIRBORNE) {
        s->stats.voices_silenced_vacuum++;
    }
}

static RtsAudioStats null_aud_stats(void *state) {
    return ((NullAudioState *)state)->stats;
}

RtsAudioBackend rts_audio_null_create(void) {
    NullAudioState *s = (NullAudioState *)calloc(1, sizeof(NullAudioState));
    RtsAudioBackend b;
    memset(&b, 0, sizeof(b));
    b.state = s;
    b.init = null_aud_init;
    b.shutdown = null_aud_shutdown;
    b.submit_voice = null_aud_submit;
    b.get_stats = null_aud_stats;
    return b;
}

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool rts_wav_decode_memory(const uint8_t *bytes, size_t size, RtsWavPcm *out_wav) {
    if (!bytes || size < 44 || !out_wav) {
        return false;
    }
    memset(out_wav, 0, sizeof(*out_wav));
    if (memcmp(bytes, "RIFF", 4) != 0 || memcmp(bytes + 8, "WAVE", 4) != 0) {
        return false;
    }
    size_t pos = 12;
    int sr = 0, ch = 0, bps = 0;
    const uint8_t *data_ptr = NULL;
    uint32_t data_sz = 0;
    while (pos + 8 <= size) {
        uint32_t csz = rd_u32(bytes + pos + 4);
        if (pos + 8 + csz > size) {
            break;
        }
        if (memcmp(bytes + pos, "fmt ", 4) == 0 && csz >= 16) {
            ch = (int)rd_u16(bytes + pos + 10);
            sr = (int)rd_u32(bytes + pos + 12);
            bps = (int)rd_u16(bytes + pos + 22);
        } else if (memcmp(bytes + pos, "data", 4) == 0) {
            data_ptr = bytes + pos + 8;
            data_sz = csz;
        }
        pos += 8 + csz + (csz & 1u);
    }
    if (!data_ptr || data_sz < 2 || sr <= 0 || ch <= 0 || bps != 16) {
        return false;
    }
    size_t count = data_sz / 2u;
    int16_t *pcm = (int16_t *)malloc(count * sizeof(int16_t));
    if (!pcm) {
        return false;
    }
    memcpy(pcm, data_ptr, count * sizeof(int16_t));
    out_wav->sample_rate = sr;
    out_wav->channels = ch;
    out_wav->samples = pcm;
    out_wav->sample_count = count;
    return true;
}

void rts_wav_free(RtsWavPcm *wav) {
    if (!wav) return;
    free(wav->samples);
    wav->samples = NULL;
    wav->sample_count = 0;
}

bool rts_audio_init(RtsAudioEngine *ae, RtsAudioBackend backend, const RtsVfs *vfs, int sample_rate) {
    if (!ae || !backend.init || !backend.init(backend.state, sample_rate)) {
        return false;
    }
    memset(ae, 0, sizeof(*ae));
    ae->backend = backend;
    ae->vfs = vfs;
    ae->listener_right = rts_v3d(1.0, 0.0, 0.0);
    ae->initialized = true;
    return true;
}

void rts_audio_shutdown(RtsAudioEngine *ae) {
    if (!ae || !ae->initialized) return;
    if (ae->backend.shutdown) {
        ae->backend.shutdown(ae->backend.state);
    }
    ae->backend.state = NULL;
    ae->initialized = false;
}

void rts_audio_set_listener(RtsAudioEngine *ae, RtsVec3d world_pos, RtsVec3d right_axis, double altitude_m, float interior_muffle) {
    if (!ae) return;
    ae->listener_pos = world_pos;
    ae->listener_right = rts_v3d_norm(right_axis);
    ae->listener_altitude_m = altitude_m;
    ae->interior_muffle = rts_clampf(interior_muffle, 0.0f, 1.0f);
}

RtsAudioVoice rts_audio_play_3d(RtsAudioEngine *ae, const RtsPlanetConfig *planet, const char *wav_vfs_path, RtsVec3d src_pos, RtsAudioChannel channel, float base_gain, float max_dist_m) {
    RtsAudioVoice v;
    memset(&v, 0, sizeof(v));
    if (!ae || !ae->initialized || !wav_vfs_path) {
        return v;
    }
    strncpy(v.asset_path, wav_vfs_path, sizeof(v.asset_path) - 1);
    v.channel = channel;

    float gain = rts_clampf(base_gain, 0.0f, 1.0f);
    if (channel == RTS_AUD_RADIO_COMMS) {
        v.gain = gain;
        v.pan = 0.0f;
        v.lowpass_muffle = 0.0f;
        v.audible = gain > 0.001f;
    } else {
        double dist = rts_v3d_dist(ae->listener_pos, src_pos);
        float max_d = max_dist_m > 1.0f ? max_dist_m : 200.0f;
        float atten = rts_clampf(1.0f - (float)(dist / (double)max_d), 0.0f, 1.0f);
        gain *= atten * atten;

        if (dist > 0.05) {
            RtsVec3d dir = rts_v3d_norm(rts_v3d_sub(src_pos, ae->listener_pos));
            v.pan = rts_clampf((float)rts_v3d_dot(dir, ae->listener_right), -1.0f, 1.0f);
        }

        if (channel == RTS_AUD_EXTERIOR_AIRBORNE) {
            double atmo_factor = 1.0;
            if (planet) {
                double rho = rts_planet_atmo_density(planet, ae->listener_altitude_m);
                atmo_factor = rts_clampd(rho / (planet->atmo_rho0 > 1e-6 ? planet->atmo_rho0 : 1.225), 0.0, 1.0);
            }
            gain *= (float)atmo_factor;
            v.lowpass_muffle = ae->interior_muffle;
            gain *= (1.0f - ae->interior_muffle * 0.45f);
        } else if (channel == RTS_AUD_HULL_CONDUCTED) {
            v.lowpass_muffle = 0.55f;
        } else {
            v.lowpass_muffle = 0.0f;
        }
        v.gain = gain;
        v.audible = gain > 0.005f;
    }

    RtsWavPcm wav;
    memset(&wav, 0, sizeof(wav));
    if (ae->vfs && v.audible) {
        uint8_t *bytes = NULL;
        size_t sz = 0;
        if (rts_vfs_read_bytes(ae->vfs, "assets", wav_vfs_path, &bytes, &sz)) {
            rts_wav_decode_memory(bytes, sz, &wav);
            rts_vfs_free(bytes);
        }
    }

    ae->last_voice = v;
    if (ae->backend.submit_voice) {
        ae->backend.submit_voice(ae->backend.state, &v, wav.samples, wav.sample_count);
    }
    rts_wav_free(&wav);
    return v;
}
