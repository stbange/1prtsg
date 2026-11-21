# Spec Delta

## ADDED Requirements

### Requirement: Sculpted Multi-Material Models and Solaris Atmospheric Shading
The renderer and OBJ loader SHALL support sculpted non-blocky multi-material `.obj` meshes with per-vertex color/emissive encoding, procedural atmospheric twilight/nebula sky domes (`sky.vert`/`sky.frag`), view-dependent Fresnel rim lighting, metallic specular highlights, analytic hexagonal energy shield lattices, and vaulted interior architecture loaded exclusively from external sibling folders.

#### Scenario: Rendering sculpted constructs under Solaris twilight sky with zero GL errors
- **GIVEN** sculpted external `.obj` models with vertex colors and external GLSL sky/mesh/shield/holo shaders
- **WHEN** all showcase scenes and the live mod overlay render via OpenGL 3.3 Core or Null backend
- **THEN** multi-material hulls, emissive accents, atmospheric sky domes, and hexagonal shields render cleanly with zero GL errors
