-- SPDX-License-Identifier: MIT
-- Resource Economy & 1:1 Construct Production Queue Rules

local M = {}

function M.queue_construct(producer_id, blueprint_id)
    local job_id = engine.economy.queue_build(producer_id, blueprint_id)
    return job_id
end

function M.advance_production(dt, steps)
    local completed = engine.economy.step(dt, steps)
    return completed
end

function M.get_spawned_unit(job_id)
    return engine.economy.job_spawned(job_id)
end

function M.get_resources(faction_id)
    return engine.economy.ledger(faction_id)
end

return M
