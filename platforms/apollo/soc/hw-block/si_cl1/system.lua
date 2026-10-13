local SI_CL1_CPU_COUNT = 4
local SI_CL1_GIC_REDIST_REGIONS = {1; 1; 1; 1}
local SI_CL1_GIC_NUM_SPI = 128

local SI_CL1_GICR_SIZE = 0x00020000
local SI_CL1_GICR_STRIDE = 0x00020000
local SI_CL1_GICR0_BASE = 0x30260000
local SI_CL1_GICD_BASE = 0x30200000
local SI_CL1_GICD_SIZE = 0x00010000

local module = {}

function module.create(ctx, _runtime)

    local block = {}

    function block.define_interrupt_controller(
        ctx,
        platform,
        qemu_instance)
        assert(
            platform.si_cl1_gic == nil,
            "duplicate object: platform.si_cl1_gic")
        platform.si_cl1_gic = {
            moduletype = "arm_gicv3";
            args = {qemu_instance};
            dist_iface = {
                address = SI_CL1_GICD_BASE;
                size = SI_CL1_GICD_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            redist_region = SI_CL1_GIC_REDIST_REGIONS;
            num_cpus = SI_CL1_CPU_COUNT;
            num_spi = SI_CL1_GIC_NUM_SPI;
        }
        for i = 0, SI_CL1_CPU_COUNT - 1 do
            platform.si_cl1_gic["redist_iface_" .. i] = {
                address = SI_CL1_GICR0_BASE + (i * SI_CL1_GICR_STRIDE);
                size = SI_CL1_GICR_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            }
        end
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
        if platform.si_cl0_vmcu_uart then
            -- External safety UART is SystemC too: clear its level before GIC.
            ordered_targets[#ordered_targets] = string.gsub(
                ordered_targets[#ordered_targets], "&si_cl0_vmcu_uart%.reset;?", "")
            table.insert(ordered_targets, 2, "&si_cl0_vmcu_uart.reset")
        end
        system_reset.reset_out.bind = table.concat(ordered_targets, ";")
    end

    return block
end

return module
