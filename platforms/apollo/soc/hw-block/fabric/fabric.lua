-- SPDX-License-Identifier: BSD-3-Clause
local fabric = {}
local SMD_PHYS_BASE = 0x2000000000000
local SMD_PHYS_SIZE = 0x1000000000000

function fabric.define(ctx, platform)
    -- Root fabric
    assert(platform.system_router == nil, "duplicate Apollo object: system_router")
    platform.system_router = {
        moduletype = "router";
        log_level = 0;
    }

    -- The 0x2 high-nibble is the architected System Management Domain.
    -- Keep its internal decode separate so an unmapped SMD address ends
    -- at the SMD boundary instead of falling through the system fabric.
    assert(platform.smd_router == nil, "duplicate Apollo object: smd_router")
    platform.smd_router = {
        moduletype = "router";
        log_level = 0;
    }

    assert(platform.system_to_smd_nci == nil, "duplicate Apollo object: system_to_smd_nci")
    platform.system_to_smd_nci = {
        moduletype = "addrtr";
        mapped_base_addr = SMD_PHYS_BASE;
        target_socket = {
            address = SMD_PHYS_BASE;
            size = SMD_PHYS_SIZE;
            bind = "&system_router.initiator_socket";
            relative_addresses = false;
        };
        initiator_socket = {bind = "&smd_router.target_socket"};
        log_level = 0;
    }

end

return fabric
