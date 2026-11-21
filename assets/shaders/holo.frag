// SPDX-License-Identifier: MIT
in vec3 v_col;
in vec2 v_uv;

uniform vec4 u_tint;
uniform float u_time;

out vec4 frag_color;

void main() {
    float pulse = 0.88 + 0.12 * sin(u_time * 4.0 + v_uv.x * 12.0);
    frag_color = vec4(v_col * u_tint.rgb * pulse, u_tint.a);
}
