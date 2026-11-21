// SPDX-License-Identifier: MIT
in vec3 v_norm;
in vec2 v_uv;

uniform vec4 u_tint;
uniform float u_time;

out vec4 frag_color;

void main() {
    float grid_u = abs(fract(v_uv.x * 24.0) - 0.5);
    float grid_v = abs(fract(v_uv.y * 14.0) - 0.5);
    float line_mask = (grid_u > 0.44 || grid_v > 0.44) ? 1.0 : 0.28;
    float scan = 0.78 + 0.22 * sin(v_uv.y * 50.0 - u_time * 3.0);
    float rim = pow(clamp(1.0 - abs(v_norm.z), 0.15, 1.0), 1.4);
    float alpha = clamp(u_tint.a * (0.28 + 0.55 * rim) * line_mask * scan, 0.05, 0.92);
    frag_color = vec4(u_tint.rgb * (0.85 + 0.35 * line_mask), alpha);
}
