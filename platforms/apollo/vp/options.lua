-- SPDX-License-Identifier: BSD-3-Clause
-- Resolve environment and backend choices once, before loading any SoC domain.
local config = {}

function config.create(apollo_dir)
    local options = {}
    local MONITOR_DEFAULT_PORT = 18080
    local REQUEST_DOMAIN = {
        system = 0;
        ap = 1;
        smd = 2;
        rse = 3;
        si_cl0 = 4;
        si_cl1 = 5;
    }
    local REQUEST_ORIGIN = {
        ap_cpu_base = 0x1000;
        ap_gpex = 0x1100;
        ap_pcie_epc = 0x1101;
        ap_global_peripheral = 0x1200;
        ap_loader = 0x1F00;
        rse_cpu = 0x3000;
        si_cl0_cpu_base = 0x4000;
        si_cl0_loader = 0x4F00;
        si_cl1_cpu_base = 0x5000;
        si_cl1_loader = 0x5F00;
    }
    local REQUEST_CAPABILITY = {
        boot_loader = 1;
        authenticated_image = 2;
    }

    dofile(apollo_dir.."vp/options/environment.lua")(options)
    dofile(apollo_dir.."vp/options/artifacts.lua")(options, apollo_dir)
    dofile(apollo_dir.."vp/options/execution.lua")(options, apollo_dir)
    dofile(apollo_dir.."vp/options/models.lua")(options, apollo_dir)
    dofile(apollo_dir.."soc/reset_targets.lua")(options)
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
    assert(not runtime_injection_enabled or monitor_enabled,
           "QBOX_APOLLO_RUNTIME_INJECTION requires QBOX_APOLLO_MONITOR")


    function options.lower_decode_priority(target, priority)
        if target ~= nil then
            target.priority = priority
        end
    end

    return {
        options = options;
        apollo_dir = apollo_dir;
        apollo_root = options.root;
        request_context = {
            domain = REQUEST_DOMAIN;
            origin = REQUEST_ORIGIN;
            capability = REQUEST_CAPABILITY;
        };
        getenv_or = options.getenv_or;
        getenv_number_or = options.getenv_number_or;
        getenv_bool_or = options.getenv_bool_or;
        lower_decode_priority = options.lower_decode_priority;
        config = {
            rse = {qemu_args = options.qemu_args};
            ap = {enable_cpus = options.enable_ap_cpus; qemu_args = options.ap_qemu_args};
            ros = {virtio = options.ap_virtio};
            system_mgmt = {};
            si_cl0 = {};
            si_cl1 = {};
            monitor = {
                enabled = monitor_enabled;
                bind_address = monitor_bind_address;
                port = monitor_port;
            };
            runtime_injection = {enabled = runtime_injection_enabled};
        };
    }
end

return config
