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

-- 6. Combat, shield & localized subsystem damage unit test
local combat = require("rules.combat")
local enemy = combat.plasma_salvo(ACTIVE_MISSION.enemy_tank, 100, combat.SUBSYS_ENGINES)
check(combat.is_shield_online(enemy), "Enemy shield should remain online after light hit")
enemy = combat.plasma_salvo(ACTIVE_MISSION.enemy_tank, 400, combat.SUBSYS_ENGINES)
check(not combat.is_shield_online(enemy), "Enemy shield should deplete after heavy salvo")
check(enemy.engine_hp < 100, "Engine subsystem HP should degrade after shield collapse")

-- 7. Hardcore Ironman status & Escape Pod branches
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

print("[Lua Unit Tests] ALL PASSED (Phase 1 + Phase 2: 76 constructs, 12 roles, economy, sensors, astrogator, skirmish AI)")
