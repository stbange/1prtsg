// SPDX-License-Identifier: MIT
in vec3 v_dir;

uniform vec4 u_tint;      /* x: 0=planetary twilight, 1=deep space nebula, 2=crimson alert space */
uniform vec3 u_light_dir; /* sun direction */
uniform float u_time;

out vec4 frag_color;

float hash21(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float star_layer(vec3 d, float scale, float threshold) {
    vec2 sp = vec2(atan(d.z, d.x) * 0.31831, asin(clamp(d.y, -1.0, 1.0)) * 0.63662) * scale;
    vec2 id = floor(sp);
    vec2 gv = fract(sp) - 0.5;
    float h = hash21(id);
    if (h < threshold) return 0.0;
    vec2 jitter = (vec2(hash21(id + 13.1), hash21(id + 29.7)) - 0.5) * 0.55;
    float dist = length(gv - jitter);
    float core = smoothstep(0.14, 0.015, dist);
    return core * ((h - threshold) / (1.0 - threshold));
}

void main() {
    vec3 d = normalize(v_dir);
    vec3 sun = normalize(u_light_dir);
    float y = clamp(d.y, -1.0, 1.0);

    vec3 col;
    if (u_tint.x < 0.5) {
        /* Signature Solaris Planetary Twilight: deep indigo zenith -> electric teal -> warm sunset coral-gold horizon */
        vec3 zenith   = vec3(0.03, 0.05, 0.16);
        vec3 upper    = vec3(0.07, 0.16, 0.35);
        vec3 mid_teal = vec3(0.08, 0.38, 0.54);
        vec3 horizon  = vec3(0.96, 0.54, 0.38);
        vec3 nadir    = vec3(0.03, 0.10, 0.22);

        col = mix(horizon, mid_teal, smoothstep(0.0, 0.22, y));
        col = mix(col, upper, smoothstep(0.20, 0.52, y));
        col = mix(col, zenith, smoothstep(0.48, 0.90, y));
        if (y < 0.0) {
            col = mix(horizon * 0.55, nadir, smoothstep(0.0, -0.22, y));
        }

        /* Crisp Golden Solar Corona near horizon */
        float sun_dot = max(dot(d, sun), 0.0);
        col += vec3(1.0, 0.76, 0.40) * pow(sun_dot, 16.0) * 0.65;
        col += vec3(1.0, 0.96, 0.84) * pow(sun_dot, 220.0) * 1.15;

        /* Unmistakable Sweeping Diagonal Planetary Ring Arc across the Solaris sky! */
        vec3 ring_n = normalize(vec3(0.32, 0.68, 0.66));
        float plane_dist = abs(dot(d, ring_n));
        float ring_outer = smoothstep(0.085, 0.025, plane_dist);
        float ring_gap = 1.0 - smoothstep(0.012, 0.004, abs(plane_dist - 0.032)) * 0.75;
        float ring_stripes = 0.62 + 0.38 * sin(plane_dist * 260.0);
        float above_h = smoothstep(-0.02, 0.12, y);
        vec3 ring_col = mix(vec3(0.45, 0.92, 1.0), vec3(1.0, 0.80, 0.52), clamp(sun_dot * 1.4, 0.0, 1.0));
        col += ring_col * ring_outer * ring_gap * ring_stripes * above_h * 0.52;

        /* Crisp pinpoint circular twilight stars */
        float st = star_layer(d, 70.0, 0.965) + star_layer(d, 120.0, 0.980) * 0.7;
        col += vec3(0.86, 0.96, 1.0) * st * smoothstep(0.14, 0.45, y);
    } else if (u_tint.x < 1.5) {
        /* Deep Orbital Space with Luminous Teal & Rose-Violet Nebula Veil + Pinpoint Stars */
        vec3 bg = mix(vec3(0.015, 0.022, 0.065), vec3(0.045, 0.025, 0.105), clamp(y * 0.5 + 0.5, 0.0, 1.0));
        float band1 = sin(d.x * 3.1 + d.y * 4.4 + d.z * 2.0) * cos(d.y * 2.6 - d.z * 3.3);
        float band2 = sin(d.x * 4.8 - d.z * 4.0 + 1.2) * cos(d.y * 3.2 + d.x * 2.2);
        vec3 neb_teal = vec3(0.05, 0.36, 0.52) * smoothstep(0.15, 0.95, band1);
        vec3 neb_rose = vec3(0.44, 0.12, 0.38) * smoothstep(0.22, 0.95, band2);
        col = bg + neb_teal * 0.68 + neb_rose * 0.48;

        float sun_dot = max(dot(d, sun), 0.0);
        col += vec3(1.0, 0.72, 0.36) * pow(sun_dot, 24.0) * 0.52;
        col += vec3(1.0, 0.96, 0.86) * pow(sun_dot, 260.0) * 1.15;

        float s1 = star_layer(d, 85.0, 0.955);
        float s2 = star_layer(d, 150.0, 0.972);
        col += vec3(0.75, 0.93, 1.0) * s1 + vec3(1.0, 0.88, 0.68) * s2 * 0.85;
    } else {
        /* Red-Alert Space Backdrop with Pinpoint Stars */
        vec3 bg = mix(vec3(0.08, 0.014, 0.03), vec3(0.03, 0.012, 0.06), clamp(y * 0.5 + 0.5, 0.0, 1.0));
        float neb = sin(d.x * 4.0 + d.y * 3.0) * cos(d.z * 3.5 - d.y * 2.5);
        col = bg + vec3(0.58, 0.10, 0.20) * smoothstep(0.1, 0.9, neb) * 0.55;
        col += vec3(1.0, 0.78, 0.80) * star_layer(d, 95.0, 0.968) * 0.75;
    }

    frag_color = vec4(col, 1.0);
}
