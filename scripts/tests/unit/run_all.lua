-- SPDX-License-Identifier: MIT
-- Lua 5.4 Unit Test Suite (runs under --lua-coverage enforcing >= 95% Lua script line coverage)

local function check(cond, msg)
    if not cond then
        error("LUA UNIT TEST FAIL: " .. msg)
    end
end

-- 1. Sandbox security check
check(io == nil, "io library must be sandboxed out")
check(dofile == nil, "dofile must be sandboxed out")
check(loadfile == nil, "loadfile must be sandboxed out")
check(os.execute == nil, "os.execute must be sandboxed out")
check(package.loadlib == nil, "package.loadlib must be sandboxed out")

-- 2. Main bootstrap + doubled 76-construct / 12-role registry
require("main")
local bp_count, role_count, ent_count = engine.world.counts()
check(bp_count == 76, "Expected 76 registered blueprints, got " .. tostring(bp_count))
check(role_count == 12, "Expected 12 registered crew roles, got " .. tostring(role_count))
check(ent_count >= 6, "Expected vertical slice entities spawned")

-- 2b. Verify ALL 76 constructs have unique sculpted .obj models AND tailored enterable interiors
local registry = require("units.registry")
local interiors = require("interiors.catalog")
check(interiors.count() == 76, "Expected 76 tailored interior specs, got " .. tostring(interiors.count()))
local seen_meshes = {}
for i = 1, #registry.all do
    local bp = registry.all[i]
    local expected_mesh = "models/" .. bp.id .. ".obj"
    check(bp.mesh == expected_mesh, "Construct " .. bp.id .. " must reference unique mesh " .. expected_mesh)
    check(not seen_meshes[bp.mesh], "Duplicate mesh path: " .. tostring(bp.mesh))
    seen_meshes[bp.mesh] = true
    local spec = interiors.get_spec(bp.id)
    check(spec ~= nil and spec.decks >= 1, "Missing interior spec for " .. bp.id)
    local layout = interiors.build_layout(bp)
    check(layout ~= nil and #layout.rooms >= 1 and #layout.consoles >= 1, "Invalid interior layout for " .. bp.id)
    if bp.holo_globe then
        local has_hg = false
        for k = 1, #layout.consoles do
            if layout.consoles[k].ctype == interiors.CONSOLE_HOLO_GLOBE then has_hg = true end
        end
        check(has_hg, "Holo-globe construct " .. bp.id .. " missing HOLO_GLOBE console")
    end
    if bp.escape_pod then
        local has_ep = false
        for k = 1, #layout.consoles do
            if layout.consoles[k].ctype == interiors.CONSOLE_ESCAPE_POD then has_ep = true end
        end
        check(has_ep, "Escape-pod construct " .. bp.id .. " missing ESCAPE_POD console")
    end
end
check(interiors.build_layout({ id = "nonexistent_bp" }) == nil, "Unknown blueprint should return nil layout")
local r0, c0, p0 = interiors.hydrate_entity(99999, registry.all[1])
check(r0 == 0 and c0 == 0 and p0 == 0, "Invalid entity hydration should return 0,0,0")
local r_bad, c_bad, p_bad = interiors.hydrate_entity(ACTIVE_MISSION.citadel, { id = "nonexistent_bp" })
check(r_bad == 0 and c_bad == 0 and p_bad == 0, "Unknown spec hydration should return 0,0,0")
local rm_cnt, pt_cnt, cn_cnt = engine.interior.counts()
check(rm_cnt >= 20 and pt_cnt >= 6 and cn_cnt >= 10, "Live vertical slice interiors must be hydrated in C engine")

-- 3. Factions unit test
local factions = require("factions.factions")
check(factions.get_by_id(1).code == "SOLAR", "Faction 1 should be SOLAR")
check(factions.get_by_id(2).code == "CRIMSON", "Faction 2 should be CRIMSON")
check(factions.get_by_id(99) == nil, "Unknown faction should return nil")

-- 4. AI & Orbital astrogator unit test
local ai = require("ai.commander_ai")
local plan = ai.plan_hohmann_transfer(400000, 35786000)
check(plan.total_dv > 3000 and plan.transfer_time > 10000, "Hohmann LEO->GEO transfer must be physically valid")
check(ai.order_squad_move(ACTIVE_MISSION.tank, 120, 6371000, -15), "Move order should succeed")
check(ai.order_engage_target(ACTIVE_MISSION.tank, ACTIVE_MISSION.enemy_tank), "Attack order should succeed")

-- 5. Phase 2 Combined-Arms, Economy, Sensors, Astrogator, Skirmish AI & Diegetic UI unit tests
local combined = require("missions.combined_arms")
local ca = combined.setup()
local eco = require("rules.economy")
local sensors = require("rules.sensors")
local astro = require("ai.astrogator")
local skirmish = require("ai.skirmish_ai")
local ui = require("ui.diegetic_ui")

local jid = eco.queue_construct(ca.foundry, "veh_plasma_tank")
check(jid > 0, "Foundry production queue should return valid job id")
check(eco.advance_production(1.0, 20) >= 1, "Production should complete within 20s")
check(eco.get_spawned_unit(jid) > 0, "Completed production job should spawn live construct")
check(eco.get_resources(1).ore > 0, "Solar Directorate ledger should remain valid")

check(sensors.can_detect(1, ca.far_enemy), "Orbital Recon Sat LEO-1 should reveal far-side enemy")
check(sensors.has_command_link(ACTIVE_MISSION.citadel, ca.far_squad), "GEO Relay Sat should bridge far-side comms")
local rg, ra = sensors.trigger_radio_alert("audio/alert_klaxon.wav")
check(ra and rg > 0.5, "Radio comms alert should play clearly")

local sk = skirmish.tick_faction(1, ca.foundry, ca.carrier, ca.far_enemy)
check(sk.queued_job > 0 and sk.attack_issued, "Skirmish AI should queue production and engage spotted enemy")

check(ui.format_page_header(1, true) == "TACTICAL_GLOBE [LINKED]", "Wrist-Pad header linked format")
check(ui.format_page_header(99, false) == "TACTICAL_GLOBE [RELAY_LOST]", "Wrist-Pad fallback offline format")

local bad_asc, _ = astro.launch_to_orbit(ACTIVE_MISSION.tank, 400000)
check(not bad_asc, "Non-orbital tank must fail orbital ascent")
local ok_asc, _ = astro.launch_to_orbit(ACTIVE_MISSION.dropship, 400000)
check(ok_asc, "Dropship should reach circular 400km LEO via Astrogator autopilot")
local bad_re, _ = astro.execute_shielded_reentry(99999)
check(not bad_re, "Invalid entity id must fail re-entry")
local ok_re, peak_heat = astro.execute_shielded_reentry(ACTIVE_MISSION.dropship)
check(ok_re and peak_heat > 10.0, "Dropship shielded re-entry should experience heating and land")

-- 6. Phase 3 Multi-Deck Capital Ship Boarding & Critical Reactor Meltdown unit tests
local cap_mission = require("missions.capital_assault")
local cap = cap_mission.setup()
local boarding = require("rules.boarding")

local op_id = boarding.launch_boarding_assault(cap.enemy_dread, 1, 180, true, true)
check(op_id > 0, "Boarding assault onto Crimson Dreadnought should launch")
local op_res = boarding.resolve_boarding(op_id, 30)
check(op_res ~= nil and op_res.status == boarding.STATUS_CAPTURED, "Solar Marines should capture Dreadnought Bridge intact")
check(engine.world.get_entity(cap.enemy_dread).faction_id == 1, "Captured Dreadnought must flip to faction 1")

check(boarding.initiate_reactor_meltdown(cap.enemy_bc, 5.0), "Reactor meltdown countdown should start")
check(boarding.meltdown_status(cap.enemy_bc).active, "Meltdown should report active before detonation")
boarding.advance_meltdowns(1.0, 6)
check(boarding.meltdown_status(cap.enemy_bc).detonated, "Meltdown should detonate after countdown expires")
check(engine.world.get_entity(cap.enemy_bc) == nil, "Detonated Battlecruiser must be destroyed")

-- 7. Phase 4 Session Management (External Config, VFS Hot-Reload Watcher, Save/Load Snapshots)
local session = require("rules.session")
local cfg = session.load_engine_config("engine.cfg")
check(cfg ~= nil and cfg.window_width == 1280 and cfg.hardcore_ironman, "External engine.cfg must load cleanly")
check(session.watch_script("main.lua"), "VFS hot-reload watch should register main.lua")
check(session.poll_changes() == 0, "Unmodified watched scripts should report 0 changes")
local snap = session.save_world_snapshot()
check(snap ~= nil and string.sub(snap, 1, 11) == "RTS_SAVE_V1", "Snapshot must emit RTS_SAVE_V1 header")
check(session.restore_world_snapshot(snap), "Restoring valid RTS_SAVE_V1 snapshot must succeed")

-- 8. Combat, shield, EMP-Prime synergy & Wave Director unit test
local combat = require("rules.combat")
local enemy = combat.plasma_salvo(ACTIVE_MISSION.enemy_tank, 100, combat.SUBSYS_ENGINES)
check(combat.is_shield_online(enemy), "Enemy shield should remain online after light hit")
local e_emp, emp_mult = combat.emp_prime_salvo(ACTIVE_MISSION.enemy_tank, 180, combat.SUBSYS_ENGINES, true)
check(emp_mult == 2.4 and not combat.is_shield_online(e_emp), "EMP-Primed salvo should apply 2.4x synergy and break shields")
check(e_emp.engine_hp < 100, "Engine subsystem HP should degrade after shield collapse")
local sk_ai = require("ai.skirmish_ai")
local ph1, wv1 = sk_ai.wave_director_phase(5.0)
local ph2, wv2 = sk_ai.wave_director_phase(20.0)
check(ph1 == "ASSAULT" and wv1 == 1 and ph2 == "BREATHER" and wv2 == 1, "Wave Director must alternate ASSAULT and BREATHER phases")

-- 9. Hardcore Ironman status & Escape Pod branches
local ironman = require("rules.ironman")
local st1 = ironman.evaluate_status()
check(st1 == "ACTIVE_IN_COMMAND", "Commander should begin active in command")
check(ironman.attempt_emergency_evac(), "Commander should eject cleanly from Citadel escape pod")
local st2 = ironman.evaluate_status()
check(st2 == "EVACUATED_SAFE", "Commander should be marked EVACUATED_SAFE inside escape pod")

engine.world.commander_board(ACTIVE_MISSION.enemy_tank)
combat.plasma_salvo(ACTIVE_MISSION.enemy_tank, 5000, combat.SUBSYS_REACTOR)
local st3 = ironman.evaluate_status()
check(st3 == "DEFEAT_IRONMAN", "Destroying commander's vessel without escape pod must trigger DEFEAT_IRONMAN")

print("[Lua Unit Tests] ALL PASSED (Phases 1-4: 76 constructs, 12 roles, economy, sensors, astrogator, skirmish AI, boarding, meltdown, session/save/reload)")
