// SPDX-License-Identifier: MIT
in vec3 v_norm;
in vec2 v_uv;
in vec3 v_col;
in vec3 v_world_pos;

uniform sampler2D u_tex;
uniform vec4 u_tint;
uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec4 u_fog_color;

out vec4 frag_color;

void main() {
    vec4 texel = texture(u_tex, v_uv);
    if (texel.a < 0.15) {
        discard;
    }

    vec3 n = normalize(v_norm);
    vec3 l = normalize(u_light_dir);
    vec3 v = normalize(u_cam_pos - v_world_pos);
    float em_peak = max(v_col.r, max(v_col.g, v_col.b));

    vec3 rgb;
    if (em_peak > 1.08) {
        /* Self-illuminated emissive accents (thrusters, canopies, light rails, reactor cores) */
        vec3 base_em = v_col * 0.72;
        float min_t = min(u_tint.r, min(u_tint.g, u_tint.b));
        float tint_shift = clamp((1.0 - min_t) * 1.15, 0.0, 1.0);
        rgb = mix(base_em, u_tint.rgb * em_peak * 0.76, tint_shift);
    } else {
        /* Solaris Stylized Rim-Lit Shading: hemisphere bounce + crisp satin specular + Fresnel rim */
        float ndl = dot(n, l);
        float diff = max(ndl, 0.0);
        float smooth_step_diff = mix(diff, smoothstep(0.04, 0.86, diff), 0.48);

        /* Cool azure/teal hemisphere ambient prevents muddy black shadows */
        vec3 amb_low = vec3(0.16, 0.24, 0.36);
        vec3 amb_high = vec3(0.32, 0.44, 0.62);
        vec3 ambient = mix(amb_low, amb_high, clamp(n.y * 0.5 + 0.5, 0.0, 1.0));

        /* Secondary cool cyan-lavender counter-fill light */
        vec3 fill_dir = normalize(vec3(-l.x, 0.32, -l.z));
        float fill = max(dot(n, fill_dir), 0.0) * 0.28;
        vec3 key_col = vec3(1.12, 1.02, 0.92);
        vec3 fill_col = vec3(0.34, 0.76, 0.98);

        /* Crisp metallic satin Blinn-Phong highlight */
        vec3 half_v = normalize(l + v);
        float spec = pow(max(dot(n, half_v), 0.0), 28.0) * 0.36;

        /* Signature Fresnel rim edge catch */
        float fres = pow(clamp(1.0 - max(dot(n, v), 0.0), 0.0, 1.0), 2.8);
        vec3 rim_col = mix(vec3(0.28, 0.92, 1.0), vec3(1.0, 0.62, 0.38), clamp(u_fog_color.r * 1.4, 0.0, 1.0));

        /* Preserve bright luminous cyan/gold/coral texture accents even in shadow */
        float tex_sat = max(texel.r, max(texel.g, texel.b)) - min(texel.r, min(texel.g, texel.b));
        float tex_lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
        float tex_glow = smoothstep(0.55, 0.90, tex_lum) * smoothstep(0.35, 0.75, tex_sat);

        vec3 albedo = texel.rgb * v_col * u_tint.rgb;
        vec3 lit = albedo * (ambient + key_col * smooth_step_diff * 0.92 + fill_col * fill);
        lit += vec3(1.0, 0.96, 0.90) * spec * (0.45 + 0.55 * tex_lum);
        lit += rim_col * fres * 0.44;
        rgb = mix(lit, albedo * 1.35, tex_glow * 0.75);
    }

    /* Atmospheric horizon perspective haze */
    float dist = length(u_cam_pos - v_world_pos);
    float fog = clamp((dist - 55.0) / 290.0, 0.0, 0.78) * u_fog_color.a;
    rgb = mix(rgb, u_fog_color.rgb, fog);

    /* Gentle cinematic tone curve to keep bright highlights silky */
    rgb = rgb / (vec3(1.0) + rgb * 0.18);
    frag_color = vec4(rgb, texel.a * u_tint.a);
}
