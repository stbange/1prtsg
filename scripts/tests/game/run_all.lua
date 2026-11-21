-- SPDX-License-Identifier: MIT
-- Automated End-to-End Headless Game Test Suite (8 Scenarios)

local function expect(cond, msg)
    if not cond then
        error("GAME TEST FAIL: " .. msg)
    end
end

-- Boot full external gameplay stack
require("main")
local ai = require("ai.commander_ai")
local combat = require("rules.combat")
local ironman = require("rules.ironman")

-- Scenario 1: Citadel War Room Holo-Globe -> Queue & Command Plasma Tank
local new_tank = engine.world.spawn("veh_plasma_tank", "Produced Tank 2", 1, 10, 6371000, 10, true)
expect(new_tank > 0, "Scenario 1: Factory/Holo-Globe production spawn failed")
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

-- Scenario 4: Hybrid Crew Visual LOD (3D up close / interior / WebXR; 8-dir sprite at range)
local lod_inside, _ = engine.world.eval_crew_lod(120, true, false, 0.0)
local lod_vr, _     = engine.world.eval_crew_lod(120, false, true, 0.0)
local lod_far, dir  = engine.world.eval_crew_lod(120, false, false, 1.57)
expect(lod_inside == 0 and lod_vr == 0, "Scenario 4: Interior & WebXR crew must use 3D mesh LOD")
expect(lod_far == 1 and dir >= 0 and dir < 8, "Scenario 4: Distant exterior crew must use 8-dir sprite LOD")

-- Scenario 5: Mobile Web Auto-Touch Virtual Thumbstick & Pinch Zoom
local t_mode, mx, my, zoom = engine.input.simulate_touch()
expect(t_mode == 2, "Scenario 5: Touch events must auto-switch input mode to MOBILE_TOUCH")
expect(mx > 0.1 and my > 0.1 and zoom > 0.1, "Scenario 5: Touch stick and Holo-Globe pinch must emit intents")

-- Scenario 6: WebXR 6-DOF Stereo Session & Controller Aim-Ray Holo-Globe Hit
local xr_mode, ray_hit, sel = engine.input.simulate_webxr()
expect(xr_mode == 3 and ray_hit and sel, "Scenario 6: WebXR controller aim-ray must hit Holo-Globe target")

-- Scenario 7: Optional Voxel Chunk Face-Culled Mesh Verification
local vverts = engine.voxel.test_mesh()
expect(vverts == 60, "Scenario 7: Two adjacent voxels must cull shared face -> 10 faces * 6 verts = 60, got " .. tostring(vverts))

-- Scenario 8: Runtime VFS Mod Mounting & External Asset Override + Hardcore Ironman Escape Pod vs Loss
expect(engine.vfs.mount_mod("example_mod"), "Scenario 8: Mounting mods/example_mod failed")
package.loaded["units.ground_vehicles"] = nil
local modded_vehicles = require("units.ground_vehicles")
expect(modded_vehicles[1].name == "Emerald Vanguard Plasma Tank", "Scenario 8: Mod override did not take precedence")
expect(engine.world.commander_board(ACTIVE_MISSION.dreadnought), "Scenario 8: Board Dreadnought failed")
expect(ironman.attempt_emergency_evac(), "Scenario 8: Emergency Escape Pod ejection failed")
combat.plasma_salvo(ACTIVE_MISSION.dreadnought, 50000, combat.SUBSYS_REACTOR)
expect(ironman.evaluate_status() == "EVACUATED_SAFE", "Scenario 8: Commander inside Escape Pod must survive Dreadnought loss")

print("[Automated Game Tests] ALL 8 END-TO-END SCENARIOS PASSED!")
