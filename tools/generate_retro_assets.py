#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generates external CC BY 4.0 retro space-opera 3D art assets in assets/ and mods/example_mod/assets/."""

import json
import math
import struct
import wave
import zlib
from pathlib import Path


def write_png(path: Path, w: int, h: int, rgba: bytearray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw.extend(rgba[y * stride : (y + 1) * stride])

    def chunk(tag: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.write_bytes(data)


def set_px(buf: bytearray, w: int, h: int, x: int, y: int, r: int, g: int, b: int, a: int = 255) -> None:
    if 0 <= x < w and 0 <= y < h:
        i = (y * w + x) * 4
        buf[i] = max(0, min(255, r))
        buf[i + 1] = max(0, min(255, g))
        buf[i + 2] = max(0, min(255, b))
        buf[i + 3] = max(0, min(255, a))


def gen_hull_texture(path: Path, base: tuple[int, int, int], trim: tuple[int, int, int], glow: tuple[int, int, int]) -> None:
    w, h = 64, 64
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            r, g, b = base
            # Panel seams
            if x % 16 == 0 or y % 16 == 0:
                r, g, b = int(r * 0.52), int(g * 0.52), int(b * 0.52)
            elif x % 16 == 1 or y % 16 == 1:
                r, g, b = min(255, int(r * 1.18)), min(255, int(g * 1.18)), min(255, int(b * 1.18))
            # Bold space-opera racing/livery stripe
            if 26 <= y <= 31:
                r, g, b = trim
            elif y in (25, 32):
                r, g, b = 30, 35, 48
            # Illuminated retro viewport windows
            if (8 <= y <= 12 or 44 <= y <= 48) and (4 <= (x % 16) <= 11):
                r, g, b = glow
            # Subtle pixel-art dither grain
            if ((x ^ y) & 3) == 0:
                r = min(255, r + 6)
                g = min(255, g + 6)
                b = min(255, b + 8)
            set_px(buf, w, h, x, y, r, g, b, 255)
    write_png(path, w, h, buf)


def gen_deck_floor(path: Path) -> None:
    w, h = 64, 64
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            r, g, b = 48, 58, 76
            if (x % 8 == 0) or (y % 8 == 0):
                r, g, b = 30, 38, 52
            elif ((x + y) % 4) == 0:
                r, g, b = 56, 68, 88
            # Bold yellow/cyan wayfinding corridor stripes
            if 4 <= x <= 7 or 56 <= x <= 59:
                r, g, b = 235, 188, 38
            elif 30 <= x <= 33:
                r, g, b = 45, 210, 245
            set_px(buf, w, h, x, y, r, g, b, 255)
    write_png(path, w, h, buf)


def gen_planet_surface(path: Path) -> None:
    w, h = 128, 64
    buf = bytearray(w * h * 4)
    for y in range(h):
        lat = (0.5 - y / float(h - 1)) * math.pi
        for x in range(w):
            lon = (x / float(w)) * 2.0 * math.pi
            nx = math.cos(lat) * math.cos(lon)
            ny = math.sin(lat)
            nz = math.cos(lat) * math.sin(lon)
            elev = math.sin(nx * 3.1 + 1.3) * math.cos(nz * 2.7 - 1.3) + 0.5 * math.sin(ny * 4.3 + nx * 2.0)
            elev += 0.25 * math.sin(nx * 9.0 - ny * 7.0) * math.cos(nz * 11.0)
            if abs(ny) > 0.84:
                r, g, b = 215, 238, 250  # Polar ice caps
            elif elev < -0.05:
                depth = max(0.0, min(1.0, (-elev) * 1.2))
                r, g, b = int(18 + 20 * (1 - depth)), int(75 + 60 * (1 - depth)), int(165 + 75 * (1 - depth))
            elif elev < 0.12:
                r, g, b = 210, 185, 115  # Coastal shelves
            elif elev < 0.55:
                r, g, b = 46, 152, 86    # Emerald continents
            else:
                r, g, b = 138, 126, 112  # Highland ridges
            # Retro coordinate grid lines
            if y % 16 == 0 or x % 16 == 0:
                r = min(255, r + 14)
                g = min(255, g + 22)
                b = min(255, b + 28)
            set_px(buf, w, h, x, y, r, g, b, 255)
    write_png(path, w, h, buf)


def gen_crew_8dir_sprite(path: Path) -> None:
    cell_w, cell_h = 16, 32
    w, h = cell_w * 8, cell_h
    buf = bytearray(w * h * 4)  # Transparent by default
    for d in range(8):
        ox = d * cell_w
        # Draw crisp retro 16x32 armored Solar Directorate Marine/Officer silhouette for angle d
        facing_front = d in (0, 1, 7)
        facing_side = d in (2, 6)
        # Helmet (rows 3..9)
        for y in range(3, 10):
            for x in range(5, 11):
                set_px(buf, w, h, ox + x, y, 225, 235, 250, 255)
        # Golden visor
        if facing_front:
            for x in range(6, 10):
                set_px(buf, w, h, ox + x, 6, 255, 210, 45, 255)
                set_px(buf, w, h, ox + x, 7, 255, 180, 25, 255)
        elif facing_side:
            vx = 9 if d == 2 else 5
            set_px(buf, w, h, ox + vx, 6, 255, 210, 45, 255)
            set_px(buf, w, h, ox + vx, 7, 255, 180, 25, 255)
        # Cobalt armor torso + shoulder pads (rows 10..20)
        for y in range(10, 21):
            for x in range(3, 13):
                col = (36, 98, 220) if y < 18 else (235, 188, 38)
                set_px(buf, w, h, ox + x, y, col[0], col[1], col[2], 255)
        # Glowing chest emblem / backpack
        for y in range(12, 16):
            for x in range(6, 10):
                set_px(buf, w, h, ox + x, y, 65, 230, 255, 255)
        # Plasma rifle silhouette offset by direction
        rx = 12 if d in (0, 1, 2, 3) else 2
        for y in range(13, 17):
            set_px(buf, w, h, ox + rx, y, 50, 58, 72, 255)
            set_px(buf, w, h, ox + rx + 1, y, 80, 240, 255, 255)
        # Armored legs & boots (rows 21..30)
        for y in range(21, 31):
            c = (28, 66, 150) if y < 27 else (42, 48, 60)
            for x in (4, 5, 6, 9, 10, 11):
                set_px(buf, w, h, ox + x, y, c[0], c[1], c[2], 255)
    write_png(path, w, h, buf)


# Compact 5x7 crisp bitmap font for ASCII 32..127 baked into external assets/fonts/retro_font.png
FONT_5X7 = {
    " ": [0, 0, 0, 0, 0, 0, 0],
    "!": [0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04],
    '"': [0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00],
    "#": [0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00],
    "$": [0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04],
    "%": [0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03],
    "&": [0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D],
    "'": [0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00],
    "(": [0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02],
    ")": [0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08],
    "*": [0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00],
    "+": [0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00],
    ",": [0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08],
    "-": [0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00],
    ".": [0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04],
    "/": [0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10],
    "0": [0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E],
    "1": [0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E],
    "2": [0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F],
    "3": [0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E],
    "4": [0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02],
    "5": [0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E],
    "6": [0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E],
    "7": [0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08],
    "8": [0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E],
    "9": [0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C],
    ":": [0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00],
    ";": [0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x08],
    "<": [0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02],
    "=": [0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00],
    ">": [0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08],
    "?": [0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04],
    "@": [0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E],
    "A": [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    "B": [0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E],
    "C": [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
    "D": [0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C],
    "E": [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F],
    "F": [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
    "G": [0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F],
    "H": [0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    "I": [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
    "J": [0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C],
    "K": [0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11],
    "L": [0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F],
    "M": [0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11],
    "N": [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11],
    "O": [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    "P": [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10],
    "Q": [0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D],
    "R": [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
    "S": [0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E],
    "T": [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
    "U": [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    "V": [0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04],
    "W": [0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11],
    "X": [0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11],
    "Y": [0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04],
    "Z": [0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F],
    "[": [0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E],
    "]": [0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E],
    "_": [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F],
    "|": [0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
}


def gen_font_atlas(path: Path) -> None:
    w, h = 128, 48  # 16 cols x 6 rows of 8x8 glyphs (ASCII 32..127)
    buf = bytearray(w * h * 4)
    for code in range(32, 128):
        ch = chr(code).upper()
        glyph = FONT_5X7.get(ch, FONT_5X7["?"])
        idx = code - 32
        gx = (idx % 16) * 8
        gy = (idx // 16) * 8
        for row in range(7):
            bits = glyph[row]
            for col in range(5):
                if bits & (1 << (4 - col)):
                    set_px(buf, w, h, gx + 1 + col, gy + row, 255, 255, 255, 255)
    write_png(path, w, h, buf)


class ObjBuilder:
    def __init__(self) -> None:
        self.v: list[tuple[float, float, float]] = []
        self.vt: list[tuple[float, float]] = [(0, 0), (1, 0), (1, 1), (0, 1)]
        self.f: list[tuple[int, int, int, int]] = []

    def add_box(self, cx: float, cy: float, cz: float, sx: float, sy: float, sz: float) -> None:
        hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
        base = len(self.v) + 1
        pts = [
            (cx - hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz + hz),
            (cx + hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz + hz),
            (cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz),
            (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz),
        ]
        self.v.extend(pts)
        quads = [
            (0, 1, 2, 3), (5, 4, 7, 6), (4, 0, 3, 7),
            (1, 5, 6, 2), (3, 2, 6, 7), (4, 5, 1, 0),
        ]
        for a, b, c, d in quads:
            self.f.append((base + a, base + b, base + c, base + d))

    def add_prism_wedge(self, cx: float, cy: float, cz: float, w_back: float, w_front: float, h_back: float, h_front: float, length: float) -> None:
        base = len(self.v) + 1
        hb, hf = h_back * 0.5, h_front * 0.5
        wb, wf = w_back * 0.5, w_front * 0.5
        zb, zf = cz + length * 0.5, cz - length * 0.5
        pts = [
            (cx - wb, cy - hb, zb), (cx + wb, cy - hb, zb), (cx + wb, cy + hb, zb), (cx - wb, cy + hb, zb),
            (cx - wf, cy - hf, zf), (cx + wf, cy - hf, zf), (cx + wf, cy + hf, zf), (cx - wf, cy + hf, zf),
        ]
        self.v.extend(pts)
        for a, b, c, d in [(0, 1, 2, 3), (5, 4, 7, 6), (4, 0, 3, 7), (1, 5, 6, 2), (3, 2, 6, 7), (4, 5, 1, 0)]:
            self.f.append((base + a, base + b, base + c, base + d))

    def save(self, path: Path, title: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        lines = [f"# {title} - CC BY 4.0 (1stPRTS / 1prtsg)"]
        for x, y, z in self.v:
            lines.append(f"v {x:.4f} {y:.4f} {z:.4f}")
        for u, v in self.vt:
            lines.append(f"vt {u:.3f} {v:.3f}")
        for a, b, c, d in self.f:
            lines.append(f"f {a}/1 {b}/2 {c}/3 {d}/4")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def gen_models(models_dir: Path) -> None:
    # 1. Planetary Command Citadel (tiered fortress + comms spire + hangar wings)
    m = ObjBuilder()
    m.add_box(0, 0.35, 0, 3.2, 0.7, 3.2)
    m.add_box(0, 1.0, 0, 2.2, 0.8, 2.2)
    m.add_box(0, 1.7, 0, 1.3, 0.7, 1.3)
    m.add_box(0, 2.45, 0, 0.25, 1.1, 0.25)
    m.add_box(-1.8, 0.55, 0, 0.9, 1.1, 1.5)
    m.add_box(1.8, 0.55, 0, 0.9, 1.1, 1.5)
    m.add_box(0, 0.45, 1.8, 1.4, 0.9, 0.8)
    m.save(models_dir / "citadel.obj", "Planetary Command Citadel")

    # 2. Troop/Cargo Dropship (walkable hull + sloped nose + twin VTOL wing pods)
    m = ObjBuilder()
    m.add_box(0, 0.6, 0.3, 1.4, 1.0, 2.8)
    m.add_prism_wedge(0, 0.55, -1.6, 1.4, 0.6, 1.0, 0.45, 1.2)
    m.add_box(-1.55, 0.85, 0.4, 1.8, 0.22, 1.1)
    m.add_box(1.55, 0.85, 0.4, 1.8, 0.22, 1.1)
    m.add_box(-2.45, 0.85, 0.4, 0.55, 0.65, 1.4)
    m.add_box(2.45, 0.85, 0.4, 0.55, 0.65, 1.4)
    m.add_box(0, 1.2, 1.4, 0.2, 0.7, 0.8)
    m.save(models_dir / "dropship.obj", "Troop Cargo Dropship")

    # 3. Plasma Heavy Tank (armored hull, side tracks, turret, twin plasma barrels)
    m = ObjBuilder()
    m.add_box(0, 0.45, 0, 2.2, 0.7, 3.6)
    m.add_box(-1.25, 0.35, 0, 0.45, 0.7, 3.9)
    m.add_box(1.25, 0.35, 0, 0.45, 0.7, 3.9)
    m.add_box(0, 1.05, 0.15, 1.5, 0.55, 1.8)
    m.add_box(-0.28, 1.08, -1.55, 0.18, 0.18, 2.1)
    m.add_box(0.28, 1.08, -1.55, 0.18, 0.18, 2.1)
    m.save(models_dir / "plasma_tank.obj", "Plasma Heavy Tank")

    # 4. Fleet Command Dreadnought (classic angular wedge + towering bridge + hangar sponsons)
    m = ObjBuilder()
    m.add_prism_wedge(0, 0, 0, 3.4, 0.45, 1.3, 0.35, 9.2)
    m.add_box(0, 1.1, 2.4, 1.2, 1.1, 1.6)
    m.add_box(0, 1.85, 2.7, 1.9, 0.55, 0.85)
    m.add_box(-2.1, -0.1, 2.2, 1.1, 0.75, 2.8)
    m.add_box(2.1, -0.1, 2.2, 1.1, 0.75, 2.8)
    m.add_box(-0.9, 0.0, 4.75, 0.8, 0.8, 0.6)
    m.add_box(0.9, 0.0, 4.75, 0.8, 0.8, 0.6)
    m.save(models_dir / "dreadnought.obj", "Fleet Command Dreadnought")

    # 5. Orbital Shipyard Drydock (colossal U-frame drydock gantries + central citadel spine)
    m = ObjBuilder()
    m.add_box(0, 0, 4.2, 6.2, 1.4, 1.6)
    m.add_box(-2.8, 0, -0.5, 0.8, 1.4, 8.8)
    m.add_box(2.8, 0, -0.5, 0.8, 1.4, 8.8)
    m.add_box(0, 2.1, -0.8, 6.4, 0.45, 0.7)
    m.add_box(0, 2.1, 2.2, 6.4, 0.45, 0.7)
    m.add_box(0, -1.8, 1.2, 6.4, 0.45, 0.7)
    m.add_box(0, 1.6, 4.2, 2.0, 1.8, 1.8)
    m.save(models_dir / "shipyard.obj", "Orbital Shipyard Drydock")

    # 6. Sleek Angular Starfighter
    m = ObjBuilder()
    m.add_prism_wedge(0, 0, 0, 0.9, 0.18, 0.55, 0.16, 3.8)
    m.add_prism_wedge(-1.4, -0.05, 0.5, 1.9, 0.4, 0.12, 0.08, 1.8)
    m.add_prism_wedge(1.4, -0.05, 0.5, 1.9, 0.4, 0.12, 0.08, 1.8)
    m.add_box(0, 0.35, 0.4, 0.42, 0.32, 1.1)
    m.add_box(0, 0.55, 1.5, 0.1, 0.75, 0.7)
    m.save(models_dir / "starfighter.obj", "Angular Starfighter")

    # 7. Low-Poly 1.75m 3D Crew / Marine Officer
    m = ObjBuilder()
    m.add_box(-0.16, 0.42, 0, 0.22, 0.84, 0.24)  # Left leg
    m.add_box(0.16, 0.42, 0, 0.22, 0.84, 0.24)   # Right leg
    m.add_box(0.0, 1.12, 0, 0.54, 0.62, 0.32)    # Armored torso
    m.add_box(-0.36, 1.12, 0, 0.18, 0.56, 0.20)  # Left arm
    m.add_box(0.36, 1.12, 0, 0.18, 0.56, 0.20)   # Right arm
    m.add_box(0.0, 1.60, 0, 0.30, 0.32, 0.30)    # Helmet
    m.add_box(0.0, 1.18, 0.22, 0.34, 0.44, 0.16) # Exo-suit pack
    m.save(models_dir / "crew_3d.obj", "Low-Poly 3D Crew Officer")


def gen_voxel_sample(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Optional Voxel Fortification Bunker Module (x y z palette_idx) - CC BY 4.0"]
    for y in range(5):
        for z in range(6):
            for x in range(6):
                on_wall = (x in (0, 5) or z in (0, 5))
                if y == 0 or (y == 4 and on_wall):
                    lines.append(f"{x} {y} {z} 1")
                elif on_wall and not (y in (1, 2) and z == 5 and x in (2, 3)):
                    pal = 2 if y == 3 else 1
                    lines.append(f"{x} {y} {z} {pal}")
    # Glowing cyan plasma shield emitter on roof center
    lines.append("2 4 2 4")
    lines.append("3 4 2 4")
    lines.append("2 4 3 4")
    lines.append("3 4 3 4")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def gen_wav_tone(path: Path, freq_start: float, freq_end: float, dur_s: float) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    sr = 22050
    n = int(sr * dur_s)
    samples = bytearray()
    phase = 0.0
    for i in range(n):
        t = i / float(n)
        f = freq_start + (freq_end - freq_start) * t
        phase += 2.0 * math.pi * f / sr
        env = max(0.0, 1.0 - t)
        val = int(math.sin(phase) * env * 14000.0)
        samples.extend(struct.pack("<h", val))
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        wf.writeframes(bytes(samples))


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    assets = root / "assets"

    gen_hull_texture(assets / "textures/hull_blue.png", (42, 86, 178), (242, 194, 42), (95, 240, 255))
    gen_hull_texture(assets / "textures/hull_red.png", (162, 34, 42), (240, 148, 32), (255, 220, 85))
    gen_deck_floor(assets / "textures/deck_floor.png")
    gen_planet_surface(assets / "textures/planet_surface.png")
    gen_crew_8dir_sprite(assets / "sprites/crew_8dir.png")
    gen_font_atlas(assets / "fonts/retro_font.png")
    gen_models(assets / "models")
    gen_voxel_sample(assets / "voxels/bunker_module.vxl")
    gen_wav_tone(assets / "audio/plasma_bolt.wav", 880.0, 180.0, 0.22)
    gen_wav_tone(assets / "audio/alert_klaxon.wav", 520.0, 640.0, 0.45)

    # Planet definition JSON
    pdir = assets / "planets"
    pdir.mkdir(parents=True, exist_ok=True)
    (pdir / "helios_prime.json").write_text(
        json.dumps(
            {
                "name": "Helios Prime",
                "radius_m": 6371000.0,
                "mu": 3.986004418e14,
                "atmo_ceiling_m": 120000.0,
                "seed": 42,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )

    # Sample mod override texture (Emerald Vanguard livery)
    gen_hull_texture(root / "mods/example_mod/assets/textures/hull_blue.png", (28, 145, 92), (250, 215, 55), (140, 255, 220))
    print("Generated all external retro space-opera assets cleanly.")


if __name__ == "__main__":
    main()
