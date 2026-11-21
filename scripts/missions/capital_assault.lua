-- SPDX-License-Identifier: MIT
-- Phase 3 Capital Fleet Warfare & Multi-Deck Boarding Mission (400km LEO)

local M = {}

function M.setup()
    local enemy_dread = engine.world.spawn("space_command_dreadnought", "Crimson Flagship 'Vengeance'", 2, 0, 0, 0, true)
    local enemy_bc    = engine.world.spawn("space_battlecruiser", "Crimson Cruiser 'Fury'", 2, 0, 0, 0, true)
    local solar_corv  = engine.world.spawn("space_boarding_pod", "Solar Boarding Shuttle 'Spear'", 1, 0, 0, 0, true)

    engine.world.set_orbital(enemy_dread, 400000, 0.25, 0.010)
    engine.world.set_orbital(enemy_bc,    400000, 0.25, 0.014)
    engine.world.set_orbital(solar_corv,  400000, 0.25, 0.009)

    return {
        enemy_dread = enemy_dread,
        enemy_bc = enemy_bc,
        solar_corv = solar_corv,
    }
end

return M
