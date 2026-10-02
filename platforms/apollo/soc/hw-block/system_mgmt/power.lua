-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    assert(platform.host_si_pik == nil, "duplicate Apollo object: host_si_pik")
    platform.host_si_pik = {
        moduletype = "host_ppu";
        trace = options.host_ppu_trace;
        trace_limit = options.host_ppu_trace_limit;
        target_socket = {
            address = hw.HOST_SI_PIK_PHYS_BASE;
            size = hw.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_si_scr == nil, "duplicate Apollo object: host_si_scr")
    platform.host_si_scr = {
        moduletype = "host_scr";
        cl1_present = true;
        target_socket = {
            address = hw.HOST_SI_SCR_PHYS_BASE;
            size = hw.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_si_atu == nil, "duplicate Apollo object: host_si_atu")
    platform.host_si_atu = {
        moduletype = "rse_atu";
        trace = options.atu_trace;
        trace_limit = options.atu_trace_limit;
        trace_filter = options.atu_trace_filter;
        trace_address_min = hw.atu_trace_address_min;
        trace_address_max = hw.atu_trace_address_max;
        build_config = hw.SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = hw.HOST_SI_ATU_PHYS_BASE;
            size = hw.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }


end
