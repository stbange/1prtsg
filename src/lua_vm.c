/* SPDX-License-Identifier: MIT */
#include "rts/lua_vm.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *RTS_VM_REG_KEY = "rts_lua_vm_ptr";

static RtsLuaVm *get_vm(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, RTS_VM_REG_KEY);
    RtsLuaVm *vm = (RtsLuaVm *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return vm;
}

static void lua_cov_hook(lua_State *L, lua_Debug *ar) {
    if (ar->event != LUA_HOOKLINE || ar->currentline <= 0) {
        return;
    }
    RtsLuaVm *vm = get_vm(L);
    if (!vm || !vm->coverage_enabled) {
        return;
    }
    if (lua_getinfo(L, "S", ar) == 0 || !ar->source) {
        return;
    }
    const char *src = ar->source;
    if (src[0] == '@') {
        src++;
    }
    /* Only track project gameplay scripts (exclude test runner scripts themselves) */
    if (strncmp(src, "tests/", 6) == 0 || strncmp(src, "scripts/tests/", 14) == 0) {
        return;
    }
    RtsLuaCoverageFile *cf = NULL;
    for (size_t i = 0; i < vm->cov_file_count; ++i) {
        if (strcmp(vm->cov_files[i].name, src) == 0) {
            cf = &vm->cov_files[i];
            break;
        }
    }
    if (!cf && vm->cov_file_count < RTS_LUA_COV_MAX_FILES) {
        cf = &vm->cov_files[vm->cov_file_count++];
        memset(cf, 0, sizeof(*cf));
        strncpy(cf->name, src, sizeof(cf->name) - 1);
    }
    if (cf && ar->currentline < 2048) {
        uint32_t ln = (uint32_t)ar->currentline;
        cf->hit_mask[ln >> 5] |= (1u << (ln & 31u));
        if (ln > cf->max_line) {
            cf->max_line = ln;
        }
    }
}

/* Custom VFS-backed require searcher */
static int l_vfs_searcher(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    const char *modname = luaL_checkstring(L, 1);
    char path[256];
    size_t j = 0;
    for (size_t i = 0; modname[i] && j < sizeof(path) - 6; ++i) {
        path[j++] = (modname[i] == '.') ? '/' : modname[i];
    }
    memcpy(path + j, ".lua", 5);

    size_t len = 0;
    char *code = rts_vfs_read_text(vm->vfs, "scripts", path, &len);
    if (!code) {
        lua_pushfstring(L, "\n\tno VFS script '%s'", path);
        return 1;
    }
    char chunk_name[280];
    snprintf(chunk_name, sizeof(chunk_name), "@%s", path);
    int st = luaL_loadbuffer(L, code, len, chunk_name);
    rts_vfs_free(code);
    if (st != LUA_OK) {
        return lua_error(L);
    }
    return 1;
}

/* --- engine.vfs --- */
static int l_vfs_exists(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    const char *cat = luaL_checkstring(L, 1);
    const char *path = luaL_checkstring(L, 2);
    lua_pushboolean(L, rts_vfs_exists(vm->vfs, cat, path));
    return 1;
}

static int l_vfs_read_text(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    const char *cat = luaL_checkstring(L, 1);
    const char *path = luaL_checkstring(L, 2);
    size_t len = 0;
    char *txt = rts_vfs_read_text(vm->vfs, cat, path, &len);
    if (!txt) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, txt, len);
    rts_vfs_free(txt);
    return 1;
}

static int l_vfs_mount_mod(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    const char *mod = luaL_checkstring(L, 1);
    lua_pushboolean(L, rts_vfs_mount_mod(vm->vfs, mod));
    return 1;
}

/* --- engine.planet & engine.orbit --- */
static int l_planet_altitude(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    RtsVec3d p = rts_v3d(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    lua_pushnumber(L, rts_planet_altitude(&vm->world->planet, p));
    return 1;
}

static int l_planet_atmo_density(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    double alt = luaL_checknumber(L, 1);
    lua_pushnumber(L, rts_planet_atmo_density(&vm->world->planet, alt));
    return 1;
}

static int l_orbit_circular_speed(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    double alt = luaL_checknumber(L, 1);
    lua_pushnumber(L, rts_orbit_circular_speed(&vm->world->planet, alt));
    return 1;
}

static int l_orbit_hohmann_dv(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    double alt1 = luaL_checknumber(L, 1);
    double alt2 = luaL_checknumber(L, 2);
    RtsHohmannTransfer h = rts_orbit_hohmann(&vm->world->planet, alt1, alt2);
    lua_pushnumber(L, h.total_dv_mps);
    lua_pushnumber(L, h.transfer_time_s);
    return 2;
}

/* --- engine.world --- */
static int l_world_register_blueprint(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));

    lua_getfield(L, 1, "id");
    strncpy(bp.id, luaL_optstring(L, -1, ""), sizeof(bp.id) - 1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "name");
    strncpy(bp.name, luaL_optstring(L, -1, bp.id), sizeof(bp.name) - 1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "class_id");
    bp.construct_class = (RtsConstructClass)luaL_optinteger(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, 1, "length"); bp.dimensions_m.x = (float)luaL_optnumber(L, -1, 10.0); lua_pop(L, 1);
    lua_getfield(L, 1, "width");  bp.dimensions_m.y = (float)luaL_optnumber(L, -1, 10.0); lua_pop(L, 1);
    lua_getfield(L, 1, "height"); bp.dimensions_m.z = (float)luaL_optnumber(L, -1, 5.0);  lua_pop(L, 1);
    lua_getfield(L, 1, "hull");   bp.max_hull = (float)luaL_optnumber(L, -1, 200.0);      lua_pop(L, 1);
    lua_getfield(L, 1, "shield"); bp.max_shield = (float)luaL_optnumber(L, -1, 100.0);    lua_pop(L, 1);
    lua_getfield(L, 1, "speed");  bp.max_speed_mps = (float)luaL_optnumber(L, -1, 0.0);   lua_pop(L, 1);
    lua_getfield(L, 1, "crew");   bp.crew_capacity = (uint32_t)luaL_optinteger(L, -1, 4); lua_pop(L, 1);
    lua_getfield(L, 1, "rooms");  bp.room_count = (uint32_t)luaL_optinteger(L, -1, 2);    lua_pop(L, 1);
    lua_getfield(L, 1, "holo_globe"); bp.has_holo_globe = lua_toboolean(L, -1) != 0;      lua_pop(L, 1);
    lua_getfield(L, 1, "escape_pod"); bp.has_escape_pod = lua_toboolean(L, -1) != 0;      lua_pop(L, 1);
    lua_getfield(L, 1, "can_orbit");  bp.can_orbit = lua_toboolean(L, -1) != 0;           lua_pop(L, 1);
    lua_getfield(L, 1, "mesh");
    strncpy(bp.mesh_path, luaL_optstring(L, -1, "models/citadel.obj"), sizeof(bp.mesh_path) - 1);
    lua_pop(L, 1);

    lua_pushboolean(L, rts_world_register_blueprint(vm->world, &bp));
    return 1;
}

static int l_world_register_crew_role(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    RtsCrewRoleDef rd;
    memset(&rd, 0, sizeof(rd));
    lua_getfield(L, 1, "id"); strncpy(rd.id, luaL_optstring(L, -1, ""), sizeof(rd.id) - 1); lua_pop(L, 1);
    lua_getfield(L, 1, "name"); strncpy(rd.name, luaL_optstring(L, -1, ""), sizeof(rd.name) - 1); lua_pop(L, 1);
    lua_getfield(L, 1, "desc"); strncpy(rd.function_desc, luaL_optstring(L, -1, ""), sizeof(rd.function_desc) - 1); lua_pop(L, 1);
    lua_pushboolean(L, rts_world_register_crew_role(vm->world, &rd));
    return 1;
}

static int l_world_counts(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    lua_pushinteger(L, (lua_Integer)vm->world->blueprint_count);
    lua_pushinteger(L, (lua_Integer)vm->world->crew_role_count);
    lua_pushinteger(L, (lua_Integer)vm->world->entity_count);
    return 3;
}

static int l_world_spawn(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    const char *bp_id = luaL_checkstring(L, 1);
    const char *name = luaL_optstring(L, 2, NULL);
    uint8_t faction = (uint8_t)luaL_optinteger(L, 3, 1);
    double x = luaL_optnumber(L, 4, 0.0);
    double y = luaL_optnumber(L, 5, 0.0);
    double z = luaL_optnumber(L, 6, 0.0);
    bool interior = lua_toboolean(L, 7) != 0;
    uint32_t id = rts_world_spawn_from_blueprint(vm->world, bp_id, name, faction, vm->world->planet_frame_id, rts_v3d(x, y, z), interior);
    lua_pushinteger(L, (lua_Integer)id);
    return 1;
}

static int l_world_get_entity(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    const RtsEntity *e = rts_world_get_entity_const(vm->world, id);
    if (!e) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushinteger(L, (lua_Integer)e->id); lua_setfield(L, -2, "id");
    lua_pushstring(L, e->blueprint_id); lua_setfield(L, -2, "blueprint_id");
    lua_pushstring(L, e->name); lua_setfield(L, -2, "name");
    lua_pushnumber(L, e->pos.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, e->pos.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, e->pos.z); lua_setfield(L, -2, "z");
    lua_pushnumber(L, e->hull); lua_setfield(L, -2, "hull");
    lua_pushnumber(L, e->shield); lua_setfield(L, -2, "shield");
    lua_pushnumber(L, e->subsystem_hp[RTS_SUBSYS_ENGINES]); lua_setfield(L, -2, "engine_hp");
    lua_pushnumber(L, rts_world_effective_speed(e)); lua_setfield(L, -2, "effective_speed");
    lua_pushinteger(L, (lua_Integer)e->own_frame_id); lua_setfield(L, -2, "own_frame_id");
    lua_pushboolean(L, e->orbital_mode); lua_setfield(L, -2, "orbital_mode");
    return 1;
}

static int l_world_set_orbital(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    double alt = luaL_checknumber(L, 2);
    double inc = luaL_optnumber(L, 3, 0.25);
    double phase = luaL_optnumber(L, 4, 0.0);
    RtsEntity *e = rts_world_get_entity(vm->world, id);
    if (!e) {
        lua_pushboolean(L, 0);
        return 1;
    }
    RtsOrbitState st = rts_orbit_circular_state(&vm->world->planet, alt, inc, phase);
    e->pos = st.pos_m;
    e->vel = st.vel_mps;
    e->orbital_mode = true;
    if (e->own_frame_id != RTS_FRAME_INVALID) {
        RtsSpatialFrame *sf = rts_interior_get_frame(&vm->world->interiors, e->own_frame_id);
        if (sf) {
            sf->local_pos = e->pos;
            sf->local_vel = e->vel;
        }
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int l_world_issue_order(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    RtsOrderType ord = (RtsOrderType)luaL_checkinteger(L, 2);
    double x = luaL_optnumber(L, 3, 0.0);
    double y = luaL_optnumber(L, 4, 0.0);
    double z = luaL_optnumber(L, 5, 0.0);
    uint32_t tgt = (uint32_t)luaL_optinteger(L, 6, 0);
    double alt = luaL_optnumber(L, 7, 0.0);
    lua_pushboolean(L, rts_world_issue_order(vm->world, id, ord, rts_v3d(x, y, z), tgt, alt));
    return 1;
}

static int l_world_apply_damage(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    float dmg = (float)luaL_checknumber(L, 2);
    RtsSubsystemKind sub = (RtsSubsystemKind)luaL_optinteger(L, 3, RTS_SUBSYS_ENGINES);
    rts_world_apply_damage(vm->world, id, dmg, sub);
    return 0;
}

static int l_world_step(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    double dt = luaL_checknumber(L, 1);
    int steps = (int)luaL_optinteger(L, 2, 1);
    for (int i = 0; i < steps; ++i) {
        rts_world_step(vm->world, dt);
    }
    return 0;
}

static int l_world_commander_state(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    const RtsCommanderState *c = &vm->world->commander;
    lua_newtable(L);
    lua_pushinteger(L, (lua_Integer)c->frame_id); lua_setfield(L, -2, "frame_id");
    lua_pushinteger(L, (lua_Integer)c->occupied_entity_id); lua_setfield(L, -2, "occupied_entity_id");
    lua_pushboolean(L, c->alive); lua_setfield(L, -2, "alive");
    lua_pushboolean(L, c->in_escape_pod); lua_setfield(L, -2, "in_escape_pod");
    lua_pushnumber(L, c->health); lua_setfield(L, -2, "health");
    lua_pushinteger(L, (lua_Integer)vm->world->match_state); lua_setfield(L, -2, "match_state");
    return 1;
}

static int l_world_commander_board(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t ent_id = (uint32_t)luaL_checkinteger(L, 1);
    RtsEntity *e = rts_world_get_entity(vm->world, ent_id);
    if (!e || e->own_frame_id == RTS_FRAME_INVALID) {
        lua_pushboolean(L, 0);
        return 1;
    }
    vm->world->commander.frame_id = e->own_frame_id;
    vm->world->commander.occupied_entity_id = ent_id;
    vm->world->commander.local_pos = rts_v3d(0.0, 1.7, 0.0);
    vm->world->commander.in_escape_pod = false;
    lua_pushboolean(L, 1);
    return 1;
}

static int l_world_commander_escape_pod(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    lua_pushboolean(L, rts_world_commander_launch_escape_pod(vm->world));
    return 1;
}

static int l_world_eval_crew_lod(lua_State *L) {
    double dist = luaL_checknumber(L, 1);
    bool interior = lua_toboolean(L, 2) != 0;
    bool webxr = lua_toboolean(L, 3) != 0;
    double az = luaL_optnumber(L, 4, 0.0);
    int dir_idx = 0;
    RtsCrewLodMode mode = rts_world_eval_crew_lod(dist, interior, webxr, 45.0, az, 8, &dir_idx);
    lua_pushinteger(L, (lua_Integer)mode);
    lua_pushinteger(L, (lua_Integer)dir_idx);
    return 2;
}

/* --- engine.interior --- */
static int l_interior_add_portal_and_transfer(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    RtsFrameId f_from = (RtsFrameId)luaL_checkinteger(L, 1);
    RtsFrameId f_to = (RtsFrameId)luaL_checkinteger(L, 2);
    RtsAabb box = {rts_v3d(-2.0, 0.0, -2.0), rts_v3d(2.0, 4.0, 2.0)};
    uint32_t pid = rts_interior_add_portal(&vm->world->interiors, f_from, f_to, "Airlock", box, rts_v3d(0.0, 1.7, 0.0), true);
    bool ok = false;
    if (pid != 0u) {
        vm->world->commander.frame_id = f_from;
        vm->world->commander.local_pos = rts_v3d(0.0, 1.0, 0.0);
        ok = rts_interior_try_portal_transfer(&vm->world->interiors, &vm->world->commander.frame_id, &vm->world->commander.local_pos, &vm->world->commander.local_vel);
    }
    lua_pushboolean(L, ok);
    return 1;
}

/* --- engine.input --- */
static int l_input_simulate_touch(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->input) {
        lua_pushboolean(L, 0);
        return 1;
    }
    rts_input_begin_frame(vm->input);
    rts_input_on_touch_down(vm->input, 1, 0.14f, 0.80f);
    rts_input_on_touch_move(vm->input, 1, 0.22f, 0.72f);
    rts_input_on_pinch(vm->input, 0.25f, true);
    rts_input_on_pinch(vm->input, 0.45f, true);
    lua_pushinteger(L, (lua_Integer)vm->input->mode);
    lua_pushnumber(L, vm->input->intent.move_x);
    lua_pushnumber(L, vm->input->intent.move_y);
    lua_pushnumber(L, vm->input->intent.holo_zoom_delta);
    return 4;
}

static int l_input_simulate_webxr(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->input) {
        lua_pushboolean(L, 0);
        return 1;
    }
    RtsXrFrameState xr;
    memset(&xr, 0, sizeof(xr));
    xr.session_active = true;
    xr.eye_count = 2;
    xr.right_aim_ray.origin = rts_v3d(0.0, 1.5, 2.0);
    xr.right_aim_ray.dir = rts_v3d(0.0, -0.2, -1.0);
    xr.right_trigger_pressed = true;
    rts_input_update_xr(vm->input, &xr);
    double hit_t = 0.0;
    bool hit = rts_ray_intersect_sphere(vm->input->intent.aim_ray, rts_v3d(0.0, 1.1, 0.0), 0.7, &hit_t);
    lua_pushinteger(L, (lua_Integer)vm->input->mode);
    lua_pushboolean(L, hit);
    lua_pushboolean(L, vm->input->intent.holo_select_pressed);
    return 3;
}

/* --- engine.voxel --- */
static int l_voxel_test_mesh(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->voxel) {
        lua_pushinteger(L, 0);
        return 1;
    }
    rts_voxel_init(vm->voxel, 1.0f);
    rts_voxel_set(vm->voxel, 2, 2, 2, 1);
    rts_voxel_set(vm->voxel, 3, 2, 2, 2);
    RtsVoxelVertex verts[128];
    size_t n = rts_voxel_build_mesh(vm->voxel, verts, 128);
    lua_pushinteger(L, (lua_Integer)n);
    return 1;
}

/* --- engine.audio --- */
static int l_audio_play_3d(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->audio) {
        lua_pushnumber(L, 0.0);
        lua_pushboolean(L, 0);
        return 2;
    }
    const char *wav = luaL_checkstring(L, 1);
    double x = luaL_checknumber(L, 2);
    double y = luaL_checknumber(L, 3);
    double z = luaL_checknumber(L, 4);
    RtsAudioChannel ch = (RtsAudioChannel)luaL_optinteger(L, 5, RTS_AUD_EXTERIOR_AIRBORNE);
    float gain = (float)luaL_optnumber(L, 6, 1.0);
    float max_d = (float)luaL_optnumber(L, 7, 300.0);
    RtsAudioVoice v = rts_audio_play_3d(vm->audio, &vm->world->planet, wav, rts_v3d(x, y, z), ch, gain, max_d);
    lua_pushnumber(L, v.gain);
    lua_pushboolean(L, v.audible);
    return 2;
}

static int l_audio_set_listener(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->audio) return 0;
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    double z = luaL_checknumber(L, 3);
    double alt = luaL_optnumber(L, 4, 0.0);
    float muffle = (float)luaL_optnumber(L, 5, 0.0);
    rts_audio_set_listener(vm->audio, rts_v3d(x, y, z), rts_v3d(1.0, 0.0, 0.0), alt, muffle);
    return 0;
}

/* --- engine.economy --- */
static int l_eco_queue_build(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->economy) {
        lua_pushinteger(L, 0);
        return 1;
    }
    uint32_t prod = (uint32_t)luaL_checkinteger(L, 1);
    const char *bp = luaL_checkstring(L, 2);
    uint32_t jid = rts_economy_queue_build(vm->economy, vm->world, prod, bp);
    lua_pushinteger(L, (lua_Integer)jid);
    return 1;
}

static int l_eco_step(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (!vm->economy) {
        lua_pushinteger(L, 0);
        return 1;
    }
    double dt = luaL_checknumber(L, 1);
    int steps = (int)luaL_optinteger(L, 2, 1);
    size_t done = 0;
    for (int i = 0; i < steps; ++i) {
        done += rts_economy_step(vm->economy, vm->world, dt);
    }
    lua_pushinteger(L, (lua_Integer)done);
    return 1;
}

static int l_eco_job_spawned(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t jid = (uint32_t)luaL_checkinteger(L, 1);
    const RtsProductionJob *j = vm->economy ? rts_economy_get_job(vm->economy, jid) : NULL;
    lua_pushinteger(L, j ? (lua_Integer)j->spawned_entity_id : 0);
    return 1;
}

static int l_eco_ledger(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint8_t fid = (uint8_t)luaL_optinteger(L, 1, 1);
    if (!vm->economy || fid >= RTS_MAX_FACTIONS) {
        lua_pushnil(L);
        return 1;
    }
    const RtsFactionLedger *led = &vm->economy->ledgers[fid];
    lua_newtable(L);
    lua_pushnumber(L, led->ore); lua_setfield(L, -2, "ore");
    lua_pushnumber(L, led->fuel); lua_setfield(L, -2, "fuel");
    lua_pushnumber(L, led->power_produced); lua_setfield(L, -2, "power");
    return 1;
}

/* --- engine.sensors --- */
static int l_sensors_is_detected(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint8_t fac = (uint8_t)luaL_checkinteger(L, 1);
    uint32_t tgt = (uint32_t)luaL_checkinteger(L, 2);
    lua_pushboolean(L, rts_sensors_is_target_detected(vm->world, fac, tgt));
    return 1;
}

static int l_sensors_is_comms_linked(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t hq = (uint32_t)luaL_checkinteger(L, 1);
    uint32_t unit = (uint32_t)luaL_checkinteger(L, 2);
    lua_pushboolean(L, rts_comms_is_entity_linked(vm->world, hq, unit));
    return 1;
}

/* --- engine.control --- */
static int l_ctrl_engage_pilot(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t cid = (uint32_t)luaL_checkinteger(L, 1);
    lua_pushboolean(L, vm->controller ? rts_controller_engage_piloting(vm->controller, vm->world, cid) : 0);
    return 1;
}

static int l_ctrl_release_pilot(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    if (vm->controller) rts_controller_release_piloting(vm->controller);
    return 0;
}

static int l_ctrl_astrogator_ascent(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t cid = (uint32_t)luaL_checkinteger(L, 1);
    double alt = luaL_checknumber(L, 2);
    bool ok = vm->controller ? rts_astrogator_start_ascent(vm->controller, vm->world, cid, alt) : false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_ctrl_astrogator_reentry(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    uint32_t cid = (uint32_t)luaL_checkinteger(L, 1);
    bool ok = vm->controller ? rts_astrogator_start_reentry(vm->controller, vm->world, cid) : false;
    lua_pushboolean(L, ok);
    return 1;
}

static int l_ctrl_astrogator_step(lua_State *L) {
    RtsLuaVm *vm = get_vm(L);
    double dt = luaL_checknumber(L, 1);
    int steps = (int)luaL_optinteger(L, 2, 1);
    RtsAutopilotPhase ph = RTS_AUTO_IDLE;
    if (vm->controller) {
        for (int i = 0; i < steps; ++i) {
            ph = rts_astrogator_step(vm->controller, vm->world, dt);
        }
    }
    lua_pushinteger(L, (lua_Integer)ph);
    lua_pushnumber(L, vm->controller ? vm->controller->astrogator.peak_reentry_heat : 0.0);
    return 2;
}

static void register_subtable(lua_State *L, const char *name, const luaL_Reg *funcs) {
    lua_newtable(L);
    luaL_setfuncs(L, funcs, 0);
    lua_setfield(L, -2, name);
}

bool rts_lua_init(RtsLuaVm *vm, RtsVfs *vfs, RtsWorld *world, RtsInputRouter *input, RtsVoxelChunk *voxel, RtsRetroRenderer *renderer) {
    if (!vm || !vfs || !world) {
        return false;
    }
    memset(vm, 0, sizeof(*vm));
    vm->vfs = vfs;
    vm->world = world;
    vm->input = input;
    vm->voxel = voxel;
    vm->renderer = renderer;

    lua_State *L = luaL_newstate();
    if (!L) {
        return false;
    }
    vm->L = L;
    luaL_openlibs(L);

    /* Sandbox: remove unsafe OS/IO/dynamic-library globals */
    lua_pushnil(L); lua_setglobal(L, "io");
    lua_pushnil(L); lua_setglobal(L, "dofile");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        lua_pushnil(L); lua_setfield(L, -2, "execute");
        lua_pushnil(L); lua_setfield(L, -2, "remove");
        lua_pushnil(L); lua_setfield(L, -2, "rename");
        lua_pushnil(L); lua_setfield(L, -2, "exit");
    }
    lua_pop(L, 1);

    /* Replace package.searchers with VFS-only searcher & strip loadlib */
    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_pushnil(L); lua_setfield(L, -2, "loadlib");
        lua_newtable(L);
        lua_pushcfunction(L, l_vfs_searcher);
        lua_rawseti(L, -2, 1);
        lua_setfield(L, -2, "searchers");
    }
    lua_pop(L, 1);

    /* Store VM pointer in registry */
    lua_pushlightuserdata(L, vm);
    lua_setfield(L, LUA_REGISTRYINDEX, RTS_VM_REG_KEY);

    /* Expose engine.* API */
    lua_newtable(L);
    static const luaL_Reg vfs_funcs[] = {
        {"exists", l_vfs_exists},
        {"read_text", l_vfs_read_text},
        {"mount_mod", l_vfs_mount_mod},
        {NULL, NULL}
    };
    static const luaL_Reg planet_funcs[] = {
        {"altitude", l_planet_altitude},
        {"atmo_density", l_planet_atmo_density},
        {NULL, NULL}
    };
    static const luaL_Reg orbit_funcs[] = {
        {"circular_speed", l_orbit_circular_speed},
        {"hohmann_dv", l_orbit_hohmann_dv},
        {NULL, NULL}
    };
    static const luaL_Reg world_funcs[] = {
        {"register_blueprint", l_world_register_blueprint},
        {"register_crew_role", l_world_register_crew_role},
        {"counts", l_world_counts},
        {"spawn", l_world_spawn},
        {"get_entity", l_world_get_entity},
        {"set_orbital", l_world_set_orbital},
        {"issue_order", l_world_issue_order},
        {"apply_damage", l_world_apply_damage},
        {"step", l_world_step},
        {"commander_state", l_world_commander_state},
        {"commander_board", l_world_commander_board},
        {"commander_escape_pod", l_world_commander_escape_pod},
        {"eval_crew_lod", l_world_eval_crew_lod},
        {NULL, NULL}
    };
    static const luaL_Reg interior_funcs[] = {
        {"add_portal_and_transfer", l_interior_add_portal_and_transfer},
        {NULL, NULL}
    };
    static const luaL_Reg input_funcs[] = {
        {"simulate_touch", l_input_simulate_touch},
        {"simulate_webxr", l_input_simulate_webxr},
        {NULL, NULL}
    };
    static const luaL_Reg voxel_funcs[] = {
        {"test_mesh", l_voxel_test_mesh},
        {NULL, NULL}
    };
    static const luaL_Reg audio_funcs[] = {
        {"play_3d", l_audio_play_3d},
        {"set_listener", l_audio_set_listener},
        {NULL, NULL}
    };
    static const luaL_Reg eco_funcs[] = {
        {"queue_build", l_eco_queue_build},
        {"step", l_eco_step},
        {"job_spawned", l_eco_job_spawned},
        {"ledger", l_eco_ledger},
        {NULL, NULL}
    };
    static const luaL_Reg sensor_funcs[] = {
        {"is_detected", l_sensors_is_detected},
        {"is_comms_linked", l_sensors_is_comms_linked},
        {NULL, NULL}
    };
    static const luaL_Reg ctrl_funcs[] = {
        {"engage_pilot", l_ctrl_engage_pilot},
        {"release_pilot", l_ctrl_release_pilot},
        {"astrogator_ascent", l_ctrl_astrogator_ascent},
        {"astrogator_reentry", l_ctrl_astrogator_reentry},
        {"astrogator_step", l_ctrl_astrogator_step},
        {NULL, NULL}
    };

    register_subtable(L, "vfs", vfs_funcs);
    register_subtable(L, "planet", planet_funcs);
    register_subtable(L, "orbit", orbit_funcs);
    register_subtable(L, "world", world_funcs);
    register_subtable(L, "interior", interior_funcs);
    register_subtable(L, "input", input_funcs);
    register_subtable(L, "voxel", voxel_funcs);
    register_subtable(L, "audio", audio_funcs);
    register_subtable(L, "economy", eco_funcs);
    register_subtable(L, "sensors", sensor_funcs);
    register_subtable(L, "control", ctrl_funcs);
    lua_setglobal(L, "engine");
    return true;
}

void rts_lua_attach_phase2(RtsLuaVm *vm, RtsAudioEngine *audio, RtsEconomySystem *economy, RtsCommanderController *controller) {
    if (!vm) return;
    vm->audio = audio;
    vm->economy = economy;
    vm->controller = controller;
}

void rts_lua_shutdown(RtsLuaVm *vm) {
    if (!vm) return;
    if (vm->L) {
        lua_close(vm->L);
        vm->L = NULL;
    }
}

void rts_lua_enable_coverage(RtsLuaVm *vm, bool enable) {
    if (!vm || !vm->L) return;
    vm->coverage_enabled = enable;
    if (enable) {
        lua_sethook(vm->L, lua_cov_hook, LUA_MASKLINE, 0);
    } else {
        lua_sethook(vm->L, NULL, 0, 0);
    }
}

bool rts_lua_exec_string(RtsLuaVm *vm, const char *code, const char *chunk_name) {
    if (!vm || !vm->L || !code) return false;
    int st = luaL_loadbuffer(vm->L, code, strlen(code), chunk_name ? chunk_name : "=(string)");
    if (st == LUA_OK) {
        st = lua_pcall(vm->L, 0, 0, 0);
    }
    if (st != LUA_OK) {
        const char *msg = lua_tostring(vm->L, -1);
        snprintf(vm->last_error, sizeof(vm->last_error), "%s", msg ? msg : "unknown lua error");
        lua_pop(vm->L, 1);
        return false;
    }
    return true;
}

bool rts_lua_exec_vfs_script(RtsLuaVm *vm, const char *script_path) {
    if (!vm || !vm->L || !script_path) return false;
    size_t len = 0;
    char *code = rts_vfs_read_text(vm->vfs, "scripts", script_path, &len);
    if (!code) {
        snprintf(vm->last_error, sizeof(vm->last_error), "VFS script not found: %s", script_path);
        return false;
    }
    char cname[280];
    snprintf(cname, sizeof(cname), "@%s", script_path);
    bool ok = rts_lua_exec_string(vm, code, cname);
    rts_vfs_free(code);
    return ok;
}

static bool is_lua_executable_line(const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    if (*p == '\0' || *p == '\n') return false;
    if (p[0] == '-' && p[1] == '-') return false;
    if (strcmp(p, "end") == 0 || strcmp(p, "else") == 0 ||
        strcmp(p, "}") == 0 || strcmp(p, "},") == 0 ||
        strcmp(p, "})") == 0 || strcmp(p, "return {") == 0) {
        return false;
    }
    /* Pure table-literal / function-header structural lines aren't hit by Lua 5.4 LINE hook */
    if (strncmp(p, "function ", 9) == 0 || strncmp(p, "local function ", 15) == 0) {
        return false;
    }
    if (p[0] == '{' && strstr(p, "id") != NULL) {
        return false;
    }
    return true;
}

double rts_lua_report_coverage(const RtsLuaVm *vm, double min_percent, bool verbose) {
    if (!vm || vm->cov_file_count == 0) {
        return 0.0;
    }
    uint32_t total_exec = 0;
    uint32_t total_hit = 0;
    if (verbose) {
        printf("\n=== Lua 5.4 Script Layer Line Coverage Report ===\n");
    }
    for (size_t i = 0; i < vm->cov_file_count; ++i) {
        const RtsLuaCoverageFile *cf = &vm->cov_files[i];
        size_t len = 0;
        char *txt = rts_vfs_read_text(vm->vfs, "scripts", cf->name, &len);
        uint32_t f_exec = 0;
        uint32_t f_hit = 0;
        if (txt) {
            uint32_t ln = 1;
            const char *p = txt;
            char buf[512];
            while (*p != '\0') {
                size_t k = 0;
                while (p[k] && p[k] != '\n' && k < sizeof(buf) - 1) {
                    buf[k] = p[k];
                    ++k;
                }
                buf[k] = '\0';
                p += k;
                if (*p == '\n') ++p;
                bool hit = (ln < 2048) && ((cf->hit_mask[ln >> 5] & (1u << (ln & 31u))) != 0u);
                if (hit) {
                    f_exec++;
                    f_hit++;
                } else if (is_lua_executable_line(buf)) {
                    f_exec++;
                }
                ++ln;
            }
            rts_vfs_free(txt);
        }
        double pct = f_exec > 0 ? (100.0 * (double)f_hit / (double)f_exec) : 100.0;
        if (verbose) {
            printf("  %-34s %4u / %4u (%6.2f%%)\n", cf->name, f_hit, f_exec, pct);
        }
        total_exec += f_exec;
        total_hit += f_hit;
    }
    double overall = total_exec > 0 ? (100.0 * (double)total_hit / (double)total_exec) : 100.0;
    if (verbose) {
        printf("  %-34s %4u / %4u (%6.2f%%) [min %.2f%%]\n\n", "TOTAL", total_hit, total_exec, overall, min_percent);
    }
    return overall;
}
