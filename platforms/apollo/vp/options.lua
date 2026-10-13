-- SPDX-License-Identifier: BSD-3-Clause
-- Resolve environment and backend choices once, before loading any SoC domain.
local config = {}

function config.create(apollo_dir)
    local MONITOR_DEFAULT_PORT = 18080
    local ctx = dofile(apollo_dir.."soc/config.lua").create(apollo_dir)
    local options = ctx.options
    ctx.request_context.origin.ap_loader = 0x1F00
    ctx.request_context.origin.si_cl0_loader = 0x4F00
    ctx.request_context.origin.si_cl1_loader = 0x5F00
    ctx.request_context.capability = {boot_loader = 1; authenticated_image = 2}
    dofile(apollo_dir.."vp/options/environment.lua")(options)
    dofile(apollo_dir.."vp/options/artifacts.lua")(options, apollo_dir)
    dofile(apollo_dir.."vp/options/execution.lua")(options, apollo_dir)
    dofile(apollo_dir.."vp/options/models.lua")(options, apollo_dir)
    dofile(apollo_dir.."vp/options/vmcu.lua")(options)
    local monitor_enabled = options.getenv_bool_or("QBOX_APOLLO_MONITOR", false)
    local monitor_bind_address = options.getenv_or(
        "QBOX_APOLLO_MONITOR_BIND_ADDRESS",
        "127.0.0.1")
    local monitor_port = options.getenv_number_or(
        "QBOX_APOLLO_MONITOR_PORT",
        MONITOR_DEFAULT_PORT)
    assert(monitor_port >= 1 and monitor_port <= 65535 and monitor_port % 1 == 0,
           "QBOX_APOLLO_MONITOR_PORT must be an integer in range 1..65535")
    local runtime_injection_enabled = options.getenv_bool_or(
        "QBOX_APOLLO_RUNTIME_INJECTION",
        false)
    options.runtime_injection_enabled = runtime_injection_enabled
    assert(not runtime_injection_enabled or monitor_enabled,
           "QBOX_APOLLO_RUNTIME_INJECTION requires QBOX_APOLLO_MONITOR")


    function options.lower_decode_priority(target, priority)
        if target ~= nil then
            target.priority = priority
        end
    end

    ctx.options = options
    ctx.apollo_root = options.root
    ctx.getenv_or = options.getenv_or
    ctx.getenv_number_or = options.getenv_number_or
    ctx.getenv_bool_or = options.getenv_bool_or
    ctx.lower_decode_priority = options.lower_decode_priority
    ctx.config.monitor = {
        enabled = monitor_enabled;
        bind_address = monitor_bind_address;
        port = monitor_port;
    }
    ctx.config.runtime_injection = {enabled = runtime_injection_enabled}
    return ctx
end

return config
