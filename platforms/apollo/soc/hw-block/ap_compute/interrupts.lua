local AP_GIC_DIST_BASE = 0x20800000
local AP_GIC_VIEW0_DIST_BASE = 0x20000000
local AP_GIC_VIEW0_REDIST_BASE = 0x20080000
local AP_GIC_ITS_BASE = 0x20840000
local AP_GIC_VIEW0_DIST_SIZE = 0x00080000
local AP_GIC_VIEW0_REDIST_SIZE = 0x00040000
local AP_GIC_ITS_SIZE = 0x00040000

local interrupts = {}

function interrupts.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_HW = hw.AP_HW

    assert(platform.ap_gic == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_gic = {
            moduletype = "arm_gicv3";
            args = {"&platform.ap_qemu_inst"};
            dist_iface = {
                address = AP_GIC_DIST_BASE;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            num_cpus = options.AP_GIC_NUM_CPUS;
            redist_region = options.repeat_value(1, AP_HW.gic_active_redist_regions);
            has_security_extensions = true;
            has_lpi = true;
            revision = AP_HW.gic_revision;
            num_spi = AP_HW.gic_num_spi;
            has_gicv4_1 = true;
            has_direct_lpi = true;
            has_rvpeid = true;
            has_vpend_valid_dirty = true;
            vpeid_bits = AP_HW.gic_vpeid_bits;
        }
    end

    assert(platform.ap_gic_multiview == nil, "duplicate Apollo object")
    platform.ap_gic_multiview = {
        moduletype = "gicx00_multiview";
        backend_dist_base = AP_GIC_DIST_BASE;
        backend_redist_base = AP_ADDRESS.gic_redist;
        backend_redist_stride = AP_SIZE.gic_redist;
        backend_redist_count = AP_HW.gic_active_redist_regions;
        backend_socket = {bind = "&ap_router.target_socket"};
        trace = ctx.getenv_bool_or(
            "QBOX_APOLLO_FULL_AP_GIC_MULTIVIEW_TRACE", false);
        trace_limit = ctx.getenv_number_or(
            "QBOX_APOLLO_FULL_AP_GIC_MULTIVIEW_TRACE_LIMIT", "256");
        view0_dist = {
            address = AP_GIC_VIEW0_DIST_BASE;
            size = AP_GIC_VIEW0_DIST_SIZE;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        };
    }

    for i = 0, (AP_HW.gic_view0_redist_regions-1) do
        platform.ap_gic_multiview["view0_redist_" .. i] = {
            address = AP_GIC_VIEW0_REDIST_BASE +
                (i * AP_GIC_VIEW0_REDIST_SIZE);
            size = AP_GIC_VIEW0_REDIST_SIZE;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        }
    end

    if options.enable_ap_cpus and
       AP_HW.gic_active_redist_regions < AP_HW.gic_redist_regions then
        platform.ap_gic_multiview.inactive_redists = {
            address = AP_ADDRESS.gic_redist +
                AP_HW.gic_active_redist_regions * AP_SIZE.gic_redist;
            size =
                (AP_HW.gic_redist_regions - AP_HW.gic_active_redist_regions) *
                AP_SIZE.gic_redist;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        }
    end

    assert(platform.ap_gic_its == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_gic_its = {
            moduletype = "arm_gicv3_its";
            args = {"&platform.ap_qemu_inst", "&platform.ap_gic"};
            has_gicv4_1 = true;
            gicv4_1_svpet = AP_HW.gic_its_svpet;
            gicv4_1_cte_size = AP_HW.gic_its_cte_size;
            mem = {
                address = AP_GIC_ITS_BASE;
                size = AP_GIC_ITS_SIZE;
                bind = "&system_router.initiator_socket";
            };
        }
    end
end

return interrupts
