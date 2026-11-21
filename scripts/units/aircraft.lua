-- SPDX-License-Identifier: MIT
-- Class 3: Aircraft (8 Enterable Atmospheric & Trans-Atmospheric Craft - Doubled Roster)

return {
    { id = "air_starfighter",     name = "Angular Starfighter Interceptor", class_id = 3, length = 15,  width = 12,  height = 4.2, hull = 260,  shield = 180,  speed = 240, crew = 1,  rooms = 1,  holo_globe = false, escape_pod = false, can_orbit = true,  mesh = "models/air_starfighter.obj" },
    { id = "air_recon_fighter",   name = "Stealth Recon Fighter",           class_id = 3, length = 17,  width = 14,  height = 4.0, hull = 240,  shield = 210,  speed = 270, crew = 2,  rooms = 1,  holo_globe = false, escape_pod = false, can_orbit = true,  mesh = "models/air_recon_fighter.obj" },
    { id = "air_plasma_bomber",   name = "Plasma Strike Bomber",            class_id = 3, length = 24,  width = 26,  height = 6.5, hull = 580,  shield = 380,  speed = 165, crew = 3,  rooms = 2,  holo_globe = false, escape_pod = false, can_orbit = true,  mesh = "models/air_plasma_bomber.obj" },
    { id = "air_dropship",        name = "Troop and Cargo Dropship",        class_id = 3, length = 28,  width = 18,  height = 8.5, hull = 950,  shield = 600,  speed = 140, crew = 16, rooms = 3,  holo_globe = false, escape_pod = true,  can_orbit = true,  mesh = "models/air_dropship.obj" },
    { id = "air_heavy_lifter",    name = "Heavy Vehicle Lifter Dropship",   class_id = 3, length = 44,  width = 32,  height = 13,  hull = 1600, shield = 900,  speed = 110, crew = 4,  rooms = 3,  holo_globe = false, escape_pod = true,  can_orbit = true,  mesh = "models/air_heavy_lifter.obj" },
    { id = "air_plasma_gunship",  name = "Heavy Plasma Gunship",            class_id = 3, length = 32,  width = 24,  height = 9,   hull = 1250, shield = 780,  speed = 125, crew = 6,  rooms = 3,  holo_globe = false, escape_pod = false, can_orbit = false, mesh = "models/air_plasma_gunship.obj" },
    { id = "air_awacs_cruiser",   name = "Airborne AWACS Comms Cruiser",    class_id = 3, length = 58,  width = 54,  height = 14,  hull = 1800, shield = 1100, speed = 135, crew = 14, rooms = 5,  holo_globe = true,  escape_pod = true,  can_orbit = false, mesh = "models/air_awacs_cruiser.obj" },
    { id = "air_command_carrier", name = "Airborne Command Carrier",        class_id = 3, length = 165, width = 110, height = 34,  hull = 5400, shield = 3400, speed = 85,  crew = 48, rooms = 10, holo_globe = true,  escape_pod = true,  can_orbit = false, mesh = "models/air_command_carrier.obj" },
}
