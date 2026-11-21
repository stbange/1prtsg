-- SPDX-License-Identifier: MIT
-- Tailored Multi-Deck Enterable Interior Catalog for ALL 76 Constructs
-- Console types match C enum RtsConsoleType:
--   0 = HOLO_GLOBE, 1 = BRIDGE_HELM, 2 = PILOT_SEAT,
--   3 = FACTORY_TERMINAL, 4 = ESCAPE_POD, 5 = TURRET_GUNNER

local M = {}

M.CONSOLE_HOLO_GLOBE       = 0
M.CONSOLE_BRIDGE_HELM      = 1
M.CONSOLE_PILOT_SEAT       = 2
M.CONSOLE_FACTORY_TERMINAL = 3
M.CONSOLE_ESCAPE_POD       = 4
M.CONSOLE_TURRET_GUNNER    = 5

local SPECS = {
    -- Class 0: 24 Planetary Buildings
    { id = "bldg_command_citadel",   decks = 3, rooms = { "War Room Holo-Deck", "Strategic Comms Hall", "Tactical Foundry Annex", "Fusion Sub-Core", "Escape Pod Shaft" }, consoles = { { "Citadel Holo-Globe", 0, 1, 0.0, -3.5 }, { "Foundry Dispatch Link", 3, 1, -5.5, -1.0 }, { "Citadel Escape Pod", 4, 1, 5.5, 2.5 } } },
    { id = "bldg_tactical_outpost",  decks = 2, rooms = { "Relay Command Deck", "Sensor Mast Core", "Airlock Vestibule" }, consoles = { { "Outpost Holo-Table", 0, 1, 0.0, -2.5 }, { "Perimeter Turret Link", 5, 2, 0.0, -2.0 } } },
    { id = "bldg_fusion_plant",      decks = 2, rooms = { "Reactor Control Gallery", "Tokamak Containment Ring", "Cooling Turbine Hall" }, consoles = { { "Fusion Core Regulator", 3, 1, 0.0, -2.5 } } },
    { id = "bldg_solar_geothermal",  decks = 2, rooms = { "Mirror Alignment Bay", "Thermal Exchange Vault", "Grid Relay Room" }, consoles = { { "Solar Array Controller", 3, 1, 0.0, -2.0 } } },
    { id = "bldg_ore_refinery",      decks = 2, rooms = { "Smelter Oversight Deck", "Crystal Crusher Bay", "Cargo Conveyor Vestibule" }, consoles = { { "Refinery Dispatch Console", 3, 1, 0.0, -3.0 } } },
    { id = "bldg_fuel_plant",        decks = 2, rooms = { "Cryo-Tank Control Room", "Deuterium Distillation Hall", "Fuel Manifold Bay" }, consoles = { { "Deuterium Synthesis Terminal", 3, 1, 0.0, -2.5 } } },
    { id = "bldg_vehicle_foundry",   decks = 2, rooms = { "Assembly Gantry Floor", "Overhead Crane Control", "Armor Press Bay", "Rollout Apron Gate" }, consoles = { { "Vehicle Foundry Terminal", 3, 1, 0.0, -3.0 } } },
    { id = "bldg_landship_yard",     decks = 3, rooms = { "Colossal Dry-Bay Floor", "Superstructure Gantry", "Main Fabrication Control", "Reactor Crane Deck" }, consoles = { { "Landship Yard Terminal", 3, 1, 0.0, -4.0 } } },
    { id = "bldg_starfighter_hangar",decks = 2, rooms = { "Launch Tunnel Deck", "Flight Control Tower", "Ordnance Prep Bay" }, consoles = { { "Starfighter Launch Terminal", 3, 1, 0.0, -3.0 } } },
    { id = "bldg_dropship_pad",      decks = 2, rooms = { "Landing Pad Apron", "Flight Dispatch Bunker", "Fuel Umbilical Bay" }, consoles = { { "Pad Telemetry Console", 3, 1, 0.0, -2.5 } } },
    { id = "bldg_shield_generator",  decks = 2, rooms = { "Emitter Control Atrium", "Harmonic Capacitor Vault", "Cooling Core" }, consoles = { { "Shield Grid Terminal", 1, 1, 0.0, -2.5 } } },
    { id = "bldg_orbital_cannon",    decks = 3, rooms = { "Fire Control Deck", "Ion Accelerator Barrel Bay", "Capacitor Sub-Vault" }, consoles = { { "Orbital Ion Targeting Gunner", 5, 1, 0.0, -3.0 } } },
    { id = "bldg_plasma_turret",     decks = 2, rooms = { "Turret Gunner Pit", "Plasma Cell Magazine" }, consoles = { { "Plasma Silo Gunner", 5, 1, 0.0, -1.8 } } },
    { id = "bldg_flak_battery",      decks = 2, rooms = { "Radar Fire Control", "SAM Reload Bunker" }, consoles = { { "Anti-Air Battery Gunner", 5, 1, 0.0, -1.8 } } },
    { id = "bldg_spaceport",         decks = 3, rooms = { "Orbital Traffic Control", "Heavy Launch Concourse", "Customs & Cargo Terminal", "Escape Shelter" }, consoles = { { "Spaceport Holo-Globe", 0, 1, 0.0, -4.0 }, { "Orbital Launch Terminal", 3, 1, -4.5, 0.0 }, { "Spaceport Escape Pod", 4, 1, 5.0, 2.0 } } },
    { id = "bldg_elevator_terminal", decks = 4, rooms = { "Tether Anchor Concourse", "Climber Dispatch Deck", "Counterweight Tension Control", "Emergency Pod Vault" }, consoles = { { "Tether Holo-Globe", 0, 1, 0.0, -4.0 }, { "Climber Production Terminal", 3, 1, -4.5, 0.0 }, { "Tether Escape Pod", 4, 1, 5.0, 2.0 } } },
    { id = "bldg_ore_mine",          decks = 2, rooms = { "Headframe Hoist Room", "Deep Extraction Shaft" }, consoles = { { "Excavation Rig Terminal", 3, 1, 0.0, -2.0 } } },
    { id = "bldg_deep_drill",        decks = 2, rooms = { "Mantle Drill Control", "Seismic Core Chamber", "Steam Turbine Bay" }, consoles = { { "Geothermal Drill Terminal", 3, 1, 0.0, -2.5 } } },
    { id = "bldg_comms_tower",       decks = 2, rooms = { "Subspace Relay Hall", "Encryption Spire Deck" }, consoles = { { "Comms Relay Terminal", 1, 1, 0.0, -2.0 } } },
    { id = "bldg_radar_array",       decks = 2, rooms = { "Deep-Space Sensor CIC", "Parabolic Drive Vault" }, consoles = { { "Radar Sweep Console", 1, 1, 0.0, -2.5 } } },
    { id = "bldg_barracks",          decks = 2, rooms = { "Marine Ready Room", "Armory & Mag-Boot Lockers", "Briefing Hall" }, consoles = { { "Marine Deployment Terminal", 3, 1, 0.0, -2.5 } } },
    { id = "bldg_academy_medbay",    decks = 2, rooms = { "Tactical Lecture Atrium", "Auto-Doc Trauma Ward", "Officer Command Sim" }, consoles = { { "Officer Training Terminal", 3, 1, 0.0, -2.5 } } },
    { id = "bldg_coastal_dock",      decks = 2, rooms = { "Harbor Master Control", "Wet Slipway Apron", "Naval Supply Wharf" }, consoles = { { "Coastal Shipyard Terminal", 3, 1, 0.0, -3.5 } } },
    { id = "bldg_naval_drydock",     decks = 3, rooms = { "Drydock Command Gantry", "Keel Assembly Basin", "Floodgate Control Room" }, consoles = { { "Capital Naval Drydock Terminal", 3, 1, 0.0, -4.0 } } },

    -- Class 1: 10 Ground Vehicles
    { id = "veh_plasma_tank",        decks = 1, rooms = { "Driver & Gunner Fighting Compartment", "Rear Plasma Core" }, consoles = { { "Tank Pilot Helm", 2, 1, 0.0, -1.6 }, { "Main Plasma Turret", 5, 1, 0.0, 0.4 } } },
    { id = "veh_siege_hovertank",    decks = 1, rooms = { "Hover Flight Deck", "Siege Capacitor Bay" }, consoles = { { "Hovertank Pilot Seat", 2, 1, 0.0, -1.8 }, { "Siege Cannon Gunner", 5, 1, 0.0, 0.5 } } },
    { id = "veh_recon_skimmer",      decks = 1, rooms = { "Twin-Seat Recon Cockpit" }, consoles = { { "Skimmer Pilot Seat", 2, 1, 0.0, -1.0 } } },
    { id = "veh_scout_rover",        decks = 1, rooms = { "Armored Sensor Cabin" }, consoles = { { "Rover Driver Console", 2, 1, 0.0, -1.2 } } },
    { id = "veh_apc",                decks = 1, rooms = { "Driver Cockpit", "Armored Marine Troop Bay" }, consoles = { { "APC Driver Helm", 2, 1, 0.0, -1.8 } } },
    { id = "veh_assault_transport",  decks = 2, rooms = { "Armored Cab", "Shielded Platoon Hold", "Roof Turret Ring" }, consoles = { { "Assault Transport Helm", 2, 1, 0.0, -2.4 }, { "Roof Plasma Gunner", 5, 2, 0.0, 0.0 } } },
    { id = "veh_cargo_truck",        decks = 1, rooms = { "Convoy Cab", "Ore Hopper Control Catwalk" }, consoles = { { "Convoy Driver Seat", 2, 1, 0.0, -2.0 } } },
    { id = "veh_repair_rig",         decks = 1, rooms = { "Engineering Cab", "Nanite Weld-Arm Control Bay" }, consoles = { { "Repair Rig Driver Seat", 2, 1, 0.0, -1.8 }, { "Field Fabrication Console", 3, 1, 0.0, 1.2 } } },
    { id = "veh_shield_crawler",     decks = 2, rooms = { "Crawler Bridge", "Shield Dome Core", "SAM Battery Deck" }, consoles = { { "Crawler Helm", 2, 1, 0.0, -2.4 }, { "SAM Gunner Station", 5, 2, 0.0, 0.8 } } },
    { id = "veh_command_landship",   decks = 3, rooms = { "Landship War Room", "Forward Bridge", "Internal Vehicle Bay", "Reactor Deck", "Escape Pod Hatch" }, consoles = { { "Landship Holo-Globe", 0, 1, 0.0, -3.5 }, { "Landship Helm", 1, 2, 0.0, -5.0 }, { "Landship Escape Pod", 4, 1, 4.5, 2.0 } } },

    -- Class 2: 10 Naval Ships
    { id = "ship_missile_boat",      decks = 1, rooms = { "Fast Attack Bridge", "VLS Missile Deck", "Turbine Room" }, consoles = { { "Missile Boat Helm", 1, 1, 0.0, -2.5 }, { "Missile Fire Control", 5, 1, 0.0, 0.8 } } },
    { id = "ship_hydrofoil",         decks = 1, rooms = { "Hydrofoil Cockpit", "Aft Foil Engine Bay" }, consoles = { { "Hydrofoil Helm", 2, 1, 0.0, -2.0 } } },
    { id = "ship_plasma_destroyer",  decks = 3, rooms = { "Destroyer Bridge", "Combat Information Center", "Main Plasma Magazine", "Engineering Deck", "Escape Raft Bay" }, consoles = { { "Destroyer Helm", 1, 2, 0.0, -3.5 }, { "Plasma Battery Control", 5, 1, 0.0, 0.5 }, { "Escape Pod Hatch", 4, 1, 3.5, 2.5 } } },
    { id = "ship_aegis_frigate",     decks = 3, rooms = { "Aegis Bridge", "Phased Array CIC", "Shield Core", "Escape Pod Bay" }, consoles = { { "Frigate Helm", 1, 2, 0.0, -3.5 }, { "Aegis Interceptor Gunner", 5, 1, 0.0, 0.6 }, { "Escape Pod Hatch", 4, 1, 3.0, 2.2 } } },
    { id = "ship_plasma_cruiser",    decks = 3, rooms = { "Cruiser Bridge", "Fleet Tactical War Room", "Heavy Plasma Deck", "Engineering Core", "Escape Pod Bay" }, consoles = { { "Cruiser Holo-Globe", 0, 1, 0.0, -3.0 }, { "Cruiser Helm", 1, 2, 0.0, -4.5 }, { "Cruiser Escape Pod", 4, 1, 4.0, 2.5 } } },
    { id = "ship_fleet_carrier",     decks = 4, rooms = { "Island Flight Bridge", "Fleet War Room CIC", "Main Starfighter Hangar", "Catapult Launch Control", "Fusion Engine Deck", "Escape Pod Bay" }, consoles = { { "Carrier Holo-Globe", 0, 2, 0.0, -3.5 }, { "Carrier Helm", 1, 3, 0.0, -5.0 }, { "Flight Deck Launch Terminal", 3, 1, -4.5, 0.0 }, { "Carrier Escape Pod", 4, 1, 5.0, 3.0 } } },
    { id = "ship_amphibious_lhd",    decks = 3, rooms = { "Amphibious Bridge", "Landing Ops War Room", "Floodable Well Deck", "Marine Briefing Bay", "Escape Pod Bay" }, consoles = { { "LHD Holo-Globe", 0, 2, 0.0, -3.5 }, { "LHD Helm", 1, 3, 0.0, -4.5 }, { "Well Deck Launch Terminal", 3, 1, -4.0, 0.5 }, { "LHD Escape Pod", 4, 1, 4.5, 2.5 } } },
    { id = "ship_attack_sub",        decks = 2, rooms = { "Conning Control Room", "Sonar & Torpedo Room", "Quiet Fusion Core", "Escape Buoy Trunk" }, consoles = { { "Submarine Dive Helm", 1, 1, 0.0, -3.0 }, { "Torpedo Fire Control", 5, 1, 0.0, 1.0 }, { "Escape Buoy Trunk", 4, 2, 2.2, 1.5 } } },
    { id = "ship_missile_sub",       decks = 2, rooms = { "Strategic Control Room", "Orbital Missile Silo Bay", "Reactor Compartment", "Escape Buoy Trunk" }, consoles = { { "Ballistic Sub Helm", 1, 1, 0.0, -3.2 }, { "Orbital Strike Launch Console", 5, 1, 0.0, 1.2 }, { "Escape Buoy Trunk", 4, 2, 2.5, 1.8 } } },
    { id = "ship_relay_tender",      decks = 2, rooms = { "Tender Bridge", "Logistics Repair Hold", "Subspace Mast Room", "Escape Pod Bay" }, consoles = { { "Tender Helm", 1, 2, 0.0, -3.0 }, { "At-Sea Resupply Terminal", 3, 1, 0.0, 1.0 }, { "Tender Escape Pod", 4, 1, 3.2, 2.0 } } },

    -- Class 3: 8 Aircraft
    { id = "air_starfighter",        decks = 1, rooms = { "Pressurized Interceptor Cockpit" }, consoles = { { "Starfighter Pilot Seat", 2, 1, 0.0, -1.2 } } },
    { id = "air_recon_fighter",      decks = 1, rooms = { "Tandem Stealth Sensor Cockpit" }, consoles = { { "Recon Pilot Seat", 2, 1, 0.0, -1.4 } } },
    { id = "air_plasma_bomber",      decks = 1, rooms = { "Bomber Flight Deck", "Plasma Ordnance Bay" }, consoles = { { "Bomber Pilot Seat", 2, 1, 0.0, -1.8 }, { "Bombardier Gunner", 5, 1, 0.0, 0.8 } } },
    { id = "air_dropship",           decks = 1, rooms = { "Dropship Flight Cockpit", "Armored Troop & Cargo Hold", "Rear Boarding Ramp" }, consoles = { { "Dropship Pilot Seat", 2, 1, 0.0, -2.5 }, { "Dropship Escape Pod", 4, 1, 2.2, 1.5 } } },
    { id = "air_heavy_lifter",       decks = 2, rooms = { "Elevated Flight Deck", "Vehicle Clamp Cargo Bay", "VTOL Engine Gallery" }, consoles = { { "Heavy Lifter Pilot Seat", 2, 2, 0.0, -3.0 }, { "Lifter Escape Pod", 4, 1, 3.0, 1.8 } } },
    { id = "air_plasma_gunship",     decks = 1, rooms = { "Gunship Flight Cockpit", "Port & Starboard Plasma Fire Deck", "Ammo Magazine" }, consoles = { { "Gunship Pilot Seat", 2, 1, 0.0, -2.4 }, { "Broadside Plasma Gunner", 5, 1, 0.0, 0.8 } } },
    { id = "air_awacs_cruiser",      decks = 2, rooms = { "AWACS Flight Deck", "Airborne Holo-Radar CIC", "Radome Core", "Escape Pod Hatch" }, consoles = { { "AWACS Holo-Globe", 0, 1, 0.0, -2.5 }, { "AWACS Pilot Helm", 2, 2, 0.0, -4.0 }, { "AWACS Escape Pod", 4, 1, 3.5, 2.0 } } },
    { id = "air_command_carrier",    decks = 3, rooms = { "Helicarrier Sky-Bridge", "Airborne War Room", "Internal Fighter Hangar", "Lift-Fan Turbine Deck", "Escape Pod Bay" }, consoles = { { "Sky-Carrier Holo-Globe", 0, 2, 0.0, -3.5 }, { "Sky-Carrier Helm", 1, 3, 0.0, -5.0 }, { "Airborne Hangar Terminal", 3, 1, -4.5, 0.5 }, { "Sky-Carrier Escape Pod", 4, 1, 5.0, 3.0 } } },

    -- Class 4: 14 Spacecraft
    { id = "space_shuttle",          decks = 1, rooms = { "Orbital Courier Cockpit", "Passenger Airlock Cabin", "Ion Thruster Bay" }, consoles = { { "Shuttle Pilot Seat", 2, 1, 0.0, -2.2 }, { "Shuttle Escape Pod", 4, 1, 2.0, 1.6 } } },
    { id = "space_boarding_pod",     decks = 1, rooms = { "Breaching Pilot Pod", "Marine Mag-Boot Assault Bay" }, consoles = { { "Breaching Pilot Seat", 2, 1, 0.0, -2.0 } } },
    { id = "space_cargo_lifter",     decks = 2, rooms = { "Tug Command Cab", "Magnetic Container Frame Bay", "Fusion Drive Deck" }, consoles = { { "Cargo Tug Pilot Seat", 2, 2, 0.0, -2.8 }, { "Cargo Tug Escape Pod", 4, 1, 2.8, 1.8 } } },
    { id = "space_mining_barge",     decks = 2, rooms = { "Mining Bridge", "Laser Ore Processing Bay", "Refinery Hopper", "Escape Pod Airlock" }, consoles = { { "Mining Barge Helm", 1, 2, 0.0, -3.2 }, { "Ore Laser Console", 3, 1, 0.0, 1.0 }, { "Barge Escape Pod", 4, 1, 3.2, 2.0 } } },
    { id = "space_escape_pod",       decks = 1, rooms = { "Emergency Life-Support Capsule" }, consoles = { { "Escape Pod Re-Entry Seat", 2, 1, 0.0, -0.8 } } },
    { id = "space_corvette",         decks = 2, rooms = { "Corvette Bridge", "Sensor & Weapons Deck", "Reactor Engineering", "Escape Pod Airlock" }, consoles = { { "Corvette Helm", 1, 2, 0.0, -3.0 }, { "Corvette Turret Control", 5, 1, 0.0, 0.6 }, { "Corvette Escape Pod", 4, 1, 3.0, 2.0 } } },
    { id = "space_stealth_corvette", decks = 2, rooms = { "Stealth CIC Bridge", "Thermal Sink Core", "Recon Probe Bay", "Escape Pod Hatch" }, consoles = { { "Stealth Corvette Helm", 1, 2, 0.0, -2.8 }, { "Stealth Escape Pod", 4, 1, 2.6, 1.8 } } },
    { id = "space_frigate",          decks = 3, rooms = { "Frigate Bridge", "Orbital CIC", "Broadside Battery Deck", "Fusion Drive Core", "Escape Pod Bay" }, consoles = { { "Frigate Helm", 1, 2, 0.0, -3.6 }, { "Frigate Weapons Console", 5, 1, 0.0, 0.8 }, { "Frigate Escape Pod", 4, 1, 3.6, 2.4 } } },
    { id = "space_destroyer_escort", decks = 3, rooms = { "Destroyer-Escort Bridge", "Point-Defense CIC", "Plasma Lance Deck", "Reactor Deck", "Escape Pod Bay" }, consoles = { { "Destroyer-Escort Helm", 1, 2, 0.0, -4.0 }, { "Plasma Lance Gunner", 5, 1, 0.0, 1.0 }, { "Destroyer Escape Pod", 4, 1, 4.0, 2.6 } } },
    { id = "space_heavy_cruiser",    decks = 3, rooms = { "Heavy Cruiser Bridge", "Fleet Tactical Holo-CIC", "Twin-Spine Lance Deck", "Fusion Core", "Escape Pod Bay" }, consoles = { { "Cruiser Holo-Globe", 0, 2, 0.0, -3.5 }, { "Heavy Cruiser Helm", 1, 3, 0.0, -4.8 }, { "Heavy Cruiser Escape Pod", 4, 1, 4.5, 2.8 } } },
    { id = "space_battlecruiser",    decks = 4, rooms = { "Battlecruiser Bridge", "Command War Room", "Axial Beam Core Deck", "Marine Garrison", "Reactor Deck", "Escape Pod Bay" }, consoles = { { "Battlecruiser Holo-Globe", 0, 2, 0.0, -3.8 }, { "Battlecruiser Helm", 1, 3, 0.0, -5.2 }, { "Axial Beam Gunner", 5, 1, 0.0, 1.2 }, { "Battlecruiser Escape Pod", 4, 1, 4.8, 3.0 } } },
    { id = "space_siege_dreadnought",decks = 4, rooms = { "Siege Citadel Bridge", "Planetary Bombardment CIC", "Main Siege Cannon Spine", "Heavy Reactor Deck", "Escape Pod Bay" }, consoles = { { "Siege Holo-Globe", 0, 2, 0.0, -4.0 }, { "Siege Dreadnought Helm", 1, 3, 0.0, -5.5 }, { "Siege Cannon Gunner", 5, 1, 0.0, 1.5 }, { "Siege Escape Pod", 4, 1, 5.2, 3.2 } } },
    { id = "space_capital_carrier",  decks = 4, rooms = { "Capital Flight Bridge", "Fleet Carrier Holo-CIC", "Dual Starfighter Hangar", "Launch Tube Control", "Reactor Deck", "Escape Pod Bay" }, consoles = { { "Capital Carrier Holo-Globe", 0, 2, 0.0, -4.0 }, { "Capital Carrier Helm", 1, 3, 0.0, -5.5 }, { "Orbital Squadron Terminal", 3, 1, -4.5, 0.5 }, { "Carrier Escape Pod", 4, 1, 5.2, 3.2 } } },
    { id = "space_command_dreadnought", decks = 4, rooms = { "Flagship Bridge & CIC", "Admiral War Room", "Axial Lance Battery", "Dropship Hangar Bay", "Fusion Reactor Core", "Escape Pod Bay" }, consoles = { { "Flagship Holo-Globe", 0, 2, 0.0, -4.2 }, { "Flagship Helm", 1, 3, 0.0, -5.8 }, { "Flagship Escape Pod", 4, 1, 5.5, 3.4 } } },

    -- Class 5: 10 Space Stations
    { id = "stn_recon_sat",          decks = 1, rooms = { "Zero-G Optical Sensor Core", "Maintenance Airlock" }, consoles = { { "Recon Optics Terminal", 1, 1, 0.0, -1.6 } } },
    { id = "stn_comms_relay",        decks = 1, rooms = { "Quantum Relay Core", "Docking Collar" }, consoles = { { "Subspace Relay Terminal", 1, 1, 0.0, -1.8 } } },
    { id = "stn_defense_platform",   decks = 3, rooms = { "Orbital Bastion CIC", "Radial Plasma Battery Deck", "Reactor Core", "Escape Pod Ring" }, consoles = { { "Bastion Fire Control", 5, 2, 0.0, -3.0 }, { "Bastion Escape Pod", 4, 1, 3.8, 2.4 } } },
    { id = "stn_solar_array",        decks = 2, rooms = { "Solar Array Control Hub", "Microwave Beam Transmitter", "Escape Pod Airlock" }, consoles = { { "Orbital Power Terminal", 3, 1, 0.0, -3.0 }, { "Solar Station Escape Pod", 4, 1, 3.5, 2.2 } } },
    { id = "stn_fuel_ore_depot",     decks = 3, rooms = { "Depot Logistics Control", "Cryo-Fuel Silo Gallery", "Cargo Docking Ring", "Escape Pod Bay" }, consoles = { { "Orbital Depot Terminal", 3, 1, 0.0, -3.5 }, { "Depot Escape Pod", 4, 1, 4.0, 2.5 } } },
    { id = "stn_asteroid_refinery",  decks = 3, rooms = { "Refinery Oversight Bridge", "Zero-G Ore Smelter Core", "Slag Ejection Bay", "Escape Pod Bay" }, consoles = { { "Asteroid Refinery Terminal", 3, 1, 0.0, -3.8 }, { "Refinery Escape Pod", 4, 1, 4.2, 2.6 } } },
    { id = "stn_repair_drydock",     decks = 3, rooms = { "Drydock Command CIC", "Nano-Repair Gantry Control", "Docking Slipway", "Escape Pod Bay" }, consoles = { { "Repair Drydock Holo-Globe", 0, 2, 0.0, -3.8 }, { "Fleet Repair Terminal", 3, 1, -4.0, 0.5 }, { "Repair Dock Escape Pod", 4, 1, 4.5, 2.8 } } },
    { id = "stn_orbital_shipyard",   decks = 4, rooms = { "Shipyard Command Atrium", "Capital Keel Gantry Control", "Dropship Docking Airlock", "Fusion Foundry Core", "Escape Pod Ring" }, consoles = { { "Shipyard Holo-Globe", 0, 2, 0.0, -4.5 }, { "Capital Shipyard Terminal", 3, 1, -5.0, 0.5 }, { "Shipyard Escape Pod", 4, 1, 5.5, 3.2 } } },
    { id = "stn_habitat_ring",       decks = 3, rooms = { "Centrifugal Concourse", "Command & Training Atrium", "Zero-G Hub Airlock", "Hydroponics Deck", "Escape Pod Ring" }, consoles = { { "Habitat Holo-Globe", 0, 2, 0.0, -4.2 }, { "Fleet Academy Terminal", 3, 1, -4.5, 0.5 }, { "Habitat Escape Pod", 4, 1, 5.0, 3.0 } } },
    { id = "stn_deep_space_citadel", decks = 4, rooms = { "Grand Admiral War Room", "Orbital Sector CIC", "Capital Docking Concourse", "Reactor Sanctum", "Escape Pod Vault" }, consoles = { { "Deep-Space Holo-Globe", 0, 2, 0.0, -4.8 }, { "Citadel Production Terminal", 3, 1, -5.2, 0.5 }, { "Deep-Space Escape Pod", 4, 1, 5.8, 3.4 } } },
}

local BY_ID = {}
for i = 1, #SPECS do
    BY_ID[SPECS[i].id] = SPECS[i]
end

function M.count()
    return #SPECS
end

function M.get_spec(bp_id)
    return BY_ID[bp_id]
end

function M.build_layout(bp)
    local spec = BY_ID[bp.id]
    if not spec then
        return nil
    end
    local half_w = math.max(3.0, math.min(18.0, (bp.width or 12) * 0.25))
    local room_len = math.max(4.5, math.min(14.0, (bp.length or 16) * 0.22))
    local deck_h = 3.8
    local rooms = {}
    for i = 1, #spec.rooms do
        local deck_idx = ((i - 1) % spec.decks)
        local row_idx = math.floor((i - 1) / spec.decks)
        local y0 = deck_idx * deck_h
        local z0 = -room_len * 0.5 - row_idx * room_len
        rooms[i] = {
            name = spec.rooms[i],
            min_x = -half_w, min_y = y0, min_z = z0 - room_len * 0.5,
            max_x =  half_w, max_y = y0 + deck_h, max_z = z0 + room_len * 0.5,
            pressurized = true,
        }
    end
    local consoles = {}
    for i = 1, #spec.consoles do
        local c = spec.consoles[i]
        local deck_idx = math.max(0, (c[3] or 1) - 1)
        consoles[i] = {
            name = c[1],
            ctype = c[2],
            x = c[4],
            y = deck_idx * deck_h + 1.1,
            z = c[5],
            radius = 3.8,
        }
    end
    return {
        id = bp.id,
        decks = spec.decks,
        rooms = rooms,
        consoles = consoles,
        exit_box = { -2.4, 0.0, room_len * 0.5 - 1.2, 2.4, 3.2, room_len * 0.5 + 1.2 },
    }
end

function M.hydrate_entity(entity_id, bp)
    local ent = engine.world.get_entity(entity_id)
    if not ent or ent.own_frame_id == 0 then
        return 0, 0, 0
    end
    local layout = M.build_layout(bp)
    if not layout then
        return 0, 0, 0
    end
    local fid = ent.own_frame_id
    local r_ok, c_ok, p_ok = 0, 0, 0
    for i = 1, #layout.rooms do
        local r = layout.rooms[i]
        if engine.interior.add_room(fid, r.name, r.min_x, r.min_y, r.min_z, r.max_x, r.max_y, r.max_z, r.pressurized) > 0 then
            r_ok = r_ok + 1
        end
    end
    for i = 1, #layout.consoles do
        local c = layout.consoles[i]
        if engine.interior.add_console(fid, c.name, c.ctype, c.x, c.y, c.z, c.radius, entity_id) > 0 then
            c_ok = c_ok + 1
        end
    end
    local eb = layout.exit_box
    local dst_x = ent.x + math.max(10.0, (bp.width or 12) * 0.35)
    local dst_y = ent.y + 1.7
    local dst_z = ent.z + math.max(10.0, (bp.length or 16) * 0.35)
    if engine.interior.add_portal(fid, 1, "Exterior Airlock Ramp", eb[1], eb[2], eb[3], eb[4], eb[5], eb[6], dst_x, dst_y, dst_z) > 0 then
        p_ok = p_ok + 1
    end
    return r_ok, c_ok, p_ok
end

return M
