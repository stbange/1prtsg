#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Generates 1stPRTS's full external CC BY 4.0 audio suite under assets/audio/:
- 24 crisp retro-futurist 22050 Hz 16-bit mono WAV SFX clips
- 3 compact adaptive Solaris space-opera synth music loops (3.2s each)
All files stay loose outside the binary and are indexed in manifest.json.
"""

import math
import random
import struct
import wave
from pathlib import Path

SR = 22050
OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "audio"


def write_wav(name: str, samples: list[float]) -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / name
    peak = max((abs(s) for s in samples), default=1.0)
    scale = 0.88 / max(peak, 1e-6) if peak > 0.88 else 1.0
    pcm = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s * scale))
        pcm += struct.pack("<h", int(v * 32767.0))
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SR)
        wf.writeframes(bytes(pcm))


def env_ad(t: float, dur: float, atk: float = 0.01) -> float:
    if t < atk:
        return t / max(atk, 1e-6)
    return max(0.0, 1.0 - (t - atk) / max(dur - atk, 1e-6))


def gen_sfx() -> None:
    rng = random.Random(1337)

    # 1. alert_klaxon.wav — Red-alert bulkhead klaxon sweep
    dur = 0.45
    write_wav("alert_klaxon.wav", [
        (0.55 * math.sin(2 * math.pi * (460 + 280 * (i / (SR * dur))) * (i / SR))
         + 0.25 * math.sin(6 * math.pi * (460 + 280 * (i / (SR * dur))) * (i / SR)))
        * env_ad(i / SR, dur, 0.02)
        for i in range(int(SR * dur))
    ])

    # 2. docking_clamp.wav — Heavy magnetic docking lock thud + ring
    dur = 0.30
    write_wav("docking_clamp.wav", [
        (0.70 * math.sin(2 * math.pi * (120 * math.exp(-7 * (i / SR)) + 55) * (i / SR))
         + 0.25 * math.sin(2 * math.pi * 620 * (i / SR)) * math.exp(-14 * (i / SR))
         + 0.20 * rng.uniform(-1, 1) * math.exp(-30 * (i / SR)))
        * env_ad(i / SR, dur, 0.005)
        for i in range(int(SR * dur))
    ])

    # 3. engine_hum.wav — Deep fusion drive harmonic hum
    dur = 0.35
    write_wav("engine_hum.wav", [
        (0.45 * math.sin(2 * math.pi * 68 * (i / SR))
         + 0.25 * math.sin(2 * math.pi * 136 * (i / SR))
         + 0.12 * math.sin(2 * math.pi * 204 * (i / SR)))
        * (0.85 + 0.15 * math.sin(2 * math.pi * 6 * (i / SR)))
        for i in range(int(SR * dur))
    ])

    # 4. plasma_bolt.wav — Retro-sci-fi plasma lance chirp
    dur = 0.22
    write_wav("plasma_bolt.wav", [
        (0.65 * math.sin(2 * math.pi * (980 * math.exp(-9 * (i / SR)) + 160) * (i / SR))
         + 0.25 * math.sin(2 * math.pi * (1960 * math.exp(-11 * (i / SR)) + 280) * (i / SR)))
        * env_ad(i / SR, dur, 0.004)
        for i in range(int(SR * dur))
    ])

    # 5. footstep_metal.wav — Magnetic-boot deck clack
    dur = 0.12
    write_wav("footstep_metal.wav", [
        (0.55 * rng.uniform(-1, 1) * math.exp(-38 * (i / SR))
         + 0.40 * math.sin(2 * math.pi * 240 * (i / SR)) * math.exp(-26 * (i / SR))
         + 0.20 * math.sin(2 * math.pi * 1120 * (i / SR)) * math.exp(-48 * (i / SR)))
        for i in range(int(SR * dur))
    ])

    # 6. airlock_hiss.wav — Pressurized bulkhead airlock pneumatic hiss
    dur = 0.38
    lp = 0.0
    s6 = []
    for i in range(int(SR * dur)):
        t = i / SR
        n = rng.uniform(-1, 1)
        lp = lp * 0.72 + n * 0.28
        s6.append((0.65 * (n - lp) + 0.20 * math.sin(2 * math.pi * 150 * t) * math.exp(-18 * t)) * env_ad(t, dur, 0.03))
    write_wav("airlock_hiss.wav", s6)

    # 7. turbolift_chime.wav — Warm Solaris dual-tone elevator chime
    dur = 0.42
    s7 = []
    for i in range(int(SR * dur)):
        t = i / SR
        f = 587.33 if t < 0.16 else 880.0
        env = math.exp(-7.5 * (t if t < 0.16 else t - 0.16))
        s7.append((0.55 * math.sin(2 * math.pi * f * t) + 0.22 * math.sin(4 * math.pi * f * t)) * env)
    write_wav("turbolift_chime.wav", s7)

    # 8. beam_lance.wav — Sustained harmonic laser beam hum
    dur = 0.36
    write_wav("beam_lance.wav", [
        (0.45 * math.sin(2 * math.pi * 320 * (i / SR))
         + 0.30 * math.sin(2 * math.pi * 644 * (i / SR))
         + 0.18 * math.sin(2 * math.pi * 1280 * (i / SR)))
        * env_ad(i / SR, dur, 0.02)
        for i in range(int(SR * dur))
    ])

    # 9. torpedo_launch.wav — Tube ignition whoosh + low thump
    dur = 0.40
    write_wav("torpedo_launch.wav", [
        (0.60 * math.sin(2 * math.pi * (140 * math.exp(-5 * (i / SR)) + 45) * (i / SR))
         + 0.35 * rng.uniform(-1, 1) * math.exp(-5.5 * (i / SR)))
        * env_ad(i / SR, dur, 0.01)
        for i in range(int(SR * dur))
    ])

    # 10. aa_flak.wav — Rapid twin AA flak burst
    dur = 0.28
    s10 = []
    for i in range(int(SR * dur)):
        t = i / SR
        pulse = math.exp(-32 * (t % 0.09))
        s10.append((0.65 * rng.uniform(-1, 1) + 0.35 * math.sin(2 * math.pi * 180 * t)) * pulse)
    write_wav("aa_flak.wav", s10)

    # 11. orbital_ion_cannon.wav — Massive orbital ion discharge + sub-drop
    dur = 0.55
    write_wav("orbital_ion_cannon.wav", [
        (0.65 * math.sin(2 * math.pi * (420 * math.exp(-4.5 * (i / SR)) + 48) * (i / SR))
         + 0.35 * math.sin(2 * math.pi * 62 * (i / SR))
         + 0.25 * rng.uniform(-1, 1) * math.exp(-6 * (i / SR)))
        * env_ad(i / SR, dur, 0.015)
        for i in range(int(SR * dur))
    ])

    # 12. shield_ripple.wav — Crystalline hex-shield deflection shimmer
    dur = 0.28
    write_wav("shield_ripple.wav", [
        (0.45 * math.sin(2 * math.pi * (1180 + 180 * math.sin(2 * math.pi * 24 * (i / SR))) * (i / SR))
         + 0.35 * math.sin(2 * math.pi * 1760 * (i / SR)))
        * env_ad(i / SR, dur, 0.008)
        for i in range(int(SR * dur))
    ])

    # 13. emp_shockwave.wav — Descending EMP pulse sweep + crackle
    dur = 0.48
    write_wav("emp_shockwave.wav", [
        (0.60 * math.sin(2 * math.pi * (1400 * math.exp(-6.5 * (i / SR)) + 52) * (i / SR))
         + 0.22 * rng.uniform(-1, 1) * (1.0 if (i % 97) < 12 else 0.15))
        * env_ad(i / SR, dur, 0.01)
        for i in range(int(SR * dur))
    ])

    # 14. hull_decompression.wav — Explosive air-rush white-noise sweep
    dur = 0.50
    s14 = []
    prev = 0.0
    for i in range(int(SR * dur)):
        t = i / SR
        n = rng.uniform(-1, 1)
        alpha = 0.15 + 0.65 * math.exp(-3.5 * t)
        prev = prev + alpha * (n - prev)
        s14.append(0.85 * prev * env_ad(t, dur, 0.015))
    write_wav("hull_decompression.wav", s14)

    # 15. escape_pod_launch.wav — Bolt shear + rocket burn
    dur = 0.45
    write_wav("escape_pod_launch.wav", [
        (0.50 * math.sin(2 * math.pi * 92 * (i / SR)) * math.exp(-10 * (i / SR))
         + 0.55 * rng.uniform(-1, 1) * (0.3 + 0.7 * min(1.0, (i / SR) * 6.0))
         + 0.25 * math.sin(2 * math.pi * (240 + 420 * (i / SR)) * (i / SR)))
        * env_ad(i / SR, dur, 0.01)
        for i in range(int(SR * dur))
    ])

    # 16. welding_arc.wav — 1:1 gantry arc welding sizzle
    dur = 0.30
    write_wav("welding_arc.wav", [
        (0.55 * rng.uniform(-1, 1) * (0.4 + 0.6 * math.sin(2 * math.pi * 60 * (i / SR)))
         + 0.25 * math.sin(2 * math.pi * 120 * (i / SR)))
        * env_ad(i / SR, dur, 0.015)
        for i in range(int(SR * dur))
    ])

    # 17. explosion_hull.wav — Deep hull detonation boom
    dur = 0.55
    s17 = []
    lp = 0.0
    for i in range(int(SR * dur)):
        t = i / SR
        n = rng.uniform(-1, 1)
        lp = lp * 0.86 + n * 0.14
        boom = math.sin(2 * math.pi * (88 * math.exp(-4.5 * t) + 34) * t)
        s17.append((0.65 * boom + 0.65 * lp) * env_ad(t, dur, 0.008))
    write_wav("explosion_hull.wav", s17)

    # 18. radar_ping.wav — Crisp sonar/orbital radar ping
    dur = 0.32
    write_wav("radar_ping.wav", [
        0.75 * math.sin(2 * math.pi * 1320 * (i / SR)) * math.exp(-10.5 * (i / SR))
        for i in range(int(SR * dur))
    ])

    # 19. comms_bleep.wav — Solaris suit radio telemetry chirp
    dur = 0.18
    write_wav("comms_bleep.wav", [
        0.65 * math.sin(2 * math.pi * (960 if (i / SR) < 0.08 else 1280) * (i / SR))
        * env_ad(i / SR, dur, 0.005)
        for i in range(int(SR * dur))
    ])

    # 20. ui_click.wav — Tactile console click
    dur = 0.06
    write_wav("ui_click.wav", [
        (0.65 * math.sin(2 * math.pi * 1480 * (i / SR)) + 0.25 * rng.uniform(-1, 1))
        * math.exp(-75 * (i / SR))
        for i in range(int(SR * dur))
    ])

    # 21. thruster_burn.wav — Orbital nozzle burn
    dur = 0.42
    s21 = []
    lp = 0.0
    for i in range(int(SR * dur)):
        t = i / SR
        lp = lp * 0.78 + rng.uniform(-1, 1) * 0.22
        s21.append((0.70 * lp + 0.25 * math.sin(2 * math.pi * 84 * t)) * env_ad(t, dur, 0.04))
    write_wav("thruster_burn.wav", s21)

    # 22. ocean_splash.wav — Naval hull wave splash
    dur = 0.38
    s22 = []
    lp = 0.0
    for i in range(int(SR * dur)):
        t = i / SR
        lp = lp * 0.65 + rng.uniform(-1, 1) * 0.35
        s22.append(0.75 * lp * (0.5 + 0.5 * math.sin(2 * math.pi * 4.5 * t)) * env_ad(t, dur, 0.04))
    write_wav("ocean_splash.wav", s22)

    # 23. hangar_bay_door.wav — Hydraulic hangar door rumble
    dur = 0.46
    write_wav("hangar_bay_door.wav", [
        (0.55 * math.sin(2 * math.pi * 54 * (i / SR))
         + 0.30 * math.sin(2 * math.pi * 109 * (i / SR))
         + 0.20 * rng.uniform(-1, 1) * 0.4)
        * env_ad(i / SR, dur, 0.04)
        for i in range(int(SR * dur))
    ])

    # 24. victory_fanfare.wav — Solaris brass-synth major arpeggio
    dur = 0.64
    notes = [293.66, 369.99, 440.00, 587.33]  # D4 F#4 A4 D5
    s24 = []
    for i in range(int(SR * dur)):
        t = i / SR
        idx = min(len(notes) - 1, int(t / 0.14))
        f = notes[idx]
        v = (0.45 * math.sin(2 * math.pi * f * t)
             + 0.22 * math.sin(4 * math.pi * f * t)
             + 0.14 * math.sin(6 * math.pi * f * t))
        s24.append(v * env_ad(t, dur, 0.02))
    write_wav("victory_fanfare.wav", s24)


def gen_music() -> None:
    # Compact 3.2-second seamless looping Solaris synth stems (~140 KB each)
    dur = 3.2
    n = int(SR * dur)

    # 25. music_solaris_orbit.wav — Warm analog pad + D Lydian arpeggio
    arp = [293.66, 369.99, 440.00, 554.37, 587.33, 554.37, 440.00, 369.99]
    s_orbit = []
    for i in range(n):
        t = i / SR
        fade = min(1.0, t / 0.08, (dur - t) / 0.08)
        pad = (0.28 * math.sin(2 * math.pi * 146.83 * t)
               + 0.20 * math.sin(2 * math.pi * 220.00 * t)
               + 0.14 * math.sin(2 * math.pi * 277.18 * t))
        step = int(t / 0.4) % len(arp)
        st = t % 0.4
        lead = 0.26 * math.sin(2 * math.pi * arp[step] * t) * math.exp(-4.0 * st)
        s_orbit.append((pad + lead) * fade)
    write_wav("music_solaris_orbit.wav", s_orbit)

    # 26. music_surface_command.wav — Determined retro-futurist synth pulse
    bass = [110.00, 110.00, 130.81, 146.83, 110.00, 110.00, 164.81, 146.83]
    s_surf = []
    for i in range(n):
        t = i / SR
        fade = min(1.0, t / 0.08, (dur - t) / 0.08)
        step = int(t / 0.4) % len(bass)
        st = t % 0.2
        b = 0.40 * math.sin(2 * math.pi * bass[step] * t) * math.exp(-5.0 * st)
        h = 0.18 * math.sin(2 * math.pi * (bass[step] * 2.0) * t)
        chime = 0.16 * math.sin(2 * math.pi * 440.0 * t) * math.exp(-6.0 * (t % 0.8))
        s_surf.append((b + h + chime) * fade)
    write_wav("music_surface_command.wav", s_surf)

    # 27. music_red_alert_battle.wav — Urgent 140 BPM tension ostinato
    ost = [146.83, 174.61, 146.83, 220.00, 146.83, 174.61, 207.65, 196.00]
    s_red = []
    for i in range(n):
        t = i / SR
        fade = min(1.0, t / 0.06, (dur - t) / 0.06)
        step = int(t / 0.2) % len(ost)
        st = t % 0.2
        saw = (0.38 * math.sin(2 * math.pi * ost[step] * t)
               + 0.22 * math.sin(4 * math.pi * ost[step] * t)
               + 0.14 * math.sin(6 * math.pi * ost[step] * t)) * math.exp(-5.5 * st)
        kick = 0.32 * math.sin(2 * math.pi * (95 * math.exp(-16 * (t % 0.4)) + 42) * t)
        s_red.append((saw + kick) * fade)
    write_wav("music_red_alert_battle.wav", s_red)


if __name__ == "__main__":
    gen_sfx()
    gen_music()
    wavs = sorted(OUT_DIR.glob("*.wav"))
    total_kb = sum(p.stat().st_size for p in wavs) / 1024.0
    print(f"Generated {len(wavs)} external CC BY 4.0 WAV files in {OUT_DIR} ({total_kb:.1f} KB total).")
