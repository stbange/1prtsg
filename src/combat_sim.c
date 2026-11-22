/* SPDX-License-Identifier: MIT */
#include "rts/combat_sim.h"
#include <math.h>
#include <string.h>

static int find_entity_slot(const RtsWorld *w, uint32_t entity_id) {
    if (!w || entity_id == 0u) return -1;
    for (size_t i = 0; i < w->entity_count; ++i) {
        if (w->entities[i].active && w->entities[i].id == entity_id) return (int)i;
    }
    return -1;
}

void rts_combat_init(RtsCombatSim *cs) {
    if (!cs) return;
    memset(cs, 0, sizeof(*cs));
    rts_voxel_init(&cs->destructible_patch, 2.0f);
    rts_voxel_set_palette(&cs->destructible_patch, 1u, (RtsVec3f){0.42f, 0.56f, 0.66f});
    rts_voxel_set_palette(&cs->destructible_patch, 2u, (RtsVec3f){0.28f, 0.34f, 0.42f});
    rts_voxel_fill_box(&cs->destructible_patch, 0, 0, 0, 15, 4, 15, 1u);
    rts_voxel_fill_box(&cs->destructible_patch, 4, 5, 4, 11, 7, 11, 2u);
    cs->voxel_patch_initialized = true;
}

RtsWeaponKind rts_combat_weapon_for_entity(const RtsEntity *e) {
    if (!e) return RTS_WPN_PLASMA_BOLT;
    if (strstr(e->blueprint_id, "emp") != NULL || strstr(e->blueprint_id, "ew") != NULL) return RTS_WPN_EMP_PULSE;
    if (strstr(e->blueprint_id, "aa") != NULL || strstr(e->blueprint_id, "flak") != NULL) return RTS_WPN_AA_FLAK;
    if (e->construct_class == RTS_CLASS_SPACE_STATION || strstr(e->blueprint_id, "ion") != NULL ||
        strstr(e->blueprint_id, "orbital") != NULL) return RTS_WPN_ORBITAL_ION;
    if (e->construct_class == RTS_CLASS_SPACECRAFT && e->max_hull >= 800.0f) return RTS_WPN_BEAM_LANCE;
    if (e->construct_class == RTS_CLASS_NAVAL_SHIP || e->construct_class == RTS_CLASS_AIRCRAFT) return RTS_WPN_HOMING_TORPEDO;
    return RTS_WPN_PLASMA_BOLT;
}

RtsSubsystemKind rts_combat_hit_subsystem(const RtsEntity *target, RtsVec3d impact_world) {
    if (!target) return RTS_SUBSYS_REACTOR;
    RtsVec3d diff = rts_v3d_sub(impact_world, target->pos);
    RtsVec3d local = rts_quat_rotate(rts_quat_conj(target->rot), diff);
    double half_l = fmax((double)target->dimensions_m.z * 0.25, 2.0);
    double half_h = fmax((double)target->dimensions_m.y * 0.25, 2.0);
    if (local.y > half_h) return RTS_SUBSYS_BRIDGE;
    if (local.z > half_l) return RTS_SUBSYS_ENGINES;
    if (local.z < -half_l) return RTS_SUBSYS_WEAPONS;
    if (fabs(local.x) > fmax((double)target->dimensions_m.x * 0.3, 2.0)) return RTS_SUBSYS_SENSORS;
    return RTS_SUBSYS_REACTOR;
}

void rts_combat_apply_emp_prime(RtsCombatSim *cs, const RtsWorld *w, uint32_t entity_id, float duration_s) {
    if (!cs || !w || duration_s <= 0.0f) return;
    int slot = find_entity_slot(w, entity_id);
    if (slot >= 0 && slot < RTS_MAX_ENTITIES) {
        if (duration_s > cs->emp_prime_timer[slot]) cs->emp_prime_timer[slot] = duration_s;
    }
}

bool rts_combat_is_emp_primed(const RtsCombatSim *cs, const RtsWorld *w, uint32_t entity_id) {
    if (!cs || !w) return false;
    int slot = find_entity_slot(w, entity_id);
    return (slot >= 0 && slot < RTS_MAX_ENTITIES && cs->emp_prime_timer[slot] > 0.0f);
}

static void spawn_shield_ripple(RtsCombatSim *cs, uint32_t entity_id, RtsVec3d dir, bool emp_tint) {
    for (size_t i = 0; i < RTS_COMBAT_MAX_RIPPLES; ++i) {
        if (!cs->ripples[i].active) {
            RtsVec3d n = rts_v3d_norm(dir);
            cs->ripples[i].active = true;
            cs->ripples[i].entity_id = entity_id;
            cs->ripples[i].local_dir = (RtsVec3f){(float)n.x, (float)n.y, (float)n.z};
            cs->ripples[i].timer_s = 0.0f;
            cs->ripples[i].duration_s = 0.65f;
            cs->ripples[i].intensity = 1.0f;
            cs->ripples[i].emp_tint = emp_tint;
            return;
        }
    }
}

int rts_combat_carve_voxel_crater(RtsCombatSim *cs, RtsVec3d world_pos, float radius_m) {
    if (!cs || radius_m <= 0.1f) return 0;
    if (!cs->voxel_patch_initialized) rts_combat_init(cs);
    if (cs->crater_count < RTS_COMBAT_MAX_CRATERS) {
        cs->craters[cs->crater_count].world_pos = world_pos;
        cs->craters[cs->crater_count].radius_m = radius_m;
        cs->craters[cs->crater_count].active = true;
        cs->crater_count++;
    }
    /* Map local world XZ into our 16x16x16 destructible voxel patch centered at (8, 5, 8) */
    int cx = 8 + ((int)floor(world_pos.x * 0.25) % 5);
    int cy = 5;
    int cz = 8 + ((int)floor(world_pos.z * 0.25) % 5);
    int r_vox = (int)ceilf(radius_m / fmaxf(cs->destructible_patch.voxel_size_m, 0.5f));
    if (r_vox < 1) r_vox = 1;
    if (r_vox > 5) r_vox = 5;
    int removed = 0;
    for (int z = cz - r_vox; z <= cz + r_vox; ++z) {
        for (int y = cy - r_vox; y <= cy + r_vox; ++y) {
            for (int x = cx - r_vox; x <= cx + r_vox; ++x) {
                int dx = x - cx, dy = y - cy, dz = z - cz;
                if (dx * dx + dy * dy + dz * dz <= r_vox * r_vox) {
                    if (rts_voxel_get(&cs->destructible_patch, x, y, z) != 0u) {
                        rts_voxel_set(&cs->destructible_patch, x, y, z, 0u);
                        removed++;
                    }
                }
            }
        }
    }
    return removed;
}

static void apply_impact(RtsCombatSim *cs, RtsWorld *w, RtsMegaInterior *active_mega,
                         const RtsLiveProjectile *p, RtsEntity *target) {
    if (!target || !target->active) return;
    RtsSubsystemKind sub = rts_combat_hit_subsystem(target, p->pos);
    float dmg = p->damage;
    bool emp_hit = (p->kind == RTS_WPN_EMP_PULSE);

    if (emp_hit) {
        rts_combat_apply_emp_prime(cs, w, target->id, 6.0f);
    } else if (rts_combat_is_emp_primed(cs, w, target->id)) {
        dmg *= 2.4f; /* Signature Solaris EMP-Prime x2.4 synergy! */
        cs->emp_combo_hits++;
    }

    float prev_sub = target->subsystem_hp[sub];
    rts_world_apply_damage(w, target->id, dmg, sub);
    cs->total_hits++;
    spawn_shield_ripple(cs, target->id, rts_v3d_sub(p->pos, target->pos), emp_hit);

    if (target->subsystem_hp[sub] < prev_sub && target->subsystem_hp[sub] < 75.0f) {
        cs->subsystem_breaches++;
        /* Propagate exterior compartment hit directly into matching interior room so Engineers rush to weld it! */
        if (active_mega && active_mega->entity_id == target->id) {
            RtsRoomKind want = (sub == RTS_SUBSYS_ENGINES || sub == RTS_SUBSYS_REACTOR) ? RTS_ROOM_REACTOR
                             : (sub == RTS_SUBSYS_BRIDGE ? RTS_ROOM_BRIDGE_CIC
                             : (sub == RTS_SUBSYS_WEAPONS ? RTS_ROOM_WEAPONS : RTS_ROOM_SENSOR_LAB));
            for (uint32_t i = 0; i < active_mega->total_rooms && i < RTS_MEGA_MAX_ROOMS; ++i) {
                if (active_mega->rooms[i].kind == want) {
                    active_mega->rooms[i].damaged_pct = fminf(100.0f, active_mega->rooms[i].damaged_pct + 22.0f);
                    break;
                }
            }
        }
    }
    if (p->kind == RTS_WPN_ORBITAL_ION || p->kind == RTS_WPN_HOMING_TORPEDO) {
        rts_combat_carve_voxel_crater(cs, target->pos, (p->kind == RTS_WPN_ORBITAL_ION) ? 6.0f : 3.5f);
    }
}

bool rts_combat_fire(RtsCombatSim *cs, const RtsWorld *w, uint32_t shooter_id,
                     uint32_t target_id, RtsVec3d target_pos, RtsWeaponKind kind) {
    if (!cs || !w) return false;
    const RtsEntity *shooter = rts_world_get_entity_const(w, shooter_id);
    if (!shooter || !shooter->active) return false;
    const RtsEntity *tgt = (target_id != 0u) ? rts_world_get_entity_const(w, target_id) : NULL;
    RtsVec3d aim = tgt ? tgt->pos : target_pos;
    RtsVec3d dir = rts_v3d_norm(rts_v3d_sub(aim, shooter->pos));
    if (rts_v3d_len_sq(dir) < 1e-9) dir = rts_v3d(0.0, 0.0, -1.0);

    for (size_t i = 0; i < RTS_COMBAT_MAX_PROJ; ++i) {
        if (!cs->projs[i].active) {
            RtsLiveProjectile *p = &cs->projs[i];
            memset(p, 0, sizeof(*p));
            p->active = true;
            p->kind = kind;
            p->shooter_id = shooter_id;
            p->target_id = target_id;
            p->faction_id = shooter->faction_id;
            p->origin = rts_v3d_add(shooter->pos, rts_v3d_scale(dir, 3.0));
            p->pos = p->origin;
            switch (kind) {
                case RTS_WPN_BEAM_LANCE:     p->speed_mps = 480.0f; p->damage = 68.0f;  p->max_life_s = 0.55f; break;
                case RTS_WPN_HOMING_TORPEDO: p->speed_mps = 125.0f; p->damage = 85.0f;  p->max_life_s = 5.5f;  break;
                case RTS_WPN_AA_FLAK:        p->speed_mps = 240.0f; p->damage = 34.0f;  p->max_life_s = 2.4f;  break;
                case RTS_WPN_ORBITAL_ION:    p->speed_mps = 620.0f; p->damage = 140.0f; p->max_life_s = 1.2f;  break;
                case RTS_WPN_EMP_PULSE:      p->speed_mps = 210.0f; p->damage = 28.0f;  p->max_life_s = 2.8f;  break;
                case RTS_WPN_PLASMA_BOLT:
                default:                     p->speed_mps = 180.0f; p->damage = 46.0f;  p->max_life_s = 3.2f;  break;
            }
            p->vel = rts_v3d_scale(dir, (double)p->speed_mps);
            cs->total_shots_fired++;
            return true;
        }
    }
    return false;
}

void rts_combat_step(RtsCombatSim *cs, RtsWorld *w, RtsMegaInterior *active_mega, double dt_s) {
    if (!cs || !w || dt_s <= 0.0) return;

    /* Tick EMP-Prime timers & weapon cooldowns, and execute autonomous live-world firing for units with Attack orders */
    for (size_t i = 0; i < w->entity_count && i < RTS_MAX_ENTITIES; ++i) {
        if (cs->emp_prime_timer[i] > 0.0f) {
            cs->emp_prime_timer[i] = fmaxf(0.0f, cs->emp_prime_timer[i] - (float)dt_s);
        }
        if (cs->weapon_cooldown[i] > 0.0f) {
            cs->weapon_cooldown[i] = fmaxf(0.0f, cs->weapon_cooldown[i] - (float)dt_s);
        }
        const RtsEntity *e = &w->entities[i];
        if (!e->active || e->subsystem_hp[RTS_SUBSYS_WEAPONS] <= 1.0f) continue;
        if (cs->weapon_cooldown[i] > 0.0f) continue;

        uint32_t tid = 0u;
        if (e->order_type == RTS_ORDER_ATTACK && e->order_target_entity != 0u) {
            const RtsEntity *t = rts_world_get_entity_const(w, e->order_target_entity);
            if (t && t->active && t->faction_id != e->faction_id && rts_v3d_dist(e->pos, t->pos) < 550.0) {
                tid = t->id;
            }
        }
        if (tid == 0u && e->construct_class != RTS_CLASS_BUILDING) {
            double best_d = 260.0;
            for (size_t j = 0; j < w->entity_count; ++j) {
                const RtsEntity *o = &w->entities[j];
                if (!o->active || o->faction_id == e->faction_id || o->orbital_mode != e->orbital_mode) continue;
                double d = rts_v3d_dist(e->pos, o->pos);
                if (d < best_d) { best_d = d; tid = o->id; }
            }
        }
        if (tid != 0u) {
            RtsWeaponKind wk = rts_combat_weapon_for_entity(e);
            if (rts_combat_fire(cs, w, e->id, tid, rts_v3d(0, 0, 0), wk)) {
                cs->weapon_cooldown[i] = (wk == RTS_WPN_ORBITAL_ION) ? 2.6f : ((wk == RTS_WPN_BEAM_LANCE) ? 1.8f : 1.15f);
            }
        }
    }

    /* Advance active projectiles & check impacts */
    for (size_t i = 0; i < RTS_COMBAT_MAX_PROJ; ++i) {
        RtsLiveProjectile *p = &cs->projs[i];
        if (!p->active) continue;
        p->age_s += (float)dt_s;
        if (p->age_s >= p->max_life_s) { p->active = false; continue; }

        if (p->kind == RTS_WPN_HOMING_TORPEDO && p->target_id != 0u) {
            const RtsEntity *tgt = rts_world_get_entity_const(w, p->target_id);
            if (tgt && tgt->active) {
                RtsVec3d steer = rts_v3d_norm(rts_v3d_sub(tgt->pos, p->pos));
                p->vel = rts_v3d_scale(rts_v3d_norm(rts_v3d_add(rts_v3d_norm(p->vel), rts_v3d_scale(steer, dt_s * 5.0))),
                                       (double)p->speed_mps);
            }
        }
        p->pos = rts_v3d_add(p->pos, rts_v3d_scale(p->vel, dt_s));

        /* Beam lances & orbital ion cannons resolve immediate energy hit on first tick while staying visible briefly */
        if (p->kind == RTS_WPN_BEAM_LANCE || p->kind == RTS_WPN_ORBITAL_ION) {
            if (p->target_id != 0u && p->age_s <= (float)dt_s * 1.5f) {
                RtsEntity *tgt = rts_world_get_entity(w, p->target_id);
                if (tgt && tgt->active && tgt->faction_id != p->faction_id) {
                    p->pos = tgt->pos;
                    apply_impact(cs, w, active_mega, p, tgt);
                }
            }
            continue;
        }

        for (size_t j = 0; j < w->entity_count; ++j) {
            RtsEntity *t = &w->entities[j];
            if (!t->active || t->id == p->shooter_id || t->faction_id == p->faction_id) continue;
            double hit_r = fmax(fmax((double)t->dimensions_m.x, (double)t->dimensions_m.z) * 0.55, 6.5);
            if (rts_v3d_dist(p->pos, t->pos) <= hit_r) {
                apply_impact(cs, w, active_mega, p, t);
                p->active = false;
                break;
            }
        }
    }

    /* Update directional hex-shield impact ripples */
    for (size_t i = 0; i < RTS_COMBAT_MAX_RIPPLES; ++i) {
        if (!cs->ripples[i].active) continue;
        cs->ripples[i].timer_s += (float)dt_s;
        if (cs->ripples[i].timer_s >= cs->ripples[i].duration_s) {
            cs->ripples[i].active = false;
        } else {
            cs->ripples[i].intensity = 1.0f - (cs->ripples[i].timer_s / cs->ripples[i].duration_s);
        }
    }
}

bool rts_combat_is_active(const RtsCombatSim *cs, const RtsWorld *w) {
    if (!cs || !w) return false;
    for (size_t i = 0; i < RTS_COMBAT_MAX_PROJ; ++i) {
        if (cs->projs[i].active) return true;
    }
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *a = &w->entities[i];
        if (!a->active || a->faction_id != 1u) continue;
        for (size_t j = 0; j < w->entity_count; ++j) {
            const RtsEntity *b = &w->entities[j];
            if (!b->active || b->faction_id == 1u) continue;
            if (rts_v3d_dist(a->pos, b->pos) < 180.0) return true;
        }
    }
    return false;
}
