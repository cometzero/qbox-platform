local SI_CL1_SRAM_BASE = 0x140000000

local module = {}

function module.create(ctx, _runtime)

    local block = {}

    function block.define_loader(ctx, platform, image)
        assert(
            platform.si_cl1_loader == nil,
            "duplicate object: platform.si_cl1_loader")
        platform.si_cl1_loader = {
            moduletype = "loader";
            request_origin_id = ctx.request_context.origin.si_cl1_loader;
            request_domain_id = ctx.request_context.domain.si_cl1;
            request_capabilities = ctx.request_context.capability.authenticated_image;
            request_secure = true;
            request_secure_valid = true;
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            { bin_file = image, address = SI_CL1_SRAM_BASE };
        }
    end

    return block
end

return module
