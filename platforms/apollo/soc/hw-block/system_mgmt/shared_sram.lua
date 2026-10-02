-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    assert(platform.host_smd_shared_sram == nil, "duplicate Apollo object: host_smd_shared_sram")
    platform.host_smd_shared_sram = {
        moduletype = "gs_memory";
        target_socket = {
            address = hw.HOST_SMD_SHARED_SRAM_PHYS_BASE;
            size = hw.HOST_SMD_SHARED_SRAM_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }


end
