-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_CL0_AP_NS_WATCHDOG_WS1_SPI = 321 - 32

local AP_GIC_REVISION = 4
local AP_GIC_NUM_SPI = 960
local AP_GIC_VPEID_BITS = 16
local AP_GIC_REDIST_BASE = 0x20880000
local AP_GIC_REDIST_SIZE = 0x00040000
local AP_GIC_VIEW0_REDIST_REGIONS = 16
local AP_GIC_REDIST_REGIONS = 16
local AP_GIC_ITS_SVPET = 1
local AP_GIC_ITS_CTE_SIZE = 2
local AP_RGIC2LGIC_MESSREG_BASE = 0x5FFF0000
local AP_RGIC2LGIC_MESSREG_SIZE = 0x00010000
local AP_GIC_DIST_BASE = 0x20800000
local AP_GIC_VIEW0_DIST_BASE = 0x20000000
local AP_GIC_VIEW0_REDIST_BASE = 0x20080000
local AP_GIC_ITS_BASE = 0x20840000
local AP_GIC_VIEW0_DIST_SIZE = 0x00080000
local AP_GIC_VIEW0_REDIST_SIZE = 0x00040000
local AP_GIC_ITS_SIZE = 0x00040000
local AP_SMMU_EVENT_SPI = 65
local AP_SMMU_PAMAX = 52
local AP_SMMU_SID_SIZE = 32
local AP_SMMU_TBU_COUNT = 1
local AP_SMMU_IIDR = 0x720AE000
local AP_SMMU_BASE = 0x1C0000000
local AP_SMMU_SIZE = 0x08000000
local AP_SID_SYSTEM_ID = 0x0047773d
local AP_SID_SOC_ID = 0x00000000
local AP_SID_CHIP_ID = 0x00000000
local AP_SID_PIDR4 = 0x00000004
local AP_SID_PIDR0 = 0x0000003d
local AP_SID_PIDR1 = 0x000000b7
local AP_SID_PIDR2 = 0x0000000b
local AP_SID_PIDR3 = 0x00000000
local AP_SID_CIDR0 = 0x0000000d
local AP_SID_CIDR1 = 0x000000f0
local AP_SID_CIDR2 = 0x00000005
local AP_SID_CIDR3 = 0x000000b1
local AP_ARCH_TIMER_FREQUENCY_HZ = 125000000
local AP_REGISTER_WINDOW_SIZE = 0x00010000
local AP_ARCH_TIMER_FRAME_COUNT = 2
local AP_SID_BASE = 0x1A4A0000
local AP_SID_SIZE = 0x00010000
local AP_NS_WDOG_WS1_SPI = 51
local AP_NS_WDOG_CONTROL_BASE = 0x1A420000
local AP_NS_WDOG_REFRESH_BASE = 0x1A430000
local AP_SECURE_WDOG_CONTROL_BASE = 0x1A460000
local AP_SECURE_WDOG_REFRESH_BASE = 0x1A470000
local AP_SYS_TIMCTL_BASE = 0x1A810000
local AP_SYS_COUNTER_SECURE_BASE = 0x1A820000
local AP_SYS_COUNTER_NON_SECURE_BASE = 0x1A830000
local AP_SECURE_WDOG_SIZE = 0x00010000
local AP_SYS_TIMER_SIZE = 0x00010000
local AP_SYS_TIMER_SECURE_SPI = 48
local AP_SYS_TIMER_NON_SECURE_SPI = 49
local AP_SECURE_WDOG_WS0_SPI = 47
local AP_NS_WDOG_WS0_SPI = 50

-- GIC SPI constants are zero-based SPI socket indices (not architectural INTIDs).

local system = {}

function system.define_reset(ctx, platform)
    local options = ctx.options

    assert(platform.ap_ns_watchdog_ws1_fanout == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_ns_watchdog_ws1_fanout = {
            moduletype = "signal_fanout";
            signal_out = {
                bind = "&ap_gic.spi_in_" .. AP_NS_WDOG_WS1_SPI ..
                    ";" .. "&si_cl0_gic.spi_in_" .. SI_CL0_AP_NS_WATCHDOG_WS1_SPI;
            };
        }
    end
end

function system.define(ctx, platform)
    local options = ctx.options

    assert(platform.ap_sid == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_sid = {
            moduletype = "host_scr";
            system_id = AP_SID_SYSTEM_ID;
            soc_id = AP_SID_SOC_ID;
            chip_id = AP_SID_CHIP_ID;
            pidr4 = AP_SID_PIDR4;
            pidr0 = AP_SID_PIDR0;
            pidr1 = AP_SID_PIDR1;
            pidr2 = AP_SID_PIDR2;
            pidr3 = AP_SID_PIDR3;
            cidr0 = AP_SID_CIDR0;
            cidr1 = AP_SID_CIDR1;
            cidr2 = AP_SID_CIDR2;
            cidr3 = AP_SID_CIDR3;
            target_socket = {
                address = AP_SID_BASE;
                size = AP_SID_SIZE;
                bind = "&system_router.initiator_socket";
            };
            log_level = 0;
        }
    end


end



-- GIC SPI constants are zero-based SPI socket indices (not architectural INTIDs).



function system.define_watchdog(ctx, platform)
    local options = ctx.options

    assert(platform.ap_watchdog_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_watchdog_0 = {
            moduletype = "zena_watchdog";
            trace = options.reset_trace;
            clock_frequency = AP_ARCH_TIMER_FREQUENCY_HZ;
            control = {
                address = AP_NS_WDOG_CONTROL_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            refresh = {
                address = AP_NS_WDOG_REFRESH_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            ws0 = {bind = "&ap_gic.spi_in_" .. AP_NS_WDOG_WS0_SPI};
            ws1 = {bind = "&ap_ns_watchdog_ws1_fanout.signal_in"};
        }
    end
end

function system.define_timers(ctx, platform)
    local options = ctx.options

    assert(platform.ap_timer_mem == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_timer_mem = {
            moduletype = "qemu_arm_arch_timer_mmio";
            args = {ctx.instances.ap};
            cntfrq = AP_ARCH_TIMER_FREQUENCY_HZ;
            nr_frames = AP_ARCH_TIMER_FRAME_COUNT;
            view_size = AP_SYS_TIMER_SIZE;
            frame_offset_0 = AP_SYS_COUNTER_NON_SECURE_BASE - AP_SYS_TIMCTL_BASE;
            frame_id_0 = 0;
            frame_offset_1 = AP_SYS_COUNTER_SECURE_BASE - AP_SYS_TIMCTL_BASE;
            frame_id_1 = 1;
            mem = {
                address = AP_SYS_TIMCTL_BASE;
                size = (AP_SYS_COUNTER_NON_SECURE_BASE - AP_SYS_TIMCTL_BASE) + AP_SYS_TIMER_SIZE;
                bind = "&system_router.initiator_socket";
            };
            -- sc_vector ports are named irq_0/irq_1, not irq.0/irq.1.
            irq_0 = {bind = "&ap_gic.spi_in_" .. AP_SYS_TIMER_NON_SECURE_SPI};
            irq_1 = {bind = "&ap_gic.spi_in_" .. AP_SYS_TIMER_SECURE_SPI};
        }
    end

    assert(platform.ap_timer_counter_mirror == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_timer_counter_mirror = {
            moduletype = "qemu_arm_mmio_counter_mirror";
            dylib_path = "qemu_arm_counter_mirror";
            args = {
                "&platform.ap_timer_mem";
                "&platform.css_system_counter";
            };
        }
    end

    assert(platform.ap_secure_wdog == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_secure_wdog = {
            moduletype = "zena_watchdog";
            trace = options.reset_trace;
            clock_frequency = AP_ARCH_TIMER_FREQUENCY_HZ;
            control = {
                address = AP_SECURE_WDOG_CONTROL_BASE;
                size = AP_SECURE_WDOG_SIZE;
                bind = "&system_router.initiator_socket";
            };
            refresh = {
                address = AP_SECURE_WDOG_REFRESH_BASE;
                size = AP_SECURE_WDOG_SIZE;
                bind = "&system_router.initiator_socket";
            };
            ws0 = {bind = "&ap_gic.spi_in_" .. AP_SECURE_WDOG_WS0_SPI};
            ws1 = {bind = "&host_reset_ctrl.ap_s_watchdog_reset"};
            log_level = 0;
        }
    end
end



function system.define_interrupts(ctx, platform)
    local options = ctx.options
    local redist_regions = {}
    for i = 1, options.AP_GIC_NUM_CPUS do redist_regions[i] = 1 end

    assert(platform.ap_gic == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_gic = {
            moduletype = "arm_gicv3";
            args = {ctx.instances.ap};
            dist_iface = {
                address = AP_GIC_DIST_BASE;
                size = AP_REGISTER_WINDOW_SIZE;
                bind = "&system_router.initiator_socket";
            };
            num_cpus = options.AP_GIC_NUM_CPUS;
            redist_region = redist_regions;
            has_security_extensions = true;
            has_lpi = true;
            revision = AP_GIC_REVISION;
            num_spi = AP_GIC_NUM_SPI;
            has_gicv4_1 = true;
            has_direct_lpi = true;
            has_rvpeid = true;
            has_vpend_valid_dirty = true;
            vpeid_bits = AP_GIC_VPEID_BITS;
        }
    end

    if options.enable_ap_cpus then
        for i = 0, (options.AP_GIC_NUM_CPUS-1) do
            assert(platform["ap_gic"]["redist_iface_" .. i] == nil, "duplicate Apollo object")
            platform["ap_gic"]["redist_iface_" .. i] = {
                address = AP_GIC_REDIST_BASE + (i * AP_GIC_REDIST_SIZE);
                size = AP_GIC_REDIST_SIZE;
                bind = "&system_router.initiator_socket";
            }
        end

    end

    assert(platform.ap_gic_multiview == nil, "duplicate Apollo object")
    platform.ap_gic_multiview = {
        moduletype = "gicx00_multiview";
        backend_dist_base = AP_GIC_DIST_BASE;
        backend_redist_base = AP_GIC_REDIST_BASE;
        backend_redist_stride = AP_GIC_REDIST_SIZE;
        backend_redist_count = options.AP_GIC_NUM_CPUS;
        backend_socket = {bind = "&ap_router.target_socket"};
        trace = options.ap_gic_multiview_trace;
        trace_limit = options.ap_gic_multiview_trace_limit;
        view0_dist = {
            address = AP_GIC_VIEW0_DIST_BASE;
            size = AP_GIC_VIEW0_DIST_SIZE;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        };
    }

    for i = 0, (AP_GIC_VIEW0_REDIST_REGIONS-1) do
        platform.ap_gic_multiview["view0_redist_" .. i] = {
            address = AP_GIC_VIEW0_REDIST_BASE +
                (i * AP_GIC_VIEW0_REDIST_SIZE);
            size = AP_GIC_VIEW0_REDIST_SIZE;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        }
    end

    if options.enable_ap_cpus and
       options.AP_GIC_NUM_CPUS < AP_GIC_REDIST_REGIONS then
        platform.ap_gic_multiview.inactive_redists = {
            address = AP_GIC_REDIST_BASE +
                options.AP_GIC_NUM_CPUS * AP_GIC_REDIST_SIZE;
            size =
                (AP_GIC_REDIST_REGIONS - options.AP_GIC_NUM_CPUS) *
                AP_GIC_REDIST_SIZE;
            bind = "&ap_router.initiator_socket";
            priority = 0;
        }
    end

    assert(platform.ap_gic_its == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_gic_its = {
            moduletype = "arm_gicv3_its";
            args = {ctx.instances.ap, "&platform.ap_gic"};
            has_gicv4_1 = true;
            gicv4_1_svpet = AP_GIC_ITS_SVPET;
            gicv4_1_cte_size = AP_GIC_ITS_CTE_SIZE;
            mem = {
                address = AP_GIC_ITS_BASE;
                size = AP_GIC_ITS_SIZE;
                bind = "&system_router.initiator_socket";
            };
        }
    end
end

function system.define_message_register(ctx, platform)
    local options = ctx.options
    assert(platform.ap_rgic2lgic_messreg == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_rgic2lgic_messreg = {
            moduletype = "gic720ae_messreg";
            target_socket = {
                address = AP_RGIC2LGIC_MESSREG_BASE;
                size = AP_RGIC2LGIC_MESSREG_SIZE;
                bind = "&system_router.initiator_socket";
            };
            window_size = AP_RGIC2LGIC_MESSREG_SIZE;
            log_level = 0;
        }
    end

end

local function ap_smmu_component(ctx)
    local options = ctx.options
    local event_irq_target = "&ap_gic.spi_in_" .. AP_SMMU_EVENT_SPI

    if options.smmu_backend == "systemc-mmu720ae" then
        return {
            moduletype = "smmuv3";
            pamax = AP_SMMU_PAMAX;
            sidsize = AP_SMMU_SID_SIZE;
            ato = false;
            num_tbu = AP_SMMU_TBU_COUNT;
            iidr = AP_SMMU_IIDR;
            target_socket = {
                address = AP_SMMU_BASE;
                size = AP_SMMU_SIZE;
                bind = "&system_router.initiator_socket";
            };
            dma = {bind = "&system_router.target_socket"};
            irq_eventq = {bind = event_irq_target};
        }
    end

    return {
        moduletype = "arm_smmuv3";
        args = {ctx.instances.ap, "&platform.ap_gpex_0"};
        mem = {
            address = AP_SMMU_BASE;
            size = AP_SMMU_SIZE;
            bind = "&system_router.initiator_socket";
        };
        irq_out_0 = {bind = event_irq_target};
        stage = "1";
    }
end

function system.define_iommu(ctx, platform)
    local options = ctx.options
    if not options.enable_ap_cpus then
        return
    end

    assert(platform.ap_smmu_0 == nil, "duplicate AP SMMU")
    platform.ap_smmu_0 = ap_smmu_component(ctx)
    if options.smmu_backend == "systemc-mmu720ae" then
        assert(platform.ap_smmu_lti00 == nil, "duplicate AP SMMU TBU")
        platform.ap_smmu_lti00 = {
            moduletype = "smmuv3_tbu";
            args = {"&platform.ap_smmu_0"};
            topology_id = 0x40;
            requester_id_from_context = true;
            upstream_socket = {};
            downstream_socket = {bind = "&system_router.target_socket"};
        }
    end
end


function system.define_cpu_reset(ctx, platform)
    local options = ctx.options
    assert(platform.ap_reset_gpio == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_reset_gpio = {
            moduletype = "reset_gpio";
            args = {"&platform.ap_qemu_inst"};
            reset_out = {bind = options.ap_cpu_reset_bind_targets(options.AP_NUM_CPUS)};
            log_level = 0;
        }
    end

end

function system.define_system_reset(ctx, platform)
    local options = ctx.options
    assert(platform.ap_cold_reset_fanout == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cold_reset_fanout = {
            moduletype = "reset_fanout";
            trace = options.reset_trace;
            reset_out = {bind = options.ap_cold_reset_bind_targets()};
            log_level = 0;
        }
    end

    assert(platform.apollo_system_reset_fanout == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.apollo_system_reset_fanout = {
            moduletype = "reset_fanout";
            trace = options.reset_trace;
            reset_out = {bind = options.apollo_system_reset_bind_targets()};
            log_level = 0;
        }
    end

end



return system
