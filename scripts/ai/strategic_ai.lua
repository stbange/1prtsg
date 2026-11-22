-- SPDX-License-Identifier: MIT
-- Autonomous Multi-Theater Strategic AI Commander (Ground, Naval, Air & Orbital)

local eco = require("rules.economy")
local sensors = require("rules.sensors")

local M = {}

function M.new_commander(faction_id, difficulty)
    local aggr = (difficulty == "aggressive")
    return {
        faction_id = faction_id,
        difficulty = aggr and "aggressive" or "balanced",
        think_interval_s = aggr and 1.5 or 3.0,
        accum_s = 0.0,
        phase = "EXPAND",
        relay_id = 0,
        recon_sat_id = 0,
        shipyard_id = 0,
        foundry_id = 0,
        built_count = 0,
        invasions_launched = 0,
        relay_rebuilds = 0
    }
end

function M.evaluate_build_order(ai, ore, has_relay, has_shipyard)
    if not has_relay and ore >= 200 then
        return "stn_comms_relay", true
    end
    if not has_shipyard and ore >= 600 then
        return "stn_orbital_shipyard", true
    end
    if ai.built_count == 0 and ore >= 250 then
        return "bldg_ore_mine", false
    end
    if ai.built_count == 1 and ore >= 300 then
        return "bldg_vehicle_foundry", false
    end
    if ai.difficulty == "aggressive" and ore >= 400 then
        return "air_dropship", false
    end
    if ore >= 350 then
        return "veh_plasma_tank", false
    end
    return nil, false
end

function M.step(ai, world_ctx, dt_s)
    ai.accum_s = ai.accum_s + dt_s
    local report = { built = nil, invasion = false, relay_restored = false }
    if ai.accum_s < ai.think_interval_s then
        return report
    end
    ai.accum_s = 0.0

    local led = eco.get_resources(ai.faction_id)
    local ore = (led ~= nil) and led.ore or 0
    local relay_alive = (ai.relay_id > 0 and engine.world.get_entity(ai.relay_id) ~= nil)
    local yard_alive = (ai.shipyard_id > 0 and engine.world.get_entity(ai.shipyard_id) ~= nil)
    local bp_id, orbital = M.evaluate_build_order(ai, ore, relay_alive, yard_alive)

    if bp_id ~= nil then
        local alt = orbital and 400000 or 0
        local bx = (ai.faction_id == 2 and 160 or -160) + ai.built_count * 22
        local ent_id = engine.world.spawn(bp_id, "AI " .. bp_id, ai.faction_id, bx, 6371000 + alt, -140, false)
        if ent_id > 0 then
            ai.built_count = ai.built_count + 1
            report.built = bp_id
            if bp_id == "stn_comms_relay" then
                if ai.relay_id > 0 then ai.relay_rebuilds = ai.relay_rebuilds + 1 end
                ai.relay_id = ent_id
                report.relay_restored = true
            elseif bp_id == "stn_orbital_shipyard" then
                ai.shipyard_id = ent_id
            elseif bp_id == "bldg_vehicle_foundry" then
                ai.foundry_id = ent_id
            end
        end
    end

    if world_ctx ~= nil and world_ctx.target_id ~= nil and world_ctx.raider_id ~= nil then
        if sensors.can_detect(ai.faction_id, world_ctx.target_id) then
            local tgt = engine.world.get_entity(world_ctx.target_id)
            if tgt ~= nil then
                engine.world.issue_order(world_ctx.raider_id, 2, tgt.x, tgt.y, tgt.z, world_ctx.target_id, 0)
                ai.phase = "ASSAULT"
                ai.invasions_launched = ai.invasions_launched + 1
                report.invasion = true
            end
        end
    end
    return report
end

return M
