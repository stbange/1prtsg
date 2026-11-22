/* SPDX-License-Identifier: MIT */
#ifndef RTS_UI_MENUS_H
#define RTS_UI_MENUS_H

#include "rts/economy.h"
#include "rts/mod_manager.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_SAVE_SLOT_COUNT 5

typedef enum RtsMenuScreen {
    RTS_MENU_NONE = 0,
    RTS_MENU_MAIN = 1,
    RTS_MENU_SKIRMISH_SETUP = 2,
    RTS_MENU_SCENARIOS = 3,
    RTS_MENU_SANDBOX_PICKER = 4,
    RTS_MENU_SAVE_LOAD = 5,
    RTS_MENU_MODS = 6,
    RTS_MENU_PAUSE = 7,
    RTS_MENU_MATCH_END = 8
} RtsMenuScreen;

typedef enum RtsMenuAction {
    RTS_MENU_ACT_NONE = 0,
    RTS_MENU_ACT_START_SKIRMISH = 1,
    RTS_MENU_ACT_START_SANDBOX = 2,
    RTS_MENU_ACT_START_TACTICAL = 3,
    RTS_MENU_ACT_LAUNCH_SCENARIO = 4,
    RTS_MENU_ACT_RESUME = 5,
    RTS_MENU_ACT_SAVE_SLOT = 6,
    RTS_MENU_ACT_LOAD_SLOT = 7,
    RTS_MENU_ACT_TOGGLE_MOD = 8,
    RTS_MENU_ACT_HOT_RELOAD = 9,
    RTS_MENU_ACT_QUIT = 10
} RtsMenuAction;

typedef struct RtsSaveSlotMeta {
    bool occupied;
    char label[64];
    char timestamp[32];
    double sim_time_s;
    size_t entity_count;
} RtsSaveSlotMeta;

typedef struct RtsMenuState {
    RtsMenuScreen screen;
    RtsMenuScreen return_screen;
    int selected_row;

    /* Skirmish Setup Configurator (User Defaults: Ironman ON, Balanced AI) */
    bool skirmish_ironman;
    bool skirmish_ai_aggressive;
    int planet_radius_preset;   /* 0 = 1,200km Moon, 1 = 6,371km Helios Prime, 2 = 12,000km Super-Earth */
    char active_mod_id[64];

    /* Dynamic External Mod Browser */
    RtsModCatalog mod_catalog;
    int selected_mod_idx;

    /* Curated Scenario & Sandbox Construct Selection */
    int scenario_idx;           /* 0..3 */
    int sandbox_bp_idx;         /* 0..75 across all 76 enterable constructs */

    /* Persistent 5-Slot Save/Load Browser */
    RtsSaveSlotMeta slots[RTS_SAVE_SLOT_COUNT];
    int active_slot;

    /* Match End Summary */
    bool victory;
    char match_summary[160];
    char status_note[160];
} RtsMenuState;

void rts_menu_init(RtsMenuState *ms);
void rts_menu_sync_mods(RtsMenuState *ms, const RtsVfs *vfs);

/* Screen metadata & item labels for rendering and keyboard/mouse input */
const char *rts_menu_screen_title(RtsMenuScreen screen);
int rts_menu_item_count(const RtsMenuState *ms);
void rts_menu_item_label(const RtsMenuState *ms, const RtsWorld *world, int idx, char *out_buf, size_t cap);

/* Keyboard navigation and confirmation */
void rts_menu_move_selection(RtsMenuState *ms, int delta);
RtsMenuAction rts_menu_activate(RtsMenuState *ms);
void rts_menu_back(RtsMenuState *ms);

/* Mouse/Touch click hit-test matching the Solaris Menu panel geometry */
RtsMenuAction rts_menu_click(RtsMenuState *ms, float click_x, float click_y, float screen_w, float screen_h);

/* 4 Curated Scenario metadata helpers */
const char *rts_scenario_id(int idx);
const char *rts_scenario_title(int idx);
const char *rts_scenario_brief(int idx);
double rts_planet_preset_radius_m(int preset);

/* Persistent 5-Slot Save/Load (OS disk on Desktop, localStorage on Emscripten Web) */
bool rts_menu_save_slot(RtsMenuState *ms, int slot, const char *save_dir, const RtsWorld *w, const RtsEconomySystem *eco);
bool rts_menu_load_slot(RtsMenuState *ms, int slot, const char *save_dir, RtsWorld *w, RtsEconomySystem *eco);
void rts_menu_refresh_slots(RtsMenuState *ms, const char *save_dir);

#endif /* RTS_UI_MENUS_H */
