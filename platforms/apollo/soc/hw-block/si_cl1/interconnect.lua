local SI_CL1_HIPC_SHARED_BASE = 0xE0130000
local SI_CL1_HIPC_SHARED_SIZE = 0x00080000

local SI_CL1_HIPC_ALIAS_BASE = 0x00100000

local module = {}

function module.create(ctx, _runtime)

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

    function block.hipc_shared_window()
        return SI_CL1_HIPC_SHARED_BASE, SI_CL1_HIPC_SHARED_SIZE
    end

    return block
end

return module
