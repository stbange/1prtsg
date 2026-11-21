/* SPDX-License-Identifier: MIT */
#ifndef RTS_INTERIOR_H
#define RTS_INTERIOR_H

#include "rts/math3d.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_MAX_FRAMES 128
#define RTS_MAX_ROOMS 384
#define RTS_MAX_PORTALS 192
#define RTS_MAX_CONSOLES 192

typedef uint32_t RtsFrameId;
#define RTS_FRAME_INVALID 0u

typedef enum RtsGravityMode {
    RTS_GRAV_PLANETARY = 0,
    RTS_GRAV_ARTIFICIAL = 1,
    RTS_GRAV_ZERO_G = 2
} RtsGravityMode;

typedef enum RtsConsoleType {
    RTS_CONSOLE_HOLO_GLOBE = 0,
    RTS_CONSOLE_BRIDGE_HELM = 1,
    RTS_CONSOLE_PILOT_SEAT = 2,
    RTS_CONSOLE_FACTORY_TERMINAL = 3,
    RTS_CONSOLE_ESCAPE_POD = 4,
    RTS_CONSOLE_TURRET_GUNNER = 5
} RtsConsoleType;

typedef enum RtsInteriorLod {
    RTS_INTERIOR_LOD_ABSTRACTED = 0,
    RTS_INTERIOR_LOD_HYDRATED = 1
} RtsInteriorLod;

typedef struct RtsSpatialFrame {
    RtsFrameId id;
    RtsFrameId parent_id;
    char name[64];
    RtsVec3d local_pos;
    RtsQuatd local_rot;
    RtsVec3d local_vel;
    RtsGravityMode gravity_mode;
    double artificial_g_mps2;
    float acoustic_muffle;
    RtsInteriorLod lod;
    bool active;
} RtsSpatialFrame;

typedef struct RtsInteriorRoom {
    uint32_t id;
    RtsFrameId frame_id;
    char name[64];
    RtsAabb local_bounds;
    RtsVec3f light_color;
    bool pressurized;
    bool active;
} RtsInteriorRoom;

typedef struct RtsInteriorPortal {
    uint32_t id;
    RtsFrameId from_frame;
    RtsFrameId to_frame;
    char name[64];
    RtsAabb trigger_box;
    RtsVec3d dest_local_pos;
    bool is_open;
    bool active;
} RtsInteriorPortal;

typedef struct RtsConsoleStation {
    uint32_t id;
    RtsFrameId frame_id;
    char name[64];
    RtsConsoleType type;
    RtsVec3d local_pos;
    double interact_radius;
    uint32_t owner_entity_id;
    bool active;
} RtsConsoleStation;

typedef struct RtsInteriorSystem {
    RtsSpatialFrame frames[RTS_MAX_FRAMES];
    size_t frame_count;
    RtsInteriorRoom rooms[RTS_MAX_ROOMS];
    size_t room_count;
    RtsInteriorPortal portals[RTS_MAX_PORTALS];
    size_t portal_count;
    RtsConsoleStation consoles[RTS_MAX_CONSOLES];
    size_t console_count;
} RtsInteriorSystem;

void rts_interior_init(RtsInteriorSystem *sys);
RtsFrameId rts_interior_create_frame(RtsInteriorSystem *sys, RtsFrameId parent_id, const char *name, RtsVec3d local_pos, RtsQuatd local_rot, RtsGravityMode grav);
RtsSpatialFrame *rts_interior_get_frame(RtsInteriorSystem *sys, RtsFrameId id);
const RtsSpatialFrame *rts_interior_get_frame_const(const RtsInteriorSystem *sys, RtsFrameId id);
bool rts_interior_reparent_frame(RtsInteriorSystem *sys, RtsFrameId frame_id, RtsFrameId new_parent_id, RtsVec3d new_local_pos, RtsQuatd new_local_rot);

bool rts_interior_local_to_world(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d local_p, RtsVec3d *out_world_p);
bool rts_interior_world_to_local(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d world_p, RtsVec3d *out_local_p);
bool rts_interior_frame_world_pose(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d *out_pos, RtsQuatd *out_rot);

uint32_t rts_interior_add_room(RtsInteriorSystem *sys, RtsFrameId frame_id, const char *name, RtsAabb bounds, RtsVec3f light_color, bool pressurized);
const RtsInteriorRoom *rts_interior_find_room_at(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d local_p);

uint32_t rts_interior_add_portal(RtsInteriorSystem *sys, RtsFrameId from_frame, RtsFrameId to_frame, const char *name, RtsAabb trigger_box, RtsVec3d dest_local_pos, bool is_open);
bool rts_interior_try_portal_transfer(const RtsInteriorSystem *sys, RtsFrameId *io_frame_id, RtsVec3d *io_local_pos, RtsVec3d *io_local_vel);

uint32_t rts_interior_add_console(RtsInteriorSystem *sys, RtsFrameId frame_id, const char *name, RtsConsoleType type, RtsVec3d local_pos, double interact_radius, uint32_t owner_entity_id);
const RtsConsoleStation *rts_interior_find_nearest_console(const RtsInteriorSystem *sys, RtsFrameId frame_id, RtsVec3d local_p);

void rts_interior_update_lod(RtsInteriorSystem *sys, RtsFrameId commander_frame, RtsVec3d commander_world_pos, double hydrate_radius_m);

#endif /* RTS_INTERIOR_H */
