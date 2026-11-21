#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generates external CC BY 4.0 Solaris Retro-Futurism 3D art assets in assets/ and mods/example_mod/assets/."""

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
        buf[i] = max(0, min(255, int(r)))
        buf[i + 1] = max(0, min(255, int(g)))
        buf[i + 2] = max(0, min(255, int(b)))
        buf[i + 3] = max(0, min(255, int(a)))


def lerp_c(c0: tuple[float, float, float], c1: tuple[float, float, float], t: float) -> tuple[float, float, float]:
    t = max(0.0, min(1.0, t))
    return (c0[0] + (c1[0] - c0[0]) * t, c0[1] + (c1[1] - c0[1]) * t, c0[2] + (c1[2] - c0[2]) * t)


def gen_hull_texture(
    path: Path,
    primary: tuple[int, int, int],
    secondary: tuple[int, int, int],
    trim: tuple[int, int, int],
    glow: tuple[int, int, int],
) -> None:
    """256x256 ultra-sleek brushed satin aerospace plating with subtle panel bevels & clean livery stripe."""
    w, h = 256, 256
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            # Silky vertical satin anodized metallic sheen
            ny = y / 255.0
            sheen = 0.90 + 0.16 * math.sin(ny * math.pi)
            use_sec = y >= 184
            base = secondary if use_sec else primary
            r, g, b = base[0] * sheen, base[1] * sheen, base[2] * sheen

            # Ultra-fine brushed aerospace grain
            grain = math.sin(x * 0.08 + y * 0.35) * 2.5
            r += grain
            g += grain
            b += grain + 1.0

            # Clean subtle macro-panel seams (128px spacing so models look sleek, not busy!)
            if x % 128 == 0 or y % 128 == 0:
                r, g, b = r * 0.62, g * 0.65, b * 0.72
            elif x % 128 == 1 or y % 128 == 1:
                r, g, b = min(255, r * 1.10), min(255, g * 1.10), min(255, b * 1.12)

            # Single crisp champagne-gold / threat-coral racing band + hairline energy seam
            if 164 <= y <= 174:
                r, g, b = trim[0], trim[1], trim[2]
            elif y in (162, 163, 175, 176):
                r, g, b = r * 0.50, g * 0.52, b * 0.58
            elif 178 <= y <= 181:
                r, g, b = glow[0], glow[1], glow[2]

            set_px(buf, w, h, x, y, r, g, b, 255)
    write_png(path, w, h, buf)


def gen_deck_floor(path: Path) -> None:
    """256x256 sleek slate-indigo hexagonal/beveled starship deck with delicate glowing light-guide inlays."""
    w, h = 256, 256
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            # Subtle brushed gunmetal-indigo deck plate
            tx, ty = x % 32, y % 32
            r, g, b = 30.0, 42.0, 64.0
            # Diagonal chamfered corner bevels on each 32px deck tile
            d_corner = min(tx + ty, (31 - tx) + ty, tx + (31 - ty), (31 - tx) + (31 - ty))
            if tx == 0 or ty == 0 or d_corner == 5:
                r, g, b = 14.0, 20.0, 34.0
            elif tx == 1 or ty == 1 or d_corner == 6:
                r, g, b = 46.0, 64.0, 92.0
            else:
                shade = 1.0 + 0.08 * math.sin((x + y) * 0.12)
                r, g, b = r * shade, g * shade, b * shade

            # Subtle recessed anti-slip micro-grid
            if (x % 8 == 4) and (y % 8 == 4):
                r += 7.0
                g += 10.0
                b += 14.0

            # Refined thin luminous cyan center-line + subtle gold edge guide rails
            if 125 <= x <= 130:
                r, g, b = 44.0, 232.0, 250.0
            elif x in (123, 124, 131, 132):
                r, g, b = 14.0, 48.0, 66.0
            elif (14 <= x <= 17) or (238 <= x <= 241):
                r, g, b = 238.0, 182.0, 66.0

            set_px(buf, w, h, x, y, r, g, b, 255)
    write_png(path, w, h, buf)


def gen_planet_surface(path: Path) -> None:
    """512x256 lush stylized Helios Prime world map (sapphire-teal seas, sage/ochre continents, cloud bands)."""
    w, h = 512, 256
    buf = bytearray(w * h * 4)
    for y in range(h):
        lat = (0.5 - y / float(h - 1)) * math.pi
        clat = math.cos(lat)
        slat = math.sin(lat)
        for x in range(w):
            lon = (x / float(w)) * 2.0 * math.pi
            nx = clat * math.cos(lon)
            ny = slat
            nz = clat * math.sin(lon)
            # Multi-octave stylized FBM terrain elevation
            e = (
                math.sin(nx * 2.8 + 1.1) * math.cos(nz * 2.6 - 1.2)
                + 0.55 * math.sin(ny * 4.2 + nx * 2.3 - nz * 1.4)
                + 0.28 * math.sin(nx * 7.5 - ny * 6.2) * math.cos(nz * 8.1 + 0.7)
                + 0.14 * math.sin(nx * 15.0 + nz * 14.0) * math.cos(ny * 13.0)
            )
            if abs(ny) > 0.82 + 0.05 * math.sin(lon * 5.0):
                c = (224.0, 242.0, 252.0)  # Glacial polar caps
            elif e < -0.02:
                # Deep sapphire -> turquoise continental shelves
                shelf = max(0.0, min(1.0, (e + 0.95) / 0.93))
                c = lerp_c((10.0, 38.0, 88.0), (26.0, 168.0, 198.0), shelf ** 2.2)
            elif e < 0.09:
                c = (218.0, 188.0, 128.0)  # Golden coastal shelves
            elif e < 0.45:
                t = (e - 0.09) / 0.36
                c = lerp_c((38.0, 146.0, 124.0), (88.0, 168.0, 118.0), t)  # Cool teal-sage lowlands
            elif e < 0.78:
                t = (e - 0.45) / 0.33
                c = lerp_c((168.0, 134.0, 96.0), (124.0, 94.0, 118.0), t)  # Warm ochre-violet mesas
            else:
                c = (214.0, 224.0, 238.0)  # Snow-dusted ridges

            # Wispy translucent stylized cloud swirls
            cloud = math.sin(nx * 5.0 + lon * 1.5 + math.sin(ny * 6.0) * 1.8) * math.cos(ny * 5.5 - nz * 3.0)
            if cloud > 0.56:
                ct = min(0.72, (cloud - 0.56) * 2.2)
                c = lerp_c(c, (242.0, 248.0, 255.0), ct)

            # Delicate holographic lat/lon grid hairlines
            if y % 32 == 0 or x % 32 == 0:
                c = lerp_c(c, (95.0, 235.0, 255.0), 0.14)

            set_px(buf, w, h, x, y, c[0], c[1], c[2], 255)
    write_png(path, w, h, buf)


def gen_ocean_texture(path: Path) -> None:
    """256x256 stylized shimmering sapphire-teal ocean with luminous wave caustics."""
    w, h = 256, 256
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            u = x / float(w) * 2.0 * math.pi
            v = y / float(h) * 2.0 * math.pi
            w1 = math.sin(u * 4.0 + v * 2.0) * math.cos(v * 3.0 - u * 1.0)
            w2 = math.sin(u * 9.0 - v * 7.0 + 1.1) * 0.45
            w3 = math.cos(u * 14.0 + v * 11.0) * 0.20
            val = w1 + w2 + w3
            base = lerp_c((9.0, 36.0, 74.0), (22.0, 118.0, 158.0), (val + 1.5) / 3.0)
            if val > 0.92:
                base = lerp_c(base, (158.0, 238.0, 252.0), min(1.0, (val - 0.92) * 2.8))
            elif val > 0.65:
                base = lerp_c(base, (58.0, 196.0, 224.0), 0.35)
            set_px(buf, w, h, x, y, base[0], base[1], base[2], 255)
    write_png(path, w, h, buf)


def gen_crt_console_texture(path: Path) -> None:
    """256x256 sleek frosted-obsidian holo-radar console with luminous cyan sweep & coral/gold contacts."""
    w, h = 256, 256
    buf = bytearray(w * h * 4)
    cx, cy = 128.0, 128.0
    for y in range(h):
        for x in range(w):
            border = x < 10 or x >= 246 or y < 10 or y >= 246
            if border:
                rim = (x in (10, 245) or y in (10, 245))
                col = (56, 228, 248) if rim else (24, 34, 54)
                set_px(buf, w, h, x, y, col[0], col[1], col[2], 255)
                continue
            dx, dy = x - cx, y - cy
            dist = math.sqrt(dx * dx + dy * dy)
            ang = math.atan2(dy, dx)
            # Subtle deep teal radar glow
            r, g, b = 7.0, 20.0, 34.0
            if y % 4 == 0:
                g += 6.0
                b += 10.0
            # Sweeping glowing radar wedge
            sweep = (ang + 0.8) % (2.0 * math.pi)
            if dist < 102.0 and sweep < 0.75:
                st = (1.0 - sweep / 0.75) ** 2.0
                r += 16.0 * st
                g += 115.0 * st
                b += 125.0 * st
            # Crisp concentric range rings & crosshairs
            if any(abs(dist - rad) < 1.3 for rad in (32.0, 66.0, 100.0)):
                r, g, b = 42.0, 232.0, 238.0
            elif (abs(dx) < 1.0 or abs(dy) < 1.0) and dist < 102.0:
                r, g, b = 22.0, 124.0, 146.0
            # Elliptical orbital trajectory line
            ell = abs(((dx / 86.0) ** 2 + (dy / 44.0) ** 2) - 1.0)
            if ell < 0.045:
                r, g, b = 248.0, 192.0, 64.0
            # Hostile coral-crimson & friendly cyan tactical contacts
            for bx, by, bc in [
                (44, -38, (255, 68, 104)),
                (54, -22, (255, 68, 104)),
                (-48, 26, (56, 245, 255)),
                (-22, -44, (248, 198, 72)),
            ]:
                bd = math.sqrt((dx - bx) ** 2 + (dy - by) ** 2)
                if bd < 4.5:
                    r, g, b = bc[0], bc[1], bc[2]
                elif bd < 8.0:
                    r, g, b = lerp_c((r, g, b), bc, 0.35)
            set_px(buf, w, h, x, y, r, g, b, 255)
    write_png(path, w, h, buf)


def gen_crew_8dir_sprite(path: Path) -> None:
    """256x64 (8 directions x 32x64) sleek stylized Solar Directorate armored marine spritesheet."""
    cw, ch = 32, 64
    w, h = cw * 8, ch
    buf = bytearray(w * h * 4)

    def fill_oval(ox: int, cx: float, cy: float, rx: float, ry: float, col: tuple[int, int, int]) -> None:
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                nx = (x - cx) / rx
                ny = (y - cy) / ry
                d = nx * nx + ny * ny
                if d <= 1.0:
                    shade = 0.82 + 0.24 * (1.0 - max(0.0, nx * 0.5 + ny * 0.4))
                    set_px(buf, w, h, ox + x, y, col[0] * shade, col[1] * shade, col[2] * shade, 255)

    for d in range(8):
        ox = d * cw
        front = d in (0, 1, 7)
        side = d in (2, 6)
        back = d in (3, 4, 5)

        # Sleek armored legs & tapered grav-boots (rows 38..61)
        for y in range(38, 62):
            t = (y - 38) / 23.0
            spread = 4.2 + 1.2 * t if not side else 1.8
            lw = 3.2 - 0.6 * t
            col = (34, 54, 88) if y < 50 else (210, 224, 240)
            for sgn in (-1.0, 1.0):
                lx = 16.0 + sgn * spread
                for x in range(int(lx - lw), int(lx + lw) + 1):
                    set_px(buf, w, h, ox + x, y, col[0], col[1], col[2], 255)
            # Cyan knee/boot trim
            if y in (49, 50, 59):
                for sgn in (-1.0, 1.0):
                    lx = int(16.0 + sgn * spread)
                    set_px(buf, w, h, ox + lx, y, 54, 242, 255, 255)

        # Tapered V-shaped glacier-ivory chest cuirass + cerulean waist (rows 18..39)
        for y in range(18, 40):
            t = (y - 18) / 21.0
            half_w = (9.5 - 3.6 * t) if not side else (6.2 - 1.5 * t)
            for x in range(int(16 - half_w), int(16 + half_w) + 1):
                if y < 33:
                    col = (224, 234, 246) if abs(x - 16) < half_w - 2.2 else (34, 62, 108)
                elif y < 36:
                    col = (242, 188, 68)  # Champagne gold belt
                else:
                    col = (32, 48, 78)
                shade = 0.86 + 0.18 * (1.0 - (x - 16) / (half_w + 1.0) * 0.5)
                set_px(buf, w, h, ox + x, y, col[0] * shade, col[1] * shade, col[2] * shade, 255)

        # Sculpted cerulean-gold shoulder pauldrons
        if not side:
            fill_oval(ox, 7.5, 22.0, 4.2, 4.0, (38, 72, 126))
            fill_oval(ox, 24.5, 22.0, 4.2, 4.0, (38, 72, 126))
            fill_oval(ox, 7.5, 21.0, 2.2, 1.5, (244, 192, 74))
            fill_oval(ox, 24.5, 21.0, 2.2, 1.5, (244, 192, 74))
        else:
            fill_oval(ox, 16.0, 22.0, 5.2, 4.2, (38, 72, 126))

        # Glowing cyan chest core / dorsal thruster pack
        if not back:
            fill_oval(ox, 16.0, 24.5, 2.5, 2.5, (52, 244, 255))
        else:
            for y in range(20, 33):
                for x in range(11, 22):
                    c = (40, 58, 90) if y < 30 else (52, 244, 255)
                    set_px(buf, w, h, ox + x, y, c[0], c[1], c[2], 255)

        # Rounded aerodynamic pearl-titanium helmet + radiant solar-gold wrap visor
        fill_oval(ox, 16.0, 11.5, 5.6, 6.4, (232, 240, 250))
        if front:
            fill_oval(ox, 16.0, 11.8, 4.2, 2.2, (252, 198, 66))
        elif side:
            vx = 19.5 if d == 2 else 12.5
            fill_oval(ox, vx, 11.8, 2.8, 2.2, (252, 198, 66))

        # Sleek diagonal cyan-tipped plasma carbine
        rx = 23 if d in (0, 1, 2, 3) else 6
        for k in range(10):
            gx = rx + (k // 2 if d in (0, 1, 2, 3) else -k // 2)
            gy = 28 - k // 3
            set_px(buf, w, h, ox + gx, gy, 42, 54, 76, 255)
            set_px(buf, w, h, ox + gx, gy + 1, 68, 244, 255, 255)

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
    w, h = 128, 48
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


# Multi-material vertex color palette (values > 1.08 trigger radiant emissive glow in mesh.frag!)
C_HULL = (1.00, 1.00, 1.00)
C_PALE = (1.06, 1.06, 1.07)
C_SLATE = (0.44, 0.54, 0.70)
C_DARK = (0.28, 0.34, 0.46)
C_GOLD = (1.04, 0.86, 0.46)
C_GLOW_CYAN = (0.35, 1.90, 2.15)
C_GLOW_GOLD = (2.05, 1.52, 0.46)
C_GLOW_CORAL = (2.15, 0.45, 0.65)


class SculptBuilder:
    """Parametric low-poly sculptor producing sleek chamfered/lofted/swept Wavefront .obj files with vertex colors."""

    def __init__(self) -> None:
        self.v: list[tuple[float, float, float, float, float, float]] = []
        self.vt: list[tuple[float, float]] = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        self.f: list[tuple[int, ...]] = []

    def add_vert(self, x: float, y: float, z: float, col: tuple[float, float, float] = C_HULL) -> int:
        self.v.append((x, y, z, col[0], col[1], col[2]))
        return len(self.v)

    def add_quad(self, a: int, b: int, c: int, d: int) -> None:
        self.f.append((a, b, c, d))

    def add_tri(self, a: int, b: int, c: int) -> None:
        self.f.append((a, b, c))

    def add_loft(
        self,
        rings: list[list[tuple[float, float, float]]],
        col: tuple[float, float, float] = C_HULL,
        cap_start: bool = True,
        cap_end: bool = True,
    ) -> None:
        if len(rings) < 2:
            return
        n = len(rings[0])
        idxs: list[list[int]] = []
        for ring in rings:
            idxs.append([self.add_vert(p[0], p[1], p[2], col) for p in ring])
        for r in range(len(idxs) - 1):
            r0, r1 = idxs[r], idxs[r + 1]
            for i in range(n):
                j = (i + 1) % n
                self.add_quad(r0[i], r0[j], r1[j], r1[i])
        if cap_start and n >= 3:
            cx = sum(p[0] for p in rings[0]) / n
            cy = sum(p[1] for p in rings[0]) / n
            cz = sum(p[2] for p in rings[0]) / n
            cidx = self.add_vert(cx, cy, cz, col)
            for i in range(n):
                j = (i + 1) % n
                self.add_tri(cidx, idxs[0][j], idxs[0][i])
        if cap_end and n >= 3:
            last = rings[-1]
            cx = sum(p[0] for p in last) / n
            cy = sum(p[1] for p in last) / n
            cz = sum(p[2] for p in last) / n
            cidx = self.add_vert(cx, cy, cz, col)
            for i in range(n):
                j = (i + 1) % n
                self.add_tri(cidx, idxs[-1][i], idxs[-1][j])

    @staticmethod
    def poly_ring_z(
        cx: float, cy: float, z: float, rx: float, ry: float, sides: int = 8, rot: float = math.pi / 8.0, flat_bottom: float = 0.0
    ) -> list[tuple[float, float, float]]:
        pts = []
        for i in range(sides):
            a = rot + (2.0 * math.pi * i) / sides
            px = cx + math.cos(a) * rx
            py = cy + math.sin(a) * ry
            if flat_bottom > 0.0 and math.sin(a) < -flat_bottom:
                py = cy - flat_bottom * ry
            pts.append((px, py, z))
        return pts

    @staticmethod
    def poly_ring_y(
        cx: float, y: float, cz: float, rx: float, rz: float, sides: int = 8, rot: float = math.pi / 8.0
    ) -> list[tuple[float, float, float]]:
        pts = []
        for i in range(sides):
            a = rot + (2.0 * math.pi * i) / sides
            pts.append((cx + math.cos(a) * rx, y, cz + math.sin(a) * rz))
        return pts

    def add_cylinder_y(
        self, cx: float, y0: float, y1: float, cz: float, r0: float, r1: float, sides: int = 10, col: tuple[float, float, float] = C_HULL
    ) -> None:
        self.add_loft(
            [self.poly_ring_y(cx, y0, cz, r0, r0, sides), self.poly_ring_y(cx, y1, cz, r1, r1, sides)],
            col=col,
        )

    def add_cylinder_z(
        self, cx: float, cy: float, z0: float, z1: float, rx0: float, ry0: float, rx1: float, ry1: float, sides: int = 10, col: tuple[float, float, float] = C_HULL
    ) -> None:
        self.add_loft(
            [self.poly_ring_z(cx, cy, z0, rx0, ry0, sides), self.poly_ring_z(cx, cy, z1, rx1, ry1, sides)],
            col=col,
        )

    def add_dome(
        self,
        cx: float,
        cy: float,
        cz: float,
        rx: float,
        ry: float,
        rz: float,
        sides: int = 10,
        stacks: int = 4,
        col: tuple[float, float, float] = C_HULL,
        full_sphere: bool = False,
    ) -> None:
        rings = []
        a_start = -math.pi * 0.48 if full_sphere else 0.0
        a_end = math.pi * 0.48
        for s in range(stacks + 1):
            t = s / float(stacks)
            phi = a_start + (a_end - a_start) * t
            cr = max(0.04, math.cos(phi))
            py = cy + math.sin(phi) * ry
            rings.append(self.poly_ring_y(cx, py, cz, rx * cr, rz * cr, sides))
        self.add_loft(rings, col=col, cap_start=True, cap_end=True)

    def add_swept_wing(
        self,
        root_x: float,
        root_y: float,
        root_z_lead: float,
        root_z_trail: float,
        root_thick: float,
        tip_x: float,
        tip_y: float,
        tip_z_lead: float,
        tip_z_trail: float,
        tip_thick: float,
        col: tuple[float, float, float] = C_HULL,
    ) -> None:
        """Sleek 4-point diamond airfoil wing (razor leading & trailing edges)."""
        r_mid = 0.35 * root_z_trail + 0.65 * root_z_lead
        t_mid = 0.35 * tip_z_trail + 0.65 * tip_z_lead
        r_ring = [
            (root_x, root_y, root_z_lead),
            (root_x, root_y + root_thick, r_mid),
            (root_x, root_y, root_z_trail),
            (root_x, root_y - root_thick * 0.7, r_mid),
        ]
        t_ring = [
            (tip_x, tip_y, tip_z_lead),
            (tip_x, tip_y + tip_thick, t_mid),
            (tip_x, tip_y, tip_z_trail),
            (tip_x, tip_y - tip_thick * 0.7, t_mid),
        ]
        if tip_x < root_x:
            self.add_loft([t_ring, r_ring], col=col)
        else:
            self.add_loft([r_ring, t_ring], col=col)

    def add_fin(
        self,
        x_base: float,
        y_base: float,
        z_lead: float,
        z_trail: float,
        x_tip: float,
        y_tip: float,
        z_tip_lead: float,
        z_tip_trail: float,
        thick: float = 0.08,
        col: tuple[float, float, float] = C_HULL,
    ) -> None:
        """Canted airfoil vertical/angled stabilizer fin."""
        bm = 0.5 * (z_lead + z_trail)
        tm = 0.5 * (z_tip_lead + z_tip_trail)
        r0 = [
            (x_base, y_base, z_lead),
            (x_base + thick, y_base, bm),
            (x_base, y_base, z_trail),
            (x_base - thick, y_base, bm),
        ]
        r1 = [
            (x_tip, y_tip, z_tip_lead),
            (x_tip + thick * 0.5, y_tip, tm),
            (x_tip, y_tip, z_tip_trail),
            (x_tip - thick * 0.5, y_tip, tm),
        ]
        self.add_loft([r0, r1], col=col)

    def add_torus_arc(
        self,
        cx: float,
        cy: float,
        cz: float,
        major_r: float,
        minor_r: float,
        minor_z: float,
        a0: float,
        a1: float,
        segs: int = 16,
        sides: int = 6,
        col: tuple[float, float, float] = C_HULL,
        plane: str = "xy",
    ) -> None:
        rings = []
        for s in range(segs + 1):
            a = a0 + (a1 - a0) * (s / float(segs))
            ca, sa = math.cos(a), math.sin(a)
            ring = []
            for k in range(sides):
                b = (2.0 * math.pi * k) / sides + math.pi / sides
                cb, sb = math.cos(b), math.sin(b)
                rr = major_r + cb * minor_r
                if plane == "xy":
                    ring.append((cx + ca * rr, cy + sa * rr, cz + sb * minor_z))
                elif plane == "xz":
                    ring.append((cx + ca * rr, cy + sb * minor_z, cz + sa * rr))
                else:
                    ring.append((cx + sb * minor_z, cy + sa * rr, cz + ca * rr))
            rings.append(ring)
        closed = abs((a1 - a0) - 2.0 * math.pi) < 1e-3
        self.add_loft(rings, col=col, cap_start=not closed, cap_end=not closed)

    def save(self, path: Path, title: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        lines = [f"# {title} - Solaris Sculpted Low-Poly CC BY 4.0 (1stPRTS / 1prtsg)"]
        for x, y, z, r, g, b in self.v:
            lines.append(f"v {x:.4f} {y:.4f} {z:.4f} {r:.3f} {g:.3f} {b:.3f}")
        for u, v in self.vt:
            lines.append(f"vt {u:.3f} {v:.3f}")
        for face in self.f:
            if len(face) == 4:
                lines.append(f"f {face[0]}/1 {face[1]}/2 {face[2]}/3 {face[3]}/4")
            else:
                lines.append(f"f {face[0]}/1 {face[1]}/2 {face[2]}/3")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def gen_models(models_dir: Path) -> None:
    # 1. Planetary Command Citadel (Syd Mead stepped octagonal acropolis + flying buttresses + dome rotunda + twin needles)
    m = SculptBuilder()
    # Octagonal stepped armored base plinth -> tapered command citadel tiers
    m.add_loft(
        [
            m.poly_ring_y(0, 0.0, 0, 2.35, 2.35, 8),
            m.poly_ring_y(0, 0.45, 0, 2.05, 2.05, 8),
            m.poly_ring_y(0, 0.55, 0, 1.75, 1.75, 8),
            m.poly_ring_y(0, 1.15, 0, 1.45, 1.45, 8),
            m.poly_ring_y(0, 1.30, 0, 1.15, 1.15, 8),
            m.poly_ring_y(0, 1.85, 0, 0.92, 0.92, 8),
        ],
        col=C_HULL,
    )
    # Glowing cyan panoramic War Room observation ring & gold crown collar
    m.add_cylinder_y(0, 1.48, 1.66, 0, 1.12, 1.05, 12, col=C_GLOW_CYAN)
    m.add_cylinder_y(0, 1.84, 1.96, 0, 0.98, 0.94, 12, col=C_GOLD)
    m.add_dome(0, 1.94, 0, 0.88, 0.52, 0.88, sides=12, stacks=4, col=C_PALE)
    # 4 Sweeping diagonal flying-buttress armor pylons around the citadel
    for i in range(4):
        ang = i * math.pi * 0.5 + math.pi * 0.25
        ca, sa = math.cos(ang), math.sin(ang)
        r0 = m.poly_ring_y(ca * 2.15, 0.0, sa * 2.15, 0.48, 0.48, 6)
        r1 = m.poly_ring_y(ca * 1.45, 0.85, sa * 1.45, 0.34, 0.34, 6)
        r2 = m.poly_ring_y(ca * 0.98, 1.70, sa * 0.98, 0.18, 0.18, 6)
        m.add_loft([r0, r1, r2], col=C_SLATE)
        m.add_cylinder_y(ca * 2.05, 0.02, 0.16, sa * 2.05, 0.38, 0.32, 8, col=C_GLOW_GOLD)
    # Sleek tapered twin comms needles rising above the War Room dome
    m.add_cylinder_y(0.0, 2.40, 3.65, 0.0, 0.14, 0.02, 8, col=C_SLATE)
    m.add_cylinder_y(0.0, 3.45, 3.62, 0.0, 0.06, 0.05, 8, col=C_GLOW_CYAN)
    m.add_cylinder_y(0.32, 2.25, 3.15, -0.18, 0.07, 0.015, 6, col=C_GOLD)
    m.save(models_dir / "citadel.obj", "Solaris Command Citadel")

    # 2. Troop/Cargo Dropship (Sleek twin-boom VTOL aerospace assault shuttle with curved canopy & tilt-thruster nacelles)
    m = SculptBuilder()
    # Sculpted curved nose -> faceted octagonal troop bay -> tapered rear boarding ramp
    m.add_loft(
        [
            m.poly_ring_z(0, 0.45, -2.55, 0.18, 0.12, 8, flat_bottom=0.5),
            m.poly_ring_z(0, 0.56, -1.85, 0.62, 0.38, 8, flat_bottom=0.65),
            m.poly_ring_z(0, 0.72, -0.95, 0.86, 0.58, 8, flat_bottom=0.72),
            m.poly_ring_z(0, 0.76,  0.95, 0.90, 0.62, 8, flat_bottom=0.72),
            m.poly_ring_z(0, 0.70,  1.85, 0.68, 0.48, 8, flat_bottom=0.65),
            m.poly_ring_z(0, 0.62,  2.35, 0.38, 0.22, 8, flat_bottom=0.5),
        ],
        col=C_HULL,
    )
    # Glowing solar-gold wrap-around aerodynamic cockpit canopy
    m.add_loft(
        [
            m.poly_ring_z(0, 0.72, -2.05, 0.25, 0.14, 6, flat_bottom=0.2),
            m.poly_ring_z(0, 0.94, -1.40, 0.52, 0.28, 6, flat_bottom=0.2),
            m.poly_ring_z(0, 0.98, -0.65, 0.46, 0.22, 6, flat_bottom=0.2),
        ],
        col=C_GLOW_GOLD,
    )
    # Swept gull-wings + sculpted circular tilt-rotor plasma thruster pods
    for sgn in (-1.0, 1.0):
        m.add_swept_wing(sgn * 0.75, 0.98, -0.45, 0.95, 0.14, sgn * 2.55, 1.15, 0.15, 1.15, 0.08, col=C_HULL)
        m.add_cylinder_z(sgn * 2.65, 1.08, -0.45, 1.25, 0.32, 0.34, 0.28, 0.30, sides=10, col=C_SLATE)
        m.add_cylinder_z(sgn * 2.65, 1.08, 1.20, 1.48, 0.26, 0.26, 0.18, 0.18, sides=10, col=C_GLOW_CYAN)
        m.add_cylinder_z(sgn * 2.65, 1.08, -0.58, -0.42, 0.24, 0.24, 0.31, 0.31, sides=10, col=C_GOLD)
        # Canted V-tail stabilizers
        m.add_fin(sgn * 0.45, 0.95, 1.05, 2.05, sgn * 1.15, 1.75, 1.55, 2.25, 0.06, col=C_SLATE)
        # Sleek ventral landing skids
        m.add_cylinder_z(sgn * 0.72, 0.14, -1.2, 1.2, 0.08, 0.07, 0.08, 0.07, sides=6, col=C_DARK)
    m.save(models_dir / "dropship.obj", "Solaris Aerospace VTOL Dropship")

    # 3. Plasma Heavy Tank (Low-slung knife-edge stealth hover/grav tank with faceted turret & twin rail barrels)
    m = SculptBuilder()
    # Aggressive sloped hexagonal wedge chassis
    m.add_loft(
        [
            m.poly_ring_z(0, 0.36, -2.35, 0.85, 0.14, 6),
            m.poly_ring_z(0, 0.48, -1.55, 1.42, 0.32, 6),
            m.poly_ring_z(0, 0.56,  0.40, 1.50, 0.38, 6),
            m.poly_ring_z(0, 0.54,  1.85, 1.32, 0.34, 6),
            m.poly_ring_z(0, 0.44,  2.25, 0.92, 0.20, 6),
        ],
        col=C_HULL,
    )
    # Angled side hover-armor skirts + glowing cyan grav-repulsor under-glow strips
    for sgn in (-1.0, 1.0):
        m.add_swept_wing(sgn * 1.12, 0.44, -1.95, 1.85, 0.18, sgn * 1.72, 0.24, -1.55, 1.70, 0.08, col=C_SLATE)
        m.add_cylinder_z(sgn * 1.18, 0.18, -1.45, 1.55, 0.12, 0.07, 0.12, 0.07, sides=6, col=C_GLOW_CYAN)
    # Sleek faceted stealth-diamond turret
    m.add_loft(
        [
            m.poly_ring_z(0, 0.92, -1.05, 0.42, 0.14, 6),
            m.poly_ring_z(0, 1.06, -0.35, 0.88, 0.28, 6),
            m.poly_ring_z(0, 1.08,  0.75, 0.82, 0.26, 6),
            m.poly_ring_z(0, 0.98,  1.25, 0.52, 0.18, 6),
        ],
        col=C_PALE,
    )
    # Commander gold optic dome + twin tapered plasma-rail barrels with glowing muzzle coils
    m.add_dome(0.28, 1.24, 0.35, 0.22, 0.16, 0.22, sides=8, stacks=3, col=C_GOLD)
    for sx in (-0.24, 0.24):
        m.add_cylinder_z(sx, 1.04, -3.15, -0.65, 0.07, 0.07, 0.11, 0.11, sides=8, col=C_DARK)
        m.add_cylinder_z(sx, 1.04, -3.28, -2.85, 0.09, 0.09, 0.08, 0.08, sides=8, col=C_GLOW_CYAN)
    m.save(models_dir / "plasma_tank.obj", "Solaris Grav-Plasma Heavy Tank")

    # 4. Fleet Command Dreadnought (Iconic Homeworld/Syd-Mead 680m dagger-prow capital spearhead)
    m = SculptBuilder()
    # Sleek faceted spearhead main hull with razor prow & wide terraced stern
    m.add_loft(
        [
            m.poly_ring_z(0, 0.00, -5.80, 0.18, 0.10, 8),
            m.poly_ring_z(0, 0.06, -3.60, 1.05, 0.45, 8),
            m.poly_ring_z(0, 0.12, -0.60, 1.95, 0.78, 8),
            m.poly_ring_z(0, 0.15,  2.80, 2.25, 0.92, 8),
            m.poly_ring_z(0, 0.08,  4.80, 1.75, 0.72, 8),
            m.poly_ring_z(0, 0.00,  5.35, 1.20, 0.46, 8),
        ],
        col=C_HULL,
    )
    # Glowing dorsal axial rail-lance trench down the spearhead nose
    m.add_cylinder_z(0, 0.46, -4.60, -0.60, 0.12, 0.09, 0.24, 0.14, sides=6, col=C_GLOW_CYAN)
    # Swept sculpted side broadside hangar wings & gold edge fins
    for sgn in (-1.0, 1.0):
        m.add_swept_wing(sgn * 1.45, 0.02, -1.40, 3.60, 0.42, sgn * 3.85, -0.18, 1.40, 4.20, 0.16, col=C_SLATE)
        m.add_swept_wing(sgn * 1.70, 0.34, 0.20, 3.40, 0.16, sgn * 3.40, 0.22, 1.80, 3.90, 0.06, col=C_GOLD)
        # Side broadside glowing hangar bay apertures
        m.add_cylinder_z(sgn * 2.35, 0.0, 1.6, 3.3, 0.14, 0.18, 0.14, 0.18, sides=6, col=C_GLOW_GOLD)
    # Swept-back terraced Command Bridge Tower with glowing golden panoramic CIC strip
    m.add_loft(
        [
            m.poly_ring_y(0, 0.72, 2.35, 0.95, 1.55, 8),
            m.poly_ring_y(0, 1.55, 2.75, 0.65, 1.05, 8),
            m.poly_ring_y(0, 2.15, 3.05, 1.25, 0.58, 8),
            m.poly_ring_y(0, 2.48, 3.12, 0.95, 0.42, 8),
        ],
        col=C_PALE,
    )
    m.add_cylinder_z(0, 2.12, 2.42, 2.70, 0.96, 0.14, 0.90, 0.12, sides=8, col=C_GLOW_GOLD)
    # Ventral dorsal fin + cluster of 4 glowing ion engine thruster bells
    m.add_fin(0.0, -0.55, 0.60, 4.20, 0.0, -1.65, 2.40, 4.50, 0.12, col=C_SLATE)
    for ex, ey in [(-0.85, 0.22), (0.85, 0.22), (-0.55, -0.26), (0.55, -0.26)]:
        m.add_cylinder_z(ex, ey, 4.95, 5.55, 0.38, 0.38, 0.28, 0.28, sides=10, col=C_DARK)
        m.add_cylinder_z(ex, ey, 5.45, 5.85, 0.27, 0.27, 0.14, 0.14, sides=10, col=C_GLOW_CYAN)
    m.save(models_dir / "dreadnought.obj", "Solaris Fleet Command Dreadnought")

    # 5. Orbital Shipyard Drydock (Dramatic curved 260-degree orbital ring-arch drydock + glowing docking ribs)
    m = SculptBuilder()
    # Twin massive sweeping circular drydock ring arches framing the docked capital ship!
    for z_ring in (-2.4, 0.6, 3.4):
        m.add_torus_arc(0, 0, z_ring, major_r=3.35, minor_r=0.34, minor_z=0.36, a0=-0.45, a1=math.pi + 0.45, segs=18, sides=8, col=C_HULL, plane="xy")
        m.add_torus_arc(0, 0, z_ring, major_r=3.02, minor_r=0.09, minor_z=0.14, a0=-0.30, a1=math.pi + 0.30, segs=16, sides=6, col=C_GLOW_CYAN, plane="xy")
    # Sweeping longitudinal drydock spine rails connecting the curved arches
    for ang in (-0.28, math.pi * 0.5, math.pi + 0.28):
        rx = math.cos(ang) * 3.35
        ry = math.sin(ang) * 3.35
        m.add_cylinder_z(rx, ry, -4.4, 4.6, 0.42, 0.42, 0.42, 0.42, sides=8, col=C_SLATE)
        m.add_cylinder_z(rx * 0.92, ry * 0.92, -4.1, 4.3, 0.12, 0.12, 0.12, 0.12, sides=6, col=C_GLOW_GOLD)
    # Zenith Circular Fusion Control Rotunda & Orbital Ring Crown
    m.add_cylinder_y(0, 3.45, 4.35, 3.4, 1.35, 0.95, 12, col=C_PALE)
    m.add_dome(0, 4.35, 3.4, 0.92, 0.55, 0.92, sides=12, stacks=4, col=C_GOLD)
    m.add_torus_arc(0, 3.85, 3.4, major_r=1.65, minor_r=0.12, minor_z=0.12, a0=0.0, a1=2.0 * math.pi, segs=20, sides=6, col=C_GLOW_CYAN, plane="xz")
    m.save(models_dir / "shipyard.obj", "Solaris Orbital Ring-Arch Shipyard")

    # 6. Sleek Angular Starfighter (Needle-nosed swept-delta interceptor with gold bubble canopy & twin cyan burners)
    m = SculptBuilder()
    m.add_loft(
        [
            m.poly_ring_z(0, 0.00, -2.45, 0.06, 0.05, 6),
            m.poly_ring_z(0, 0.04, -1.10, 0.36, 0.22, 6),
            m.poly_ring_z(0, 0.06,  0.65, 0.48, 0.28, 6),
            m.poly_ring_z(0, 0.02,  1.85, 0.38, 0.20, 6),
        ],
        col=C_HULL,
    )
    # Golden faceted bubble canopy
    m.add_dome(0, 0.22, -0.15, 0.24, 0.22, 0.68, sides=8, stacks=3, col=C_GLOW_GOLD)
    # Swept razor-thin delta wings + inverted wingtip winglets + canted twin tail fins
    for sgn in (-1.0, 1.0):
        m.add_swept_wing(sgn * 0.32, 0.02, -0.55, 1.45, 0.09, sgn * 2.35, -0.16, 0.75, 1.65, 0.04, col=C_HULL)
        m.add_fin(sgn * 2.30, -0.15, 0.75, 1.62, sgn * 2.55, 0.35, 1.15, 1.78, 0.03, col=C_GOLD)
        m.add_fin(sgn * 0.34, 0.16, 0.65, 1.75, sgn * 0.68, 0.88, 1.25, 1.92, 0.04, col=C_SLATE)
        # Twin glowing cyan afterburner cones
        m.add_cylinder_z(sgn * 0.22, 0.02, 1.75, 2.25, 0.14, 0.14, 0.06, 0.06, sides=8, col=C_GLOW_CYAN)
    m.save(models_dir / "starfighter.obj", "Solaris Delta-Wing Starfighter")

    # 7. Stylized Low-Poly 1.75m 3D Commander / Marine Officer (Sculpted silhouette, helmet visor, pauldrons, tapered armor)
    m = SculptBuilder()
    # Tapered armored legs & sleek grav-boots
    for sgn in (-1.0, 1.0):
        m.add_loft(
            [
                m.poly_ring_y(sgn * 0.15, 0.00,  0.03, 0.11, 0.15, 8),
                m.poly_ring_y(sgn * 0.15, 0.14,  0.00, 0.09, 0.10, 8),
                m.poly_ring_y(sgn * 0.15, 0.46, -0.02, 0.10, 0.11, 8),
                m.poly_ring_y(sgn * 0.14, 0.84,  0.00, 0.12, 0.12, 8),
            ],
            col=C_SLATE,
        )
    # Tapered V-shaped chest cuirass (narrow waist -> broad armored chest)
    m.add_loft(
        [
            m.poly_ring_y(0, 0.82, 0, 0.22, 0.14, 8),
            m.poly_ring_y(0, 1.02, 0, 0.20, 0.13, 8),
            m.poly_ring_y(0, 1.38, 0, 0.30, 0.18, 8),
            m.poly_ring_y(0, 1.48, 0, 0.22, 0.14, 8),
        ],
        col=C_HULL,
    )
    # Glowing cyan chest core emblem & gold belt ring
    m.add_cylinder_z(0, 1.24, -0.20, -0.14, 0.07, 0.07, 0.07, 0.07, sides=8, col=C_GLOW_CYAN)
    m.add_cylinder_y(0, 0.88, 0.95, 0, 0.225, 0.225, 8, col=C_GOLD)
    # Sculpted domed shoulder pauldrons + tapered articulated arms
    for sgn in (-1.0, 1.0):
        m.add_dome(sgn * 0.34, 1.32, 0.0, 0.13, 0.13, 0.14, sides=8, stacks=3, col=C_GOLD, full_sphere=True)
        m.add_loft(
            [
                m.poly_ring_y(sgn * 0.36, 1.30, 0.0, 0.09, 0.09, 8),
                m.poly_ring_y(sgn * 0.39, 1.05, -0.04, 0.08, 0.08, 8),
                m.poly_ring_y(sgn * 0.38, 0.82, -0.10, 0.06, 0.06, 8),
            ],
            col=C_SLATE,
        )
    # Sculpted rounded aerospace helmet + radiant solar-gold wrap visor + dorsal jetpack
    m.add_dome(0, 1.63, 0.0, 0.16, 0.18, 0.18, sides=10, stacks=4, col=C_PALE, full_sphere=True)
    m.add_dome(0, 1.64, -0.07, 0.14, 0.07, 0.14, sides=10, stacks=3, col=C_GLOW_GOLD, full_sphere=True)
    m.add_loft(
        [
            m.poly_ring_y(0, 1.04, 0.22, 0.16, 0.08, 6),
            m.poly_ring_y(0, 1.38, 0.21, 0.18, 0.09, 6),
        ],
        col=C_DARK,
    )
    m.save(models_dir / "crew_3d.obj", "Solaris Stylized 3D Officer")

    # 8. 340m Fleet Starfighter Carrier (Sleek wave-piercing trimaran angled-deck supercarrier + stealth ATC island)
    m = SculptBuilder()
    # Sleek tapered knife-bow main trimaran hull
    m.add_loft(
        [
            m.poly_ring_z(0, 0.55, -5.60, 0.25, 0.38, 6),
            m.poly_ring_z(0, 0.62, -3.40, 1.45, 0.55, 6),
            m.poly_ring_z(0, 0.65,  2.80, 1.65, 0.58, 6),
            m.poly_ring_z(0, 0.58,  5.10, 1.15, 0.45, 6),
        ],
        col=C_HULL,
    )
    # Wide chamfered angled flight deck + glowing cyan electromagnetic catapult strip
    m.add_loft(
        [
            m.poly_ring_z(-0.25, 1.12, -5.20, 1.05, 0.12, 6),
            m.poly_ring_z(-0.35, 1.16, -2.00, 2.05, 0.14, 6),
            m.poly_ring_z(-0.15, 1.16,  3.80, 1.95, 0.14, 6),
            m.poly_ring_z(-0.10, 1.12,  5.05, 1.35, 0.12, 6),
        ],
        col=C_SLATE,
    )
    m.add_cylinder_z(-0.42, 1.30, -4.90, 4.10, 0.12, 0.04, 0.12, 0.04, sides=6, col=C_GLOW_CYAN)
    # Twin wave-piercing trimaran outrigger hulls
    for sgn in (-1.0, 1.0):
        m.add_loft(
            [
                m.poly_ring_z(sgn * 2.15, 0.35, -3.20, 0.08, 0.18, 6),
                m.poly_ring_z(sgn * 2.15, 0.42, -0.50, 0.38, 0.35, 6),
                m.poly_ring_z(sgn * 2.15, 0.42,  3.20, 0.36, 0.32, 6),
                m.poly_ring_z(sgn * 2.15, 0.35,  4.40, 0.12, 0.16, 6),
            ],
            col=C_PALE,
        )
    # Sculpted stealth-faceted Starboard ATC Island Tower + golden wrap-around bridge glass + hexagonal radar dome
    m.add_loft(
        [
            m.poly_ring_y(1.38, 1.22, 0.90, 0.58, 1.45, 6),
            m.poly_ring_y(1.42, 2.35, 1.10, 0.44, 1.05, 6),
            m.poly_ring_y(1.42, 2.90, 1.20, 0.52, 0.72, 6),
            m.poly_ring_y(1.42, 3.25, 1.25, 0.36, 0.52, 6),
        ],
        col=C_HULL,
    )
    m.add_cylinder_y(1.42, 2.66, 2.86, 1.20, 0.54, 0.52, 8, col=C_GLOW_GOLD)
    m.add_dome(1.42, 3.45, 1.25, 0.34, 0.28, 0.34, sides=8, stacks=3, col=C_GLOW_CYAN, full_sphere=True)
    m.add_cylinder_y(1.42, 3.65, 4.45, 1.25, 0.06, 0.015, 6, col=C_GOLD)
    m.save(models_dir / "carrier.obj", "Solaris Trimaran Fleet Carrier")

    # 9. Architectural Vaulted Interior Rib Arch (Clean sleek continuous curved cathedral rib!)
    m = SculptBuilder()
    m.add_torus_arc(0, 0.0, 0, major_r=9.75, minor_r=0.52, minor_z=0.55, a0=-0.04, a1=math.pi + 0.04, segs=18, sides=8, col=C_SLATE, plane="xy")
    m.add_torus_arc(0, 0.0, 0, major_r=9.16, minor_r=0.10, minor_z=0.14, a0=0.04, a1=math.pi - 0.04, segs=18, sides=6, col=C_GLOW_CYAN, plane="xy")
    m.add_torus_arc(0, 0.0, 0.38, major_r=9.55, minor_r=0.14, minor_z=0.10, a0=0.06, a1=math.pi - 0.06, segs=18, sides=6, col=C_GOLD, plane="xy")
    m.save(models_dir / "interior_arch.obj", "Solaris Vaulted Interior Arch Rib")

    # 10. Sculpted Low-Poly Planetary Peninsula & Distant Crystalline Mountain Ridges (pure stylized vertex coloring)
    m = SculptBuilder()
    grid_n = 20
    span = 230.0
    pts_grid: list[list[int]] = []
    for iz in range(grid_n + 1):
        row = []
        for ix in range(grid_n + 1):
            fx = (ix / float(grid_n) - 0.5) * 2.0
            fz = (iz / float(grid_n) - 0.5) * 2.0
            x = fx * span
            z = fz * span
            r = math.sqrt(fx * fx + fz * fz)
            if r < 0.30:
                y = 0.0
            elif fx > 0.28 and fz < 0.22:
                y = -4.2 * min(1.0, (fx - 0.28) * 2.2)
            else:
                ridge = max(0.0, math.sin(fx * 5.2 + 0.8) * math.cos(fz * 4.5 - 0.5) + 0.45 * math.sin((fx - fz) * 8.0))
                back_boost = max(0.0, -fz - 0.14) * 34.0 + max(0.0, -fx - 0.34) * 20.0
                y = (ridge * 22.0 + back_boost) * min(1.0, max(0.0, (r - 0.28) * 2.4))
            # Iconic stylized Helios Prime terrain palette: sleek slate tarmac -> teal-sage coastal plains -> terracotta/rose sunset mesas!
            if r < 0.24:
                col = (0.24, 0.34, 0.48)  # Sleek Citadel tarmac apron
            elif y < -0.4:
                col = (0.14, 0.42, 0.56)  # Shallow coastal shelf
            elif y < 2.2:
                col = (0.24, 0.52, 0.50)  # Cool sage-teal coastal meadow
            elif y < 12.0:
                col = (0.36, 0.42, 0.56)  # Twilight slate-violet foothills
            elif y < 24.0:
                col = (0.76, 0.52, 0.48)  # Sunset rose-ochre ridges
            else:
                col = (0.98, 0.80, 0.68)  # Sunlit coral-gold mountain crests
            row.append(m.add_vert(x, y, z, col))
        pts_grid.append(row)
    for iz in range(grid_n):
        for ix in range(grid_n):
            a = pts_grid[iz][ix]
            b = pts_grid[iz][ix + 1]
            c = pts_grid[iz + 1][ix + 1]
            d = pts_grid[iz + 1][ix]
            m.add_tri(a, c, b)
            m.add_tri(a, d, c)
    m.save(models_dir / "terrain_island.obj", "Helios Prime Sculpted Peninsula & Ridges")


def gen_voxel_sample(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Optional Voxel Fortification Bunker Module (x y z palette_idx) - CC BY 4.0"]
    for y in range(5):
        for z in range(6):
            for x in range(6):
                on_wall = x in (0, 5) or z in (0, 5)
                if y == 0 or (y == 4 and on_wall):
                    lines.append(f"{x} {y} {z} 1")
                elif on_wall and not (y in (1, 2) and z == 5 and x in (2, 3)):
                    pal = 2 if y == 3 else 1
                    lines.append(f"{x} {y} {z} {pal}")
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

    # Signature Solaris Color Palettes:
    # Solar Directorate: Glacier-Titanium Ivory + Deep Cerulean-Slate + Champagne Solar-Gold + Electric Aqua
    gen_hull_texture(assets / "textures/hull_blue.png", (214, 228, 244), (34, 62, 110), (242, 188, 72), (56, 244, 255))
    # Crimson Dominion: Matte Volcanic Obsidian + Deep Plum-Steel + Molten Coral-Crimson + Incandescent Amber
    gen_hull_texture(assets / "textures/hull_red.png", (52, 40, 68), (88, 30, 64), (255, 62, 102), (255, 168, 56))
    gen_deck_floor(assets / "textures/deck_floor.png")
    gen_planet_surface(assets / "textures/planet_surface.png")
    gen_ocean_texture(assets / "textures/ocean_wave.png")
    gen_crt_console_texture(assets / "textures/crt_console.png")
    gen_crew_8dir_sprite(assets / "sprites/crew_8dir.png")
    gen_font_atlas(assets / "fonts/retro_font.png")
    gen_models(assets / "models")
    gen_voxel_sample(assets / "voxels/bunker_module.vxl")
    gen_wav_tone(assets / "audio/plasma_bolt.wav", 880.0, 180.0, 0.22)
    gen_wav_tone(assets / "audio/alert_klaxon.wav", 520.0, 640.0, 0.45)
    gen_wav_tone(assets / "audio/engine_hum.wav", 110.0, 130.0, 0.35)
    gen_wav_tone(assets / "audio/docking_clamp.wav", 240.0, 95.0, 0.30)

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

    # Sample Mod Override: Vibrant Emerald Vanguard (Rich Jade-Emerald Armor + Deep Forest-Teal + Solar Gold + Neon Mint)
    gen_hull_texture(
        root / "mods/example_mod/assets/textures/hull_blue.png",
        (36, 186, 138),
        (14, 72, 62),
        (252, 214, 86),
        (82, 255, 206),
    )
    print("Generated all Solaris Retro-Futurism sculpted 3D meshes and stylized textures cleanly.")


if __name__ == "__main__":
    main()
