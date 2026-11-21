#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generates 76 unique, sculpted Solaris Retro-Futurism Wavefront .obj 3D models (CC BY 4.0)
— one dedicated architectural/vehicle/vessel model for EVERY construct in the doubled roster."""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_retro_assets import (  # noqa: E402
    C_DARK,
    C_GLOW_CORAL,
    C_GLOW_CYAN,
    C_GLOW_GOLD,
    C_GOLD,
    C_HULL,
    C_PALE,
    C_SLATE,
    SculptBuilder,
)

C_GLOW_MINT = (0.35, 2.05, 1.45)
C_COPPER = (0.96, 0.68, 0.42)
C_TEAL = (0.38, 0.78, 0.86)


class SolarisSculptor(SculptBuilder):
    """Extended parametric sculptor with struts, beveled blocks, dishes, arches, and torus rings."""

    def add_bevel_box(
        self,
        cx: float,
        cy: float,
        cz: float,
        sx: float,
        sy: float,
        sz: float,
        bevel: float = 0.14,
        col: tuple[float, float, float] = C_HULL,
    ) -> None:
        hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
        b = min(bevel, hx * 0.4, hz * 0.4)
        bx, bz = max(0.02, hx - b), max(0.02, hz - b)
        def oct_ring(y: float, s: float) -> list[tuple[float, float, float]]:
            return [
                (cx - bx * s, y, cz - hz * s),
                (cx + bx * s, y, cz - hz * s),
                (cx + hx * s, y, cz - bz * s),
                (cx + hx * s, y, cz + bz * s),
                (cx + bx * s, y, cz + hz * s),
                (cx - bx * s, y, cz + hz * s),
                (cx - hx * s, y, cz + bz * s),
                (cx - hx * s, y, cz - bz * s),
            ]
        rings = [oct_ring(cy - hy, 0.96), oct_ring(cy - hy * 0.7, 1.0), oct_ring(cy + hy * 0.7, 1.0), oct_ring(cy + hy, 0.90)]
        self.add_loft(rings, col=col)

    def add_strut(
        self,
        p0: tuple[float, float, float],
        p1: tuple[float, float, float],
        r0: float,
        r1: float,
        sides: int = 6,
        col: tuple[float, float, float] = C_SLATE,
    ) -> None:
        dx, dy, dz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
        L = math.sqrt(dx * dx + dy * dy + dz * dz)
        if L < 1e-5:
            return
        ux, uy, uz = dx / L, dy / L, dz / L
        ref = (0.0, 1.0, 0.0) if abs(uy) < 0.85 else (1.0, 0.0, 0.0)
        # tangent 1 = normalize(u x ref)
        tx = uy * ref[2] - uz * ref[1]
        ty = uz * ref[0] - ux * ref[2]
        tz = ux * ref[1] - uy * ref[0]
        tl = math.sqrt(tx * tx + ty * ty + tz * tz)
        tx, ty, tz = tx / tl, ty / tl, tz / tl
        bx = uy * tz - uz * ty
        by = uz * tx - ux * tz
        bz = ux * ty - uy * tx

        def ring(p: tuple[float, float, float], r: float) -> list[tuple[float, float, float]]:
            out = []
            for i in range(sides):
                a = 2.0 * math.pi * i / sides
                ca, sa = math.cos(a) * r, math.sin(a) * r
                out.append((p[0] + tx * ca + bx * sa, p[1] + ty * ca + by * sa, p[2] + tz * ca + bz * sa))
            return out

        self.add_loft([ring(p0, r0), ring(p1, r1)], col=col)

    def add_torus_y(
        self,
        cx: float,
        cy: float,
        cz: float,
        major_r: float,
        minor_r: float,
        segs: int = 18,
        tube: int = 6,
        col: tuple[float, float, float] = C_HULL,
    ) -> None:
        rings = []
        for s in range(segs + 1):
            a = 2.0 * math.pi * s / segs
            ca, sa = math.cos(a), math.sin(a)
            pts = []
            for t in range(tube):
                b = 2.0 * math.pi * t / tube
                r = major_r + math.cos(b) * minor_r
                pts.append((cx + ca * r, cy + math.sin(b) * minor_r, cz + sa * r))
            rings.append(pts)
        self.add_loft(rings, col=col, cap_start=False, cap_end=False)

    def add_torus_z(
        self,
        cx: float,
        cy: float,
        cz: float,
        major_r: float,
        minor_r: float,
        segs: int = 18,
        tube: int = 6,
        col: tuple[float, float, float] = C_HULL,
    ) -> None:
        rings = []
        for s in range(segs + 1):
            a = 2.0 * math.pi * s / segs
            ca, sa = math.cos(a), math.sin(a)
            pts = []
            for t in range(tube):
                b = 2.0 * math.pi * t / tube
                r = major_r + math.cos(b) * minor_r
                pts.append((cx + ca * r, cy + sa * r, cz + math.sin(b) * minor_r))
            rings.append(pts)
        self.add_loft(rings, col=col, cap_start=False, cap_end=False)

    def add_arch_z(
        self,
        cx: float,
        cz: float,
        span_x: float,
        height_y: float,
        depth_z: float,
        thick: float,
        segs: int = 10,
        col: tuple[float, float, float] = C_HULL,
    ) -> None:
        rings = []
        for s in range(segs + 1):
            a = math.pi * s / segs
            ca, sa = math.cos(a), math.sin(a)
            ox, oy = cx + ca * span_x, sa * height_y
            ix, iy = cx + ca * (span_x - thick), max(0.0, sa * (height_y - thick))
            rings.append([
                (ox, oy, cz - depth_z * 0.5),
                (ox, oy, cz + depth_z * 0.5),
                (ix, iy, cz + depth_z * 0.5),
                (ix, iy, cz - depth_z * 0.5),
            ])
        self.add_loft(rings, col=col)

    def add_dish(
        self,
        cx: float,
        cy: float,
        cz: float,
        radius: float,
        depth: float,
        tilt_deg: float = 35.0,
        sides: int = 12,
        col: tuple[float, float, float] = C_PALE,
    ) -> None:
        rad = math.radians(tilt_deg)
        ct, st = math.cos(rad), math.sin(rad)

        def xform(x: float, y: float, z: float) -> tuple[float, float, float]:
            return (cx + x, cy + y * ct - z * st, cz + y * st + z * ct)

        r_in = [xform(math.cos(2 * math.pi * i / sides) * radius * 0.2, 0.0, math.sin(2 * math.pi * i / sides) * radius * 0.2) for i in range(sides)]
        r_mid = [xform(math.cos(2 * math.pi * i / sides) * radius * 0.65, depth * 0.35, math.sin(2 * math.pi * i / sides) * radius * 0.65) for i in range(sides)]
        r_out = [xform(math.cos(2 * math.pi * i / sides) * radius, depth, math.sin(2 * math.pi * i / sides) * radius) for i in range(sides)]
        r_back = [xform(math.cos(2 * math.pi * i / sides) * radius * 0.92, depth * 0.72, math.sin(2 * math.pi * i / sides) * radius * 0.92) for i in range(sides)]
        self.add_loft([r_in, r_mid, r_out, r_back], col=col)
        self.add_strut(xform(0.0, 0.0, 0.0), xform(0.0, depth * 1.45, 0.0), 0.08, 0.04, 6, C_GLOW_CYAN)

    def write_obj(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        lines = ["# SPDX-License-Identifier: CC-BY-4.0", "# 1stPRTS Solaris Retro-Futurism Unique Sculpted Construct"]
        for x, y, z, r, g, b in self.v:
            lines.append(f"v {x:.4f} {y:.4f} {z:.4f} {r:.3f} {g:.3f} {b:.3f}")
        for u, v in self.vt:
            lines.append(f"vt {u:.3f} {v:.3f}")
        for face in self.f:
            if len(face) == 4:
                a, b, c, d = face
                lines.append(f"f {a}/1 {b}/2 {c}/3 {d}/4")
            else:
                a, b, c = face
                lines.append(f"f {a}/1 {b}/2 {c}/3")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")


# =============================================================================
# CLASS 0: 24 UNIQUE BUILDINGS (bldg_*)
# =============================================================================

def build_bldg_command_citadel() -> SolarisSculptor:
    s = SolarisSculptor()
    rings = [
        s.poly_ring_y(0, 0.0, 0, 2.30, 2.30, 8),
        s.poly_ring_y(0, 0.45, 0, 2.10, 2.10, 8),
        s.poly_ring_y(0, 1.35, 0, 1.55, 1.55, 8),
        s.poly_ring_y(0, 2.05, 0, 1.15, 1.15, 8),
    ]
    s.add_loft(rings, C_HULL)
    s.add_cylinder_y(0, 1.70, 2.02, 0, 1.38, 1.24, 8, C_GLOW_CYAN)
    s.add_dome(0, 2.05, 0, 1.10, 0.65, 1.10, 12, 4, C_PALE)
    s.add_torus_y(0, 2.35, 0, 1.25, 0.08, 16, 6, C_GOLD)
    for k in range(4):
        a = k * math.pi * 0.5 + math.pi * 0.25
        s.add_strut((math.cos(a) * 2.35, 0.0, math.sin(a) * 2.35), (math.cos(a) * 1.15, 2.0, math.sin(a) * 1.15), 0.22, 0.12, 6, C_SLATE)
    s.add_cylinder_y(0, 2.68, 3.65, 0, 0.14, 0.04, 8, C_SLATE)
    s.add_dome(0, 3.65, 0, 0.11, 0.14, 0.11, 8, 3, C_GLOW_CYAN, full_sphere=True)
    return s

def build_bldg_tactical_outpost() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([s.poly_ring_y(0, 0.0, 0, 1.9, 1.9, 6), s.poly_ring_y(0, 0.95, 0, 1.55, 1.55, 6), s.poly_ring_y(0, 1.35, 0, 1.15, 1.15, 6)], C_HULL)
    s.add_dome(0, 1.35, 0, 0.92, 0.55, 0.92, 10, 3, C_GLOW_CYAN)
    for sx in (-1.25, 1.25):
        s.add_cylinder_y(sx, 0.0, 2.55, -0.3, 0.16, 0.07, 6, C_SLATE)
        s.add_torus_y(sx, 2.15, -0.3, 0.34, 0.05, 12, 5, C_GOLD)
        s.add_dome(sx, 2.58, -0.3, 0.10, 0.10, 0.10, 6, 2, C_GLOW_GOLD, True)
    s.add_bevel_box(0, 0.35, 1.45, 1.1, 0.7, 0.8, 0.12, C_SLATE)
    return s

def build_bldg_fusion_plant() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.42, 0, 2.25, 2.05, 12, C_SLATE)
    s.add_torus_y(0, 0.98, 0, 1.32, 0.42, 18, 8, C_HULL)
    s.add_torus_y(0, 0.98, 0, 1.35, 0.14, 18, 6, C_GLOW_CYAN)
    s.add_dome(0, 0.45, 0, 0.72, 1.15, 0.72, 12, 4, C_GLOW_CYAN)
    for k in range(4):
        a = k * math.pi * 0.5 + math.pi * 0.25
        s.add_strut((math.cos(a) * 2.15, 0.0, math.sin(a) * 2.15), (math.cos(a) * 1.45, 2.25, math.sin(a) * 1.45), 0.28, 0.16, 6, C_PALE)
        s.add_cylinder_y(math.cos(a) * 1.45, 2.15, 2.38, math.sin(a) * 1.45, 0.17, 0.14, 6, C_GLOW_GOLD)
    return s

def build_bldg_solar_geothermal() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 1.65, 0, 0.78, 0.48, 10, C_SLATE)
    s.add_dome(0, 1.65, 0, 0.52, 0.48, 0.52, 10, 3, C_GLOW_GOLD)
    for k in range(6):
        a = k * math.pi / 3.0
        ca, sa = math.cos(a), math.sin(a)
        p_in = (ca * 0.65, 0.65, sa * 0.65)
        p_out = (ca * 2.35, 1.45, sa * 2.35)
        s.add_strut(p_in, p_out, 0.10, 0.08, 6, C_DARK)
        # Solar collector petal
        px, pz = -sa * 0.46, ca * 0.46
        r0 = [(ca * 0.75 - px * 0.5, 0.72, sa * 0.75 - pz * 0.5), (ca * 0.75 + px * 0.5, 0.72, sa * 0.75 + pz * 0.5), (ca * 0.75 + px * 0.5, 0.80, sa * 0.75 + pz * 0.5), (ca * 0.75 - px * 0.5, 0.80, sa * 0.75 - pz * 0.5)]
        r1 = [(ca * 2.30 - px, 1.50, sa * 2.30 - pz), (ca * 2.30 + px, 1.50, sa * 2.30 + pz), (ca * 2.30 + px, 1.58, sa * 2.30 + pz), (ca * 2.30 - px, 1.58, sa * 2.30 - pz)]
        s.add_loft([r0, r1], C_GOLD)
    return s

def build_bldg_ore_refinery() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.45, 0, 3.4, 0.9, 2.2, 0.2, C_SLATE)
    for sx in (-1.05, 0.05):
        s.add_cylinder_y(sx, 0.85, 2.35, -0.25, 0.58, 0.46, 10, C_HULL)
        s.add_cylinder_y(sx, 2.25, 2.65, -0.25, 0.32, 0.24, 8, C_DARK)
        s.add_torus_y(sx, 1.55, -0.25, 0.56, 0.06, 14, 5, C_GLOW_GOLD)
    s.add_strut((0.4, 1.3, 0.0), (1.9, 0.35, 0.8), 0.24, 0.24, 6, C_GOLD)
    s.add_bevel_box(1.35, 0.85, 0.35, 1.0, 1.1, 1.1, 0.14, C_PALE)
    s.add_bevel_box(1.35, 0.55, 0.92, 0.55, 0.42, 0.12, 0.04, C_GLOW_CORAL)
    return s

def build_bldg_fuel_plant() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.22, 0, 3.6, 0.44, 2.2, 0.18, C_SLATE)
    for sx in (-1.2, 0.0, 1.2):
        s.add_dome(sx, 1.12, 0.0, 0.62, 0.64, 0.62, 12, 5, C_PALE, full_sphere=True)
        s.add_torus_y(sx, 1.12, 0.0, 0.64, 0.05, 14, 5, C_GLOW_CYAN)
        s.add_cylinder_y(sx, 0.40, 0.72, 0.0, 0.25, 0.28, 8, C_DARK)
    s.add_strut((-1.2, 1.80, 0.0), (1.2, 1.80, 0.0), 0.10, 0.10, 6, C_GOLD)
    s.add_cylinder_y(1.45, 0.40, 2.45, -0.75, 0.16, 0.10, 8, C_SLATE)
    return s

def build_bldg_vehicle_foundry() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.18, 0, 3.6, 0.36, 3.2, 0.15, C_SLATE)
    for rz in (-1.2, -0.4, 0.4, 1.2):
        s.add_arch_z(0.0, rz, 1.65, 1.85, 0.34, 0.28, 10, C_HULL)
        s.add_arch_z(0.0, rz, 1.38, 1.58, 0.12, 0.08, 10, C_GLOW_CYAN)
    s.add_bevel_box(-1.55, 0.72, 0.0, 0.55, 1.1, 2.9, 0.1, C_PALE)
    s.add_bevel_box(1.55, 0.72, 0.0, 0.55, 1.1, 2.9, 0.1, C_PALE)
    s.add_strut((-1.3, 1.72, 0.0), (1.3, 1.72, 0.0), 0.10, 0.10, 6, C_GOLD)
    return s

def build_bldg_landship_yard() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.18, 0, 4.4, 0.36, 3.4, 0.15, C_DARK)
    for sx in (-1.85, 1.85):
        for sz in (-1.2, 0.0, 1.2):
            s.add_cylinder_y(sx, 0.30, 2.45, sz, 0.22, 0.16, 6, C_SLATE)
        s.add_strut((sx, 2.40, -1.35), (sx, 2.40, 1.35), 0.16, 0.16, 6, C_GOLD)
    for sz in (-0.9, 0.9):
        s.add_strut((-1.9, 2.42, sz), (1.9, 2.42, sz), 0.15, 0.15, 6, C_HULL)
        s.add_bevel_box(0.0, 2.18, sz, 0.7, 0.28, 0.4, 0.05, C_GLOW_CYAN)
    s.add_bevel_box(0.0, 0.36, 0.0, 2.6, 0.12, 3.0, 0.05, C_GLOW_GOLD)
    return s

def build_bldg_starfighter_hangar() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.16, 0, 3.4, 0.32, 3.0, 0.16, C_SLATE)
    rings = [
        [(-1.6, 0.30, -1.3), (1.6, 0.30, -1.3), (1.3, 1.35, -1.3), (-1.3, 1.35, -1.3)],
        [(-1.7, 0.30, 0.2), (1.7, 0.30, 0.2), (1.45, 1.55, 0.2), (-1.45, 1.55, 0.2)],
        [(-1.5, 0.30, 1.3), (1.5, 0.30, 1.3), (1.0, 0.85, 1.3), (-1.0, 0.85, 1.3)],
    ]
    s.add_loft(rings, C_HULL)
    s.add_bevel_box(0.0, 0.65, -1.32, 2.0, 0.58, 0.12, 0.04, C_GLOW_CYAN)
    s.add_bevel_box(1.15, 1.68, 0.35, 0.72, 0.56, 0.85, 0.1, C_PALE)
    s.add_bevel_box(1.15, 1.72, -0.08, 0.56, 0.24, 0.08, 0.02, C_GLOW_GOLD)
    return s

def build_bldg_dropship_pad() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.52, 0, 2.55, 2.35, 8, C_SLATE)
    s.add_torus_y(0, 0.55, 0, 1.75, 0.07, 16, 6, C_GLOW_GOLD)
    s.add_torus_y(0, 0.55, 0, 0.85, 0.05, 16, 5, C_GLOW_CYAN)
    for k in range(4):
        a = k * math.pi * 0.5 + math.pi * 0.25
        px, pz = math.cos(a) * 2.15, math.sin(a) * 2.15
        s.add_cylinder_y(px, 0.50, 1.55, pz, 0.14, 0.08, 6, C_HULL)
        s.add_dome(px, 1.58, pz, 0.12, 0.12, 0.12, 6, 2, C_GLOW_CYAN, True)
    s.add_bevel_box(0, 0.85, 2.05, 1.3, 0.65, 0.55, 0.1, C_PALE)
    return s

def build_bldg_shield_generator() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([s.poly_ring_y(0, 0.0, 0, 1.9, 1.9, 8), s.poly_ring_y(0, 0.75, 0, 1.35, 1.35, 8), s.poly_ring_y(0, 2.45, 0, 0.55, 0.55, 8)], C_HULL)
    for ry, rr in ((1.15, 1.48), (1.75, 1.18), (2.30, 0.88)):
        s.add_torus_y(0, ry, 0, rr, 0.09, 18, 6, C_GLOW_CYAN)
    s.add_dome(0, 2.85, 0, 0.42, 0.45, 0.42, 10, 4, C_GLOW_CYAN, True)
    for k in range(3):
        a = k * 2.0 * math.pi / 3.0
        s.add_strut((math.cos(a) * 1.95, 0.0, math.sin(a) * 1.95), (math.cos(a) * 0.72, 2.4, math.sin(a) * 0.72), 0.18, 0.10, 6, C_GOLD)
    return s

def build_bldg_orbital_cannon() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.85, 0, 2.15, 1.75, 10, C_SLATE)
    s.add_dome(0, 0.85, 0, 1.35, 0.85, 1.35, 10, 4, C_HULL)
    # Massive inclined twin-rail planetary ion cannon barrel
    for sx in (-0.34, 0.34):
        s.add_strut((sx, 1.25, 0.45), (sx, 3.55, -1.55), 0.24, 0.15, 8, C_PALE)
    s.add_strut((0.0, 1.30, 0.40), (0.0, 3.45, -1.48), 0.14, 0.10, 6, C_GLOW_CYAN)
    s.add_torus_y(0, 0.88, 0, 1.62, 0.08, 16, 6, C_GOLD)
    return s

def build_bldg_plasma_turret() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([s.poly_ring_y(0, 0.0, 0, 1.65, 1.65, 8), s.poly_ring_y(0, 0.75, 0, 1.30, 1.30, 8)], C_SLATE)
    s.add_dome(0, 0.75, 0, 1.05, 0.65, 1.05, 10, 3, C_HULL)
    for sx in (-0.28, 0.28):
        s.add_cylinder_z(sx, 1.08, -0.4, -2.25, 0.14, 0.14, 0.10, 0.10, 8, C_PALE)
        s.add_cylinder_z(sx, 1.08, -2.1, -2.35, 0.11, 0.11, 0.09, 0.09, 8, C_GLOW_CYAN)
    s.add_bevel_box(0, 1.48, 0.15, 0.6, 0.25, 0.6, 0.05, C_GOLD)
    return s

def build_bldg_flak_battery() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.72, 0, 1.75, 1.45, 8, C_SLATE)
    s.add_bevel_box(0, 1.05, 0.1, 1.35, 0.65, 1.15, 0.14, C_HULL)
    for sx in (-0.75, 0.75):
        s.add_strut((sx, 1.05, 0.35), (sx, 2.05, -0.85), 0.30, 0.26, 6, C_PALE)
        s.add_dome(sx, 2.05, -0.85, 0.20, 0.20, 0.20, 6, 2, C_GLOW_CORAL, True)
    s.add_dish(0.0, 1.55, 0.45, 0.55, 0.20, 40.0, 10, C_GOLD)
    return s

def build_bldg_spaceport() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.20, 0, 4.4, 0.40, 2.8, 0.18, C_SLATE)
    for sx in (-1.45, 1.45):
        s.add_torus_y(sx, 0.44, 0.0, 0.82, 0.06, 16, 5, C_GLOW_GOLD)
        s.add_torus_y(sx, 0.44, 0.0, 0.42, 0.04, 14, 5, C_GLOW_CYAN)
    s.add_loft([s.poly_ring_y(0, 0.35, -0.75, 0.75, 0.55, 8), s.poly_ring_y(0, 2.15, -0.75, 0.50, 0.42, 8), s.poly_ring_y(0, 2.55, -0.75, 0.92, 0.72, 8)], C_HULL)
    s.add_cylinder_y(0, 2.25, 2.52, -0.75, 0.95, 0.95, 10, C_GLOW_CYAN)
    s.add_dome(0, 2.55, -0.75, 0.85, 0.35, 0.68, 10, 3, C_PALE)
    return s

def build_bldg_elevator_terminal() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.55, 0, 2.45, 2.15, 8, C_SLATE)
    for k in range(4):
        a = k * math.pi * 0.5 + math.pi * 0.25
        s.add_strut((math.cos(a) * 2.25, 0.0, math.sin(a) * 2.25), (math.cos(a) * 0.62, 2.65, math.sin(a) * 0.62), 0.34, 0.18, 6, C_HULL)
    s.add_cylinder_y(0, 0.45, 4.40, 0, 0.26, 0.18, 10, C_GLOW_CYAN)
    for ry in (1.2, 2.2, 3.2):
        s.add_torus_y(0, ry, 0, 0.82 - ry * 0.10, 0.08, 16, 6, C_GOLD)
    return s

def build_bldg_ore_mine() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([s.poly_ring_y(0, 0.0, 0, 2.1, 1.9, 6), s.poly_ring_y(0, 0.55, 0, 1.75, 1.6, 6)], C_SLATE)
    s.add_strut((-0.9, 0.45, 0.7), (0.0, 2.35, -0.2), 0.16, 0.10, 6, C_GOLD)
    s.add_strut((0.9, 0.45, 0.7), (0.0, 2.35, -0.2), 0.16, 0.10, 6, C_GOLD)
    s.add_strut((0.0, 0.45, -1.1), (0.0, 2.35, -0.2), 0.18, 0.10, 6, C_HULL)
    s.add_cylinder_y(0.0, 0.20, 2.25, -0.2, 0.22, 0.22, 8, C_GLOW_MINT)
    s.add_bevel_box(-1.1, 0.75, -0.6, 0.9, 0.85, 0.95, 0.1, C_PALE)
    return s

def build_bldg_deep_drill() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.42, 0, 2.15, 1.95, 12, C_DARK)
    for k in range(3):
        a = k * 2.0 * math.pi / 3.0
        s.add_strut((math.cos(a) * 1.90, 0.30, math.sin(a) * 1.90), (math.cos(a) * 0.42, 3.15, math.sin(a) * 0.42), 0.22, 0.12, 6, C_HULL)
    s.add_cylinder_y(0, 0.0, 3.25, 0, 0.32, 0.24, 10, C_GLOW_CORAL)
    s.add_torus_y(0, 1.45, 0, 1.15, 0.10, 16, 6, C_GOLD)
    s.add_torus_y(0, 2.45, 0, 0.72, 0.08, 16, 6, C_GLOW_GOLD)
    return s

def build_bldg_comms_tower() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([s.poly_ring_y(0, 0.0, 0, 1.55, 1.55, 6), s.poly_ring_y(0, 0.85, 0, 0.95, 0.95, 6), s.poly_ring_y(0, 3.85, 0, 0.16, 0.16, 6)], C_HULL)
    for ry, rr in ((1.65, 0.98), (2.45, 0.75), (3.15, 0.52)):
        s.add_torus_y(0, ry, 0, rr, 0.07, 16, 5, C_GOLD)
    s.add_dome(0, 3.92, 0, 0.20, 0.24, 0.20, 8, 3, C_GLOW_CYAN, True)
    return s

def build_bldg_radar_array() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, 0.0, 0.95, 0, 1.65, 1.15, 8, C_SLATE)
    s.add_strut((-0.65, 0.90, 0.0), (-0.65, 1.85, 0.0), 0.16, 0.14, 6, C_HULL)
    s.add_strut((0.65, 0.90, 0.0), (0.65, 1.85, 0.0), 0.16, 0.14, 6, C_HULL)
    s.add_dish(0.0, 1.75, 0.0, 1.65, 0.62, 48.0, 14, C_PALE)
    s.add_torus_y(0, 0.95, 0, 1.25, 0.06, 16, 5, C_GLOW_CYAN)
    return s

def build_bldg_barracks() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(-1.15, 0.55, 0.0, 1.15, 1.10, 2.4, 0.14, C_HULL)
    s.add_bevel_box(1.15, 0.55, 0.0, 1.15, 1.10, 2.4, 0.14, C_HULL)
    s.add_bevel_box(0.0, 0.95, -0.55, 1.6, 0.55, 0.95, 0.10, C_PALE)
    s.add_arch_z(0.0, 0.65, 0.85, 1.15, 0.35, 0.18, 8, C_GOLD)
    s.add_bevel_box(0.0, 0.08, 0.35, 1.4, 0.16, 1.8, 0.04, C_GLOW_CYAN)
    return s

def build_bldg_academy_medbay() -> SolarisSculptor:
    s = SolarisSculptor()
    for sx in (-1.25, 1.25):
        s.add_dome(sx, 0.45, 0.0, 0.95, 0.72, 1.25, 12, 4, C_PALE)
        s.add_cylinder_y(sx, 0.0, 0.48, 0.0, 1.02, 0.98, 12, C_SLATE)
        s.add_torus_y(sx, 0.52, 0.0, 0.96, 0.05, 14, 5, C_GLOW_MINT)
    s.add_bevel_box(0.0, 0.78, 0.0, 1.4, 0.38, 0.65, 0.08, C_HULL)
    s.add_cylinder_y(0.0, 0.95, 1.65, 0.0, 0.25, 0.12, 8, C_GLOW_MINT)
    return s

def build_bldg_coastal_dock() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0.0, 0.28, 0.95, 3.8, 0.56, 1.2, 0.14, C_SLATE)
    for sx in (-1.45, 1.45):
        s.add_bevel_box(sx, 0.26, -0.75, 0.75, 0.52, 2.5, 0.12, C_HULL)
        s.add_strut((sx, 0.52, -1.2), (sx * 0.65, 1.65, -0.8), 0.12, 0.08, 6, C_GOLD)
    s.add_bevel_box(-1.35, 1.05, 1.05, 0.85, 1.05, 0.85, 0.10, C_PALE)
    s.add_bevel_box(-1.35, 1.32, 0.62, 0.62, 0.26, 0.08, 0.02, C_GLOW_CYAN)
    return s

def build_bldg_naval_drydock() -> SolarisSculptor:
    s = SolarisSculptor()
    for sx in (-1.65, 1.65):
        s.add_bevel_box(sx, 0.72, 0.0, 0.82, 1.45, 3.8, 0.16, C_SLATE)
    s.add_bevel_box(0.0, 0.72, 1.65, 3.9, 1.45, 0.75, 0.16, C_HULL)
    for sz in (-1.1, 0.0, 1.1):
        s.add_arch_z(0.0, sz, 1.65, 2.25, 0.42, 0.28, 10, C_PALE)
        s.add_arch_z(0.0, sz, 1.38, 1.98, 0.12, 0.08, 10, C_GLOW_CYAN)
    return s


# =============================================================================
# CLASS 1: 10 UNIQUE GROUND VEHICLES (veh_*)
# =============================================================================

def build_veh_plasma_tank() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.42, -1.85, 1.05, 0.30, 6, math.pi / 6, 0.55),
        s.poly_ring_z(0, 0.54, -0.55, 1.25, 0.42, 6, math.pi / 6, 0.55),
        s.poly_ring_z(0, 0.52, 1.55, 1.18, 0.38, 6, math.pi / 6, 0.55),
    ], C_HULL)
    for sx in (-1.12, 1.12):
        s.add_bevel_box(sx, 0.28, -0.1, 0.42, 0.42, 3.1, 0.1, C_SLATE)
    s.add_dome(0, 0.88, 0.05, 0.86, 0.42, 1.05, 8, 3, C_PALE)
    s.add_cylinder_z(0, 1.04, -0.6, -2.85, 0.16, 0.14, 0.11, 0.10, 8, C_SLATE)
    s.add_cylinder_z(0, 1.04, -2.65, -2.95, 0.12, 0.11, 0.10, 0.09, 8, C_GLOW_CYAN)
    return s

def build_veh_siege_hovertank() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.55, 0.1, 1.6, 0.55, 3.2, 0.18, C_HULL)
    for sx in (-1.45, 1.45):
        s.add_loft([s.poly_ring_z(sx, 0.38, -1.9, 0.46, 0.28, 6), s.poly_ring_z(sx, 0.42, 1.6, 0.52, 0.34, 6)], C_SLATE)
        s.add_cylinder_y(sx, 0.12, 0.22, -0.2, 0.42, 0.45, 8, C_GLOW_CYAN)
    s.add_bevel_box(0, 1.05, 0.2, 1.4, 0.52, 1.5, 0.18, C_PALE)
    for sx in (-0.30, 0.30):
        s.add_cylinder_z(sx, 1.12, -0.4, -3.15, 0.16, 0.16, 0.12, 0.12, 8, C_GOLD)
        s.add_cylinder_z(sx, 1.12, -2.95, -3.28, 0.13, 0.13, 0.11, 0.11, 8, C_GLOW_CYAN)
    return s

def build_veh_recon_skimmer() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.48, -2.15, 0.24, 0.16, 6),
        s.poly_ring_z(0, 0.56, -0.45, 0.82, 0.32, 6),
        s.poly_ring_z(0, 0.52, 1.45, 1.15, 0.30, 6),
    ], C_HULL)
    s.add_swept_wing(0.55, 0.48, -0.4, 1.2, 0.12, 1.85, 0.56, 0.4, 1.4, 0.05, C_PALE)
    s.add_swept_wing(-0.55, 0.48, -0.4, 1.2, 0.12, -1.85, 0.56, 0.4, 1.4, 0.05, C_PALE)
    s.add_dome(0, 0.78, -0.25, 0.45, 0.28, 0.72, 8, 3, C_GLOW_GOLD)
    s.add_cylinder_z(0, 0.52, 1.42, 1.62, 0.38, 0.22, 0.32, 0.18, 8, C_GLOW_CYAN)
    return s

def build_veh_scout_rover() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.65, 0, 1.45, 0.65, 2.55, 0.18, C_HULL)
    s.add_dome(0, 1.02, -0.35, 0.55, 0.28, 0.62, 8, 3, C_GLOW_GOLD)
    for sx in (-0.95, 0.95):
        for sz in (-0.95, 0.0, 0.95):
            s.add_cylinder_y(sx, 0.0, 0.46, sz, 0.34, 0.34, 8, C_DARK)
    s.add_cylinder_y(0, 0.98, 1.85, 0.55, 0.08, 0.06, 6, C_SLATE)
    s.add_dome(0, 1.92, 0.55, 0.26, 0.18, 0.26, 8, 3, C_GLOW_CYAN, True)
    return s

def build_veh_apc() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.68, -1.95, 0.95, 0.45, 6),
        s.poly_ring_z(0, 0.82, -0.85, 1.25, 0.68, 6),
        s.poly_ring_z(0, 0.82, 1.75, 1.25, 0.68, 6),
    ], C_HULL)
    for sx in (-1.18, 1.18):
        s.add_bevel_box(sx, 0.32, 0.0, 0.35, 0.52, 3.2, 0.08, C_SLATE)
    s.add_dome(0, 1.38, -0.35, 0.48, 0.26, 0.55, 8, 2, C_PALE)
    s.add_cylinder_z(0, 1.48, -0.75, -1.85, 0.08, 0.08, 0.06, 0.06, 6, C_GOLD)
    s.add_bevel_box(0, 0.72, 1.78, 1.15, 0.72, 0.14, 0.03, C_GLOW_CYAN)
    return s

def build_veh_assault_transport() -> SolarisSculptor:
    s = SolarisSculptor()
    for sx in (-0.85, 0.85):
        s.add_loft([s.poly_ring_z(sx, 0.62, -2.25, 0.68, 0.48, 6), s.poly_ring_z(sx, 0.72, 1.95, 0.72, 0.58, 6)], C_HULL)
    s.add_bevel_box(0, 0.85, 0.1, 1.45, 0.82, 3.2, 0.16, C_PALE)
    s.add_bevel_box(0, 0.62, -2.20, 1.85, 0.52, 0.32, 0.08, C_GLOW_CYAN)
    for sx in (-0.85, 0.85):
        s.add_dome(sx, 1.25, -0.4, 0.36, 0.22, 0.42, 8, 2, C_SLATE)
        s.add_cylinder_z(sx, 1.34, -0.75, -1.75, 0.08, 0.08, 0.06, 0.06, 6, C_GOLD)
    return s

def build_veh_cargo_truck() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.82, -1.55, 1.45, 1.05, 1.25, 0.16, C_HULL)
    s.add_bevel_box(0, 1.02, -2.18, 1.15, 0.38, 0.10, 0.03, C_GLOW_GOLD)
    s.add_bevel_box(0, 0.38, 0.45, 1.55, 0.35, 3.1, 0.1, C_SLATE)
    for sz in (-0.45, 0.50, 1.45):
        s.add_bevel_box(0, 0.96, sz, 1.38, 0.82, 0.82, 0.16, C_GOLD)
        s.add_bevel_box(0, 0.96, sz, 1.42, 0.14, 0.24, 0.02, C_GLOW_CYAN)
    return s

def build_veh_repair_rig() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.62, 0.1, 1.75, 0.82, 2.85, 0.16, C_HULL)
    s.add_bevel_box(0, 0.78, -1.32, 1.25, 0.36, 0.12, 0.03, C_GLOW_GOLD)
    for sx in (-0.55, 0.55):
        s.add_strut((sx, 1.0, 0.65), (sx * 0.7, 2.05, -0.45), 0.13, 0.10, 6, C_GOLD)
        s.add_strut((sx * 0.7, 2.05, -0.45), (sx * 0.45, 1.45, -1.95), 0.10, 0.08, 6, C_PALE)
        s.add_dome(sx * 0.45, 1.45, -1.98, 0.14, 0.14, 0.14, 6, 2, C_GLOW_MINT, True)
    return s

def build_veh_shield_crawler() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.48, 0.0, 2.45, 0.72, 3.1, 0.20, C_SLATE)
    s.add_cylinder_y(0, 0.82, 1.35, 0.0, 0.95, 0.78, 10, C_HULL)
    s.add_dome(0, 1.65, 0.0, 0.62, 0.62, 0.62, 12, 4, C_GLOW_CYAN, True)
    s.add_torus_y(0, 1.65, 0.0, 0.92, 0.08, 16, 6, C_GOLD)
    for sx in (-1.15, 1.15):
        s.add_strut((sx, 0.85, 0.55), (sx, 1.55, -0.25), 0.24, 0.20, 6, C_PALE)
    return s

def build_veh_command_landship() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.58, -2.55, 1.45, 0.48, 8),
        s.poly_ring_z(0, 0.72, -1.25, 1.95, 0.68, 8),
        s.poly_ring_z(0, 0.75, 2.15, 1.95, 0.68, 8),
    ], C_HULL)
    for sx in (-2.05, 2.05):
        s.add_bevel_box(sx, 0.36, -0.1, 0.58, 0.62, 4.1, 0.12, C_SLATE)
    s.add_bevel_box(0, 1.45, -0.2, 2.15, 0.82, 2.6, 0.2, C_PALE)
    s.add_loft([s.poly_ring_y(0, 1.85, -0.4, 0.82, 0.82, 8), s.poly_ring_y(0, 2.65, -0.4, 0.52, 0.52, 8)], C_HULL)
    s.add_cylinder_y(0, 2.28, 2.52, -0.4, 0.68, 0.62, 8, C_GLOW_CYAN)
    s.add_torus_y(0, 1.25, 1.55, 0.58, 0.05, 14, 5, C_GLOW_GOLD)
    return s


# =============================================================================
# CLASS 2: 10 UNIQUE NAVAL SHIPS (ship_*)
# =============================================================================

def build_ship_missile_boat() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.38, -2.35, 0.12, 0.22, 6),
        s.poly_ring_z(0, 0.42, -0.85, 0.78, 0.42, 6, math.pi / 6, 0.6),
        s.poly_ring_z(0, 0.42, 1.85, 0.88, 0.40, 6, math.pi / 6, 0.6),
    ], C_HULL)
    s.add_bevel_box(0, 0.88, -0.15, 0.92, 0.55, 1.15, 0.14, C_PALE)
    s.add_bevel_box(0, 0.98, -0.74, 0.65, 0.18, 0.08, 0.02, C_GLOW_CYAN)
    for sx in (-0.42, 0.42):
        s.add_strut((sx, 0.72, 1.15), (sx, 1.35, 0.45), 0.22, 0.18, 6, C_GOLD)
        s.add_cylinder_z(sx, 0.35, 1.85, 2.05, 0.18, 0.16, 0.14, 0.12, 8, C_GLOW_CYAN)
    return s

def build_ship_hydrofoil() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.92, -2.2, 0.15, 0.18, 6),
        s.poly_ring_z(0, 0.96, -0.2, 0.82, 0.36, 6),
        s.poly_ring_z(0, 0.92, 1.8, 0.75, 0.30, 6),
    ], C_HULL)
    s.add_dome(0, 1.25, -0.25, 0.48, 0.30, 0.82, 8, 3, C_GLOW_GOLD)
    for sz in (-0.95, 0.95):
        s.add_strut((-0.55, 0.82, sz), (-1.25, 0.12, sz), 0.09, 0.07, 6, C_SLATE)
        s.add_strut((0.55, 0.82, sz), (1.25, 0.12, sz), 0.09, 0.07, 6, C_SLATE)
        s.add_strut((-1.35, 0.12, sz), (1.35, 0.12, sz), 0.08, 0.08, 6, C_GLOW_CYAN)
    return s

def build_ship_plasma_destroyer() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.45, -2.85, 0.10, 0.35, 6),
        s.poly_ring_z(0, 0.48, -1.25, 0.72, 0.48, 6, math.pi / 6, 0.65),
        s.poly_ring_z(0, 0.48, 1.65, 0.85, 0.48, 6, math.pi / 6, 0.65),
        s.poly_ring_z(0, 0.42, 2.55, 0.62, 0.38, 6, math.pi / 6, 0.65),
    ], C_HULL)
    s.add_bevel_box(0, 1.05, 0.15, 0.98, 0.85, 1.45, 0.16, C_PALE)
    s.add_cylinder_y(0, 1.45, 2.35, 0.05, 0.16, 0.05, 6, C_SLATE)
    for sz in (-1.45, -0.78):
        s.add_dome(0, 0.82, sz, 0.34, 0.22, 0.40, 8, 2, C_SLATE)
        s.add_cylinder_z(0, 0.92, sz - 0.25, sz - 1.05, 0.08, 0.08, 0.06, 0.06, 6, C_GLOW_CYAN)
    return s

def build_ship_aegis_frigate() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.45, -2.55, 0.12, 0.32, 6),
        s.poly_ring_z(0, 0.48, -0.95, 0.82, 0.46, 6, math.pi / 6, 0.65),
        s.poly_ring_z(0, 0.48, 2.15, 0.86, 0.44, 6, math.pi / 6, 0.65),
    ], C_HULL)
    # Iconic glowing AEGIS octagonal pyramid superstructure
    s.add_loft([s.poly_ring_y(0, 0.82, 0.0, 0.82, 0.95, 8), s.poly_ring_y(0, 1.85, 0.0, 0.38, 0.45, 8)], C_PALE)
    s.add_cylinder_y(0, 1.25, 1.55, 0.0, 0.66, 0.56, 8, C_GLOW_CYAN)
    s.add_torus_y(0, 1.92, 0.0, 0.46, 0.06, 14, 5, C_GOLD)
    return s

def build_ship_plasma_cruiser() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.48, -2.95, 0.16, 0.36, 8),
        s.poly_ring_z(0, 0.52, -1.45, 0.98, 0.54, 8),
        s.poly_ring_z(0, 0.52, 1.75, 1.08, 0.54, 8),
        s.poly_ring_z(0, 0.46, 2.65, 0.78, 0.42, 8),
    ], C_HULL)
    s.add_bevel_box(0, 1.15, 0.15, 1.25, 0.92, 1.75, 0.18, C_PALE)
    s.add_bevel_box(0, 1.78, -0.05, 0.78, 0.55, 0.88, 0.12, C_SLATE)
    s.add_bevel_box(0, 1.85, -0.50, 0.62, 0.18, 0.08, 0.02, C_GLOW_CYAN)
    for sz in (-1.75, -1.05, 1.65):
        s.add_dome(0, 0.92, sz, 0.40, 0.24, 0.45, 8, 2, C_GOLD)
        dir_z = -0.95 if sz < 0 else 0.95
        s.add_cylinder_z(0, 1.02, sz, sz + dir_z, 0.10, 0.10, 0.07, 0.07, 6, C_GLOW_CYAN)
    return s

def build_ship_fleet_carrier() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.55, 0.0, 2.35, 0.65, 4.8, 0.22, C_HULL)
    for sx in (-1.45, 1.45):
        s.add_loft([s.poly_ring_z(sx, 0.35, -2.2, 0.34, 0.30, 6), s.poly_ring_z(sx, 0.35, 2.2, 0.38, 0.32, 6)], C_SLATE)
    s.add_bevel_box(-0.22, 0.90, -0.1, 0.20, 0.06, 4.4, 0.02, C_GLOW_CYAN)
    s.add_bevel_box(0.45, 0.90, 0.2, 0.14, 0.06, 3.8, 0.02, C_GLOW_GOLD)
    # Starboard stealth ATC island spire
    s.add_loft([s.poly_ring_y(0.92, 0.85, 0.35, 0.38, 0.85, 6), s.poly_ring_y(0.92, 2.15, 0.35, 0.22, 0.45, 6)], C_PALE)
    s.add_cylinder_y(0.92, 1.68, 1.92, 0.35, 0.30, 0.28, 6, C_GLOW_CYAN)
    return s

def build_ship_amphibious_lhd() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.65, 0.0, 2.05, 0.98, 4.2, 0.20, C_HULL)
    s.add_bevel_box(0, 0.45, 2.12, 1.35, 0.65, 0.16, 0.04, C_GLOW_GOLD)  # Stern well-deck bay
    s.add_bevel_box(0, 1.18, 0.0, 1.65, 0.08, 3.8, 0.04, C_SLATE)
    s.add_bevel_box(0.82, 1.62, 0.15, 0.48, 0.88, 1.45, 0.10, C_PALE)
    s.add_bevel_box(0.82, 1.78, -0.58, 0.36, 0.20, 0.08, 0.02, C_GLOW_CYAN)
    return s

def build_ship_attack_sub() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.55, -2.55, 0.22, 0.22, 10),
        s.poly_ring_z(0, 0.55, -1.75, 0.66, 0.56, 10),
        s.poly_ring_z(0, 0.55, 1.35, 0.64, 0.54, 10),
        s.poly_ring_z(0, 0.55, 2.55, 0.18, 0.18, 10),
    ], C_DARK)
    s.add_fin(0.0, 1.02, -0.75, 0.45, 0.0, 1.85, -0.55, 0.35, 0.22, C_SLATE)
    s.add_swept_wing(0.22, 1.45, -0.45, 0.05, 0.06, 0.92, 1.45, -0.25, 0.12, 0.04, C_GOLD)
    s.add_swept_wing(-0.22, 1.45, -0.45, 0.05, 0.06, -0.92, 1.45, -0.25, 0.12, 0.04, C_GOLD)
    s.add_torus_z(0, 0.55, 2.48, 0.26, 0.06, 12, 5, C_GLOW_CYAN)
    return s

def build_ship_missile_sub() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.58, -2.85, 0.24, 0.24, 10),
        s.poly_ring_z(0, 0.62, -1.85, 0.84, 0.64, 10),
        s.poly_ring_z(0, 0.62, 1.75, 0.84, 0.64, 10),
        s.poly_ring_z(0, 0.58, 2.75, 0.22, 0.22, 10),
    ], C_DARK)
    # Raised 12-tube orbital-strike missile dorsal spine
    s.add_bevel_box(0, 1.12, 0.35, 1.02, 0.42, 2.25, 0.14, C_SLATE)
    for sz in (-0.35, 0.25, 0.85):
        for sx in (-0.28, 0.28):
            s.add_cylinder_y(sx, 1.32, 1.42, sz, 0.14, 0.14, 8, C_GLOW_CORAL)
    s.add_fin(0.0, 1.05, -1.65, -0.85, 0.0, 1.92, -1.45, -0.95, 0.20, C_HULL)
    return s

def build_ship_relay_tender() -> SolarisSculptor:
    s = SolarisSculptor()
    for sx in (-0.95, 0.95):
        s.add_loft([s.poly_ring_z(sx, 0.42, -2.35, 0.16, 0.26, 6), s.poly_ring_z(sx, 0.45, 0.0, 0.48, 0.42, 6), s.poly_ring_z(sx, 0.42, 2.25, 0.42, 0.38, 6)], C_HULL)
    s.add_bevel_box(0, 0.68, 0.0, 2.1, 0.36, 3.2, 0.14, C_SLATE)
    for sz in (-0.8, 0.1, 1.0):
        s.add_dome(0, 1.15, sz, 0.45, 0.45, 0.45, 10, 4, C_GOLD, True)
    s.add_dish(0.0, 1.68, -1.25, 0.72, 0.26, 32.0, 10, C_GLOW_CYAN)
    return s


# =============================================================================
# CLASS 3: 8 UNIQUE AIRCRAFT (air_*)
# =============================================================================

def build_air_starfighter() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.45, 0.08, 0.06, 6),
        s.poly_ring_z(0, 0.04, -0.95, 0.46, 0.26, 6),
        s.poly_ring_z(0, 0.02, 1.35, 0.56, 0.30, 6),
        s.poly_ring_z(0, 0.0, 1.85, 0.42, 0.24, 6),
    ], C_HULL)
    s.add_swept_wing(0.42, 0.0, -0.75, 1.15, 0.12, 2.45, -0.08, 0.65, 1.55, 0.04, C_PALE)
    s.add_swept_wing(-0.42, 0.0, -0.75, 1.15, 0.12, -2.45, -0.08, 0.65, 1.55, 0.04, C_PALE)
    s.add_fin(0.32, 0.15, 0.45, 1.65, 0.92, 0.92, 1.15, 1.75, 0.06, C_GOLD)
    s.add_fin(-0.32, 0.15, 0.45, 1.65, -0.92, 0.92, 1.15, 1.75, 0.06, C_GOLD)
    s.add_dome(0, 0.22, -0.45, 0.28, 0.22, 0.62, 8, 3, C_GLOW_GOLD)
    for sx in (-0.22, 0.22):
        s.add_cylinder_z(sx, 0.0, 1.80, 2.02, 0.16, 0.16, 0.13, 0.13, 8, C_GLOW_CYAN)
    return s

def build_air_recon_fighter() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.65, 0.06, 0.05, 6),
        s.poly_ring_z(0, 0.05, -0.65, 0.52, 0.22, 6),
        s.poly_ring_z(0, 0.02, 1.55, 0.48, 0.20, 6),
    ], C_DARK)
    # Forward-swept sleek stealth wings
    s.add_swept_wing(0.42, 0.0, 0.15, 1.45, 0.09, 2.35, 0.05, -1.15, -0.35, 0.04, C_SLATE)
    s.add_swept_wing(-0.42, 0.0, 0.15, 1.45, 0.09, -2.35, 0.05, -1.15, -0.35, 0.04, C_SLATE)
    s.add_dome(0, 0.18, -0.55, 0.26, 0.18, 0.72, 8, 3, C_GLOW_CYAN)
    s.add_cylinder_z(0, 0.02, 1.52, 1.75, 0.26, 0.14, 0.20, 0.10, 8, C_GLOW_CYAN)
    return s

def build_air_plasma_bomber() -> SolarisSculptor:
    s = SolarisSculptor()
    # Wide diamond flying-wing heavy bomber
    s.add_loft([
        s.poly_ring_z(0, 0.0, -1.75, 0.32, 0.18, 6),
        s.poly_ring_z(0, 0.08, -0.25, 1.15, 0.42, 6),
        s.poly_ring_z(0, 0.04, 1.35, 0.85, 0.28, 6),
    ], C_HULL)
    s.add_swept_wing(0.85, 0.04, -0.85, 1.05, 0.26, 2.95, -0.06, 0.55, 1.45, 0.06, C_SLATE)
    s.add_swept_wing(-0.85, 0.04, -0.85, 1.05, 0.26, -2.95, -0.06, 0.55, 1.45, 0.06, C_SLATE)
    s.add_dome(0, 0.38, -0.45, 0.42, 0.24, 0.65, 8, 3, C_GLOW_GOLD)
    for sx in (-1.15, 1.15):
        s.add_cylinder_z(sx, -0.24, -0.3, 0.85, 0.22, 0.18, 0.20, 0.16, 8, C_GLOW_CORAL)
    return s

def build_air_dropship() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.22, -2.15, 0.45, 0.34, 8),
        s.poly_ring_z(0, 0.32, -0.75, 0.96, 0.62, 8),
        s.poly_ring_z(0, 0.30, 1.65, 0.94, 0.60, 8),
        s.poly_ring_z(0, 0.24, 2.25, 0.72, 0.44, 8),
    ], C_HULL)
    s.add_swept_wing(0.85, 0.38, -0.5, 0.85, 0.16, 2.25, 0.42, -0.2, 0.75, 0.12, C_PALE)
    s.add_swept_wing(-0.85, 0.38, -0.5, 0.85, 0.16, -2.25, 0.42, -0.2, 0.75, 0.12, C_PALE)
    for sx in (-2.25, 2.25):
        s.add_cylinder_z(sx, 0.38, -0.75, 0.95, 0.36, 0.36, 0.32, 0.32, 10, C_SLATE)
        s.add_cylinder_z(sx, 0.38, 0.92, 1.10, 0.30, 0.30, 0.24, 0.24, 10, C_GLOW_CYAN)
    s.add_dome(0, 0.62, -1.35, 0.48, 0.28, 0.55, 8, 3, C_GLOW_GOLD)
    return s

def build_air_heavy_lifter() -> SolarisSculptor:
    s = SolarisSculptor()
    # Quad-nacelle twin-boom skycrane frame with open ventral tank-clamp gantry
    s.add_bevel_box(0, 0.95, -1.55, 1.55, 0.72, 1.35, 0.16, C_HULL)
    s.add_bevel_box(0, 1.05, -2.24, 1.15, 0.30, 0.10, 0.02, C_GLOW_GOLD)
    for sx in (-1.35, 1.35):
        s.add_bevel_box(sx, 0.98, 0.25, 0.52, 0.48, 3.6, 0.12, C_PALE)
        for sz in (-0.95, 1.35):
            s.add_cylinder_y(sx * 1.35, 0.55, 1.25, sz, 0.44, 0.44, 10, C_SLATE)
            s.add_torus_y(sx * 1.35, 0.58, sz, 0.36, 0.07, 12, 5, C_GLOW_CYAN)
    s.add_strut((-1.35, 1.0, 1.85), (1.35, 1.0, 1.85), 0.18, 0.18, 6, C_GOLD)
    return s

def build_air_plasma_gunship() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.25, -2.35, 0.28, 0.28, 6),
        s.poly_ring_z(0, 0.36, -0.55, 0.78, 0.52, 6),
        s.poly_ring_z(0, 0.30, 1.85, 0.55, 0.38, 6),
    ], C_DARK)
    s.add_swept_wing(0.65, 0.42, -0.5, 0.7, 0.16, 2.25, 0.08, -0.1, 0.8, 0.08, C_SLATE)
    s.add_swept_wing(-0.65, 0.42, -0.5, 0.7, 0.16, -2.25, 0.08, -0.1, 0.8, 0.08, C_SLATE)
    s.add_cylinder_z(0, -0.14, -1.1, -2.85, 0.16, 0.14, 0.11, 0.10, 8, C_GLOW_CORAL)
    s.add_dome(0, 0.62, -1.05, 0.42, 0.30, 0.72, 8, 3, C_GLOW_GOLD)
    for sx in (-1.45, 1.45):
        s.add_cylinder_z(sx, 0.02, -0.4, 0.75, 0.20, 0.20, 0.18, 0.18, 8, C_GOLD)
    return s

def build_air_awacs_cruiser() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.55, 0.18, 0.18, 8),
        s.poly_ring_z(0, 0.05, -0.85, 0.58, 0.48, 8),
        s.poly_ring_z(0, 0.05, 1.85, 0.54, 0.44, 8),
        s.poly_ring_z(0, 0.02, 2.45, 0.24, 0.22, 8),
    ], C_HULL)
    s.add_swept_wing(0.52, 0.0, -0.55, 0.75, 0.14, 2.85, 0.12, 0.55, 1.25, 0.05, C_PALE)
    s.add_swept_wing(-0.52, 0.0, -0.55, 0.75, 0.14, -2.85, 0.12, 0.55, 1.25, 0.05, C_PALE)
    # Iconic glowing dorsal AWACS rotodome disc
    s.add_strut((-0.3, 0.45, 0.2), (-0.2, 1.05, 0.2), 0.09, 0.07, 6, C_SLATE)
    s.add_strut((0.3, 0.45, 0.2), (0.2, 1.05, 0.2), 0.09, 0.07, 6, C_SLATE)
    s.add_cylinder_y(0, 1.02, 1.22, 0.2, 1.15, 1.15, 16, C_PALE)
    s.add_torus_y(0, 1.12, 0.2, 1.16, 0.07, 16, 6, C_GLOW_CYAN)
    return s

def build_air_command_carrier() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.25, 0.0, 2.55, 0.65, 3.8, 0.25, C_HULL)
    s.add_swept_wing(1.15, 0.22, -1.35, 1.55, 0.28, 3.15, 0.18, -0.35, 1.65, 0.12, C_PALE)
    s.add_swept_wing(-1.15, 0.22, -1.35, 1.55, 0.28, -3.15, 0.18, -0.35, 1.65, 0.12, C_PALE)
    for sx in (-2.05, 2.05):
        for sz in (-0.45, 0.85):
            s.add_torus_y(sx, 0.20, sz, 0.52, 0.12, 14, 6, C_SLATE)
            s.add_torus_y(sx, 0.16, sz, 0.36, 0.06, 14, 5, C_GLOW_CYAN)
    s.add_bevel_box(0, 0.88, -0.35, 0.95, 0.62, 1.35, 0.12, C_SLATE)
    s.add_bevel_box(0, 0.98, -1.04, 0.72, 0.22, 0.08, 0.02, C_GLOW_GOLD)
    return s


# =============================================================================
# CLASS 4: 14 UNIQUE SPACECRAFT (space_*)
# =============================================================================

def build_space_shuttle() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.35, 0.20, 0.16, 8),
        s.poly_ring_z(0, 0.08, -0.95, 0.78, 0.46, 8, math.pi / 8, 0.6),
        s.poly_ring_z(0, 0.08, 1.75, 0.92, 0.50, 8, math.pi / 8, 0.6),
    ], C_PALE)
    s.add_swept_wing(0.75, -0.06, -0.65, 1.65, 0.14, 2.15, 0.05, 0.85, 1.75, 0.05, C_HULL)
    s.add_swept_wing(-0.75, -0.06, -0.65, 1.65, 0.14, -2.15, 0.05, 0.85, 1.75, 0.05, C_HULL)
    s.add_fin(0.0, 0.45, 0.55, 1.75, 0.0, 1.35, 1.25, 1.85, 0.08, C_GOLD)
    for sx, sy in ((-0.32, 0.0), (0.32, 0.0), (0.0, 0.32)):
        s.add_cylinder_z(sx, sy, 1.72, 1.98, 0.18, 0.18, 0.14, 0.14, 8, C_GLOW_CYAN)
    return s

def build_space_boarding_pod() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.25, 0.38, 0.26, 6),
        s.poly_ring_z(0, 0.0, -0.75, 0.98, 0.72, 6),
        s.poly_ring_z(0, 0.0, 1.65, 0.95, 0.68, 6),
    ], C_SLATE)
    # Forward glowing plasma-cutter breaching jaws
    for sx in (-0.42, 0.42):
        s.add_strut((sx, 0.0, -1.65), (sx * 0.45, 0.0, -2.65), 0.18, 0.06, 6, C_GLOW_CORAL)
    for sx in (-0.65, 0.65):
        for sy in (-0.42, 0.42):
            s.add_cylinder_z(sx, sy, 1.55, 1.95, 0.20, 0.20, 0.16, 0.16, 8, C_GLOW_GOLD)
    return s

def build_space_cargo_lifter() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.0, -2.25, 1.25, 0.85, 1.05, 0.15, C_HULL)
    s.add_bevel_box(0, 0.12, -2.78, 0.92, 0.28, 0.08, 0.02, C_GLOW_CYAN)
    s.add_cylinder_z(0, 0.0, -1.75, 1.85, 0.22, 0.22, 0.22, 0.22, 8, C_SLATE)
    for sz in (-0.85, 0.25):
        for sx in (-0.75, 0.75):
            s.add_bevel_box(sx, 0.0, sz, 0.88, 0.88, 0.92, 0.14, C_GOLD)
    s.add_bevel_box(0, 0.0, 2.05, 1.65, 1.05, 0.85, 0.16, C_SLATE)
    for sx in (-0.52, 0.52):
        s.add_cylinder_z(sx, 0.0, 2.45, 2.72, 0.28, 0.28, 0.22, 0.22, 8, C_GLOW_CYAN)
    return s

def build_space_mining_barge() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, 0.0, 0.75, 2.15, 1.05, 2.25, 0.20, C_SLATE)
    for sx in (-1.05, 1.05):
        s.add_bevel_box(sx, 0.0, -1.25, 0.55, 0.72, 2.15, 0.14, C_GOLD)
        s.add_strut((sx, 0.0, -2.25), (sx * 0.55, 0.0, -2.75), 0.18, 0.08, 6, C_GLOW_GOLD)
    s.add_cylinder_z(0, 0.0, -0.35, -2.25, 0.20, 0.20, 0.12, 0.12, 8, C_GLOW_MINT)
    for sx in (-0.65, 0.65):
        s.add_dome(sx, 0.65, 0.85, 0.45, 0.42, 0.55, 8, 3, C_PALE)
    return s

def build_space_escape_pod() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -1.45, 0.22, 0.22, 10),
        s.poly_ring_z(0, 0.0, -0.75, 0.95, 0.95, 10),
        s.poly_ring_z(0, 0.0, 0.85, 0.88, 0.88, 10),
        s.poly_ring_z(0, 0.0, 1.45, 0.55, 0.55, 10),
    ], C_PALE)
    s.add_dome(0, 0.0, -1.15, 0.75, 0.75, 0.45, 10, 3, C_GLOW_GOLD)
    s.add_torus_z(0, 0.0, 0.15, 0.96, 0.07, 14, 5, C_GLOW_CYAN)
    return s

def build_space_corvette() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.55, 0.16, 0.14, 6),
        s.poly_ring_z(0, 0.04, -0.85, 0.88, 0.44, 6),
        s.poly_ring_z(0, 0.04, 1.85, 0.95, 0.48, 6),
    ], C_HULL)
    s.add_dome(0, 0.48, -0.25, 0.42, 0.26, 0.58, 8, 3, C_PALE)
    s.add_cylinder_z(0, 0.58, -0.65, -1.85, 0.10, 0.10, 0.07, 0.07, 6, C_GLOW_CYAN)
    s.add_swept_wing(0.85, 0.0, 0.2, 1.5, 0.12, 1.85, -0.18, 0.8, 1.7, 0.05, C_GOLD)
    s.add_swept_wing(-0.85, 0.0, 0.2, 1.5, 0.12, -1.85, -0.18, 0.8, 1.7, 0.05, C_GOLD)
    for sx in (-0.38, 0.38):
        s.add_cylinder_z(sx, 0.0, 1.82, 2.12, 0.24, 0.24, 0.18, 0.18, 8, C_GLOW_CYAN)
    return s

def build_space_stealth_corvette() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.75, 0.08, 0.06, 4, math.pi / 4),
        s.poly_ring_z(0, 0.0, -0.55, 1.15, 0.36, 4, math.pi / 4),
        s.poly_ring_z(0, 0.0, 1.95, 0.92, 0.32, 4, math.pi / 4),
    ], C_DARK)
    s.add_fin(0.55, 0.12, 0.55, 1.95, 1.45, 0.72, 1.35, 2.15, 0.07, C_SLATE)
    s.add_fin(-0.55, 0.12, 0.55, 1.95, -1.45, 0.72, 1.35, 2.15, 0.07, C_SLATE)
    s.add_bevel_box(0, 0.24, -0.65, 0.55, 0.14, 0.95, 0.04, C_GLOW_CYAN)
    return s

def build_space_frigate() -> SolarisSculptor:
    s = SolarisSculptor()
    # Homeworld-inspired tall-keeled orbital frigate
    s.add_loft([
        s.poly_ring_z(0, 0.0, -2.75, 0.22, 0.52, 6),
        s.poly_ring_z(0, 0.05, -1.15, 0.82, 1.05, 6),
        s.poly_ring_z(0, 0.05, 1.85, 0.88, 0.98, 6),
        s.poly_ring_z(0, 0.0, 2.45, 0.65, 0.68, 6),
    ], C_HULL)
    for sx in (-1.05, 1.05):
        s.add_bevel_box(sx, 0.0, -0.35, 0.45, 0.58, 1.85, 0.12, C_PALE)
        s.add_cylinder_z(sx, 0.0, -1.25, -1.75, 0.10, 0.10, 0.08, 0.08, 6, C_GLOW_GOLD)
    for sy in (-0.32, 0.32):
        s.add_cylinder_z(0, sy, 2.42, 2.72, 0.26, 0.24, 0.20, 0.18, 8, C_GLOW_CYAN)
    return s

def build_space_destroyer_escort() -> SolarisSculptor:
    s = SolarisSculptor()
    # Forked twin-prong bow with central glowing plasma accelerator channel
    s.add_bevel_box(0, 0.0, 0.95, 1.75, 0.88, 2.15, 0.20, C_HULL)
    for sx in (-0.68, 0.68):
        s.add_loft([s.poly_ring_z(sx, 0.0, -2.95, 0.14, 0.18, 6), s.poly_ring_z(sx, 0.0, -0.05, 0.46, 0.44, 6)], C_PALE)
    s.add_cylinder_z(0, 0.0, -0.1, -2.55, 0.16, 0.16, 0.10, 0.10, 8, C_GLOW_CYAN)
    s.add_bevel_box(0, 0.72, 0.85, 0.85, 0.58, 1.15, 0.12, C_SLATE)
    for sx in (-0.55, 0.55):
        s.add_cylinder_z(sx, 0.0, 2.02, 2.35, 0.30, 0.30, 0.24, 0.24, 8, C_GLOW_CYAN)
    return s

def build_space_heavy_cruiser() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -3.15, 0.16, 0.14, 8),
        s.poly_ring_z(0, 0.05, -1.45, 1.15, 0.56, 8),
        s.poly_ring_z(0, 0.05, 1.85, 1.25, 0.64, 8),
        s.poly_ring_z(0, 0.0, 2.65, 0.92, 0.48, 8),
    ], C_HULL)
    s.add_swept_wing(1.05, 0.0, -0.65, 1.55, 0.22, 2.25, 0.0, 0.55, 1.75, 0.08, C_GOLD)
    s.add_swept_wing(-1.05, 0.0, -0.65, 1.55, 0.22, -2.25, 0.0, 0.55, 1.75, 0.08, C_GOLD)
    s.add_loft([s.poly_ring_y(0, 0.55, 0.65, 0.58, 0.95, 6), s.poly_ring_y(0, 1.38, 0.85, 0.36, 0.58, 6)], C_PALE)
    s.add_bevel_box(0, 1.18, 0.32, 0.58, 0.18, 0.10, 0.02, C_GLOW_CYAN)
    for sx in (-0.52, 0.52):
        for sy in (-0.22, 0.22):
            s.add_cylinder_z(sx, sy, 2.62, 2.95, 0.20, 0.18, 0.16, 0.14, 8, C_GLOW_CYAN)
    return s

def build_space_battlecruiser() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([
        s.poly_ring_z(0, 0.0, -3.35, 0.18, 0.24, 6),
        s.poly_ring_z(0, 0.08, -1.55, 1.05, 0.68, 6),
        s.poly_ring_z(0, 0.08, 2.05, 1.32, 0.78, 6),
        s.poly_ring_z(0, 0.0, 2.85, 0.95, 0.58, 6),
    ], C_HULL)
    s.add_fin(0.0, 0.58, -1.85, 1.95, 0.0, 1.55, 0.35, 2.25, 0.22, C_PALE)
    s.add_fin(0.0, -0.58, -1.25, 1.65, 0.0, -1.25, 0.45, 1.85, 0.18, C_SLATE)
    s.add_cylinder_z(0, 0.0, -3.15, -3.55, 0.24, 0.24, 0.16, 0.16, 8, C_GLOW_CORAL)
    for sx in (-0.58, 0.0, 0.58):
        s.add_cylinder_z(sx, 0.0, 2.82, 3.18, 0.24, 0.24, 0.20, 0.20, 8, C_GLOW_CYAN)
    return s

def build_space_siege_dreadnought() -> SolarisSculptor:
    s = SolarisSculptor()
    # Colossal split twin-hull axial plasma accelerator dreadnought
    for sx in (-0.98, 0.98):
        s.add_loft([
            s.poly_ring_z(sx, 0.0, -3.35, 0.28, 0.36, 6),
            s.poly_ring_z(sx, 0.0, -1.25, 0.68, 0.78, 6),
            s.poly_ring_z(sx, 0.0, 2.45, 0.78, 0.86, 6),
        ], C_HULL)
    s.add_bevel_box(0, 0.0, 1.55, 2.55, 1.15, 1.65, 0.22, C_SLATE)
    s.add_cylinder_z(0, 0.0, 1.15, -3.25, 0.28, 0.28, 0.22, 0.22, 10, C_GLOW_GOLD)
    for sz in (-2.2, -1.0, 0.2):
        s.add_torus_z(0, 0.0, sz, 0.58, 0.08, 14, 5, C_GLOW_CYAN)
    for sx in (-1.05, 0.0, 1.05):
        s.add_cylinder_z(sx, 0.0, 2.42, 2.82, 0.34, 0.34, 0.26, 0.26, 8, C_GLOW_CYAN)
    return s

def build_space_capital_carrier() -> SolarisSculptor:
    s = SolarisSculptor()
    # Towering twin-deck hangar-slab carrier with glowing side launch trenches
    s.add_bevel_box(0, 0.42, 0.0, 2.65, 0.55, 5.4, 0.26, C_HULL)
    s.add_bevel_box(0, -0.42, 0.15, 2.45, 0.52, 5.0, 0.24, C_SLATE)
    s.add_bevel_box(0, 0.0, 0.0, 2.15, 0.32, 4.6, 0.06, C_GLOW_CYAN)  # Illuminated launch hangar bay trench
    s.add_fin(0.0, 0.68, -0.85, 2.05, 0.0, 1.88, 0.55, 2.25, 0.32, C_PALE)
    s.add_bevel_box(0, 1.48, 0.42, 0.58, 0.22, 0.14, 0.02, C_GLOW_GOLD)
    for sx in (-0.85, 0.0, 0.85):
        s.add_cylinder_z(sx, 0.0, 2.65, 3.05, 0.32, 0.28, 0.24, 0.22, 8, C_GLOW_CYAN)
    return s

def build_space_command_dreadnought() -> SolarisSculptor:
    s = SolarisSculptor()
    # Sovereign 680m dagger-prow command flagship with cathedral bridge spire & 6 fusion drives
    s.add_loft([
        s.poly_ring_z(0, 0.0, -3.55, 0.14, 0.12, 8),
        s.poly_ring_z(0, 0.08, -1.65, 1.35, 0.68, 8),
        s.poly_ring_z(0, 0.10, 1.85, 1.65, 0.88, 8),
        s.poly_ring_z(0, 0.0, 2.85, 1.25, 0.68, 8),
    ], C_HULL)
    s.add_swept_wing(1.35, 0.0, -1.15, 1.85, 0.28, 2.85, -0.05, 0.65, 2.15, 0.10, C_PALE)
    s.add_swept_wing(-1.35, 0.0, -1.15, 1.85, 0.28, -2.85, -0.05, 0.65, 2.15, 0.10, C_PALE)
    s.add_loft([s.poly_ring_y(0, 0.78, 0.75, 0.75, 1.15, 8), s.poly_ring_y(0, 1.95, 0.95, 0.48, 0.72, 8), s.poly_ring_y(0, 2.35, 0.95, 0.78, 0.52, 8)], C_SLATE)
    s.add_cylinder_y(0, 1.98, 2.26, 0.95, 0.80, 0.76, 8, C_GLOW_GOLD)
    for sx in (-0.85, 0.0, 0.85):
        for sy in (-0.28, 0.28):
            s.add_cylinder_z(sx, sy, 2.80, 3.18, 0.24, 0.22, 0.18, 0.16, 8, C_GLOW_CYAN)
    return s


# =============================================================================
# CLASS 5: 10 UNIQUE SPACE STATIONS (stn_*)
# =============================================================================

def build_stn_recon_sat() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, -0.65, 0.85, 0, 0.55, 0.48, 8, C_GOLD)
    s.add_dome(0, -0.65, 0, 0.46, -0.35, 0.46, 10, 3, C_GLOW_CYAN)
    for sx in (-1.0, 1.0):
        s.add_bevel_box(sx * 1.65, 0.12, 0.0, 2.15, 0.08, 0.95, 0.03, C_TEAL)
        s.add_strut((sx * 0.45, 0.12, 0.0), (sx * 2.65, 0.12, 0.0), 0.06, 0.05, 6, C_PALE)
    s.add_dish(0, 1.15, 0, 0.65, 0.24, 25.0, 10, C_PALE)
    return s

def build_stn_comms_relay() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, -1.85, 2.15, 0, 0.34, 0.12, 8, C_HULL)
    s.add_dome(0, 2.22, 0, 0.22, 0.22, 0.22, 8, 3, C_GLOW_CYAN, True)
    for k in range(3):
        a = k * 2.0 * math.pi / 3.0
        px, pz = math.cos(a) * 1.65, math.sin(a) * 1.65
        s.add_strut((0, 0.15, 0), (px, 0.45, pz), 0.14, 0.10, 6, C_SLATE)
        s.add_dish(px, 0.55, pz, 0.78, 0.28, 55.0, 10, C_GOLD)
    s.add_torus_y(0, -0.35, 0, 1.15, 0.08, 16, 6, C_GLOW_CYAN)
    return s

def build_stn_defense_platform() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_loft([s.poly_ring_y(0, -0.95, 0, 1.35, 1.35, 8), s.poly_ring_y(0, 0.0, 0, 2.15, 2.15, 8), s.poly_ring_y(0, 0.95, 0, 1.35, 1.35, 8)], C_HULL)
    s.add_dome(0, 0.0, 0, 0.95, 1.35, 0.95, 10, 4, C_GLOW_CYAN, True)
    for k in range(4):
        a = k * math.pi * 0.5
        ca, sa = math.cos(a), math.sin(a)
        s.add_bevel_box(ca * 2.25, 0.0, sa * 2.25, 0.85, 0.72, 0.85, 0.12, C_SLATE)
        s.add_strut((ca * 2.45, 0.15, sa * 2.45), (ca * 3.25, 0.15, sa * 3.25), 0.14, 0.09, 6, C_GLOW_CORAL)
    return s

def build_stn_solar_array() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, -0.85, 0.85, 0, 0.68, 0.68, 10, C_HULL)
    s.add_torus_y(0, 0.0, 0, 0.92, 0.12, 16, 6, C_GLOW_GOLD)
    for k in range(4):
        a = k * math.pi * 0.5
        ca, sa = math.cos(a), math.sin(a)
        s.add_strut((ca * 0.65, 0.0, sa * 0.65), (ca * 3.15, 0.0, sa * 3.15), 0.10, 0.08, 6, C_SLATE)
        for d in (1.45, 2.45):
            if abs(ca) > 0.5:
                s.add_bevel_box(ca * d, 0.0, 0.0, 0.82, 0.08, 1.65, 0.02, C_GOLD)
            else:
                s.add_bevel_box(0.0, 0.0, sa * d, 1.65, 0.08, 0.82, 0.02, C_GOLD)
    return s

def build_stn_fuel_ore_depot() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_cylinder_y(0, -2.15, 2.15, 0, 0.45, 0.45, 10, C_SLATE)
    s.add_torus_y(0, 1.65, 0, 1.15, 0.10, 16, 6, C_GLOW_CYAN)
    for k in range(6):
        a = k * math.pi / 3.0
        px, pz = math.cos(a) * 1.38, math.sin(a) * 1.38
        s.add_strut((0, 0.0, 0), (px, 0.0, pz), 0.14, 0.14, 6, C_HULL)
        s.add_dome(px, 0.0, pz, 0.62, 0.85, 0.62, 10, 4, C_PALE if k % 2 == 0 else C_GOLD, True)
    return s

def build_stn_asteroid_refinery() -> SolarisSculptor:
    s = SolarisSculptor()
    # Faceted irregular mineral asteroid wrapped in industrial Solaris ring clamps
    rings = []
    for st in range(6):
        phi = -math.pi * 0.44 + math.pi * 0.88 * (st / 5.0)
        py = math.sin(phi) * 1.35
        cr = math.cos(phi)
        pts = []
        for i in range(9):
            a = 2.0 * math.pi * i / 9.0
            jitter = 1.0 + 0.24 * math.sin(a * 3.0 + st * 1.7) + 0.14 * math.cos(a * 5.0 - st)
            pts.append((math.cos(a) * 1.55 * cr * jitter, py, math.sin(a) * 1.35 * cr * jitter))
        rings.append(pts)
    s.add_loft(rings, C_COPPER)
    s.add_torus_y(0, 0.0, 0, 2.05, 0.16, 18, 6, C_HULL)
    s.add_torus_y(0, 0.0, 0, 1.92, 0.06, 18, 5, C_GLOW_GOLD)
    for sx in (-2.05, 2.05):
        s.add_cylinder_y(sx, -1.15, 1.45, 0.0, 0.26, 0.20, 8, C_SLATE)
        s.add_dome(sx, 1.50, 0.0, 0.20, 0.20, 0.20, 8, 2, C_GLOW_CYAN, True)
    return s

def build_stn_repair_drydock() -> SolarisSculptor:
    s = SolarisSculptor()
    s.add_bevel_box(0, -1.15, 0.0, 2.35, 0.45, 4.4, 0.16, C_SLATE)
    for sz in (-1.55, 0.0, 1.55):
        s.add_strut((-1.15, -1.0, sz), (-1.95, 0.25, sz), 0.18, 0.15, 6, C_HULL)
        s.add_strut((-1.95, 0.25, sz), (-1.25, 1.45, sz), 0.15, 0.12, 6, C_GOLD)
        s.add_strut((1.15, -1.0, sz), (1.95, 0.25, sz), 0.18, 0.15, 6, C_HULL)
        s.add_strut((1.95, 0.25, sz), (1.25, 1.45, sz), 0.15, 0.12, 6, C_GOLD)
        s.add_dome(-1.25, 1.45, sz, 0.14, 0.14, 0.14, 6, 2, C_GLOW_CYAN, True)
        s.add_dome(1.25, 1.45, sz, 0.14, 0.14, 0.14, 6, 2, C_GLOW_CYAN, True)
    return s

def build_stn_orbital_shipyard() -> SolarisSculptor:
    s = SolarisSculptor()
    for sz in (-1.65, 0.0, 1.65):
        s.add_torus_z(0, 0.0, sz, 1.85, 0.22, 18, 6, C_HULL)
        s.add_torus_z(0, 0.0, sz, 1.64, 0.06, 18, 5, C_GLOW_CYAN)
    for sy in (-1.95, 1.95):
        s.add_bevel_box(0, sy, 0.0, 0.65, 0.35, 4.6, 0.10, C_SLATE)
    s.add_dome(0, 2.15, -1.25, 0.82, 0.58, 0.82, 12, 4, C_PALE)
    s.add_torus_y(0, 2.28, -1.25, 0.88, 0.07, 16, 5, C_GLOW_GOLD)
    return s

def build_stn_habitat_ring() -> SolarisSculptor:
    s = SolarisSculptor()
    # Iconic Stanford-torus 1.4km rotating artificial-gravity wheel
    s.add_torus_y(0, 0.0, 0, 2.45, 0.36, 24, 8, C_HULL)
    s.add_torus_y(0, 0.0, 0, 2.12, 0.08, 24, 5, C_GLOW_CYAN)
    s.add_cylinder_y(0, -1.45, 1.45, 0, 0.55, 0.55, 12, C_SLATE)
    s.add_dome(0, 1.45, 0, 0.55, 0.45, 0.55, 12, 3, C_PALE)
    for k in range(4):
        a = k * math.pi * 0.5
        s.add_strut((0, 0.0, 0), (math.cos(a) * 2.35, 0.0, math.sin(a) * 2.35), 0.18, 0.14, 6, C_GOLD)
    return s

def build_stn_deep_space_citadel() -> SolarisSculptor:
    s = SolarisSculptor()
    # Monumental concentric double-ring orbital headquarters + soaring crystalline spire
    s.add_torus_y(0, 0.0, 0, 2.55, 0.24, 24, 8, C_HULL)
    s.add_torus_y(0, 0.45, 0, 1.55, 0.20, 18, 6, C_PALE)
    s.add_torus_y(0, 0.0, 0, 2.32, 0.07, 24, 5, C_GLOW_CYAN)
    s.add_loft([s.poly_ring_y(0, -2.15, 0, 0.18, 0.18, 8), s.poly_ring_y(0, 0.0, 0, 0.95, 0.95, 8), s.poly_ring_y(0, 2.65, 0, 0.16, 0.16, 8)], C_SLATE)
    s.add_dome(0, 2.70, 0, 0.28, 0.35, 0.28, 10, 3, C_GLOW_GOLD, True)
    for k in range(4):
        a = k * math.pi * 0.5 + math.pi * 0.25
        s.add_strut((0, 0.0, 0), (math.cos(a) * 2.95, 0.0, math.sin(a) * 2.95), 0.20, 0.12, 6, C_GOLD)
    return s


ALL_76_BUILDERS = {
    # Class 0: 24 Buildings
    "bldg_command_citadel": build_bldg_command_citadel,
    "bldg_tactical_outpost": build_bldg_tactical_outpost,
    "bldg_fusion_plant": build_bldg_fusion_plant,
    "bldg_solar_geothermal": build_bldg_solar_geothermal,
    "bldg_ore_refinery": build_bldg_ore_refinery,
    "bldg_fuel_plant": build_bldg_fuel_plant,
    "bldg_vehicle_foundry": build_bldg_vehicle_foundry,
    "bldg_landship_yard": build_bldg_landship_yard,
    "bldg_starfighter_hangar": build_bldg_starfighter_hangar,
    "bldg_dropship_pad": build_bldg_dropship_pad,
    "bldg_shield_generator": build_bldg_shield_generator,
    "bldg_orbital_cannon": build_bldg_orbital_cannon,
    "bldg_plasma_turret": build_bldg_plasma_turret,
    "bldg_flak_battery": build_bldg_flak_battery,
    "bldg_spaceport": build_bldg_spaceport,
    "bldg_elevator_terminal": build_bldg_elevator_terminal,
    "bldg_ore_mine": build_bldg_ore_mine,
    "bldg_deep_drill": build_bldg_deep_drill,
    "bldg_comms_tower": build_bldg_comms_tower,
    "bldg_radar_array": build_bldg_radar_array,
    "bldg_barracks": build_bldg_barracks,
    "bldg_academy_medbay": build_bldg_academy_medbay,
    "bldg_coastal_dock": build_bldg_coastal_dock,
    "bldg_naval_drydock": build_bldg_naval_drydock,
    # Class 1: 10 Ground Vehicles
    "veh_plasma_tank": build_veh_plasma_tank,
    "veh_siege_hovertank": build_veh_siege_hovertank,
    "veh_recon_skimmer": build_veh_recon_skimmer,
    "veh_scout_rover": build_veh_scout_rover,
    "veh_apc": build_veh_apc,
    "veh_assault_transport": build_veh_assault_transport,
    "veh_cargo_truck": build_veh_cargo_truck,
    "veh_repair_rig": build_veh_repair_rig,
    "veh_shield_crawler": build_veh_shield_crawler,
    "veh_command_landship": build_veh_command_landship,
    # Class 2: 10 Naval Ships
    "ship_missile_boat": build_ship_missile_boat,
    "ship_hydrofoil": build_ship_hydrofoil,
    "ship_plasma_destroyer": build_ship_plasma_destroyer,
    "ship_aegis_frigate": build_ship_aegis_frigate,
    "ship_plasma_cruiser": build_ship_plasma_cruiser,
    "ship_fleet_carrier": build_ship_fleet_carrier,
    "ship_amphibious_lhd": build_ship_amphibious_lhd,
    "ship_attack_sub": build_ship_attack_sub,
    "ship_missile_sub": build_ship_missile_sub,
    "ship_relay_tender": build_ship_relay_tender,
    # Class 3: 8 Aircraft
    "air_starfighter": build_air_starfighter,
    "air_recon_fighter": build_air_recon_fighter,
    "air_plasma_bomber": build_air_plasma_bomber,
    "air_dropship": build_air_dropship,
    "air_heavy_lifter": build_air_heavy_lifter,
    "air_plasma_gunship": build_air_plasma_gunship,
    "air_awacs_cruiser": build_air_awacs_cruiser,
    "air_command_carrier": build_air_command_carrier,
    # Class 4: 14 Spacecraft
    "space_shuttle": build_space_shuttle,
    "space_boarding_pod": build_space_boarding_pod,
    "space_cargo_lifter": build_space_cargo_lifter,
    "space_mining_barge": build_space_mining_barge,
    "space_escape_pod": build_space_escape_pod,
    "space_corvette": build_space_corvette,
    "space_stealth_corvette": build_space_stealth_corvette,
    "space_frigate": build_space_frigate,
    "space_destroyer_escort": build_space_destroyer_escort,
    "space_heavy_cruiser": build_space_heavy_cruiser,
    "space_battlecruiser": build_space_battlecruiser,
    "space_siege_dreadnought": build_space_siege_dreadnought,
    "space_capital_carrier": build_space_capital_carrier,
    "space_command_dreadnought": build_space_command_dreadnought,
    # Class 5: 10 Space Stations
    "stn_recon_sat": build_stn_recon_sat,
    "stn_comms_relay": build_stn_comms_relay,
    "stn_defense_platform": build_stn_defense_platform,
    "stn_solar_array": build_stn_solar_array,
    "stn_fuel_ore_depot": build_stn_fuel_ore_depot,
    "stn_asteroid_refinery": build_stn_asteroid_refinery,
    "stn_repair_drydock": build_stn_repair_drydock,
    "stn_orbital_shipyard": build_stn_orbital_shipyard,
    "stn_habitat_ring": build_stn_habitat_ring,
    "stn_deep_space_citadel": build_stn_deep_space_citadel,
}


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    out_dir = root / "assets" / "models"
    assert len(ALL_76_BUILDERS) == 76, f"Expected 76 builders, got {len(ALL_76_BUILDERS)}"
    total_verts = 0
    total_faces = 0
    for cid, fn in ALL_76_BUILDERS.items():
        sc = fn()
        sc.write_obj(out_dir / f"{cid}.obj")
        total_verts += len(sc.v)
        total_faces += len(sc.f)
    print(f"Generated {len(ALL_76_BUILDERS)} unique sculpted Solaris .obj models ({total_verts} verts, {total_faces} faces total) in {out_dir}")


if __name__ == "__main__":
    main()
