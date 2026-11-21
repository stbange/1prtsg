-- SPDX-License-Identifier: MIT
-- Phase 1 Vertical Slice Mission: Citadel, Dropship, Plasma Tank & 400km Orbital Shipyard

local M = {}

function M.setup()
    local registry = require("units.registry")
    local interiors = require("interiors.catalog")

    local citadel_id = engine.world.spawn("bldg_command_citadel", "Solar Citadel Alpha", 1, 0, 6371000, 0, true)
    local foundry_id = engine.world.spawn("bldg_vehicle_foundry", "Solaris Vehicle Foundry", 1, -46, 6371000, -68, true)
    local dropship_id = engine.world.spawn("air_dropship", "Dropship Valkyrie-2", 1, 28, 6371000, -44, true)
    local tank_id = engine.world.spawn("veh_plasma_tank", "Vanguard Tank 1", 1, 16, 6371000, -72, true)
    local enemy_tank_id = engine.world.spawn("veh_plasma_tank", "Crimson Raider 1", 2, 260, 6371000, -15, true)
    local shipyard_id = engine.world.spawn("stn_orbital_shipyard", "Orbital Shipyard Alpha", 1, 0, 0, 0, true)
    local dread_id = engine.world.spawn("space_command_dreadnought", "Dreadnought Solaris", 1, 0, 0, 0, true)
    local enemy_dread_id = engine.world.spawn("space_siege_dreadnought", "Crimson Siege Dreadnought", 2, 0, 0, 0, true)

    engine.world.set_orbital(shipyard_id, 400000, 0.25, 0.0)
    engine.world.set_orbital(dread_id, 400000, 0.25, 0.002)
    engine.world.set_orbital(enemy_dread_id, 400000, 0.25, 0.004)

    interiors.hydrate_entity(citadel_id, registry.get_blueprint("bldg_command_citadel"))
    interiors.hydrate_entity(foundry_id, registry.get_blueprint("bldg_vehicle_foundry"))
    interiors.hydrate_entity(dropship_id, registry.get_blueprint("air_dropship"))
    interiors.hydrate_entity(tank_id, registry.get_blueprint("veh_plasma_tank"))
    interiors.hydrate_entity(shipyard_id, registry.get_blueprint("stn_orbital_shipyard"))
    interiors.hydrate_entity(dread_id, registry.get_blueprint("space_command_dreadnought"))

    engine.world.commander_board(citadel_id)

    return {
        citadel = citadel_id,
        foundry = foundry_id,
        dropship = dropship_id,
        tank = tank_id,
        enemy_tank = enemy_tank_id,
        shipyard = shipyard_id,
        dreadnought = dread_id,
        enemy_dreadnought = enemy_dread_id,
    }
end

return M
