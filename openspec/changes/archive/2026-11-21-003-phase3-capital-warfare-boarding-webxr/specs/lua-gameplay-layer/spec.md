# Spec Delta

## ADDED Requirements

### Requirement: Boarding and Reactor Meltdown Lua API
The Lua 5.4 runtime SHALL expose `engine.boarding` functions enabling external Lua scripts to start boarding assaults, breach/decompress decks, trigger reactor meltdowns, and step boarding/meltdown state deterministically.

#### Scenario: Lua capital assault mission captures enemy flagship
- **GIVEN** `scripts/missions/capital_assault.lua` and `scripts/rules/boarding.lua`
- **WHEN** Lua launches a boarding action and advances simulation ticks
- **THEN** the target flagship transitions cleanly to friendly ownership via `engine.boarding`
