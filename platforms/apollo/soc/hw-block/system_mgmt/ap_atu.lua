-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    assert(platform.host_ap_atu == nil, "duplicate Apollo object: host_ap_atu")
    platform.host_ap_atu = {
        moduletype = "rse_atu";
        trace = options.atu_trace;
        trace_limit = options.atu_trace_limit;
        trace_filter = options.atu_trace_filter;
        trace_address_min = hw.atu_trace_address_min;
        trace_address_max = hw.atu_trace_address_max;
        enable_dmi = options.enable_ap_cpus and options.atu_dmi;
        build_config = hw.SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = hw.HOST_AP_ATU_PHYS_BASE;
            size = hw.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        translation_socket = options.enable_ap_cpus and {
            address = hw.HOST_AP_ATU_LOGICAL_BASE;
            size = hw.HOST_AP_ATU_LOGICAL_SIZE;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
            priority = 10;
        } or nil;
        initiator_socket = options.enable_ap_cpus and {bind = "&system_router.target_socket"} or nil;
        log_level = 0;
    }


end
