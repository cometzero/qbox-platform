local fabric = {}

function fabric.create(ctx)
    assert(ctx.machine_contract.topology.machine == "apollo-qvp")
    return {
        moduletype = "Container";
        quantum_ns = 10000000;

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
            mapped_base_addr = 0x2000000000000;
            target_socket = {
                address = 0x2000000000000;
                size = 0x1000000000000;
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
