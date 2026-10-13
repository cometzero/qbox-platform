local SI_CL1_GICD_BASE = 0x30200000
local SI_CL1_GICD_SIZE = 0x00010000

local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local SI_CL1_CPU_COUNT = hardware.SI_CL1_CPU_COUNT
    local SI_CL1_GIC_REDIST_REGIONS = hardware.SI_CL1_GIC_REDIST_REGIONS
    local SI_CL1_GIC_NUM_SPI = hardware.SI_CL1_GIC_NUM_SPI

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
    end

    return block
end

return module
