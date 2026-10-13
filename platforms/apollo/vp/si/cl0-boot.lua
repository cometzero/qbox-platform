local SI_CL0_SRAM_BASE = 0x120000000

local module = {}

function module.create(ctx, _runtime)

    local block = {}

    function block.define_loader(ctx, platform, image)
        assert(
            platform.si_cl0_loader == nil,
            "duplicate object: platform.si_cl0_loader")
        platform.si_cl0_loader = {
            moduletype = "loader";
            request_origin_id = ctx.request_context.origin.si_cl0_loader;
            request_domain_id = ctx.request_context.domain.si_cl0;
            request_capabilities = ctx.request_context.capability.authenticated_image;
            request_secure = true;
            request_secure_valid = true;
            initiator_socket = {bind = "&si_cl0_router.target_socket"};
            { bin_file = image, address = SI_CL0_SRAM_BASE };
        }
    end

    function block.configure_cpu(ctx, cpu)
        cpu.trace_pc = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_CL0_PC_TRACE", false);
        cpu.trace_exception_state = ctx.getenv_bool_or(
                "QBOX_APOLLO_FULL_SI_CL0_EXCEPTION_TRACE",
                false);
        cpu.trace_pc_file = ctx.getenv_or(
                "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_FILE",
                ctx.apollo_root .. "build/qbox-apollo-fvp/full-system/si-cl0-pc-trace.log");
        cpu.trace_pc_interval = ctx.getenv_number_or(
                "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_INTERVAL",
                "1");
        cpu.trace_pc_limit = ctx.getenv_number_or(
                "QBOX_APOLLO_FULL_SI_CL0_PC_TRACE_LIMIT",
                "4096");
    end

    return block
end

return module
