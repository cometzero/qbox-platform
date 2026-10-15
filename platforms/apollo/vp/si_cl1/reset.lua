local SI_CL1_CPU_COUNT = 4
local reset = {}

    function reset.finalize_reset_order(platform)
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

        for _, name in ipairs({
            "si_cl0_uart", "si_cl1_uart", "si_gic_multiview", "ap_rgic2lgic_messreg",
        }) do
            local count
            reset_targets, count = reset_targets:gsub("&" .. name .. "%.reset;?", "")
            assert(count == 1, "SI reset routing requires one target: " .. name)
        end
        reset_targets = reset_targets:gsub(";$", "")

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
        if platform.si_cl0_vmcu_uart then
            -- External safety UART is SystemC too: clear its level before GIC.
            ordered_targets[#ordered_targets] = string.gsub(
                ordered_targets[#ordered_targets], "&si_cl0_vmcu_uart%.reset;?", "")
            table.insert(ordered_targets, 2, "&si_cl0_vmcu_uart.reset")
        end
        if platform.si_cl0_vmcu_spi then
            ordered_targets[#ordered_targets] = string.gsub(
                ordered_targets[#ordered_targets], "&si_cl0_vmcu_spi%.reset;?", "")
            table.insert(ordered_targets, 2, "&si_cl0_vmcu_spi.reset")
        end
        system_reset.reset_out.bind = table.concat(ordered_targets, ";")
    end

return reset
