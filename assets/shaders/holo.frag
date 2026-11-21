// SPDX-License-Identifier: MIT
in vec3 v_col;
in vec2 v_uv;

uniform vec4 u_tint;
uniform float u_time;

out vec4 frag_color;

void main() {
    float scan = 0.82 + 0.18 * sin(u_time * 5.0 + v_uv.y * 42.0);
    float core = 1.0 + 0.35 * (1.0 - abs(v_uv.x - 0.5) * 2.0);
    frag_color = vec4(v_col * u_tint.rgb * scan * core, u_tint.a);
}
