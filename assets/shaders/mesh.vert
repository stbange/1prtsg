// SPDX-License-Identifier: MIT
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_norm;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec3 a_col;

uniform mat4 u_mvp;
uniform mat4 u_model;

out vec3 v_norm;
out vec2 v_uv;
out vec3 v_col;
out vec3 v_world_pos;

void main() {
    vec4 wp = u_model * vec4(a_pos, 1.0);
    v_world_pos = wp.xyz;
    v_norm = normalize(mat3(u_model) * a_norm);
    v_uv = a_uv;
    v_col = a_col;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
