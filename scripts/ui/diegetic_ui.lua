-- SPDX-License-Identifier: MIT
-- Diegetic Tactical Wrist-Pad & Holo-Globe Page Controller

local M = {}

M.PAGES = { "TACTICAL_GLOBE", "PRODUCTION_QUEUE", "CREW_ROSTER", "COMMS_NETWORK" }

function M.format_page_header(page_index, comms_ok)
    local title = M.PAGES[page_index] or "TACTICAL_GLOBE"
    local link = comms_ok and "LINKED" or "RELAY_LOST"
    return title .. " [" .. link .. "]"
end

return M
