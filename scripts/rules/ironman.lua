-- SPDX-License-Identifier: MIT
-- Hardcore Ironman Commander Match Rules (Lua-moddable)

local M = {}

M.MATCH_ACTIVE = 0
M.MATCH_VICTORY = 1
M.MATCH_DEFEAT_IRONMAN = 2

function M.evaluate_status()
    local cmd = engine.world.commander_state()
    if not cmd.alive then
        return "DEFEAT_IRONMAN", cmd
    end
    if cmd.in_escape_pod then
        return "EVACUATED_SAFE", cmd
    end
    return "ACTIVE_IN_COMMAND", cmd
end

function M.attempt_emergency_evac()
    local launched = engine.world.commander_escape_pod()
    return launched
end

return M
