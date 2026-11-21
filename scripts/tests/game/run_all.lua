-- SPDX-License-Identifier: MIT
-- Automated End-to-End Headless Game Test Suite (12 Scenarios: Phase 1 + Phase 2)

local function expect(cond, msg)
    if not cond then
        error("GAME TEST FAIL: " .. msg)
    end
end

require("main")
local ai = require("ai.commander_ai")
local combat = require("rules.combat")
local ironman = require("rules.ironman")
local eco = require("rules.economy")
local sensors = require("rules.sensors")
local astro = require("ai.astrogator")
local skirmish = require("ai.skirmish_ai")
local combined = require("missions.combined_arms")

-- Scenario 1: Citadel War Room Holo-Globe -> Queue & Command Plasma Tank
local new_tank = engine.world.spawn("veh_plasma_tank", "Produced Tank 2", 1, 10, 6371000, 10, true)
expect(new_tank > 0, "Scenario 1: Production spawn failed")
expect(ai.order_squad_move(new_tank, 60, 6371000, 10), "Scenario 1: Issuing move order failed")
engine.world.step(0.1, 40)
local t_ent = engine.world.get_entity(new_tank)
expect(math.abs(t_ent.x - 60) < 0.5, "Scenario 1: Tank failed to reach Holo-Globe waypoint")

-- Scenario 2: Dropship Boarding, Orbital Ascent & Portal Airlock Transfer to 400km Orbital Shipyard
expect(engine.world.commander_board(ACTIVE_MISSION.dropship), "Scenario 2: Commander board dropship failed")
expect(engine.world.set_orbital(ACTIVE_MISSION.dropship, 400000, 0.25, 0.001), "Scenario 2: Orbital insertion failed")
engine.world.step(1.0, 60)
local ship_ent = engine.world.get_entity(ACTIVE_MISSION.dropship)
local alt_m = engine.planet.altitude(ship_ent.x, ship_ent.y, ship_ent.z)
expect(alt_m > 395000 and alt_m < 405000, "Scenario 2: Dropship orbit altitude drifted out of 400km LEO")
expect(engine.planet.atmo_density(alt_m) == 0, "Scenario 2: 400km LEO must be vacuum")
local stn_ent = engine.world.get_entity(ACTIVE_MISSION.shipyard)
expect(engine.interior.add_portal_and_transfer(ship_ent.own_frame_id, stn_ent.own_frame_id), "Scenario 2: Airlock docking transfer failed")
expect(engine.world.commander_state().frame_id == stn_ent.own_frame_id, "Scenario 2: Commander not inside Orbital Shipyard frame")

-- Scenario 3: Localized Engine Damage Degrades Speed & Engineer Crew Repairs It
local speed_before = engine.world.get_entity(ACTIVE_MISSION.tank).effective_speed
combat.plasma_salvo(ACTIVE_MISSION.tank, 430, combat.SUBSYS_ENGINES)
local damaged = engine.world.get_entity(ACTIVE_MISSION.tank)
expect(damaged.effective_speed < speed_before, "Scenario 3: Engine damage must degrade effective speed")
engine.world.step(1.0, 30)
local repaired = engine.world.get_entity(ACTIVE_MISSION.tank)
expect(repaired.engine_hp > damaged.engine_hp, "Scenario 3: Crew engineers must repair damaged engines")

-- Scenario 4: Hybrid Crew Visual LOD
local lod_inside, _ = engine.world.eval_crew_lod(120, true, false, 0.0)
local lod_vr, _     = engine.world.eval_crew_lod(120, false, true, 0.0)
local lod_far, dir  = engine.world.eval_crew_lod(120, false, false, 1.57)
expect(lod_inside == 0 and lod_vr == 0, "Scenario 4: Interior & WebXR crew must use 3D mesh LOD")
expect(lod_far == 1 and dir >= 0 and dir < 8, "Scenario 4: Distant exterior crew must use 8-dir sprite LOD")

-- Scenario 5: Mobile Web Auto-Touch Virtual Thumbstick & Pinch Zoom
local t_mode, mx, my, zoom = engine.input.simulate_touch()
expect(t_mode == 2 and mx > 0.1 and my > 0.1 and zoom > 0.1, "Scenario 5: Touch stick & Holo-Globe pinch failed")

-- Scenario 6: WebXR 6-DOF Stereo Session & Controller Aim-Ray Holo-Globe Hit
local xr_mode, ray_hit, sel = engine.input.simulate_webxr()
expect(xr_mode == 3 and ray_hit and sel, "Scenario 6: WebXR aim-ray hit failed")

-- Scenario 7: Optional Voxel Chunk Face-Culled Mesh Verification
expect(engine.voxel.test_mesh() == 60, "Scenario 7: Voxel face culling mismatch")

-- Scenario 8: Combined-Arms Foundry 1:1 Production Queue & Multi-Theater Skirmish AI
local ca = combined.setup()
local prod_job = eco.queue_construct(ca.foundry, "veh_apc")
expect(prod_job > 0, "Scenario 8: Foundry failed to queue APC")
expect(eco.advance_production(1.0, 20) >= 1, "Scenario 8: APC build failed to complete")
expect(eco.get_spawned_unit(prod_job) > 0, "Scenario 8: Spawned APC id invalid")
local sk = skirmish.tick_faction(1, ca.foundry, ca.carrier, ca.far_enemy)
expect(sk.queued_job > 0 and sk.attack_issued, "Scenario 8: Skirmish AI failed multi-theater tick")

-- Scenario 9: Spherical Horizon Fog-of-War & Comms Relay Blackout on Satellite Destruction
expect(sensors.can_detect(1, ca.far_enemy), "Scenario 9: Recon Sat must spot far-side enemy")
expect(sensors.has_command_link(ACTIVE_MISSION.citadel, ca.far_squad), "Scenario 9: GEO Relay must link far squad")
combat.plasma_salvo(ca.relay_sat, 5000, combat.SUBSYS_SENSORS)
expect(not sensors.has_command_link(ACTIVE_MISSION.citadel, ca.far_squad), "Scenario 9: Destroying GEO Relay must sever far-side comms")

-- Scenario 10: Astrogator Automated Orbital Ascent & Shielded Atmospheric Re-entry
local shuttle = engine.world.spawn("space_shuttle", "Courier-1", 1, 0, 6371020, 0, true)
local ok_up, _ = astro.launch_to_orbit(shuttle, 400000)
expect(ok_up, "Scenario 10: Shuttle automated ascent to 400km LEO failed")
local ok_down, heat = astro.execute_shielded_reentry(shuttle)
expect(ok_down and heat > 10.0, "Scenario 10: Shuttle shielded re-entry heating/landing failed")

-- Scenario 11: Spatial Audio Interior Muffling & Orbital Vacuum Silence
engine.audio.set_listener(0, 0, 0, 400000, 0.0)
local vac_gain, vac_aud = engine.audio.play_3d("audio/plasma_bolt.wav", 20, 0, 0, 0, 1.0, 200)
local rad_gain, rad_aud = engine.audio.play_3d("audio/alert_klaxon.wav", 9000, 0, 0, 2, 0.9, 200)
expect(not vac_aud and vac_gain == 0 and rad_aud and rad_gain > 0.5, "Scenario 11: Vacuum must silence airborne audio while preserving radio comms")

-- Scenario 12: VFS Mod Mounting + Hardcore Ironman Escape Pod Survival
expect(engine.vfs.mount_mod("example_mod"), "Scenario 12: Mounting mods/example_mod failed")
package.loaded["units.ground_vehicles"] = nil
local modded = require("units.ground_vehicles")
expect(modded[1].name == "Emerald Vanguard Plasma Tank", "Scenario 12: Mod override did not take precedence")
expect(engine.world.commander_board(ACTIVE_MISSION.dreadnought), "Scenario 12: Board Dreadnought failed")
expect(ironman.attempt_emergency_evac(), "Scenario 12: Emergency Escape Pod ejection failed")
combat.plasma_salvo(ACTIVE_MISSION.dreadnought, 50000, combat.SUBSYS_REACTOR)
expect(ironman.evaluate_status() == "EVACUATED_SAFE", "Scenario 12: Commander inside Escape Pod must survive Dreadnought loss")

-- Scenario 13: Multi-Deck Capital Ship Boarding, Hull Decompression & Intact Bridge Capture
local cap_m = require("missions.capital_assault")
local cap = cap_m.setup()
local boarding = require("rules.boarding")
local bop = boarding.launch_boarding_assault(cap.enemy_dread, 1, 180, true, true)
expect(bop > 0, "Scenario 13: Failed to launch boarding operation onto Crimson Dreadnought")
local bres = boarding.resolve_boarding(bop, 30)
expect(bres ~= nil and bres.status == boarding.STATUS_CAPTURED and bres.decompressed, "Scenario 13: Decompressed multi-deck boarding capture failed")
expect(engine.world.get_entity(cap.enemy_dread).faction_id == 1, "Scenario 13: Captured Dreadnought did not transfer to Solar Directorate")

-- Scenario 14: Critical Reactor Meltdown Countdown -> Commander Escape-Pod Sprint Survival
expect(engine.world.commander_board(cap.enemy_dread), "Scenario 14: Commander failed to board captured Dreadnought")
expect(boarding.initiate_reactor_meltdown(cap.enemy_dread, 8.0), "Scenario 14: Reactor meltdown failed to trigger")
boarding.advance_meltdowns(1.0, 4)
expect(boarding.meltdown_status(cap.enemy_dread).countdown_s > 0, "Scenario 14: Meltdown should still be counting down")
expect(ironman.attempt_emergency_evac(), "Scenario 14: Commander failed Escape Pod sprint ejection")
boarding.advance_meltdowns(1.0, 5)
expect(boarding.meltdown_status(cap.enemy_dread).detonated, "Scenario 14: Dreadnought reactor failed to detonate")
expect(ironman.evaluate_status() == "EVACUATED_SAFE", "Scenario 14: Commander must survive meltdown via Escape Pod")

-- Scenario 15: Reactor Meltdown Ironman Defeat when Commander Remains Aboard Without Escape Pod
expect(engine.world.commander_board(cap.solar_corv), "Scenario 15: Commander failed to board Corvette")
expect(boarding.initiate_reactor_meltdown(cap.solar_corv, 3.0), "Scenario 15: Corvette meltdown failed to start")
boarding.advance_meltdowns(1.0, 4)
expect(ironman.evaluate_status() == "DEFEAT_IRONMAN", "Scenario 15: Remaining aboard during meltdown must trigger DEFEAT_IRONMAN")

print("[Automated Game Tests] ALL 15 END-TO-END SCENARIOS PASSED!")
