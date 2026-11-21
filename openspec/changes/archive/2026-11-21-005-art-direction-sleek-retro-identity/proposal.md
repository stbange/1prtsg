# Proposal: Signature Visual Identity — Sleek Sculpted Low-Poly & Solaris Synth-Opera Art Direction

## Why
The initial Phase 1–4 art used stacked rectangular boxes (`add_box`), flat primary colors, and basic diffuse lighting, creating a blocky look without a distinctive visual identity. Upgrading the external meshes, textures, shaders, sky domes, and interior architecture will establish a cohesive, iconic **"Solaris Retro-Futurism"** identity (sleek chamfered Syd Mead / Homeworld-inspired silhouettes, glacier-titanium & cerulean-gold vs. volcanic-obsidian & molten-coral palettes, atmospheric twilight/nebula sky domes, Fresnel rim lighting, emissive accents, and vaulted architectural interiors) while preserving 100% external non-baked assets and >= 95% coverage.

## What Changes
- **OBJ Vertex Color & Emissive Material Support (`src/obj_loader.c`):** Extend Wavefront `.obj` parsing to support standard 6-float vertex color entries (`v x y z r g b`) so sculpted external models carry multi-material armor plating, dark titanium recesses, brushed solar-gold chevrons, and self-illuminated emissive thrusters/canopies (> 1.05 intensity) in a single draw call.
- **Sculpted Non-Blocky External `.obj` Meshes & 256px Stylized Textures (`tools/generate_retro_assets.py`):**
  - Replace box-stacked meshes with lofted polygonal fuselages, swept airfoil delta wings, tapered octagonal hulls, dome canopies, curved orbital shipyard ring-arches, articulated sculpted 3D officers, vaulted architectural bridge ribs (`interior_arch.obj`), and sculpted coastal mountain terrain (`terrain_island.obj`).
  - Upgrade external textures (`256×256` brushed-titanium & obsidian hulls, `512×256` atmospheric Helios Prime planet map, high-contrast caustic ocean, `256×64` crisp 8-dir crew sprites, sleek emerald-jade mod livery).
- **Stylized Rim-Lit & Procedural Sky Shaders (`assets/shaders/`, `include/rts/gfx.h`, `src/gfx_gl.c`, `src/retro_renderer.c`):**
  - Add `sky.vert`/`sky.frag` atmospheric twilight/nebula dome shader (sunset coral-gold horizon, electric teal zenith, planetary ring arc, binary sun halo, star clusters).
  - Upgrade `mesh.vert`/`mesh.frag` with camera-aware Fresnel rim lighting, hemisphere sky/ground ambient bounce, satin metallic Blinn-Phong specular highlights, self-illuminated emissive surfaces, and horizon haze.
  - Upgrade `shield.frag` with true analytic hexagonal honeycomb lattice and `holo.frag` with luminous holographic scanlines.
  - Recompose all showcase scenes with curved vaulted interior arches, sculpted terrain/islands, glowing thruster plumes, and refined frosted-glass angled HUD framing.
