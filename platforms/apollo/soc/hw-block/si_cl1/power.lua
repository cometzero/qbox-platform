local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local host_ppu_trace = ctx.options.host_ppu_trace
    local host_ppu_trace_limit = ctx.options.host_ppu_trace_limit
    local SI_CL1_CPU_COUNT = hardware.SI_CL1_CPU_COUNT
    local HOST_SI_CL1_CL_UTIL_BASE = hardware.HOST_SI_CL1_CL_UTIL_BASE
    local HOST_SI_CLUS_PPU_OFFSET = hardware.HOST_SI_CLUS_PPU_OFFSET
    local HOST_SI_CONTROL_WINDOW_SIZE = hardware.HOST_SI_CONTROL_WINDOW_SIZE

    local block = {}

    function block.define_host(ctx, platform)
        assert(
            platform.host_si_cl1_clus_ppu == nil,
            "duplicate object: platform.host_si_cl1_clus_ppu")
        platform.host_si_cl1_clus_ppu = {
            moduletype = "host_ppu";
            trace = host_ppu_trace;
            trace_limit = host_ppu_trace_limit;
            assert_power_on_load = true;
            power_on_load = {bind = "&si_cl1_loader.reset"};
            power_on_load_pulse_width_ns = 0;
            target_socket = {
                address = HOST_SI_CL1_CL_UTIL_BASE + HOST_SI_CLUS_PPU_OFFSET;
                size = HOST_SI_CONTROL_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
                priority = 10;
            };
            log_level = 0;
        }
    end

    function block.finalize_reset_order(platform)
        local system_reset = platform.apollo_system_reset_fanout
        if system_reset == nil then
            return
        end

        local reset_targets = system_reset.reset_out.bind
        local cl0_count
        reset_targets, cl0_count = string.gsub(
            reset_targets,
            "&si_cl0_qemu_inst%.reset;?",
            "")
        local cl1_count
        reset_targets, cl1_count = string.gsub(
            reset_targets,
            "&si_cl1_qemu_inst%.reset;?",
            "")
        assert(
            cl0_count == 1 and cl1_count == 1,
            "split SI reset routing requires one QEMU target per cluster")

        -- Establish each external CPU hold before requesting the asynchronous
        -- instance reset, whose QEMU completion resumes all vCPUs. The managed
        -- CPU hold survives that resume and is released only by firmware PPU ON.
        local ppu_targets = {
            "host_si_cl0_clus_ppu";
            "host_si_cl0_core0_ppu";
            "host_si_cl1_clus_ppu";
            "si_cl1_cluster_ppu";
        }
        for cpu = 0,(SI_CL1_CPU_COUNT-1) do
            ppu_targets[#ppu_targets + 1] = "si_cl1_core" .. cpu .. "_ppu"
        end
        local ppu_resets = {}
        for _, name in ipairs(ppu_targets) do
            local count
            reset_targets, count = string.gsub(
                reset_targets, "&" .. name .. "%.reset;?", "")
            assert(count == 1, "SI reset routing requires one PPU target: " .. name)
            ppu_resets[#ppu_resets + 1] = "&" .. name .. ".reset"
        end

        local ordered_targets = {
            table.concat(ppu_resets, ";");
            -- Drop retained SystemC UART IRQ levels before resetting their GICs.
            "&si_cl0_uart.reset";
            "&si_cl1_uart.reset";
            "&si_gic_multiview.reset";
            "&si_cl0_qemu_inst.reset";
            "&si_cl1_qemu_inst.reset";
            "&ap_rgic2lgic_messreg.reset";
            reset_targets;
        }
        system_reset.reset_out.bind = table.concat(ordered_targets, ";")
    end

    return block
end

return module
