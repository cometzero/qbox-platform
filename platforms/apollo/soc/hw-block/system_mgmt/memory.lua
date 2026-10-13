-- SPDX-License-Identifier: BSD-3-Clause
local HOST_RSE_SI_SSRAM_PHYS_BASE = 0x4000040000000
local HOST_RSE_SI_SSRAM_SIZE = 0x00040000
local HOST_SMD_SHARED_SRAM_PHYS_BASE = 0x2000060000000
local HOST_SMD_SHARED_SRAM_SIZE = 0x00100000
local HOST_AP_MHU_POINTER_ACCESS_PHYS_BASE = 0x0FFFE0000
local HOST_AP_MHU_POINTER_ACCESS_SIZE = 0x00020000

local memory = {}

function memory.define_rse_si(ctx, platform)
    local options = ctx.options
    assert(platform.host_rse_si_ssram == nil, "duplicate Apollo object: host_rse_si_ssram")
    platform.host_rse_si_ssram = {
        moduletype = "gs_memory";
        dmi_allow = options.host_memory_dmi;
        target_socket = {
            address = HOST_RSE_SI_SSRAM_PHYS_BASE;
            size = HOST_RSE_SI_SSRAM_SIZE;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }
end

function memory.define_shared(ctx, platform)
    assert(platform.host_smd_shared_sram == nil, "duplicate Apollo object: host_smd_shared_sram")
    platform.host_smd_shared_sram = {
        moduletype = "gs_memory";
        target_socket = {
            address = HOST_SMD_SHARED_SRAM_PHYS_BASE;
            size = HOST_SMD_SHARED_SRAM_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }
end

function memory.define_ap_rse(ctx, platform)
    assert(platform.host_ap_rse_mailbox == nil, "duplicate Apollo object: host_ap_rse_mailbox")
    platform.host_ap_rse_mailbox = {
        moduletype = "gs_memory";
        dmi_allow = false;
        target_socket = {
            address = HOST_AP_MHU_POINTER_ACCESS_PHYS_BASE;
            size = HOST_AP_MHU_POINTER_ACCESS_SIZE;
            bind = "&system_router.initiator_socket";
        };
        init_mem = true;
        log_level = 0;
    }
end

return memory
