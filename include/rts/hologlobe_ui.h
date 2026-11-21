/* SPDX-License-Identifier: MIT */
#ifndef RTS_HOLOGLOBE_UI_H
#define RTS_HOLOGLOBE_UI_H

#include "rts/boarding.h"
#include "rts/controller.h"
#include "rts/economy.h"
#include "rts/math3d.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_HOLO_MAX_SEL 32

typedef enum RtsHoloOrderMode {
    RTS_HOLO_MODE_SELECT = 0,
    RTS_HOLO_MODE_MOVE = 1,
    RTS_HOLO_MODE_ATTACK = 2,
    RTS_HOLO_MODE_ORBIT = 3,
    RTS_HOLO_MODE_HOHMANN = 4,
    RTS_HOLO_MODE_DOCK = 5,
    RTS_HOLO_MODE_REENTRY = 6,
    RTS_HOLO_MODE_BOARD = 7,
    RTS_HOLO_MODE_PLACE_BUILDING = 8,
    RTS_HOLO_MODE_COUNT = 9
} RtsHoloOrderMode;

typedef struct RtsHoloGlobeState {
    double yaw_rad;
    double pitch_rad;
    double zoom; /* 0.55 (36,000km GEO view) .. 3.2 (coastal surface zoom) */

    RtsHoloOrderMode mode;
    uint32_t selected_ids[RTS_HOLO_MAX_SEL];
    size_t selected_count;

    uint32_t hovered_entity_id;
    RtsVec3d hovered_world_pos;
    bool hovered_valid;

    double target_orbit_alt_m;
    double target_inclination_rad;

    /* 3D Base & Orbital Station Placement Ghost */
    char place_blueprint_id[64];
    int place_catalog_idx;
    RtsVec3d ghost_world_pos;
    bool ghost_valid;
    char ghost_reason[64];

    char last_feedback[128];
} RtsHoloGlobeState;

void rts_hologlobe_init(RtsHoloGlobeState *hg);
const char *rts_hologlobe_mode_name(RtsHoloOrderMode mode);
double rts_hologlobe_view_altitude_km(const RtsHoloGlobeState *hg);

void rts_hologlobe_orbit_drag(RtsHoloGlobeState *hg, double dx_rad, double dy_rad);
void rts_hologlobe_zoom(RtsHoloGlobeState *hg, double delta);

/* Exact invertible mapping between 64-bit geocentric world coords and 3D War Room Holo-Globe space */
RtsVec3d rts_hologlobe_world_to_globe(const RtsHoloGlobeState *hg, const RtsPlanetConfig *planet,
                                      RtsVec3d world_pos, RtsVec3d globe_center, double globe_radius);
RtsVec3d rts_hologlobe_globe_to_world(const RtsHoloGlobeState *hg, const RtsPlanetConfig *planet,
                                      RtsVec3d hit_point, RtsVec3d globe_center, double globe_radius,
                                      bool orbital_layer);

bool rts_hologlobe_is_selected(const RtsHoloGlobeState *hg, uint32_t entity_id);
void rts_hologlobe_clear_selection(RtsHoloGlobeState *hg);
bool rts_hologlobe_select_entity(RtsHoloGlobeState *hg, uint32_t entity_id, bool additive);

/* Fog-of-War aware 3D raycast hover & click pick (mouse, touch tap, or WebXR controller ray) */
bool rts_hologlobe_raycast(RtsHoloGlobeState *hg, const RtsWorld *w, uint8_t faction_id,
                           RtsRay ray, RtsVec3d globe_center, double globe_radius);
size_t rts_hologlobe_box_select(RtsHoloGlobeState *hg, const RtsWorld *w, uint8_t faction_id,
                                RtsVec3d globe_center, double globe_radius, RtsVec3d min_pt, RtsVec3d max_pt);

/* Arbitrary Order Issuance across all 7 surface & orbital command types */
bool rts_hologlobe_issue_order(RtsHoloGlobeState *hg, RtsWorld *w, RtsCommanderController *ctrl,
                               RtsBoardingSystem *boarding, uint8_t faction_id,
                               RtsHoloOrderMode order_mode, RtsVec3d target_pos, uint32_t target_entity_id);

/* 3D Base & Station Placement across all 24 Planetary Buildings + 10 Space Stations */
const RtsConstructBlueprint *rts_hologlobe_cycle_building(RtsHoloGlobeState *hg, const RtsWorld *w, int delta);
bool rts_hologlobe_validate_placement(RtsHoloGlobeState *hg, const RtsWorld *w, const RtsEconomySystem *eco,
                                      uint8_t faction_id, const char *blueprint_id, RtsVec3d world_pos);
uint32_t rts_hologlobe_confirm_placement(RtsHoloGlobeState *hg, RtsWorld *w, RtsEconomySystem *eco,
                                         uint8_t faction_id, const char *blueprint_id, RtsVec3d world_pos);

#endif /* RTS_HOLOGLOBE_UI_H */
