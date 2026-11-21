# Design: Solaris Retro-Futurism Visual Identity

## Color & Material Language
- **Solar Directorate ("Glacier Titanium & Cerulean Gold"):** Cool pearl-titanium primary hulls (`#DCE7F2`) paired with deep cerulean-slate shadow ribs (`#1E375A`), champagne solar-gold command chevrons (`#F4BE52`), and radiant turquoise-cyan plasma conduits (`#2CF2FF`).
- **Crimson Dominion ("Obsidian & Molten Coral"):** Sleek matte volcanic-obsidian armor (`#2A2334`) with dark plum-steel facets (`#452340`), molten coral-crimson radiator vanes (`#FF3B66`), and incandescent amber-orange threat stripes (`#FF9F38`).
- **Emerald Vanguard Mod (`mods/example_mod`):** Matte jade-obsidian plating with pearl-mint armor panels, bioluminescent aqua-emerald emitters, and warm champagne trim.

## Renderer & Shader Pipeline
- `RtsDrawCmd` adds `RtsVec3f cam_pos` and `RtsVec4f fog_color` (passed as `u_cam_pos` and `u_fog_color` in `src/gfx_gl.c`).
- `load_png_tex_ex(rr, path, nearest)` loads smooth linear-filtered (`nearest=false`) 256×256/512×256 sculpted hull, floor, ocean, and planet textures while preserving crisp nearest-neighbor filtering (`nearest=true`) for `retro_font.png` and `crew_8dir.png`.
- `sky.vert` + `sky.frag` render a unit sphere centered on the camera (`depth_write=false`) producing rich multi-stop twilight gradients, binary sun corona, diagonal planetary ring arc, and cosmic nebula backdrops.
- `mesh.frag` detects self-illuminated vertex colors (`max_rgb > 1.05`) for radiant thrusters/windows/beacons and applies hemisphere ambient + stylized satin specular + Fresnel rim lighting + atmospheric horizon haze to all armor surfaces.
- `obj_loader.c` parses optional `r g b` vertex colors on `v` lines (falling back seamlessly to `tint` when absent), maintaining full backward compatibility with standard 3-float `.obj` files.
