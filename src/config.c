/* SPDX-License-Identifier: MIT */
#include "rts/config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void rts_config_defaults(RtsEngineConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->window_width = 1280;
    cfg->window_height = 720;
    cfg->retro_nearest_textures = true;
    cfg->hardcore_ironman = true;
    cfg->crew_sprite_lod_distance_m = 45.0;
    cfg->planet_radius_m = 6371000.0;
}

static char *trim_span(char *s) {
    while (*s && isspace((unsigned char)*s)) ++s;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
    return s;
}

static bool parse_bool_val(const char *v) {
    return strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0;
}

bool rts_config_parse_text(const char *text, RtsEngineConfig *cfg) {
    if (!text || !cfg) return false;
    rts_config_defaults(cfg);

    const char *p = text;
    char line[512];
    while (*p != '\0') {
        size_t len = 0;
        while (p[len] && p[len] != '\n' && len < sizeof(line) - 1) {
            line[len] = p[len];
            ++len;
        }
        line[len] = '\0';
        p += len;
        if (*p == '\n') ++p;

        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_span(line);
        char *val = trim_span(eq + 1);
        if (key[0] == '\0') continue;

        if (strcmp(key, "window_width") == 0) {
            int w = atoi(val);
            if (w >= 320) cfg->window_width = w;
        } else if (strcmp(key, "window_height") == 0) {
            int h = atoi(val);
            if (h >= 240) cfg->window_height = h;
        } else if (strcmp(key, "retro_nearest_textures") == 0) {
            cfg->retro_nearest_textures = parse_bool_val(val);
        } else if (strcmp(key, "hardcore_ironman") == 0) {
            cfg->hardcore_ironman = parse_bool_val(val);
        } else if (strcmp(key, "crew_sprite_lod_distance_m") == 0) {
            double d = atof(val);
            if (d > 1.0) cfg->crew_sprite_lod_distance_m = d;
        } else if (strcmp(key, "planet_radius_m") == 0) {
            double r = atof(val);
            if (r >= 100000.0) cfg->planet_radius_m = r;
        } else if (strcmp(key, "enabled_mods") == 0 && val[0] != '\0') {
            char *cur = val;
            while (*cur && cfg->enabled_mod_count < RTS_CONFIG_MAX_MODS) {
                char *comma = strchr(cur, ',');
                if (comma) *comma = '\0';
                char *mod_id = trim_span(cur);
                if (mod_id[0] != '\0') {
                    strncpy(cfg->enabled_mods[cfg->enabled_mod_count], mod_id, 63);
                    cfg->enabled_mods[cfg->enabled_mod_count][63] = '\0';
                    cfg->enabled_mod_count++;
                }
                if (!comma) break;
                cur = comma + 1;
            }
        }
    }
    return true;
}

bool rts_config_load_vfs(const RtsVfs *vfs, const char *logical_path, RtsEngineConfig *cfg) {
    if (!vfs || !logical_path || !cfg) return false;
    size_t len = 0;
    char *txt = rts_vfs_read_text(vfs, "config", logical_path, &len);
    if (!txt) {
        rts_config_defaults(cfg);
        return false;
    }
    bool ok = rts_config_parse_text(txt, cfg);
    rts_vfs_free(txt);
    return ok;
}

void rts_config_apply(const RtsEngineConfig *cfg, RtsVfs *vfs, RtsWorld *world) {
    if (!cfg) return;
    if (world) {
        world->hardcore_ironman = cfg->hardcore_ironman;
        world->planet.radius_m = cfg->planet_radius_m;
    }
    if (vfs) {
        for (size_t i = 0; i < cfg->enabled_mod_count; ++i) {
            rts_vfs_mount_mod(vfs, cfg->enabled_mods[i]);
        }
    }
}
