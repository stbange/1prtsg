/* SPDX-License-Identifier: MIT */
#include "rts/audio.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

typedef struct SdlAudioState {
    SDL_AudioStream *stream;
    RtsAudioStats stats;
} SdlAudioState;

static bool sdl_aud_init(void *state, int sample_rate) {
    SdlAudioState *s = (SdlAudioState *)state;
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            return true; /* Graceful silent fallback on headless CI */
        }
    }
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 1;
    spec.freq = sample_rate > 0 ? sample_rate : 22050;
    s->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (s->stream) {
        SDL_ResumeAudioStreamDevice(s->stream);
    }
    return true;
}

static void sdl_aud_shutdown(void *state) {
    SdlAudioState *s = (SdlAudioState *)state;
    if (s && s->stream) {
        SDL_DestroyAudioStream(s->stream);
        s->stream = NULL;
    }
    free(s);
}

static void sdl_aud_submit(void *state, const RtsAudioVoice *voice, const int16_t *pcm, size_t sample_count) {
    SdlAudioState *s = (SdlAudioState *)state;
    s->stats.voices_submitted++;
    if (!voice->audible) {
        if (voice->channel == RTS_AUD_EXTERIOR_AIRBORNE) s->stats.voices_silenced_vacuum++;
        return;
    }
    s->stats.voices_audible++;
    if (pcm && sample_count > 0) {
        s->stats.wavs_decoded++;
        if (s->stream) {
            int16_t *scaled = (int16_t *)malloc(sample_count * sizeof(int16_t));
            if (scaled) {
                float prev = 0.0f;
                float alpha = 1.0f - voice->lowpass_muffle * 0.78f;
                for (size_t i = 0; i < sample_count; ++i) {
                    float raw = (float)pcm[i] * voice->gain;
                    prev = prev + alpha * (raw - prev);
                    scaled[i] = (int16_t)rts_clampf(prev, -32767.0f, 32767.0f);
                }
                SDL_PutAudioStreamData(s->stream, scaled, (int)(sample_count * sizeof(int16_t)));
                free(scaled);
            }
        }
    }
}

static RtsAudioStats sdl_aud_stats(void *state) {
    return ((SdlAudioState *)state)->stats;
}

RtsAudioBackend rts_audio_sdl3_create(void) {
    SdlAudioState *s = (SdlAudioState *)calloc(1, sizeof(SdlAudioState));
    RtsAudioBackend b;
    memset(&b, 0, sizeof(b));
    b.state = s;
    b.init = sdl_aud_init;
    b.shutdown = sdl_aud_shutdown;
    b.submit_voice = sdl_aud_submit;
    b.get_stats = sdl_aud_stats;
    return b;
}
