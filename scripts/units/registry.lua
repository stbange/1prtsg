-- SPDX-License-Identifier: MIT
-- Loads all 6 doubled construct classes (76 constructs) + 12 personnel roles into the C core engine.

local M = {}

local CLASS_MODULES = {
    "units.buildings",
    "units.ground_vehicles",
    "units.naval_ships",
    "units.aircraft",
    "units.spacecraft",
    "units.space_stations",
}

function M.load_all()
    local total_constructs = 0
    for i = 1, #CLASS_MODULES do
        local list = require(CLASS_MODULES[i])
        for j = 1, #list do
            local ok = engine.world.register_blueprint(list[j])
            assert(ok, "Failed to register blueprint: " .. tostring(list[j].id))
            total_constructs = total_constructs + 1
        end
    end
    local roles = require("crew.roles")
    local total_roles = 0
    for i = 1, #roles do
        local ok = engine.world.register_crew_role(roles[i])
        assert(ok, "Failed to register crew role: " .. tostring(roles[i].id))
        total_roles = total_roles + 1
    end
    return total_constructs, total_roles
end

return M
