-- SPDX-License-Identifier: MIT
-- Spherical Horizon Fog-of-War & Planetary-to-Orbital Comms Relay Helpers

local M = {}

function M.can_detect(observer_faction, target_id)
    return engine.sensors.is_detected(observer_faction, target_id)
end

function M.has_command_link(hq_id, unit_id)
    return engine.sensors.is_comms_linked(hq_id, unit_id)
end

function M.trigger_radio_alert(wav_path)
    local gain, audible = engine.audio.play_3d(wav_path, 0, 0, 0, 2, 0.9, 500)
    return gain, audible
end

return M
