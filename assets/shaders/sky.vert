// SPDX-License-Identifier: MIT
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_norm;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec3 a_col;

uniform mat4 u_mvp;

out vec3 v_dir;

void main() {
    v_dir = normalize(a_pos);
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
