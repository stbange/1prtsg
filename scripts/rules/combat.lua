-- SPDX-License-Identifier: MIT
-- Shield Absorption, Localized Subsystem Damage & Crew Repair Helpers

local M = {}

M.SUBSYS_REACTOR = 0
M.SUBSYS_ENGINES = 1
M.SUBSYS_SENSORS = 2
M.SUBSYS_WEAPONS = 3
M.SUBSYS_BRIDGE = 4

function M.plasma_salvo(target_id, power, subsys)
    engine.world.apply_damage(target_id, power, subsys)
    return engine.world.get_entity(target_id)
end

function M.is_shield_online(ent)
    return ent ~= nil and ent.shield > 0
end

function M.emp_prime_salvo(target_id, base_power, subsys, emp_primed)
    local mult = emp_primed and 2.4 or 1.0
    engine.world.apply_damage(target_id, base_power * mult, subsys)
    return engine.world.get_entity(target_id), mult
end

return M
