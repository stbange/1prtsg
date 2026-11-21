// SPDX-License-Identifier: MIT
in vec3 v_norm;
in vec2 v_uv;
in vec3 v_col;
in vec3 v_world_pos;

uniform sampler2D u_tex;
uniform vec4 u_tint;
uniform vec3 u_light_dir;

out vec4 frag_color;

void main() {
    vec4 texel = texture(u_tex, v_uv);
    if (texel.a < 0.15) {
        discard;
    }
    vec3 n = normalize(v_norm);
    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);
    float rim = pow(1.0 - abs(n.y), 2.0) * 0.18;
    /* Classic retro space-opera quantized Gouraud-feel lighting */
    float shade = clamp(0.32 + floor(diff * 5.0 + 0.5) / 5.0 * 0.78 + rim, 0.25, 1.25);
    vec3 rgb = texel.rgb * v_col * u_tint.rgb * shade;
    /* Subtle retro atmospheric depth tint */
    float dist = length(v_world_pos);
    float fog = clamp((dist - 160.0) / 420.0, 0.0, 0.42);
    rgb = mix(rgb, vec3(0.06, 0.14, 0.28), fog);
    frag_color = vec4(rgb, texel.a * u_tint.a);
}
