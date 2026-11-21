/* SPDX-License-Identifier: MIT */
#ifndef RTS_FIELD_GEAR_H
#define RTS_FIELD_GEAR_H

#include "rts/boarding.h"
#include "rts/controller.h"
#include "rts/economy.h"
#include "rts/hologlobe_ui.h"
#include "rts/math3d.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RtsWristPadTab {
    RTS_WPAD_RADAR = 0,
    RTS_WPAD_DECK_MAP = 1,
    RTS_WPAD_FLEET_ROSTER = 2,
    RTS_WPAD_ECONOMY = 3,
    RTS_WPAD_TAB_COUNT = 4
} RtsWristPadTab;

typedef enum RtsRadialOrder {
    RTS_RADIAL_ESCORT = 0,
    RTS_RADIAL_MOVE = 1,
    RTS_RADIAL_ATTACK = 2,
    RTS_RADIAL_ORBIT = 3,
    RTS_RADIAL_DOCK = 4,
    RTS_RADIAL_BOARD = 5,
    RTS_RADIAL_EVACUATE = 6,
    RTS_RADIAL_COUNT = 7
} RtsRadialOrder;

#define RTS_FACILITY_MAX_ITEMS 16
#define RTS_RADAR_MAX_CONTACTS 48

typedef struct RtsRadarContact {
    uint32_t entity_id;
    uint8_t faction_id;
    float rel_x_norm; /* -1..+1 relative radar offset */
    float rel_z_norm;
    bool orbital;
    bool comms_linked;
    char label[32];
} RtsRadarContact;

typedef struct RtsFieldGearState {
    /* Tier 1a: Diegetic Tactical Wrist-Pad */
    bool wrist_pad_open;
    RtsWristPadTab wrist_tab;

    /* Tier 1b: Optical Binoculars (1x / 4x / 12x) & Laser Designator */
    int optics_level; /* 0 = 1x, 1 = 4x, 2 = 12x */
    bool paint_active;
    uint32_t painted_entity_id;
    RtsVec3d painted_world_pos;
    char painted_label[64];

    /* Tier 1c: 7-Order Radial Quick Wheel */
    bool radial_open;
    RtsRadialOrder radial_hovered;

    /* Tier 2: Interactive Facility, Drydock & Flight-Deck Build Console */
    bool facility_open;
    uint32_t facility_entity_id;
    char facility_name[64];
    RtsConstructClass facility_output_class;
    char facility_items[RTS_FACILITY_MAX_ITEMS][64];
    size_t facility_item_count;
    int facility_selected_idx;

    char last_status[160];
} RtsFieldGearState;

void rts_field_gear_init(RtsFieldGearState *fg);
const char *rts_wrist_tab_name(RtsWristPadTab tab);
const char *rts_radial_order_name(RtsRadialOrder ord);
float rts_field_gear_fov_deg(const RtsFieldGearState *fg);
int rts_field_gear_optics_zoom_x(const RtsFieldGearState *fg);

void rts_field_gear_toggle_wrist_pad(RtsFieldGearState *fg, RtsWorld *w);
void rts_field_gear_cycle_wrist_tab(RtsFieldGearState *fg, int delta);
int rts_field_gear_cycle_optics(RtsFieldGearState *fg);
void rts_field_gear_toggle_radial(RtsFieldGearState *fg);

/* Spherical-horizon Fog-of-War radar contact scan + Comms Relay status */
size_t rts_field_gear_scan_radar(const RtsWorld *w, uint8_t faction_id, uint32_t hq_entity_id,
                                 RtsVec3d observer_world_pos, double radar_range_m,
                                 RtsRadarContact *out_contacts, size_t max_contacts, bool *out_comms_ok);

/* Optical Laser Designator: raycasts live entities & planetary surface and designates squad strikes/LZs */
bool rts_field_gear_paint_target(RtsFieldGearState *fg, const RtsWorld *w, uint8_t faction_id, RtsRay aim_ray);
size_t rts_field_gear_designate_strike(RtsFieldGearState *fg, RtsWorld *w, uint8_t faction_id);

/* Radial Quick Order Wheel execution */
bool rts_field_gear_exec_radial(RtsFieldGearState *fg, RtsWorld *w, RtsCommanderController *ctrl,
                                RtsBoardingSystem *boarding, RtsHoloGlobeState *hg,
                                uint8_t faction_id, RtsRadialOrder ord, RtsVec3d commander_world_pos);

/* Tier 2 Interactive Factory / Drydock / Flight-Deck Console */
bool rts_facility_open_for_entity(RtsFieldGearState *fg, const RtsWorld *w, uint32_t facility_entity_id);
void rts_facility_close(RtsFieldGearState *fg);
uint32_t rts_facility_queue_index(RtsFieldGearState *fg, RtsEconomySystem *eco, const RtsWorld *w, int item_idx);

#endif /* RTS_FIELD_GEAR_H */
