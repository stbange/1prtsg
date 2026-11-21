-- SPDX-License-Identifier: MIT
-- Multi-Deck Capital Ship Boarding & Critical Reactor Meltdown Rules (External Lua)

local M = {}

M.STATUS_ACTIVE = 0
M.STATUS_CAPTURED = 1
M.STATUS_REPELLED = 2

function M.launch_boarding_assault(target_id, attacker_faction, marines, mag_boots, decompress)
    local op_id = engine.boarding.start(target_id, attacker_faction, marines, mag_boots)
    if op_id > 0 and decompress then
        engine.boarding.set_decompression(op_id, true)
    end
    return op_id
end

function M.resolve_boarding(op_id, max_seconds)
    engine.boarding.step(1.0, max_seconds)
    return engine.boarding.get_op(op_id)
end

function M.initiate_reactor_meltdown(entity_id, countdown_s)
    local ok = engine.boarding.trigger_meltdown(entity_id, countdown_s)
    if ok then
        engine.audio.play_3d("audio/alert_klaxon.wav", 0, 0, 0, 2, 1.0, 500)
    end
    return ok
end

function M.advance_meltdowns(dt_s, steps)
    return engine.boarding.step(dt_s, steps)
end

function M.meltdown_status(entity_id)
    return engine.boarding.get_meltdown(entity_id)
end

return M
