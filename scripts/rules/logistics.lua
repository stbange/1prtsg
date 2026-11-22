-- SPDX-License-Identifier: MIT
-- Physical Logistics Network (Ore Cargo Trucks, Naval Relay Tenders & Orbital Lifters)

local M = {}

function M.new_network()
    return {
        routes = {},
        total_ore_delivered = 0,
        total_fuel_delivered = 0,
        convoys_raided = 0
    }
end

function M.add_route(net, faction_id, carrier_id, kind, ore_payload, fuel_payload, transit_s)
    local route = {
        faction_id = faction_id,
        carrier_id = carrier_id,
        kind = kind,
        ore_payload = ore_payload,
        fuel_payload = fuel_payload,
        transit_s = transit_s,
        progress_s = 0.0,
        deliveries = 0,
        active = true
    }
    net.routes[#net.routes + 1] = route
    return #net.routes
end

function M.raid_convoy(net, route_idx)
    local r = net.routes[route_idx]
    if r == nil or not r.active then
        return false
    end
    r.active = false
    net.convoys_raided = net.convoys_raided + 1
    if r.carrier_id > 0 then
        engine.world.apply_damage(r.carrier_id, 99999, 0)
    end
    return true
end

function M.step(net, dt_s)
    local delivered_ore = 0
    local delivered_fuel = 0
    for i = 1, #net.routes do
        local r = net.routes[i]
        if r.active then
            local ent = engine.world.get_entity(r.carrier_id)
            if ent == nil then
                r.active = false
            else
                r.progress_s = r.progress_s + dt_s
                if r.progress_s >= r.transit_s then
                    r.progress_s = r.progress_s - r.transit_s
                    r.deliveries = r.deliveries + 1
                    delivered_ore = delivered_ore + r.ore_payload
                    delivered_fuel = delivered_fuel + r.fuel_payload
                end
            end
        end
    end
    net.total_ore_delivered = net.total_ore_delivered + delivered_ore
    net.total_fuel_delivered = net.total_fuel_delivered + delivered_fuel
    return delivered_ore, delivered_fuel
end

return M
