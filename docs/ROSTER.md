# 1stPRTS (`1prtsg`) — Complete Doubled Unit, Building & Personnel Roster

> **Total Roster:** **76 Enterable / Operational Constructs** (24 Buildings · 10 Ground Vehicles · 10 Naval Ships · 8 Aircraft · 14 Spacecraft · 10 Space Stations) + **12 Personnel & Crew Roles**  
> **Data Source:** Every construct and role below is defined in editable external Lua 5.4 scripts under [`scripts/units/`](../scripts/units/) and [`scripts/crew/`](../scripts/crew/) — zero hardcoded units exist in the C11 core engine.  
> **Scale Rule:** 1 world unit = 1 meter (`1:1` human scale; Commander eye height ≈ `1.7m`). Every construct is manned and enterable.

---

## 1. Buildings — Planetary Structures (24)

| # | ID | Name | Approx. Scale (L×W×H) | Crew | Role | Key Enterable Spaces |
| :- | :--- | :--- | :--- | :- | :--- | :--- |
| 1 | `bldg_command_citadel` | **Planetary Command Citadel** | 120×120×55m | 48 | Main planetary HQ & strategic hub | War Room (3D Planetary Holo-Globe), Comms Hub, Panic Citadel, Observation Gantry, Escape Pod Bay |
| 2 | `bldg_tactical_outpost` | **Tactical Relay Outpost** | 38×38×22m | 14 | Forward sector command & local holo-table | Tactical Map Room, Armored Bunker Deck, Roof Observation Post |
| 3 | `bldg_fusion_plant` | **Fusion Power Plant** | 64×48×36m | 16 | Primary high-output plasma power generation | Fusion Reactor Core, Cooling Catwalks, Power Routing Console |
| 4 | `bldg_solar_geothermal` | **Geothermal / Solar Collector** | 44×44×28m | 6 | Low-signature auxiliary power generation | Turbine Shaft, Capacitor Control Room |
| 5 | `bldg_ore_refinery` | **Ore & Crystal Refinery** | 85×55×34m | 22 | Processes raw planetary ore/crystal into alloy | Smelting Floor, Conveyor Catwalks, Logistics Terminal |
| 6 | `bldg_fuel_plant` | **Deuterium Fuel Plant** | 72×48×30m | 14 | Refines fusion propellant for ships & spacecraft | Pressurized Tank Bays, Pump Control Room, Loading Dock |
| 7 | `bldg_vehicle_foundry` | **Vehicle Foundry** | 110×70×32m | 28 | Constructs light & heavy ground vehicles at 1:1 | Cavernous Assembly Hall w/ Gantry Cranes, Foreman CRT Office |
| 8 | `bldg_landship_yard` | **Heavy Landship Assembly Yard** | 160×110×48m | 42 | Builds colossal Mobile Command Landships | Open/Roofed Heavy Gantry Bay, Overhead Crane Bridge, Shield Test Pit |
| 9 | `bldg_starfighter_hangar` | **Starfighter Hangar** | 95×65×26m | 24 | Houses, launches & rearms atmospheric/orbital fighters | Main Flight Hangar, Air Traffic Control (ATC) Tower, Ordnance Vault |
| 10 | `bldg_dropship_pad` | **Heavy Dropship Landing Pad** | 80×80×18m | 16 | Rapid VTOL dropship staging & troop embarkation | Sub-Pad Crew Ready Room, Pad Control Booth, Blast Elevator |
| 11 | `bldg_shield_generator` | **Planetary Shield Generator** | 52×52×44m | 14 | Projects shimmering retro energy dome over a base | Shield Capacitor Core, Emitter Catwalk, Cooling Control Room |
| 12 | `bldg_orbital_cannon` | **Planetary Ion / Orbital Cannon** | 68×68×95m | 26 | Surface-to-orbit anti-capital beam defense | Capacitor Hall, Elevation Turret Room, Targeting Bridge |
| 13 | `bldg_plasma_turret` | **Plasma Turret Silo** | 22×22×20m | 4 | Static heavy anti-armor surface defense | Underground Loader Room, Manual Plasma Gunner Seat |
| 14 | `bldg_flak_battery` | **Flak & Missile Anti-Air Battery** | 26×26×18m | 6 | Rapid point-defense against fighters & dropships | Radar Tracking Room, Magazine Deck, Gunner Cupola |
| 15 | `bldg_spaceport` | **Heavy Orbital Spaceport** | 220×180×42m | 60 | Heavy surface-to-orbit launch & cargo hub | Multi-Pad Launch Apron, High ATC Tower, Customs/Cargo Transfer Bay |
| 16 | `bldg_elevator_terminal` | **Orbital Tether Base Terminal** | 180×180×140m | 55 | High-throughput surface-to-orbit climber station | Climber Embarkation Hall, Tension Control Room, Security Concourse |
| 17 | `bldg_ore_mine` | **Planetary Ore / Crystal Mine** | 46×38×24m | 12 | Extracts surface ore & crystal deposits | Drill Control Room, Ore Hopper Loading Bay |
| 18 | `bldg_deep_drill` | **Deep Crust Geothermal Drill Rig** | 62×62×54m | 20 | High-yield deep planetary extraction | Multi-Level Rig Catwalks, Core Extraction Chamber, Derrick Booth |
| 19 | `bldg_comms_tower` | **Planetary Comms Tower** | 24×24×85m | 8 | Links ground forces to orbital relay satellites | Signal Array Control Room, Tall Elevator Shaft, Roof Dish Deck |
| 20 | `bldg_radar_array` | **Deep-Space Sensor & Radar Array** | 48×48×52m | 12 | Long-range planetary & low-orbit target tracking | Sensor Operations Room, Dome Maintenance Catwalk |
| 21 | `bldg_barracks` | **Marine & Crew Barracks** | 60×42×18m | 30 | Recruits & houses infantry, pilots & technicians | Bunks, Briefing Hall, Armory, Staging Bay |
| 22 | `bldg_academy_medbay` | **Officer Academy & Medbay Complex** | 75×50×24m | 24 | Trains specialist astrogators/officers & treats casualties | Surgical Medbay, Holographic Flight Sim Hall, Command Briefing Room |
| 23 | `bldg_coastal_dock` | **Coastal Naval Dock** | 140×80×22m | 26 | Refits, supplies & stages coastal naval vessels | Pier Control Booth, Amphibious Ramp, Torpedo/Fuel Wharf |
| 24 | `bldg_naval_drydock` | **Armored Naval Drydock Bunker** | 380×120×55m | 54 | Constructs 340m Fleet Carriers & Destroyers at 1:1 | Floodable Graving Dock, Gantry Walkways, Shipyard Control Bridge |

---

## 2. Ground Vehicles (10)

| # | ID | Name | Approx. Scale (L×W×H) | Crew | Role | Key Enterable Spaces |
| :- | :--- | :--- | :--- | :- | :--- | :--- |
| 1 | `veh_plasma_tank` | **Plasma Heavy Tank** | 11×5×4m | 3 | Frontline armored plasma combatant | Armored Driver Hull, Plasma Gunner Seat, Commander Cupola |
| 2 | `veh_siege_hovertank` | **Hover Siege Plasma Tank** | 15×7×5m | 4 | Long-range shielded repulsor artillery | Pressurized Crew Compartment, Siege Cannon Control, Commander Hatch |
| 3 | `veh_recon_skimmer` | **Recon Skimmer / Jeep** | 5.5×2.6×2.2m | 2 | High-speed planetary scouting & escort | Open/Canopy Cockpit, Rear Utility & Sensor Mount |
| 4 | `veh_scout_rover` | **Armored Scout Rover** | 7×3.4×3m | 3 | All-terrain sensor & laser-designator platform | Enclosed Crew Cab, Roof Optics & Comms Mast |
| 5 | `veh_apc` | **Armored Personnel Carrier (APC)** | 9.5×4×3.4m | 2+8 | Marine squad transport | Driver/Turret Cabin, Walkable Troop Bay w/ Lowering Rear Ramp |
| 6 | `veh_assault_transport` | **Shielded Heavy Assault Transport** | 16×6.5×4.8m | 3+16 | Frontline breaching & heavy exo-trooper carrier | Front Assault Ramp, Double-Row Troop Bay, Shield Capacitor Deck |
| 7 | `veh_cargo_truck` | **Surface Ore / Cargo Convoy Truck** | 14×4.5×4.6m | 2 | Physical resource hauling between mines & refineries | Elevated Driving Cab, Armored Ore Hopper / Cargo Bed |
| 8 | `veh_repair_rig` | **Mobile Engineering & Repair Rig** | 13×5×4.5m | 4 | Field hull welding, turret construction & salvage | Cab, Rear Workshop Bay, Crane Operator Seat |
| 9 | `veh_shield_crawler` | **Mobile Shield & SAM Crawler** | 18×8×6.5m | 6 | Mobile energy dome & anti-air umbrella for columns | Generator Core Bay, Radar/SAM Control Cabin |
| 10 | `veh_command_landship` | **Mobile Command Landship** | 70×28×20m | 30 | Multi-deck rolling fortress & mobile planetary HQ | Command Bridge, War Room Mini-Holo-Globe, Rear Vehicle Mini-Hangar, Shield & Reactor Deck |

---

## 3. Naval Ships (10)

| # | ID | Name | Approx. Scale (L×W×H) | Crew | Role | Key Enterable Spaces |
| :- | :--- | :--- | :--- | :- | :--- | :--- |
| 1 | `ship_missile_boat` | **Fast Missile Boat** | 34×9×10m | 6 | Agile littoral plasma-torpedo raider | Pilothouse, Missile Fire-Control Room, Engine Compartment |
| 2 | `ship_hydrofoil` | **Hydrofoil Interceptor** | 28×10×8m | 4 | High-speed naval recon & submarine hunter | Forward Cockpit Bridge, Aft Sonar & Depth-Charge Bay |
| 3 | `ship_plasma_destroyer` | **Plasma Destroyer** | 145×18×28m | 34 | Medium multi-role naval escort warship | Armored Bridge, CIC, Plasma Battery Control, Reactor Room, Crew Mess |
| 4 | `ship_aegis_frigate` | **Aegis Shield Frigate** | 125×16×26m | 28 | Fleet energy-shield & anti-air naval escort | Bridge, Shield Capacitor Deck, Flak/Missile Ops Room, Aft Helipad |
| 5 | `ship_plasma_cruiser` | **Heavy Plasma Cruiser** | 210×26×36m | 58 | Heavy naval surface & coastal bombardment | Twin-Level Bridge, CIC, Heavy Beam/Plasma Turret Barbette, Engineering |
| 6 | `ship_fleet_carrier` | **Fleet Starfighter Carrier** | 340×78×58m | 120 | Mobile ocean airfield & naval flagship | Full Flight Deck, Island ATC Tower, Sub-Deck Hangar & Aircraft Lifts, CIC 3D Holo-Globe, Reactor, Quarters |
| 7 | `ship_amphibious_lhd` | **Amphibious Assault Landing Ship** | 240×38×42m | 72 | Deploys tanks, APCs & dropships onto beaches | Floodable Well Deck, Vehicle Hangar, Flight Deck, Bridge, Marine Staging |
| 8 | `ship_attack_sub` | **Attack Submarine** | 110×12×18m | 26 | Stealth anti-ship & anti-sub hunter | Control Room w/ Periscope/Sonar, Torpedo Bay, Fusion Reactor, Crew Bunks |
| 9 | `ship_missile_sub` | **Orbital-Strike Ballistic Submarine** | 165×15×22m | 40 | Submerged heavy strategic & anti-orbit missile sub | Control Room, Towering Vertical Missile Silo Corridor, Reactor Deck |
| 10 | `ship_relay_tender` | **Naval Relay & Logistics Tender** | 150×24×34m | 32 | Maritime fuel/ammo resupply & oceanic comms relay | Bridge, Huge Comms Array Room, Fuel/Ordnance Transfer Deck |

---

## 4. Aircraft (8)

| # | ID | Name | Approx. Scale (L×W×H) | Crew | Role | Key Enterable Spaces |
| :- | :--- | :--- | :--- | :- | :--- | :--- |
| 1 | `air_starfighter` | **Angular Starfighter / Interceptor** | 15×12×4.2m | 1 | High-speed atmospheric & trans-atmospheric interceptor | Enterable Fighter Cockpit (Boarding Ladder & Canopy) |
| 2 | `air_recon_fighter` | **Stealth Recon Fighter** | 17×14×4m | 2 | High-altitude planetary recon & laser-designation craft | Tandem Pilot + Sensor Officer Cockpit |
| 3 | `air_plasma_bomber` | **Plasma Strike Bomber** | 24×26×6.5m | 3 | Heavy anti-armor & anti-ship plasma torpedo striker | Cockpit, Bombardier Optics Station, Internal Bomb Bay Catwalk |
| 4 | `air_dropship` | **Troop / Cargo Dropship** | 28×18×8.5m | 2+14 | Walkable squad transport & shielded surface-to-orbit craft | Flight Cockpit, Walkable Troop/Cargo Bay w/ Lowering Rear Ramp, Emergency Pod |
| 5 | `air_heavy_lifter` | **Heavy Vehicle Lifter Dropship** | 44×32×13m | 4 | Airlifts Plasma Tanks & APCs across continents or orbit | Elevated Bridge Cockpit, Cavernous Drive-Through Vehicle Bay |
| 6 | `air_plasma_gunship` | **Heavy Plasma Gunship** | 32×24×9m | 6 | Loitering armored close-air support platform | Cockpit, Walkable Waist-Turret & Munitions Deck |
| 7 | `air_awacs_cruiser` | **Airborne AWACS / Comms Cruiser** | 58×54×14m | 14 | Long-range airborne radar & tactical comms relay | Flight Deck, Sensor & Comms Operations Cabin, Crew Rest Bay |
| 8 | `air_command_carrier` | **Airborne Command Carrier** | 165×110×34m | 48 | Flying repulsor fortress & mobile atmospheric hangar | Upper Command Bridge, CIC Holo-Globe, Internal Fighter Hangar, Shield & Reactor Core |

---

## 5. Spacecraft (14)

| # | ID | Name | Approx. Scale (L×W×H) | Crew | Role | Key Enterable Spaces |
| :- | :--- | :--- | :--- | :- | :--- | :--- |
| 1 | `space_shuttle` | **Orbital Courier Shuttle** | 24×14×7m | 2+8 | Fast Commander & crew transit across surface, ships & stations | Cockpit, Passenger/Tactical Cabin, Docking Airlock & Ramp |
| 2 | `space_boarding_pod` | **Armored Breaching Shuttle** | 20×11×6.5m | 2+12 | Shielded hull-cutting marine boarding craft | Armored Cockpit, Breaching Airlock Nose, Marine Jump Bay |
| 3 | `space_cargo_lifter` | **Orbital Cargo Lifter / Tug** | 58×28×18m | 6 | Hauls ore & fusion fuel between spaceports, asteroids & depots | Pilothouse, Cargo Clamp Control Deck, Airlock Spine |
| 4 | `space_mining_barge` | **Asteroid Mining Barge** | 85×42×26m | 14 | Extracts orbital asteroid ore & refines raw crystal | Bridge, Laser-Drill Control Room, Ore Hold, Engineering |
| 5 | `space_escape_pod` | **Emergency Escape Pod** | 5×3×3m | 1–4 | Physical Hardcore Ironman emergency ejection capsule | Shielded Re-entry Capsule w/ Thruster & Beacon Controls |
| 6 | `space_corvette` | **Shielded Corvette** | 72×24×16m | 16 | Fast shielded orbital skirmisher & fighter screen | Bridge, Airlock, Shield/Engine Deck, Crew Quarters |
| 7 | `space_stealth_corvette` | **Stealth Recon Corvette** | 66×22×14m | 12 | Low-signature orbital scout & electronic warfare craft | Cloaked Sensor Bridge, Jammer Core, Rear Airlock |
| 8 | `space_frigate` | **Orbital Frigate** | 155×42×32m | 42 | Multi-deck orbital patrol & fleet escort warship | Bridge, CIC, Dorsal/Ventral Gun Decks, Docking Airlocks, Reactor |
| 9 | `space_destroyer_escort` | **Plasma Destroyer-Escort** | 210×54×40m | 64 | Heavy anti-frigate plasma & torpedo hunter | Armored Bridge, Torpedo Magazine Deck, Twin Reactor Hall, Escape Pods |
| 10 | `space_heavy_cruiser` | **Orbital Heavy Cruiser** | 320×78×58m | 96 | Line-of-battle warship with shuttle hangar | Bridge, CIC Holo-Globe, Shuttle Bay, Shield Core, Gun Decks, Quarters |
| 11 | `space_battlecruiser` | **Beam Battlecruiser** | 440×105×74m | 135 | Fast capital raider armed with forward beam lances | Command Bridge, CIC, Axial Beam Capacitor Tunnel, Hangar Bay, Escape Pods |
| 12 | `space_siege_dreadnought` | **Orbital Siege Dreadnought** | 560×130×92m | 170 | Planetary bombardment & anti-station super-heavy | High Bridge, Bombardment Targeting Room, Shield Core, Reactor, Marine Barracks |
| 13 | `space_capital_carrier` | **Capital Starfighter Carrier** | 620×165×110m | 210 | Deep-space starfighter & dropship mothership | Flight Control Bridge, Twin Pressurized Hangar Bays, CIC 3D Holo-Globe, Armory, Escape Pods |
| 14 | `space_command_dreadnought` | **Fleet Command Dreadnought** | 680×180×125m | 240 | Ultimate flagship combining fleet CIC, hangars & heavy batteries | Towering Bridge, Grand CIC 3D Holo-Globe, Pressurized Hangars, Reactor & Shield Cores, Gun Decks, Escape Pod Bays, Internal Turbolifts |

---

## 6. Space Stations & Orbital Constructs (10)

| # | ID | Name | Approx. Scale (L×W×H) | Crew | Role | Key Enterable Spaces |
| :- | :--- | :--- | :--- | :- | :--- | :--- |
| 1 | `stn_recon_sat` | **Orbital Recon Satellite** | 28×24×16m | 2 | Sweeps planetary hemispheres for real-time orbital intel | Compact Maintenance Airlock, Optical/Sensor Core |
| 2 | `stn_comms_relay` | **Orbital Comms Relay Satellite** | 42×42×24m | 4 | LEO/GEO global planetary-to-orbital comms backbone | Docking Collar, Signal Core, Backup Console |
| 3 | `stn_defense_platform` | **Orbital Defense Platform** | 140×140×95m | 36 | Heavily shielded plasma & torpedo orbital bastion | Central Command Deck, Shield Core, Turret Control Rings, Docking Airlock |
| 4 | `stn_solar_array` | **Orbital Solar Power Station** | 260×180×40m | 18 | Beamed orbital energy & capacitor recharging station | Hub Control Room, Power Conduit Spine, Docking Collar |
| 5 | `stn_fuel_ore_depot` | **Orbital Fuel & Ore Depot** | 310×180×120m | 44 | Deep-orbit propellant & asteroid-ore logistics hub | Pressurized Docking Bay, Tank Control Room, Cargo Transfer Spine |
| 6 | `stn_asteroid_refinery` | **Asteroid Refinery Station** | 480×320×210m | 72 | Processes mined asteroid chunks directly in orbit | Cavernous Smelting Bay, Zero-G Ore Intake, Control Bridge, Crew Ring |
| 7 | `stn_repair_drydock` | **Fleet Repair Drydock** | 650×240×180m | 88 | Rapid orbital hull/shield repair & rearming slip | Open Repair Cradle, Welding Gantry Catwalks, Dockmaster Bridge |
| 8 | `stn_orbital_shipyard` | **Orbital Shipyard Drydock** | 2,200×680×420m | 260 | Constructs 680m Capital Ships & Dreadnoughts at 1:1 | Pressurized Docking Bays w/ Containment Fields, Internal Monorail Spine, Open 1:1 Drydock Gantries, Rotating 0.8g Habitat Ring + Zero-G Core |
| 9 | `stn_habitat_ring` | **Orbital Habitat & Training Ring** | 1,400×1,400×220m | 220 | Houses orbital manpower, marine divisions & medbay | Rotating Centrifugal Habitat Concourse, Barracks, Medbay, Central Zero-G Docking Hub |
| 10 | `stn_deep_space_citadel` | **Deep-Space Command Citadel** | 1,800×1,200×750m | 320 | Colossal orbital fortress & strategic theater HQ | Grand Orbital War Room (3D Planetary Holo-Globe), Capital Hangar Bays, Multi-Layer Shield Core, Escape Shuttle Deck, Monorail Spine |

---

## 7. Personnel & Crew Roles (12)

*(Rendered via Hybrid Crew LOD: low-poly retro 3D models inside interiors and in WebXR; 8-/16-directional pixel sprites at medium/far battlefield range.)*

| # | ID | Role | Function |
| :- | :--- | :--- | :--- |
| 1 | `role_commander` | **The Commander (Player)** | Physical 1st-person player in Tactical Exo-Suit (personal deflector, EVA pack, mag-boots, Wrist-Pad, Laser Designator, Radio). Hardcore Ironman: death ends the match. |
| 2 | `role_xo` | **Executive Officer (XO)** | Manages War Room alerts, relays fleet status briefings, coordinates base/ship readiness. |
| 3 | `role_pilot` | **Pilots & Starfighter Aces** | Fly atmospheric fighters, bombers, gunships, dropships & courier shuttles. |
| 4 | `role_astrogator` | **Astrogators & Helmsmen** | Steer naval ships & spacecraft; plan and execute Keplerian orbital burns, transfers & dockings automatically on order. |
| 5 | `role_gunner` | **Plasma & Beam Gunners** | Aim and fire turrets, torpedo launchers, beam lances & orbital defense cannons. |
| 6 | `role_shield_officer` | **Shield & Power Officers** | Balance directional energy shield arcs and allocate reactor power across engines, weapons & shields. |
| 7 | `role_sensor_officer` | **Sensor & Comms Officers** | Operate radar, sonar, orbital scanners & relay uplinks; announce bogeys & shield collapse over intercom. |
| 8 | `role_engineer` | **Engineers & Damage Control** | Maintain fusion reactors; physically rush with welders and extinguishers to repair damaged decks & hull breaches. |
| 9 | `role_deck_crew` | **Deck & Hangar Crew** | Guide fighters/dropships on flight decks, operate factory & shipyard gantry cranes, load munitions & cargo. |
| 10 | `role_medic` | **Medics & Field Surgeons** | Staff medbays and stabilize wounded crew to keep stations operational. |
| 11 | `role_marine` | **Marines & Boarding Squads** | Escort the Commander, guard bridges/reactors, deploy from APCs/Dropships & storm enemy decks. |
| 12 | `role_breacher` | **Heavy Exo-Troopers / Breachers** | Shielded shock infantry specialized in zero-G airlock breaching & capital-ship bridge capture. |
