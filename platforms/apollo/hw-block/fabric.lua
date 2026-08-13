local fabric = {}

local SMD_PHYS_BASE = 0x2000000000000
local SMD_PHYS_SIZE = 0x1000000000000
local SYSTEMC_QUANTUM_NS = 10000000

function fabric.create(ctx)
    return {
        moduletype = "Container";
        quantum_ns = SYSTEMC_QUANTUM_NS;

        -- Root fabric
        system_router = {
            moduletype = "router";
            log_level = 0;
        },

        -- The 0x2 high-nibble is the architected System Management Domain.
        -- Keep its internal decode separate so an unmapped SMD address ends
        -- at the SMD boundary instead of falling through the system fabric.
        smd_router = {
            moduletype = "router";
            log_level = 0;
        },

        system_to_smd_nci = {
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
        },

        keep_alive_0 = {
            moduletype = "keep_alive";
        },

    }
end

return fabric
