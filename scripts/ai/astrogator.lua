-- SPDX-License-Identifier: MIT
-- Automated Orbital Astrogator (Ascent, Circularization & Shielded Re-entry)

local M = {}

M.PHASE_IDLE = 0
M.PHASE_ASCENT = 1
M.PHASE_CIRCULARIZED = 2
M.PHASE_RENDEZVOUS = 3
M.PHASE_REENTRY = 4
M.PHASE_LANDED = 5

function M.launch_to_orbit(craft_id, target_alt_m)
    local started = engine.control.astrogator_ascent(craft_id, target_alt_m)
    if not started then
        return false, M.PHASE_IDLE
    end
    local phase, _ = engine.control.astrogator_step(1.0, 80)
    return phase == M.PHASE_CIRCULARIZED, phase
end

function M.execute_shielded_reentry(craft_id)
    local started = engine.control.astrogator_reentry(craft_id)
    if not started then
        return false, 0.0
    end
    local phase, peak_heat = engine.control.astrogator_step(1.0, 90)
    return phase == M.PHASE_LANDED, peak_heat
end

return M
