-- SPDX-License-Identifier: MIT
-- Phase 1 Vertical Slice Mission: Citadel, Dropship, Plasma Tank & 400km Orbital Shipyard

local M = {}

function M.setup()
    local citadel_id = engine.world.spawn("bldg_command_citadel", "Solar Citadel Alpha", 1, 0, 6371000, 0, true)
    local dropship_id = engine.world.spawn("air_dropship", "Dropship Valkyrie-2", 1, 45, 6371000, 20, true)
    local tank_id = engine.world.spawn("veh_plasma_tank", "Vanguard Tank 1", 1, 80, 6371000, -15, true)
    local enemy_tank_id = engine.world.spawn("veh_plasma_tank", "Crimson Raider 1", 2, 260, 6371000, -15, true)
    local shipyard_id = engine.world.spawn("stn_orbital_shipyard", "Orbital Shipyard Alpha", 1, 0, 0, 0, true)
    local dread_id = engine.world.spawn("space_command_dreadnought", "Dreadnought Solaris", 1, 0, 0, 0, true)

    engine.world.set_orbital(shipyard_id, 400000, 0.25, 0.0)
    engine.world.set_orbital(dread_id, 400000, 0.25, 0.002)
    engine.world.commander_board(citadel_id)

    return {
        citadel = citadel_id,
        dropship = dropship_id,
        tank = tank_id,
        enemy_tank = enemy_tank_id,
        shipyard = shipyard_id,
        dreadnought = dread_id,
    }
end

return M
