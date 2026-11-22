/* SPDX-License-Identifier: MIT */
#include "rts/ui_menus.h"
#include "rts/save_load.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#include <emscripten.h>
EM_JS(void, rts_web_storage_save, (int slot, const char *data), {
    try { localStorage.setItem("1prtsg_save_slot_" + slot, UTF8ToString(data)); } catch (e) {}
});
EM_JS(int, rts_web_storage_load, (int slot, char *out_buf, int cap), {
    try {
        var s = localStorage.getItem("1prtsg_save_slot_" + slot);
        if (!s) return 0;
        stringToUTF8(s, out_buf, cap);
        return s.length;
    } catch (e) { return 0; }
});
EM_JS(int, rts_web_storage_exists, (int slot), {
    try { return localStorage.getItem("1prtsg_save_slot_" + slot) ? 1 : 0; } catch (e) { return 0; }
});
#pragma clang diagnostic pop
#endif

static const char *const k_scenario_ids[4] = {
    "orbital_drop_defense",
    "carrier_strike",
    "escape_flagship_60s",
    "drydock_boarding"
};

static const char *const k_scenario_titles[4] = {
    "ORBITAL DROP DEFENSE",
    "CARRIER STRIKE",
    "ESCAPE THE FLAGSHIP (60S MELTDOWN)",
    "DRYDOCK BOARDING (420 ROOMS)"
};

static const char *const k_scenario_briefs[4] = {
    "DEFEND SOLAR CITADEL ALPHA AGAINST CRIMSON ORBITAL DROPSHIPS",
    "COMMAND FLEET CARRIER VANGUARD ACROSS THE SOLARIS OCEAN BAY",
    "RACE THROUGH 20 DECKS TO AN ESCAPE POD BEFORE REACTOR MELTDOWN",
    "REPEL CRIMSON BREACHERS INSIDE ORBITAL SHIPYARD ALPHA CIC"
};

void rts_menu_init(RtsMenuState *ms) {
    if (!ms) return;
    memset(ms, 0, sizeof(*ms));
    ms->screen = RTS_MENU_MAIN;
    ms->return_screen = RTS_MENU_MAIN;
    ms->skirmish_ironman = true;         /* Hardcore Ironman ON by default (toggleable) */
    ms->skirmish_ai_aggressive = false;  /* Balanced Strategic AI pacing by default */
    ms->planet_radius_preset = 1;        /* 6,371 km Helios Prime */
    snprintf(ms->active_mod_id, sizeof(ms->active_mod_id), "none");
    snprintf(ms->status_note, sizeof(ms->status_note), "COMMAND A PLANETARY-TO-ORBITAL EMPIRE FROM THE BOOTS UP");
}

const char *rts_scenario_id(int idx) {
    if (idx < 0 || idx >= 4) return k_scenario_ids[0];
    return k_scenario_ids[idx];
}

const char *rts_scenario_title(int idx) {
    if (idx < 0 || idx >= 4) return k_scenario_titles[0];
    return k_scenario_titles[idx];
}

const char *rts_scenario_brief(int idx) {
    if (idx < 0 || idx >= 4) return k_scenario_briefs[0];
    return k_scenario_briefs[idx];
}

double rts_planet_preset_radius_m(int preset) {
    if (preset == 0) return 1200000.0;
    if (preset == 2) return 12000000.0;
    return 6371000.0;
}

const char *rts_menu_screen_title(RtsMenuScreen screen) {
    switch (screen) {
        case RTS_MENU_MAIN:           return "1STPRTS // SOLARIS COMMAND";
        case RTS_MENU_SKIRMISH_SETUP: return "PLANETARY-TO-ORBITAL SKIRMISH SETUP";
        case RTS_MENU_SCENARIOS:      return "CURATED FIRST-PERSON SCENARIOS";
        case RTS_MENU_SANDBOX_PICKER: return "FREE-EXPLORE SANDBOX (ALL 76 CONSTRUCTS)";
        case RTS_MENU_SAVE_LOAD:      return "PERSISTENT MULTI-SLOT SAVES (1-5)";
        case RTS_MENU_MODS:           return "MOD MANAGER & LIVE HOT-RELOAD";
        case RTS_MENU_PAUSE:          return "MISSION PAUSED [ESC TO RESUME]";
        case RTS_MENU_MATCH_END:      return "CAMPAIGN OPERATION COMPLETE";
        case RTS_MENU_NONE:
        default:                      return "";
    }
}

int rts_menu_item_count(const RtsMenuState *ms) {
    if (!ms) return 0;
    switch (ms->screen) {
        case RTS_MENU_MAIN:           return 6;
        case RTS_MENU_SKIRMISH_SETUP: return 5;
        case RTS_MENU_SCENARIOS:      return 5;
        case RTS_MENU_SANDBOX_PICKER: return 4;
        case RTS_MENU_SAVE_LOAD:      return RTS_SAVE_SLOT_COUNT + 1;
        case RTS_MENU_MODS:           return 3;
        case RTS_MENU_PAUSE:          return 5;
        case RTS_MENU_MATCH_END:      return 2;
        case RTS_MENU_NONE:
        default:                      return 0;
    }
}

void rts_menu_item_label(const RtsMenuState *ms, const RtsWorld *world, int idx, char *out_buf, size_t cap) {
    if (!ms || !out_buf || cap == 0u) return;
    out_buf[0] = '\0';
    switch (ms->screen) {
        case RTS_MENU_MAIN:
            switch (idx) {
                case 0: snprintf(out_buf, cap, "[1] PLANETARY-TO-ORBITAL SKIRMISH"); break;
                case 1: snprintf(out_buf, cap, "[2] FREE-EXPLORE SANDBOX (76 CONSTRUCTS)"); break;
                case 2: snprintf(out_buf, cap, "[3] TACTICAL DEFENSE (SOLARIS ARCADE)"); break;
                case 3: snprintf(out_buf, cap, "[4] 4 CURATED SCENARIOS"); break;
                case 4: snprintf(out_buf, cap, "[5] LOAD / SAVE CAMPAIGN SLOTS"); break;
                default: snprintf(out_buf, cap, "[6] MOD MANAGER & HOT-RELOAD"); break;
            }
            break;
        case RTS_MENU_SKIRMISH_SETUP:
            switch (idx) {
                case 0: snprintf(out_buf, cap, "PLANET RADIUS: %.0f KM (ENTER TO CYCLE)",
                                 rts_planet_preset_radius_m(ms->planet_radius_preset) / 1000.0); break;
                case 1: snprintf(out_buf, cap, "STRATEGIC AI: %s", ms->skirmish_ai_aggressive ? "AGGRESSIVE" : "BALANCED"); break;
                case 2: snprintf(out_buf, cap, "HARDCORE IRONMAN: %s", ms->skirmish_ironman ? "ON (ONE LIFE)" : "OFF (RESPAWN)"); break;
                case 3: snprintf(out_buf, cap, ">> LAUNCH PLANETARY-TO-ORBITAL SKIRMISH <<"); break;
                default: snprintf(out_buf, cap, "< BACK TO MAIN MENU"); break;
            }
            break;
        case RTS_MENU_SCENARIOS:
            if (idx >= 0 && idx < 4) {
                snprintf(out_buf, cap, "[%d] %s", idx + 1, k_scenario_titles[idx]);
            } else {
                snprintf(out_buf, cap, "< BACK TO MAIN MENU");
            }
            break;
        case RTS_MENU_SANDBOX_PICKER: {
            const char *bp_name = "SOLAR COMMAND CITADEL";
            if (world && world->blueprint_count > 0u) {
                size_t bi = (size_t)((ms->sandbox_bp_idx < 0 ? 0 : ms->sandbox_bp_idx) % (int)world->blueprint_count);
                bp_name = world->blueprints[bi].name;
            }
            switch (idx) {
                case 0: snprintf(out_buf, cap, "CONSTRUCT [%d/76]: %.34s", ms->sandbox_bp_idx + 1, bp_name); break;
                case 1: snprintf(out_buf, cap, "NEXT CONSTRUCT (+1) >>"); break;
                case 2: snprintf(out_buf, cap, ">> ENTER & EXPLORE CONSTRUCT INTERIOR <<"); break;
                default: snprintf(out_buf, cap, "< BACK TO MAIN MENU"); break;
            }
            break;
        }
        case RTS_MENU_SAVE_LOAD:
            if (idx >= 0 && idx < RTS_SAVE_SLOT_COUNT) {
                const RtsSaveSlotMeta *sm = &ms->slots[idx];
                if (sm->occupied) {
                    snprintf(out_buf, cap, "SLOT %d: %.24s (%.0FS | %zu UNITS)", idx + 1, sm->label, sm->sim_time_s, sm->entity_count);
                } else {
                    snprintf(out_buf, cap, "SLOT %d: [EMPTY SLOT - CLICK TO SAVE/LOAD]", idx + 1);
                }
            } else {
                snprintf(out_buf, cap, "< BACK");
            }
            break;
        case RTS_MENU_MODS:
            switch (idx) {
                case 0: snprintf(out_buf, cap, "ACTIVE MOD: %s (CLICK TO TOGGLE EXAMPLE_MOD)", ms->active_mod_id); break;
                case 1: snprintf(out_buf, cap, "TRIGGER LIVE VFS HOT-RELOAD"); break;
                default: snprintf(out_buf, cap, "< BACK"); break;
            }
            break;
        case RTS_MENU_PAUSE:
            switch (idx) {
                case 0: snprintf(out_buf, cap, "RESUME COMMAND [ESC]"); break;
                case 1: snprintf(out_buf, cap, "QUICK-SAVE TO SLOT 1"); break;
                case 2: snprintf(out_buf, cap, "OPEN SAVE / LOAD SLOTS (1-5)"); break;
                case 3: snprintf(out_buf, cap, "MOD MANAGER & HOT-RELOAD"); break;
                default: snprintf(out_buf, cap, "RETURN TO SOLARIS MAIN MENU"); break;
            }
            break;
        case RTS_MENU_MATCH_END:
            if (idx == 0) snprintf(out_buf, cap, "DEPLOY NEW SKIRMISH");
            else snprintf(out_buf, cap, "RETURN TO SOLARIS MAIN MENU");
            break;
        case RTS_MENU_NONE:
        default:
            break;
    }
}

void rts_menu_move_selection(RtsMenuState *ms, int delta) {
    if (!ms) return;
    int cnt = rts_menu_item_count(ms);
    if (cnt <= 0) return;
    ms->selected_row = (ms->selected_row + delta) % cnt;
    if (ms->selected_row < 0) ms->selected_row += cnt;
}

void rts_menu_back(RtsMenuState *ms) {
    if (!ms) return;
    if (ms->screen == RTS_MENU_PAUSE) {
        ms->screen = RTS_MENU_NONE;
    } else if (ms->screen == RTS_MENU_SAVE_LOAD || ms->screen == RTS_MENU_MODS) {
        ms->screen = ms->return_screen;
        ms->selected_row = 0;
    } else if (ms->screen != RTS_MENU_NONE && ms->screen != RTS_MENU_MAIN) {
        ms->screen = RTS_MENU_MAIN;
        ms->selected_row = 0;
    }
}

RtsMenuAction rts_menu_activate(RtsMenuState *ms) {
    if (!ms) return RTS_MENU_ACT_NONE;
    int row = ms->selected_row;
    switch (ms->screen) {
        case RTS_MENU_MAIN:
            if (row == 0) { ms->screen = RTS_MENU_SKIRMISH_SETUP; ms->selected_row = 3; return RTS_MENU_ACT_NONE; }
            if (row == 1) { ms->screen = RTS_MENU_SANDBOX_PICKER; ms->selected_row = 2; return RTS_MENU_ACT_NONE; }
            if (row == 2) { ms->screen = RTS_MENU_NONE; return RTS_MENU_ACT_START_TACTICAL; }
            if (row == 3) { ms->screen = RTS_MENU_SCENARIOS; ms->selected_row = 0; return RTS_MENU_ACT_NONE; }
            if (row == 4) { ms->return_screen = RTS_MENU_MAIN; ms->screen = RTS_MENU_SAVE_LOAD; ms->selected_row = 0; return RTS_MENU_ACT_NONE; }
            if (row == 5) { ms->return_screen = RTS_MENU_MAIN; ms->screen = RTS_MENU_MODS; ms->selected_row = 0; return RTS_MENU_ACT_NONE; }
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_SKIRMISH_SETUP:
            if (row == 0) { ms->planet_radius_preset = (ms->planet_radius_preset + 1) % 3; return RTS_MENU_ACT_NONE; }
            if (row == 1) { ms->skirmish_ai_aggressive = !ms->skirmish_ai_aggressive; return RTS_MENU_ACT_NONE; }
            if (row == 2) { ms->skirmish_ironman = !ms->skirmish_ironman; return RTS_MENU_ACT_NONE; }
            if (row == 3) { ms->screen = RTS_MENU_NONE; return RTS_MENU_ACT_START_SKIRMISH; }
            rts_menu_back(ms);
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_SCENARIOS:
            if (row >= 0 && row < 4) {
                ms->scenario_idx = row;
                ms->screen = RTS_MENU_NONE;
                return RTS_MENU_ACT_LAUNCH_SCENARIO;
            }
            rts_menu_back(ms);
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_SANDBOX_PICKER:
            if (row == 0 || row == 1) { ms->sandbox_bp_idx = (ms->sandbox_bp_idx + 1) % 76; return RTS_MENU_ACT_NONE; }
            if (row == 2) { ms->screen = RTS_MENU_NONE; return RTS_MENU_ACT_START_SANDBOX; }
            rts_menu_back(ms);
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_SAVE_LOAD:
            if (row >= 0 && row < RTS_SAVE_SLOT_COUNT) {
                ms->active_slot = row;
                if (ms->return_screen == RTS_MENU_PAUSE) return RTS_MENU_ACT_SAVE_SLOT;
                return RTS_MENU_ACT_LOAD_SLOT;
            }
            rts_menu_back(ms);
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_MODS:
            if (row == 0) return RTS_MENU_ACT_TOGGLE_MOD;
            if (row == 1) return RTS_MENU_ACT_HOT_RELOAD;
            rts_menu_back(ms);
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_PAUSE:
            if (row == 0) { ms->screen = RTS_MENU_NONE; return RTS_MENU_ACT_RESUME; }
            if (row == 1) { ms->active_slot = 0; return RTS_MENU_ACT_SAVE_SLOT; }
            if (row == 2) { ms->return_screen = RTS_MENU_PAUSE; ms->screen = RTS_MENU_SAVE_LOAD; ms->selected_row = 0; return RTS_MENU_ACT_NONE; }
            if (row == 3) { ms->return_screen = RTS_MENU_PAUSE; ms->screen = RTS_MENU_MODS; ms->selected_row = 0; return RTS_MENU_ACT_NONE; }
            ms->screen = RTS_MENU_MAIN;
            ms->selected_row = 0;
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_MATCH_END:
            if (row == 0) { ms->screen = RTS_MENU_NONE; return RTS_MENU_ACT_START_SKIRMISH; }
            ms->screen = RTS_MENU_MAIN;
            ms->selected_row = 0;
            return RTS_MENU_ACT_NONE;
        case RTS_MENU_NONE:
        default:
            return RTS_MENU_ACT_NONE;
    }
}

RtsMenuAction rts_menu_click(RtsMenuState *ms, float click_x, float click_y, float screen_w, float screen_h) {
    if (!ms || ms->screen == RTS_MENU_NONE) return RTS_MENU_ACT_NONE;
    int cnt = rts_menu_item_count(ms);
    float pw = 640.0f;
    float px = screen_w * 0.5f - pw * 0.5f;
    float py = screen_h * 0.5f - 210.0f;
    if (click_x < px + 20.0f || click_x > px + pw - 20.0f) return RTS_MENU_ACT_NONE;
    for (int i = 0; i < cnt; ++i) {
        float ry = py + 86.0f + (float)i * 48.0f;
        if (click_y >= ry && click_y <= ry + 40.0f) {
            ms->selected_row = i;
            return rts_menu_activate(ms);
        }
    }
    return RTS_MENU_ACT_NONE;
}

#ifndef __EMSCRIPTEN__
static void build_slot_path(const char *save_dir, int slot, char *out, size_t cap) {
    snprintf(out, cap, "%s/1prtsg_slot_%d.rts", (save_dir && save_dir[0]) ? save_dir : ".", slot + 1);
}
#endif

bool rts_menu_save_slot(RtsMenuState *ms, int slot, const char *save_dir, const RtsWorld *w, const RtsEconomySystem *eco) {
    if (!ms || !w || !eco || slot < 0 || slot >= RTS_SAVE_SLOT_COUNT) return false;
    char *buf = (char *)calloc(1, 32768u);
    if (!buf) return false;
    size_t len = rts_save_serialize(w, eco, buf, 32768u);
    if (len == 0u) { free(buf); return false; }

#ifdef __EMSCRIPTEN__
    rts_web_storage_save(slot + 1, buf);
    (void)save_dir;
    bool ok = true;
#else
    char path[512];
    build_slot_path(save_dir, slot, path, sizeof(path));
    bool ok = rts_save_to_file(path, w, eco);
#endif
    free(buf);
    if (ok) {
        RtsSaveSlotMeta *sm = &ms->slots[slot];
        sm->occupied = true;
        sm->sim_time_s = w->sim_time_s;
        sm->entity_count = w->entity_count;
        snprintf(sm->label, sizeof(sm->label), "%s", w->hardcore_ironman ? "IRONMAN SKIRMISH" : "SANDBOX CAMPAIGN");
        snprintf(sm->timestamp, sizeof(sm->timestamp), "SAVED");
        snprintf(ms->status_note, sizeof(ms->status_note), "SAVED CAMPAIGN TO SLOT %d (%zu ENTITIES)", slot + 1, w->entity_count);
    }
    return ok;
}

bool rts_menu_load_slot(RtsMenuState *ms, int slot, const char *save_dir, RtsWorld *w, RtsEconomySystem *eco) {
    if (!ms || !w || !eco || slot < 0 || slot >= RTS_SAVE_SLOT_COUNT) return false;
    bool ok = false;
#ifdef __EMSCRIPTEN__
    char *buf = (char *)calloc(1, 32768u);
    if (!buf) return false;
    int got = rts_web_storage_load(slot + 1, buf, 32768);
    if (got > 0) ok = rts_save_deserialize(buf, w, eco);
    free(buf);
    (void)save_dir;
#else
    char path[512];
    build_slot_path(save_dir, slot, path, sizeof(path));
    ok = rts_load_from_file(path, w, eco);
#endif
    if (ok) {
        ms->screen = RTS_MENU_NONE;
        snprintf(ms->status_note, sizeof(ms->status_note), "LOADED CAMPAIGN FROM SLOT %d (%.0FS)", slot + 1, w->sim_time_s);
    } else {
        snprintf(ms->status_note, sizeof(ms->status_note), "SLOT %d IS EMPTY", slot + 1);
    }
    return ok;
}

void rts_menu_refresh_slots(RtsMenuState *ms, const char *save_dir) {
    if (!ms) return;
    for (int i = 0; i < RTS_SAVE_SLOT_COUNT; ++i) {
#ifdef __EMSCRIPTEN__
        int exists = rts_web_storage_exists(i + 1);
        ms->slots[i].occupied = (exists != 0);
        if (exists && ms->slots[i].label[0] == '\0') {
            snprintf(ms->slots[i].label, sizeof(ms->slots[i].label), "WEB SAVED CAMPAIGN");
        }
        (void)save_dir;
#else
        char path[512];
        build_slot_path(save_dir, i, path, sizeof(path));
        FILE *fp = fopen(path, "rb");
        if (fp) {
            fclose(fp);
            ms->slots[i].occupied = true;
            if (ms->slots[i].label[0] == '\0') {
                snprintf(ms->slots[i].label, sizeof(ms->slots[i].label), "SAVED CAMPAIGN");
            }
        }
#endif
    }
}
