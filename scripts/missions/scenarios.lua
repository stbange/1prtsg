-- SPDX-License-Identifier: MIT
-- 4 Curated First-Person RTS Scenarios

local M = {}

M.catalog = {
    {
        id = "orbital_drop_defense",
        title = "Orbital Drop Defense",
        brief = "Defend Solar Citadel Alpha against Crimson orbital dropship assaults.",
        start_blueprint = "bldg_command_citadel",
        ironman = true,
        meltdown_s = 0,
        in_orbit = false
    },
    {
        id = "carrier_strike",
        title = "Carrier Strike",
        brief = "Command Fleet Carrier Vanguard across the Solaris ocean bay and sink Crimson raiders.",
        start_blueprint = "ship_fleet_carrier",
        ironman = true,
        meltdown_s = 0,
        in_orbit = false
    },
    {
        id = "escape_flagship_60s",
        title = "Escape the Flagship (60s Meltdown)",
        brief = "Dreadnought Solaris core is critical! Race through 20 decks to an Escape Pod in 60 seconds.",
        start_blueprint = "space_command_dreadnought",
        ironman = true,
        meltdown_s = 60,
        in_orbit = true
    },
    {
        id = "drydock_boarding",
        title = "Drydock Boarding",
        brief = "Repel Crimson breachers inside the 420-room Orbital Shipyard Alpha and secure Deck 1 CIC.",
        start_blueprint = "stn_orbital_shipyard",
        ironman = true,
        meltdown_s = 0,
        in_orbit = true
    }
}

function M.count()
    return #M.catalog
end

function M.find(id)
    for i = 1, #M.catalog do
        if M.catalog[i].id == id then
            return M.catalog[i]
        end
    end
    return nil
end

function M.launch_scenario(id)
    local sc = M.find(id)
    if sc == nil then
        return nil
    end
    local alt = sc.in_orbit and 400000 or 0
    local flagship = engine.world.spawn(sc.start_blueprint, sc.title .. " HQ", 1, 0, 6371000 + alt, 0, true)
    local enemy = engine.world.spawn("veh_siege_hovertank", "Crimson Vanguard", 2, 110, 6371000 + alt, -95, false)
    if sc.meltdown_s > 0 then
        engine.boarding.trigger_meltdown(flagship, sc.meltdown_s)
    end
    return {
        scenario = sc,
        flagship_id = flagship,
        enemy_id = enemy
    }
end

return M
