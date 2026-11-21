/* SPDX-License-Identifier: MIT */
#include "rts/world.h"
#include <math.h>
#include <string.h>

void rts_world_init(RtsWorld *w, RtsPlanetConfig planet, uint64_t rng_seed) {
    memset(w, 0, sizeof(*w));
    w->planet = planet;
    rts_interior_init(&w->interiors);
    w->planet_frame_id = rts_interior_create_frame(
        &w->interiors,
        RTS_FRAME_INVALID,
        planet.name,
        rts_v3d(0.0, 0.0, 0.0),
        rts_quat_identity(),
        RTS_GRAV_PLANETARY
    );
    w->next_entity_id = 1u;
    w->hardcore_ironman = true;
    w->match_state = RTS_MATCH_ACTIVE;
    w->rng_state = rng_seed != 0u ? rng_seed : 0x9E3779B97F4A7C15ull;

    w->commander.frame_id = w->planet_frame_id;
    w->commander.health = 100.0f;
    w->commander.max_health = 100.0f;
    w->commander.exo_shield = 75.0f;
    w->commander.max_exo_shield = 75.0f;
    w->commander.alive = true;

    w->resources_ore = 2500.0;
    w->resources_fuel = 1200.0;
    w->resources_power = 400.0;
}

double rts_world_rand01(RtsWorld *w) {
    /* Deterministic xorshift64* PRNG */
    uint64_t x = w->rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    w->rng_state = x;
    return (double)((x * 2685821657736338717ull) >> 11) * (1.0 / 9007199254740992.0);
}

bool rts_world_register_blueprint(RtsWorld *w, const RtsConstructBlueprint *bp) {
    if (!w || !bp || bp->id[0] == '\0') {
        return false;
    }
    /* Allow mod override of existing blueprint id */
    for (size_t i = 0; i < w->blueprint_count; ++i) {
        if (w->blueprints[i].active && strcmp(w->blueprints[i].id, bp->id) == 0) {
            w->blueprints[i] = *bp;
            w->blueprints[i].active = true;
            return true;
        }
    }
    if (w->blueprint_count >= RTS_MAX_BLUEPRINTS) {
        return false;
    }
    w->blueprints[w->blueprint_count] = *bp;
    w->blueprints[w->blueprint_count].active = true;
    w->blueprint_count++;
    return true;
}

const RtsConstructBlueprint *rts_world_find_blueprint(const RtsWorld *w, const char *id) {
    if (!w || !id) {
        return NULL;
    }
    for (size_t i = 0; i < w->blueprint_count; ++i) {
        if (w->blueprints[i].active && strcmp(w->blueprints[i].id, id) == 0) {
            return &w->blueprints[i];
        }
    }
    return NULL;
}

bool rts_world_register_crew_role(RtsWorld *w, const RtsCrewRoleDef *role) {
    if (!w || !role || role->id[0] == '\0') {
        return false;
    }
    for (size_t i = 0; i < w->crew_role_count; ++i) {
        if (w->crew_roles[i].active && strcmp(w->crew_roles[i].id, role->id) == 0) {
            w->crew_roles[i] = *role;
            w->crew_roles[i].active = true;
            return true;
        }
    }
    if (w->crew_role_count >= RTS_MAX_CREW_ROLES) {
        return false;
    }
    w->crew_roles[w->crew_role_count] = *role;
    w->crew_roles[w->crew_role_count].active = true;
    w->crew_role_count++;
    return true;
}

uint32_t rts_world_spawn_from_blueprint(RtsWorld *w, const char *blueprint_id, const char *custom_name, uint8_t faction_id, RtsFrameId host_frame, RtsVec3d pos, bool create_interior_frame) {
    if (!w || w->entity_count >= RTS_MAX_ENTITIES) {
        return 0u;
    }
    const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, blueprint_id);
    if (!bp) {
        return 0u;
    }
    RtsEntity *e = &w->entities[w->entity_count];
    memset(e, 0, sizeof(*e));
    e->id = w->next_entity_id++;
    strncpy(e->blueprint_id, bp->id, sizeof(e->blueprint_id) - 1);
    strncpy(e->name, custom_name ? custom_name : bp->name, sizeof(e->name) - 1);
    e->construct_class = bp->construct_class;
    e->faction_id = faction_id;
    e->host_frame_id = host_frame != RTS_FRAME_INVALID ? host_frame : w->planet_frame_id;
    e->pos = pos;
    e->rot = rts_quat_identity();
    e->dimensions_m = bp->dimensions_m;
    e->max_hull = bp->max_hull > 0.0f ? bp->max_hull : 100.0f;
    e->hull = e->max_hull;
    e->max_shield = bp->max_shield;
    e->shield = bp->max_shield;
    e->shield_regen_per_s = bp->max_shield * 0.03f;
    e->max_speed_mps = bp->max_speed_mps;
    e->crew_count = bp->crew_capacity;
    e->engineer_count = bp->crew_capacity >= 4u ? bp->crew_capacity / 4u : 1u;
    e->has_holo_globe = bp->has_holo_globe;
    e->has_escape_pod = bp->has_escape_pod;
    for (int s = 0; s < RTS_SUBSYS_COUNT; ++s) {
        e->subsystem_hp[s] = 100.0f;
    }
    if (create_interior_frame) {
        RtsGravityMode gm = (bp->construct_class == RTS_CLASS_SPACE_STATION || bp->construct_class == RTS_CLASS_SPACECRAFT)
            ? RTS_GRAV_ARTIFICIAL : RTS_GRAV_PLANETARY;
        e->own_frame_id = rts_interior_create_frame(&w->interiors, e->host_frame_id, e->name, pos, e->rot, gm);
    }
    e->active = true;
    w->entity_count++;
    return e->id;
}

RtsEntity *rts_world_get_entity(RtsWorld *w, uint32_t id) {
    if (!w || id == 0u) {
        return NULL;
    }
    for (size_t i = 0; i < w->entity_count; ++i) {
        if (w->entities[i].active && w->entities[i].id == id) {
            return &w->entities[i];
        }
    }
    return NULL;
}

const RtsEntity *rts_world_get_entity_const(const RtsWorld *w, uint32_t id) {
    if (!w || id == 0u) {
        return NULL;
    }
    for (size_t i = 0; i < w->entity_count; ++i) {
        if (w->entities[i].active && w->entities[i].id == id) {
            return &w->entities[i];
        }
    }
    return NULL;
}

bool rts_world_destroy_entity(RtsWorld *w, uint32_t id) {
    RtsEntity *e = rts_world_get_entity(w, id);
    if (!e) {
        return false;
    }
    e->active = false;
    e->hull = 0.0f;
    if (w->commander.alive && !w->commander.in_escape_pod &&
        (w->commander.occupied_entity_id == id ||
         (e->own_frame_id != RTS_FRAME_INVALID && w->commander.frame_id == e->own_frame_id))) {
        rts_world_damage_commander(w, w->commander.max_health + w->commander.max_exo_shield + 100.0f);
    }
    return true;
}

bool rts_world_issue_order(RtsWorld *w, uint32_t entity_id, RtsOrderType order, RtsVec3d target_pos, uint32_t target_entity, double target_alt_m) {
    RtsEntity *e = rts_world_get_entity(w, entity_id);
    if (!e) {
        return false;
    }
    e->order_type = order;
    e->order_target_pos = target_pos;
    e->order_target_entity = target_entity;
    e->order_target_altitude_m = target_alt_m;
    return true;
}

void rts_world_apply_damage(RtsWorld *w, uint32_t entity_id, float damage, RtsSubsystemKind subsys) {
    RtsEntity *e = rts_world_get_entity(w, entity_id);
    if (!e || damage <= 0.0f) {
        return;
    }
    float rem = damage;
    if (e->shield > 0.0f) {
        e->shield_hit_flash = true;
        e->shield_hit_timer = 0.6f;
        float absorbed = rem < e->shield ? rem : e->shield;
        e->shield -= absorbed;
        rem -= absorbed;
    }
    if (rem > 0.0f) {
        e->hull -= rem;
        if (subsys < RTS_SUBSYS_COUNT) {
            e->subsystem_hp[subsys] -= rem * 0.8f;
            if (e->subsystem_hp[subsys] < 0.0f) {
                e->subsystem_hp[subsys] = 0.0f;
            }
        }
        if (e->hull <= 0.0f) {
            rts_world_destroy_entity(w, entity_id);
        }
    }
}

float rts_world_effective_speed(const RtsEntity *e) {
    if (!e || !e->active) {
        return 0.0f;
    }
    float eng_ratio = rts_clampf(e->subsystem_hp[RTS_SUBSYS_ENGINES] / 100.0f, 0.1f, 1.0f);
    float rct_ratio = rts_clampf(e->subsystem_hp[RTS_SUBSYS_REACTOR] / 100.0f, 0.2f, 1.0f);
    return e->max_speed_mps * eng_ratio * rct_ratio;
}

RtsCrewLodMode rts_world_eval_crew_lod(double dist_m, bool inside_interior, bool webxr_active, double sprite_threshold_m, double view_azimuth_rad, int dir_count, int *out_dir_index) {
    if (out_dir_index) {
        int n = dir_count > 0 ? dir_count : 8;
        double norm = fmod(view_azimuth_rad, 2.0 * RTS_PI);
        if (norm < 0.0) {
            norm += 2.0 * RTS_PI;
        }
        int idx = (int)floor((norm / (2.0 * RTS_PI)) * (double)n + 0.5) % n;
        *out_dir_index = idx;
    }
    if (inside_interior || webxr_active || dist_m < sprite_threshold_m) {
        return RTS_CREW_LOD_3D_MESH;
    }
    return RTS_CREW_LOD_SPRITE;
}

bool rts_world_commander_launch_escape_pod(RtsWorld *w) {
    if (!w || !w->commander.alive || w->commander.in_escape_pod) {
        return false;
    }
    RtsEntity *host = rts_world_get_entity(w, w->commander.occupied_entity_id);
    if (!host || !host->has_escape_pod) {
        return false;
    }
    RtsVec3d world_pos = host->pos;
    rts_interior_local_to_world(&w->interiors, w->commander.frame_id, w->commander.local_pos, &world_pos);
    RtsVec3d eject_dir = rts_v3d_norm(world_pos);
    if (rts_v3d_len_sq(eject_dir) < 0.5) {
        eject_dir = rts_v3d(0.0, 1.0, 0.0);
    }
    RtsVec3d pod_pos = rts_v3d_add(world_pos, rts_v3d_scale(eject_dir, 45.0));
    uint32_t pod_id = rts_world_spawn_from_blueprint(w, "space_escape_pod", "Commander Escape Pod", host->faction_id, w->planet_frame_id, pod_pos, true);
    if (pod_id == 0u) {
        return false;
    }
    RtsEntity *pod = rts_world_get_entity(w, pod_id);
    pod->vel = rts_v3d_add(host->vel, rts_v3d_scale(eject_dir, 60.0));
    w->commander.frame_id = pod->own_frame_id;
    w->commander.local_pos = rts_v3d(0.0, 1.2, 0.0);
    w->commander.occupied_entity_id = pod_id;
    w->commander.in_escape_pod = true;
    return true;
}

void rts_world_damage_commander(RtsWorld *w, float damage) {
    if (!w || !w->commander.alive || damage <= 0.0f) {
        return;
    }
    float rem = damage;
    if (w->commander.exo_shield > 0.0f) {
        float abs_d = rem < w->commander.exo_shield ? rem : w->commander.exo_shield;
        w->commander.exo_shield -= abs_d;
        rem -= abs_d;
    }
    if (rem > 0.0f) {
        w->commander.health -= rem;
        if (w->commander.health <= 0.0f) {
            w->commander.health = 0.0f;
            w->commander.alive = false;
            if (w->hardcore_ironman) {
                w->match_state = RTS_MATCH_DEFEAT_IRONMAN;
            }
        }
    }
}

void rts_world_step(RtsWorld *w, double dt_s) {
    if (!w || dt_s <= 0.0) {
        return;
    }
    w->sim_time_s += dt_s;
    w->tick_count++;

    for (size_t i = 0; i < w->entity_count; ++i) {
        RtsEntity *e = &w->entities[i];
        if (!e->active) {
            continue;
        }
        /* Shield flash decay & passive shield regeneration */
        if (e->shield_hit_timer > 0.0f) {
            e->shield_hit_timer -= (float)dt_s;
            if (e->shield_hit_timer <= 0.0f) {
                e->shield_hit_timer = 0.0f;
                e->shield_hit_flash = false;
            }
        } else if (e->shield < e->max_shield && e->subsystem_hp[RTS_SUBSYS_REACTOR] > 20.0f) {
            e->shield += e->shield_regen_per_s * (float)dt_s;
            if (e->shield > e->max_shield) {
                e->shield = e->max_shield;
            }
        }

        /* Engineer damage-control repair */
        if (e->engineer_count > 0u) {
            float repair_rate = (float)e->engineer_count * 2.5f * (float)dt_s;
            for (int s = 0; s < RTS_SUBSYS_COUNT; ++s) {
                if (e->subsystem_hp[s] < 100.0f) {
                    e->subsystem_hp[s] += repair_rate;
                    if (e->subsystem_hp[s] > 100.0f) {
                        e->subsystem_hp[s] = 100.0f;
                    }
                    break;
                }
            }
        }

        /* Movement & Orbital Propagation */
        if (e->orbital_mode) {
            RtsOrbitState st = {e->pos, e->vel};
            st = rts_orbit_propagate_step(&w->planet, st, rts_v3d(0.0, 0.0, 0.0), 0.0, dt_s);
            e->pos = st.pos_m;
            e->vel = st.vel_mps;
        } else if (e->order_type == RTS_ORDER_MOVE) {
            RtsVec3d diff = rts_v3d_sub(e->order_target_pos, e->pos);
            double dist = rts_v3d_len(diff);
            double step_dist = (double)rts_world_effective_speed(e) * dt_s;
            if (dist <= step_dist || dist < 0.25) {
                e->pos = e->order_target_pos;
                e->vel = rts_v3d(0.0, 0.0, 0.0);
                e->order_type = RTS_ORDER_NONE;
            } else {
                RtsVec3d dir = rts_v3d_scale(diff, 1.0 / dist);
                e->vel = rts_v3d_scale(dir, (double)rts_world_effective_speed(e));
                e->pos = rts_v3d_add(e->pos, rts_v3d_scale(dir, step_dist));
            }
        } else if (e->order_type == RTS_ORDER_ATTACK) {
            RtsEntity *tgt = rts_world_get_entity(w, e->order_target_entity);
            if (!tgt) {
                e->order_type = RTS_ORDER_NONE;
            } else if (e->subsystem_hp[RTS_SUBSYS_WEAPONS] > 10.0f) {
                double dist = rts_v3d_dist(e->pos, tgt->pos);
                if (dist <= 2000.0) {
                    rts_world_apply_damage(w, tgt->id, 25.0f * (float)dt_s, RTS_SUBSYS_ENGINES);
                }
            }
        }

        /* Sync entity's owned spatial frame with entity pose */
        if (e->own_frame_id != RTS_FRAME_INVALID) {
            RtsSpatialFrame *sf = rts_interior_get_frame(&w->interiors, e->own_frame_id);
            if (sf) {
                sf->local_pos = e->pos;
                sf->local_rot = e->rot;
                sf->local_vel = e->vel;
            }
        }
    }

    RtsVec3d cmd_world = rts_v3d(0.0, 0.0, 0.0);
    rts_interior_local_to_world(&w->interiors, w->commander.frame_id, w->commander.local_pos, &cmd_world);
    rts_interior_update_lod(&w->interiors, w->commander.frame_id, cmd_world, 500.0);
}
