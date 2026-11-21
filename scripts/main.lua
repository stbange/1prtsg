-- SPDX-License-Identifier: MIT
-- 1stPRTS Main External Lua Bootstrap

local registry = require("units.registry")
local factions = require("factions.factions")
local mission = require("missions.vertical_slice")

local n_units, n_roles = registry.load_all()
assert(n_units == 76, "Expected 76 doubled constructs, got " .. tostring(n_units))
assert(n_roles == 12, "Expected 12 doubled crew roles, got " .. tostring(n_roles))
assert(factions.get_by_id(1) ~= nil, "Missing Solar Directorate faction")

_G.ACTIVE_MISSION = mission.setup()
