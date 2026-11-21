-- SPDX-License-Identifier: MIT
-- Tactical Surface & Orbital Astrogator AI Helpers

local M = {}

M.ORDER_MOVE = 1
M.ORDER_ATTACK = 2
M.ORDER_ORBIT = 3

function M.plan_hohmann_transfer(alt1_m, alt2_m)
    local total_dv, transfer_time = engine.orbit.hohmann_dv(alt1_m, alt2_m)
    return { total_dv = total_dv, transfer_time = transfer_time }
end

function M.order_squad_move(entity_id, x, y, z)
    return engine.world.issue_order(entity_id, M.ORDER_MOVE, x, y, z, 0, 0)
end

function M.order_engage_target(attacker_id, target_id)
    return engine.world.issue_order(attacker_id, M.ORDER_ATTACK, 0, 0, 0, target_id, 0)
end

return M
