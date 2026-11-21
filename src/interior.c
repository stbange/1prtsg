/* SPDX-License-Identifier: MIT */
#include "rts/interior.h"
#include <string.h>

void rts_interior_init(RtsInteriorSystem *sys) {
    memset(sys, 0, sizeof(*sys));
}

RtsFrameId rts_interior_create_frame(RtsInteriorSystem *sys, RtsFrameId parent_id, const char *name, RtsVec3d local_pos, RtsQuatd local_rot, RtsGravityMode grav) {
    if (!sys || sys->frame_count >= RTS_MAX_FRAMES) {
        return RTS_FRAME_INVALID;
    }
    if (parent_id != RTS_FRAME_INVALID && !rts_interior_get_frame(sys, parent_id)) {
        return RTS_FRAME_INVALID;
    }
    RtsSpatialFrame *f = &sys->frames[sys->frame_count];
    memset(f, 0, sizeof(*f));
    f->id = (RtsFrameId)(sys->frame_count + 1u);
    f->parent_id = parent_id;
    strncpy(f->name, name ? name : "frame", sizeof(f->name) - 1);
    f->local_pos = local_pos;
    f->local_rot = rts_quat_norm(local_rot);
    f->gravity_mode = grav;
    f->artificial_g_mps2 = (grav == RTS_GRAV_ZERO_G) ? 0.0 : 9.81;
    f->acoustic_muffle = (parent_id == RTS_FRAME_INVALID) ? 0.0f : 0.65f;
    f->lod = RTS_INTERIOR_LOD_ABSTRACTED;
    f->active = true;
    sys->frame_count++;
    return f->id;
}

RtsSpatialFrame *rts_interior_get_frame(RtsInteriorSystem *sys, RtsFrameId id) {
    if (!sys || id == RTS_FRAME_INVALID || id > sys->frame_count) {
        return NULL;
    }
    RtsSpatialFrame *f = &sys->frames[id - 1u];
    return f->active ? f : NULL;
}

const RtsSpatialFrame *rts_interior_get_frame_const(const RtsInteriorSystem *sys, RtsFrameId id) {
    if (!sys || id == RTS_FRAME_INVALID || id > sys->frame_count) {
        return NULL;
    }
    const RtsSpatialFrame *f = &sys->frames[id - 1u];
    return f->active ? f : NULL;
}

bool rts_interior_reparent_frame(RtsInteriorSystem *sys, RtsFrameId frame_id, RtsFrameId new_parent_id, RtsVec3d new_local_pos, RtsQuatd new_local_rot) {
    RtsSpatialFrame *f = rts_interior_get_frame(sys, frame_id);
    if (!f || frame_id == new_parent_id) {
        return false;
    }
    /* Prevent cycles in the frame hierarchy */
    RtsFrameId walk = new_parent_id;
    int guard = 0;
    while (walk != RTS_FRAME_INVALID && guard < RTS_MAX_FRAMES) {
        if (walk == frame_id) {
            return false;
        }
        const RtsSpatialFrame *p = rts_interior_get_frame_const(sys, walk);
        if (!p) {
            return false;
        }
        walk = p->parent_id;
        ++guard;
    }
    f->parent_id = new_parent_id;
    f->local_pos = new_local_pos;
    f->local_rot = rts_quat_norm(new_local_rot);
    return true;
}

bool rts_interior_frame_world_pose(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d *out_pos, RtsQuatd *out_rot) {
    const RtsSpatialFrame *chain[RTS_MAX_FRAMES];
    size_t depth = 0;
    RtsFrameId cur = frame_id;
    while (cur != RTS_FRAME_INVALID && depth < RTS_MAX_FRAMES) {
        const RtsSpatialFrame *f = rts_interior_get_frame_const(sys, cur);
        if (!f) {
            return false;
        }
        chain[depth++] = f;
        cur = f->parent_id;
    }
    if (depth == 0) {
        return false;
    }
    RtsVec3d pos = rts_v3d(0.0, 0.0, 0.0);
    RtsQuatd rot = rts_quat_identity();
    for (size_t i = depth; i > 0; --i) {
        const RtsSpatialFrame *f = chain[i - 1];
        pos = rts_v3d_add(pos, rts_quat_rotate(rot, f->local_pos));
        rot = rts_quat_norm(rts_quat_mul(rot, f->local_rot));
    }
    if (out_pos) *out_pos = pos;
    if (out_rot) *out_rot = rot;
    return true;
}

bool rts_interior_local_to_world(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d local_p, RtsVec3d *out_world_p) {
    RtsVec3d fpos;
    RtsQuatd frot;
    if (!out_world_p || !rts_interior_frame_world_pose(sys, frame_id, &fpos, &frot)) {
        return false;
    }
    *out_world_p = rts_v3d_add(fpos, rts_quat_rotate(frot, local_p));
    return true;
}

bool rts_interior_world_to_local(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d world_p, RtsVec3d *out_local_p) {
    RtsVec3d fpos;
    RtsQuatd frot;
    if (!out_local_p || !rts_interior_frame_world_pose(sys, frame_id, &fpos, &frot)) {
        return false;
    }
    *out_local_p = rts_quat_rotate(rts_quat_conj(frot), rts_v3d_sub(world_p, fpos));
    return true;
}

uint32_t rts_interior_add_room(RtsInteriorSystem *sys, RtsFrameId frame_id, const char *name, RtsAabb bounds, RtsVec3f light_color, bool pressurized) {
    if (!sys || sys->room_count >= RTS_MAX_ROOMS || !rts_interior_get_frame(sys, frame_id)) {
        return 0u;
    }
    RtsInteriorRoom *r = &sys->rooms[sys->room_count];
    memset(r, 0, sizeof(*r));
    r->id = (uint32_t)(sys->room_count + 1u);
    r->frame_id = frame_id;
    strncpy(r->name, name ? name : "Room", sizeof(r->name) - 1);
    r->local_bounds = bounds;
    r->light_color = light_color;
    r->pressurized = pressurized;
    r->active = true;
    sys->room_count++;
    return r->id;
}

const RtsInteriorRoom *rts_interior_find_room_at(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d local_p) {
    if (!sys) {
        return NULL;
    }
    for (size_t i = 0; i < sys->room_count; ++i) {
        const RtsInteriorRoom *r = &sys->rooms[i];
        if (r->active && r->frame_id == frame_id && rts_aabb_contains(r->local_bounds, local_p)) {
            return r;
        }
    }
    return NULL;
}

uint32_t rts_interior_add_portal(RtsInteriorSystem *sys, RtsFrameId from_frame, RtsFrameId to_frame, const char *name, RtsAabb trigger_box, RtsVec3d dest_local_pos, bool is_open) {
    if (!sys || sys->portal_count >= RTS_MAX_PORTALS ||
        !rts_interior_get_frame(sys, from_frame) || !rts_interior_get_frame(sys, to_frame)) {
        return 0u;
    }
    RtsInteriorPortal *p = &sys->portals[sys->portal_count];
    memset(p, 0, sizeof(*p));
    p->id = (uint32_t)(sys->portal_count + 1u);
    p->from_frame = from_frame;
    p->to_frame = to_frame;
    strncpy(p->name, name ? name : "Portal", sizeof(p->name) - 1);
    p->trigger_box = trigger_box;
    p->dest_local_pos = dest_local_pos;
    p->is_open = is_open;
    p->active = true;
    sys->portal_count++;
    return p->id;
}

bool rts_interior_try_portal_transfer(const RtsInteriorSystem *sys, RtsFrameId *io_frame_id, RtsVec3d *io_local_pos, RtsVec3d *io_local_vel) {
    if (!sys || !io_frame_id || !io_local_pos) {
        return false;
    }
    for (size_t i = 0; i < sys->portal_count; ++i) {
        const RtsInteriorPortal *p = &sys->portals[i];
        if (p->active && p->is_open && p->from_frame == *io_frame_id && rts_aabb_contains(p->trigger_box, *io_local_pos)) {
            RtsVec3d f_pos_from, f_pos_to;
            RtsQuatd f_rot_from, f_rot_to;
            if (io_local_vel &&
                rts_interior_frame_world_pose(sys, p->from_frame, &f_pos_from, &f_rot_from) &&
                rts_interior_frame_world_pose(sys, p->to_frame, &f_pos_to, &f_rot_to)) {
                RtsVec3d world_vel = rts_quat_rotate(f_rot_from, *io_local_vel);
                *io_local_vel = rts_quat_rotate(rts_quat_conj(f_rot_to), world_vel);
            }
            *io_frame_id = p->to_frame;
            *io_local_pos = p->dest_local_pos;
            return true;
        }
    }
    return false;
}

uint32_t rts_interior_add_console(RtsInteriorSystem *sys, RtsFrameId frame_id, const char *name, RtsConsoleType type, RtsVec3d local_pos, double interact_radius, uint32_t owner_entity_id) {
    if (!sys || sys->console_count >= RTS_MAX_CONSOLES || !rts_interior_get_frame(sys, frame_id)) {
        return 0u;
    }
    RtsConsoleStation *c = &sys->consoles[sys->console_count];
    memset(c, 0, sizeof(*c));
    c->id = (uint32_t)(sys->console_count + 1u);
    c->frame_id = frame_id;
    strncpy(c->name, name ? name : "Console", sizeof(c->name) - 1);
    c->type = type;
    c->local_pos = local_pos;
    c->interact_radius = interact_radius > 0.1 ? interact_radius : 2.5;
    c->owner_entity_id = owner_entity_id;
    c->active = true;
    sys->console_count++;
    return c->id;
}

const RtsConsoleStation *rts_interior_find_nearest_console(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d local_p) {
    if (!sys) {
        return NULL;
    }
    const RtsConsoleStation *best = NULL;
    double best_d = 1e30;
    for (size_t i = 0; i < sys->console_count; ++i) {
        const RtsConsoleStation *c = &sys->consoles[i];
        if (!c->active || c->frame_id != frame_id) {
            continue;
        }
        double d = rts_v3d_dist(c->local_pos, local_p);
        if (d <= c->interact_radius && d < best_d) {
            best_d = d;
            best = c;
        }
    }
    return best;
}

void rts_interior_update_lod(RtsInteriorSystem *sys, RtsFrameId commander_frame, RtsVec3d commander_world_pos, double hydrate_radius_m) {
    if (!sys) {
        return;
    }
    for (size_t i = 0; i < sys->frame_count; ++i) {
        RtsSpatialFrame *f = &sys->frames[i];
        if (!f->active) {
            continue;
        }
        RtsVec3d fpos;
        RtsQuatd frot;
        bool near_cmd = false;
        if (f->id == commander_frame || f->parent_id == commander_frame) {
            near_cmd = true;
        } else if (rts_interior_frame_world_pose(sys, f->id, &fpos, &frot)) {
            if (rts_v3d_dist(fpos, commander_world_pos) <= hydrate_radius_m) {
                near_cmd = true;
            }
        }
        f->lod = near_cmd ? RTS_INTERIOR_LOD_HYDRATED : RTS_INTERIOR_LOD_ABSTRACTED;
    }
}
