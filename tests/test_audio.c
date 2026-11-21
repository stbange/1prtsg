/* SPDX-License-Identifier: MIT */
#include "rts/audio.h"
#include "unity.h"
#include <string.h>

static void test_wav_decoder_and_spatial_acoustics(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));

    /* Decode real external WAV file + reject malformed WAVs */
    uint8_t *bytes = NULL;
    size_t sz = 0;
    TEST_ASSERT_TRUE(rts_vfs_read_bytes(&vfs, "assets", "audio/plasma_bolt.wav", &bytes, &sz));
    RtsWavPcm wav;
    TEST_ASSERT_TRUE(rts_wav_decode_memory(bytes, sz, &wav));
    TEST_ASSERT_EQUAL_INT(22050, wav.sample_rate);
    TEST_ASSERT_TRUE(wav.sample_count > 1000u);
    rts_wav_free(&wav);
    rts_vfs_free(bytes);

    uint8_t bad[48];
    memset(bad, 0, sizeof(bad));
    TEST_ASSERT_FALSE(rts_wav_decode_memory(NULL, 0, &wav));
    TEST_ASSERT_FALSE(rts_wav_decode_memory(bad, sizeof(bad), &wav));
    memcpy(bad, "RIFF", 4);
    memcpy(bad + 8, "WAVE", 4);
    TEST_ASSERT_FALSE(rts_wav_decode_memory(bad, sizeof(bad), &wav));

    /* Spatial audio engine with interior muffling & orbital vacuum silence */
    RtsPlanetConfig p = rts_planet_earthlike(42u);
    RtsAudioEngine ae;
    RtsAudioBackend bad_b = rts_audio_null_create();
    TEST_ASSERT_FALSE(rts_audio_init(&ae, bad_b, &vfs, 0));
    bad_b.shutdown(bad_b.state);
    TEST_ASSERT_TRUE(rts_audio_init(&ae, rts_audio_null_create(), &vfs, 22050));

    /* Exterior sea-level listener */
    rts_audio_set_listener(&ae, rts_v3d(0, 0, 0), rts_v3d(1, 0, 0), 0.0, 0.0f);
    RtsAudioVoice v_ext = rts_audio_play_3d(&ae, &p, "audio/plasma_bolt.wav", rts_v3d(50, 0, 0), RTS_AUD_EXTERIOR_AIRBORNE, 1.0f, 200.0f);
    TEST_ASSERT_TRUE(v_ext.audible && v_ext.gain > 0.5f && v_ext.pan > 0.9f && v_ext.lowpass_muffle == 0.0f);

    /* Armored interior listener -> reduced gain & 65% low-pass muffle */
    rts_audio_set_listener(&ae, rts_v3d(0, 0, 0), rts_v3d(1, 0, 0), 0.0, 0.65f);
    RtsAudioVoice v_in = rts_audio_play_3d(&ae, &p, "audio/plasma_bolt.wav", rts_v3d(50, 0, 0), RTS_AUD_EXTERIOR_AIRBORNE, 1.0f, 200.0f);
    TEST_ASSERT_TRUE(v_in.audible && v_in.gain < v_ext.gain && v_in.lowpass_muffle > 0.6f);

    /* Interior SFX & hull-conducted channels */
    RtsAudioVoice v_hull = rts_audio_play_3d(&ae, &p, "audio/engine_hum.wav", rts_v3d(10, 0, 0), RTS_AUD_HULL_CONDUCTED, 0.8f, 100.0f);
    TEST_ASSERT_TRUE(v_hull.audible && v_hull.lowpass_muffle > 0.5f);
    RtsAudioVoice v_sfx = rts_audio_play_3d(&ae, &p, "audio/docking_clamp.wav", rts_v3d(5, 0, 0), RTS_AUD_INTERIOR_SFX, 0.9f, 100.0f);
    TEST_ASSERT_TRUE(v_sfx.audible && v_sfx.lowpass_muffle == 0.0f);

    /* Orbital vacuum (400km LEO): exterior airborne silenced, radio comms crystal clear! */
    rts_audio_set_listener(&ae, rts_v3d(0, 0, 0), rts_v3d(1, 0, 0), 400000.0, 0.0f);
    RtsAudioVoice v_vac = rts_audio_play_3d(&ae, &p, "audio/plasma_bolt.wav", rts_v3d(20, 0, 0), RTS_AUD_EXTERIOR_AIRBORNE, 1.0f, 200.0f);
    TEST_ASSERT_FALSE(v_vac.audible);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, v_vac.gain);

    RtsAudioVoice v_rad = rts_audio_play_3d(&ae, &p, "audio/alert_klaxon.wav", rts_v3d(9999, 0, 0), RTS_AUD_RADIO_COMMS, 0.95f, 100.0f);
    TEST_ASSERT_TRUE(v_rad.audible && v_rad.gain > 0.9f && v_rad.lowpass_muffle == 0.0f);

    RtsAudioStats st = ae.backend.get_stats(ae.backend.state);
    TEST_ASSERT_EQUAL_UINT32(6u, st.voices_submitted);
    TEST_ASSERT_EQUAL_UINT32(1u, st.voices_silenced_vacuum);
    TEST_ASSERT_TRUE(st.wavs_decoded >= 5u);

    rts_audio_shutdown(&ae);
    RtsAudioVoice v_off = rts_audio_play_3d(&ae, &p, "audio/plasma_bolt.wav", rts_v3d(0,0,0), RTS_AUD_RADIO_COMMS, 1.0f, 100.0f);
    TEST_ASSERT_FALSE(v_off.audible);
}

void register_audio_tests(void) {
    RUN_TEST(test_wav_decoder_and_spatial_acoustics);
}
