-- SPDX-License-Identifier: MIT
-- External Session Management: Engine Config, VFS Hot-Reload Watcher & Deterministic Save/Load

local M = {}

function M.load_engine_config(path)
    return engine.config.load(path or "engine.cfg")
end

function M.watch_script(script_path)
    return engine.vfs.watch("scripts", script_path)
end

function M.poll_changes()
    return engine.vfs.poll_hot_reload()
end

function M.save_world_snapshot()
    return engine.save.snapshot()
end

function M.restore_world_snapshot(snapshot_text)
    return engine.save.restore(snapshot_text)
end

return M
