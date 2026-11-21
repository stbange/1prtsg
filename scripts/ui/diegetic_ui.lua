-- SPDX-License-Identifier: MIT
-- Diegetic Tactical Wrist-Pad, Optical Designator, Radial Quick Wheel & 3D Holo-Globe Controller

local M = {}

M.PAGES = { "TACTICAL_GLOBE", "PRODUCTION_QUEUE", "CREW_ROSTER", "COMMS_NETWORK" }
M.RADIAL_ORDERS = { "ESCORT", "MOVE", "ATTACK", "ORBIT", "DOCK", "BOARD", "EVACUATE" }
M.OPTICS_ZOOM = { 1, 4, 12 }

function M.format_page_header(page_index, comms_ok)
    local title = M.PAGES[page_index] or "TACTICAL_GLOBE"
    local link = comms_ok and "LINKED" or "RELAY_LOST"
    return title .. " [" .. link .. "]"
end

function M.optics_label(level)
    local z = M.OPTICS_ZOOM[level] or 1
    if z == 1 then
        return "VISOR 1X"
    end
    return "OPTICS " .. tostring(z) .. "X [LASER DESIGNATOR]"
end

function M.radial_label(slot)
    return M.RADIAL_ORDERS[slot] or "ESCORT"
end

return M
