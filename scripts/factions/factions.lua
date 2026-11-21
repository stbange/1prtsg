-- SPDX-License-Identifier: MIT
-- Classic Bold-Color Retro Space-Opera Factions

local M = {}

M.LIST = {
    { id = 1, code = "SOLAR",   name = "Solar Directorate", primary = { 0.16, 0.42, 0.92 }, trim = { 0.96, 0.78, 0.18 } },
    { id = 2, code = "CRIMSON", name = "Crimson Dominion",  primary = { 0.85, 0.16, 0.22 }, trim = { 0.95, 0.58, 0.15 } },
}

function M.get_by_id(fid)
    for i = 1, #M.LIST do
        if M.LIST[i].id == fid then
            return M.LIST[i]
        end
    end
    return nil
end

return M
