// SPDX-License-Identifier: MIT
in vec3 v_norm;
in vec2 v_uv;

uniform vec4 u_tint;
uniform float u_time;

out vec4 frag_color;

float hex_dist(vec2 p) {
    p = abs(p);
    return max(dot(p, normalize(vec2(1.0, 1.73205))), p.x);
}

void main() {
    /* Analytic Hexagonal Energy Lattice */
    vec2 uv = vec2(v_uv.x * 26.0, v_uv.y * 14.0);
    vec2 r = vec2(1.0, 1.73205);
    vec2 h = r * 0.5;
    vec2 a = mod(uv, r) - h;
    vec2 b = mod(uv - h, r) - h;
    vec2 gv = dot(a, a) < dot(b, b) ? a : b;
    float hd = hex_dist(gv);
    float hex_edge = smoothstep(0.41, 0.49, hd);

    float pulse = 0.72 + 0.28 * sin(v_uv.y * 34.0 - u_time * 3.5);
    float rim = pow(clamp(1.0 - abs(v_norm.z), 0.12, 1.0), 1.6);
    float energy = clamp(0.18 + hex_edge * 0.85 + rim * 0.65, 0.0, 1.4);
    float alpha = clamp(u_tint.a * (0.14 + 0.48 * rim + 0.34 * hex_edge) * pulse, 0.04, 0.82);

    vec3 glow_col = mix(u_tint.rgb, vec3(0.85, 0.98, 1.0), hex_edge * 0.55);
    frag_color = vec4(glow_col * energy, alpha);
}
