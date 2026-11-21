// SPDX-License-Identifier: MIT
in vec2 v_uv;
in vec3 v_col;

uniform sampler2D u_tex;
uniform vec4 u_tint;

out vec4 frag_color;

void main() {
    vec4 t = texture(u_tex, v_uv);
    float a = t.a * u_tint.a;
    if (a < 0.02) {
        discard;
    }
    frag_color = vec4(t.rgb * v_col, a);
}
