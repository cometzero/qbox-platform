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
            has_multi_view = true;
            args = {qemu_instance};
            dist_iface = {
                address = 0x60200000;
                size = 0x200000;
                bind = "&si_gic_backend_router.initiator_socket";
            };
            redist_region = SI_CL1_GIC_REDIST_REGIONS;
            num_cpus = SI_CL1_CPU_COUNT;
            num_spi = SI_CL1_GIC_NUM_SPI;
        }
        for i = 0, SI_CL1_CPU_COUNT - 1 do
            platform.si_cl1_gic["redist_iface_" .. i] = {
                address = 0x61020000 + (i * SI_CL1_GICR_STRIDE);
                size = SI_CL1_GICR_SIZE;
                bind = "&si_gic_backend_router.initiator_socket";
            }
        end
    end

    return block
end

return module
