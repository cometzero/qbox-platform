-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    assert(platform.host_rse_si_ssram == nil, "duplicate Apollo object: host_rse_si_ssram")
    platform.host_rse_si_ssram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = hw.HOST_RSE_SI_SSRAM_PHYS_BASE;
            size = hw.HOST_RSE_SI_SSRAM_SIZE;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }


end
