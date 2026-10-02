local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local SI_CL1_HIPC_SHARED_BASE = hardware.SI_CL1_HIPC_SHARED_BASE
    local SI_CL1_HIPC_SHARED_SIZE = hardware.SI_CL1_HIPC_SHARED_SIZE
    local SI_CL1_HIPC_ALIAS_BASE = hardware.SI_CL1_HIPC_ALIAS_BASE

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl1_router == nil,
            "duplicate object: platform.si_cl1_router")
        platform.si_cl1_router = {
            moduletype = "router";
            log_level = 0;
        }

        assert(
            platform.si_cl1_hipc_bridge == nil,
            "duplicate object: platform.si_cl1_hipc_bridge")
        platform.si_cl1_hipc_bridge = {
            moduletype = "addrtr";
            mapped_base_addr = SI_CL1_HIPC_ALIAS_BASE;
            target_socket = {
                address = SI_CL1_HIPC_SHARED_BASE;
                size = SI_CL1_HIPC_SHARED_SIZE;
                bind = "&si_cl1_router.initiator_socket";
                relative_addresses = false;
            };
            initiator_socket = {bind = "&ap_router.target_socket"};
            log_level = 0;
        }
    end

    return block
end

return module
