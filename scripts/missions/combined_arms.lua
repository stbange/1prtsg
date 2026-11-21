-- SPDX-License-Identifier: MIT
-- Combined-Arms Multi-Theater Scenario (Land Foundry, Naval Carrier, Recon/Relay Satellites & Orbital Shipyard)

local M = {}

function M.setup()
    local R = 6371000
    local foundry = engine.world.spawn("bldg_vehicle_foundry", "Solar Foundry 1", 1, 120, R, 30, true)
    local carrier = engine.world.spawn("ship_fleet_carrier", "Carrier Vanguard", 1, -2400, R, 900, true)
    local recon_sat = engine.world.spawn("stn_recon_sat", "Recon Sat LEO-1", 1, 0, 0, 0, true)
    local relay_sat = engine.world.spawn("stn_comms_relay", "Relay Sat GEO-1", 1, 0, 0, 0, true)
    local far_squad = engine.world.spawn("veh_apc", "Far-Side Recon APC", 1, R + 20, 0, 0, true)
    local far_enemy = engine.world.spawn("bldg_command_citadel", "Crimson Polar Base", 2, 0, -R - 20, 0, true)

    -- Place satellites cleanly over the South Pole (recon) and 45-deg high orbit (GEO relay) for true spherical LOS
    engine.world.set_orbital(recon_sat, 900000, 1.5707963, 4.712389)
    engine.world.set_orbital(relay_sat, 8500000, 1.5707963, 0.785398)

    return {
        foundry = foundry,
        carrier = carrier,
        recon_sat = recon_sat,
        relay_sat = relay_sat,
        far_squad = far_squad,
        far_enemy = far_enemy,
    }
end

return M
