/* SPDX-License-Identifier: MIT */
#include "rts/save_load.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool append_fmt(char *buf, size_t cap, size_t *pos, const char *s) {
    size_t n = strlen(s);
    if (*pos + n + 1 > cap) return false;
    memcpy(buf + *pos, s, n);
    *pos += n;
    buf[*pos] = '\0';
    return true;
}

size_t rts_save_serialize(const RtsWorld *w, const RtsEconomySystem *eco, char *out_buf, size_t buf_cap) {
    if (!w || !out_buf || buf_cap < 128u) return 0u;
    size_t pos = 0;
    out_buf[0] = '\0';
    char line[512];

    snprintf(line, sizeof(line), "RTS_SAVE_V1\n");
    if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;

    snprintf(line, sizeof(line), "WORLD %" PRIu64 " %.17g %" PRIu64 " %d %d %u\n",
             w->tick_count, w->sim_time_s, w->rng_state,
             w->hardcore_ironman ? 1 : 0, (int)w->match_state, w->next_entity_id);
    if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;

    snprintf(line, sizeof(line), "CMD %d %d %u %.17g %.17g %.17g %.17g %.17g %d %d\n",
             w->commander.alive ? 1 : 0, w->commander.in_escape_pod ? 1 : 0,
             w->commander.occupied_entity_id,
             w->commander.local_pos.x, w->commander.local_pos.y, w->commander.local_pos.z,
             w->commander.yaw_rad, w->commander.pitch_rad,
             w->commander.mag_boots ? 1 : 0, w->commander.wrist_pad_open ? 1 : 0);
    if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;

    if (eco) {
        for (int f = 0; f < RTS_MAX_FACTIONS; ++f) {
            const RtsFactionLedger *l = &eco->ledgers[f];
            snprintf(line, sizeof(line), "LEDGER %d %.17g %.17g %.17g %.17g %u %u\n",
                     f, l->ore, l->fuel, l->power_produced, l->power_consumed,
                     l->manpower_cap, l->manpower_used);
            if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;
        }
    }

    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        snprintf(line, sizeof(line),
                 "ENT %u %s %u %.17g %.17g %.17g %.17g %.17g %.17g %.9g %.9g %.9g %.9g %.9g %.9g %u %d %d %d %.17g %.17g %.17g\n",
                 e->id, e->blueprint_id, (unsigned)e->faction_id,
                 e->pos.x, e->pos.y, e->pos.z,
                 e->vel.x, e->vel.y, e->vel.z,
                 (double)e->hull, (double)e->shield,
                 (double)e->subsystem_hp[0], (double)e->subsystem_hp[1],
                 (double)e->subsystem_hp[2], (double)e->subsystem_hp[3],
                 e->crew_count, e->orbital_mode ? 1 : 0, e->own_frame_id != RTS_FRAME_INVALID ? 1 : 0, (int)e->order_type,
                 e->order_target_pos.x, e->order_target_pos.y, e->order_target_pos.z);
        if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;
    }

    for (size_t i = 0; i < w->interiors.room_count; ++i) {
        const RtsInteriorRoom *rm = &w->interiors.rooms[i];
        if (!rm->active) continue;
        snprintf(line, sizeof(line), "ROOM %u %.9g %.9g %.9g %.9g %.9g %.9g %d %.63s\n",
                 rm->frame_id,
                 rm->local_bounds.min.x, rm->local_bounds.min.y, rm->local_bounds.min.z,
                 rm->local_bounds.max.x, rm->local_bounds.max.y, rm->local_bounds.max.z,
                 rm->pressurized ? 1 : 0, rm->name);
        if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;
    }

    for (size_t i = 0; i < w->interiors.portal_count; ++i) {
        const RtsInteriorPortal *pt = &w->interiors.portals[i];
        if (!pt->active) continue;
        snprintf(line, sizeof(line), "PORTAL %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %.63s\n",
                 pt->from_frame, pt->to_frame,
                 pt->trigger_box.min.x, pt->trigger_box.min.y, pt->trigger_box.min.z,
                 pt->trigger_box.max.x, pt->trigger_box.max.y, pt->trigger_box.max.z,
                 pt->dest_local_pos.x, pt->dest_local_pos.y, pt->dest_local_pos.z,
                 pt->is_open ? 1 : 0, pt->name);
        if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;
    }

    for (size_t i = 0; i < w->interiors.console_count; ++i) {
        const RtsConsoleStation *cs = &w->interiors.consoles[i];
        if (!cs->active) continue;
        snprintf(line, sizeof(line), "CONSOLE %u %d %.9g %.9g %.9g %.9g %u %.63s\n",
                 cs->frame_id, (int)cs->type,
                 cs->local_pos.x, cs->local_pos.y, cs->local_pos.z,
                 cs->interact_radius, cs->owner_entity_id, cs->name);
        if (!append_fmt(out_buf, buf_cap, &pos, line)) return 0u;
    }
    return pos;
}

bool rts_save_deserialize(const char *snapshot_text, RtsWorld *w, RtsEconomySystem *eco) {
    if (!snapshot_text || !w || strncmp(snapshot_text, "RTS_SAVE_V1", 11) != 0) {
        return false;
    }
    /* Clear active entities and interior frames cleanly while preserving registered blueprints & crew roles */
    w->entity_count = 0u;
    memset(w->entities, 0, sizeof(w->entities));
    rts_interior_init(&w->interiors);
    w->planet_frame_id = rts_interior_create_frame(&w->interiors, RTS_FRAME_INVALID, "Planet",
                                                   rts_v3d(0.0, 0.0, 0.0), rts_quat_identity(), RTS_GRAV_PLANETARY);

    const char *p = snapshot_text;
    char line[512];
    uint32_t saved_next_id = 1u;
    while (*p != '\0') {
        size_t len = 0;
        while (p[len] && p[len] != '\n' && len < sizeof(line) - 1) {
            line[len] = p[len];
            ++len;
        }
        line[len] = '\0';
        p += len;
        if (*p == '\n') ++p;

        if (strncmp(line, "WORLD ", 6) == 0) {
            int iron = 1, mstate = 0;
            if (sscanf(line + 6, "%" SCNu64 " %lf %" SCNu64 " %d %d %u",
                       &w->tick_count, &w->sim_time_s, &w->rng_state,
                       &iron, &mstate, &saved_next_id) == 6) {
                w->hardcore_ironman = (iron != 0);
                w->match_state = (RtsMatchState)mstate;
            }
        } else if (strncmp(line, "CMD ", 4) == 0) {
            int alive = 1, pod = 0, mag = 0, pad = 0;
            if (sscanf(line + 4, "%d %d %u %lf %lf %lf %lf %lf %d %d",
                       &alive, &pod, &w->commander.occupied_entity_id,
                       &w->commander.local_pos.x, &w->commander.local_pos.y, &w->commander.local_pos.z,
                       &w->commander.yaw_rad, &w->commander.pitch_rad, &mag, &pad) == 10) {
                w->commander.alive = (alive != 0);
                w->commander.in_escape_pod = (pod != 0);
                w->commander.mag_boots = (mag != 0);
                w->commander.wrist_pad_open = (pad != 0);
            }
        } else if (eco && strncmp(line, "LEDGER ", 7) == 0) {
            int f = 0;
            RtsFactionLedger tmp;
            memset(&tmp, 0, sizeof(tmp));
            if (sscanf(line + 7, "%d %lf %lf %lf %lf %u %u",
                       &f, &tmp.ore, &tmp.fuel, &tmp.power_produced, &tmp.power_consumed,
                       &tmp.manpower_cap, &tmp.manpower_used) == 7 && f >= 0 && f < RTS_MAX_FACTIONS) {
                eco->ledgers[f] = tmp;
            }
        } else if (strncmp(line, "ENT ", 4) == 0) {
            uint32_t id = 0, crew = 0;
            unsigned fac = 0;
            char bpid[64];
            RtsVec3d ep, ev, otarget;
            double hull = 0, sh = 0, s0 = 100, s1 = 100, s2 = 100, s3 = 100;
            int orb = 0, has_frame = 0, otype = 0;
            if (sscanf(line + 4, "%u %63s %u %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %u %d %d %d %lf %lf %lf",
                       &id, bpid, &fac, &ep.x, &ep.y, &ep.z, &ev.x, &ev.y, &ev.z,
                       &hull, &sh, &s0, &s1, &s2, &s3, &crew, &orb, &has_frame, &otype,
                       &otarget.x, &otarget.y, &otarget.z) == 22) {
                uint32_t spawned = rts_world_spawn_from_blueprint(w, bpid, bpid, (uint8_t)fac, w->planet_frame_id, ep, has_frame != 0);
                RtsEntity *e = rts_world_get_entity(w, spawned);
                if (e) {
                    e->id = id;
                    e->vel = ev;
                    e->hull = (float)hull;
                    e->shield = (float)sh;
                    e->subsystem_hp[0] = (float)s0;
                    e->subsystem_hp[1] = (float)s1;
                    e->subsystem_hp[2] = (float)s2;
                    e->subsystem_hp[3] = (float)s3;
                    e->crew_count = crew;
                    e->orbital_mode = (orb != 0);
                    e->order_type = (RtsOrderType)otype;
                    e->order_target_pos = otarget;
                }
            }
        } else if (strncmp(line, "ROOM ", 5) == 0) {
            RtsFrameId fid = 0;
            RtsAabb b;
            int press = 1;
            char rname[64];
            if (sscanf(line + 5, "%u %lf %lf %lf %lf %lf %lf %d %63[^\n]",
                       &fid, &b.min.x, &b.min.y, &b.min.z, &b.max.x, &b.max.y, &b.max.z, &press, rname) == 9) {
                rts_interior_add_room(&w->interiors, fid, rname, b, (RtsVec3f){0.72f, 0.88f, 1.0f}, press != 0);
            }
        } else if (strncmp(line, "PORTAL ", 7) == 0) {
            RtsFrameId ff = 0, ft = 0;
            RtsAabb b;
            RtsVec3d dst;
            int is_open = 1;
            char pname[64];
            if (sscanf(line + 7, "%u %u %lf %lf %lf %lf %lf %lf %lf %lf %lf %d %63[^\n]",
                       &ff, &ft, &b.min.x, &b.min.y, &b.min.z, &b.max.x, &b.max.y, &b.max.z,
                       &dst.x, &dst.y, &dst.z, &is_open, pname) == 13) {
                rts_interior_add_portal(&w->interiors, ff, ft, pname, b, dst, is_open != 0);
            }
        } else if (strncmp(line, "CONSOLE ", 8) == 0) {
            RtsFrameId fid = 0;
            int ctype = 0;
            RtsVec3d cp;
            double rad = 3.0;
            uint32_t owner = 0;
            char cname[64];
            if (sscanf(line + 8, "%u %d %lf %lf %lf %lf %u %63[^\n]",
                       &fid, &ctype, &cp.x, &cp.y, &cp.z, &rad, &owner, cname) == 8) {
                rts_interior_add_console(&w->interiors, fid, cname, (RtsConsoleType)ctype, cp, rad, owner);
            }
        }
    }
    w->next_entity_id = saved_next_id;
    const RtsEntity *occ = rts_world_get_entity_const(w, w->commander.occupied_entity_id);
    w->commander.frame_id = (occ && occ->own_frame_id != RTS_FRAME_INVALID) ? occ->own_frame_id : w->planet_frame_id;
    return true;
}

bool rts_save_to_file(const char *os_path, const RtsWorld *w, const RtsEconomySystem *eco) {
    if (!os_path || !w) return false;
    char *buf = (char *)malloc(65536u);
    if (!buf) return false;
    size_t n = rts_save_serialize(w, eco, buf, 65536u);
    if (n == 0u) {
        free(buf);
        return false;
    }
    FILE *f = fopen(os_path, "wb");
    if (!f) {
        free(buf);
        return false;
    }
    bool ok = fwrite(buf, 1, n, f) == n;
    fclose(f);
    free(buf);
    return ok;
}

bool rts_load_from_file(const char *os_path, RtsWorld *w, RtsEconomySystem *eco) {
    if (!os_path || !w) return false;
    FILE *f = fopen(os_path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1048576L) {
        fclose(f);
        return false;
    }
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    bool ok = rts_save_deserialize(buf, w, eco);
    free(buf);
    return ok;
}
