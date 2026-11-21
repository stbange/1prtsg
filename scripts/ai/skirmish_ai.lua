-- SPDX-License-Identifier: MIT
-- Multi-Theater Skirmish AI Commander (Ground, Naval, Air & Orbital)

local eco = require("rules.economy")
local sensors = require("rules.sensors")
local ai = require("ai.commander_ai")

local M = {}

function M.tick_faction(faction_id, foundry_id, flagship_id, target_id)
    local actions = { queued_job = 0, attack_issued = false }
    local led = eco.get_resources(faction_id)
    if led ~= nil and led.ore >= 500 then
        actions.queued_job = eco.queue_construct(foundry_id, "veh_plasma_tank")
    end
    if sensors.can_detect(faction_id, target_id) then
        actions.attack_issued = ai.order_engage_target(flagship_id, target_id)
    end
    return actions
end

function M.wave_director_phase(elapsed_s)
    local cycle = elapsed_s % 24.5
    if cycle < 18.0 then
        return "ASSAULT", math.floor(elapsed_s / 24.5) + 1
    end
    return "BREATHER", math.floor(elapsed_s / 24.5) + 1
end

return M
