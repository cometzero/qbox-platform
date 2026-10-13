-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_CL0_SYSTEM_TIMER_SPI = 34 - 32
local SI_CL0_WATCHDOG_WS0_SPI = 37 - 32

local SI_CL0_GICD_VIEW0_BASE = 0x30000000
local SI_CL0_GICR_VIEW0_BASES = {
    0x30040000;
    0x30060000;
    0x30080000;
    0x300a0000;
    0x300c0000;
}
local SI_GICR_SIZE = 0x00020000
local SI_CL0_WDOG_CLOCK_HZ = 125000000
local SI_CL0_SMD_EXPANSION_PHYS_BASE = 0x20000D8000000
local SI_GIC_BACKEND_REDIST_COUNT = 0
local SI_GIC_VIEW1_REDIST_FIRST = 0
local SI_GIC_VIEW2_REDIST_FIRST = 1
local SI_GIC_SPI_COUNT = 960

local SI_GIC_BACKEND_DIST_BASE = 0x30f00000
local SI_GIC_BACKEND_REDIST_BASE = 0x30f40000
local SI_GIC_DIST_CFGID_OFFSET = 0x0000F000
local SI_GIC_DIST_CFGID_SIZE = 0x00000008
local SI_GIC_DIST_IVIEWR_OFFSET = 0x0000F600
local SI_GIC_DIST_IVIEWR_SIZE = 0x00000400
local SI_GIC_REDIST_PWRR_OFFSET = 0x00000024
local SI_GIC_REDIST_VIEWR_OFFSET = 0x0000002C
local SI_GIC_REDIST_FLUSHR_OFFSET = 0x00000030
local SI_GIC_REDIST_REGISTER_SIZE = 0x00000004

local SI_CL0_GICD_VIEW1_BASE = 0x30100000
local SI_CL0_GICR_VIEW1_BASE = 0x30140000
local SI_GICD_SIZE = 0x00010000
local SI_CL0_SCR_BASE = 0x2A6B0000
local SI_CL0_SCR_SIZE = 0x00010000
local SI_CL0_TIMER_CNTCTL_BASE = 0x2A6F0000
local SI_CL0_TIMER_CNTCTL_SIZE = 0x00010000
local SI_CL0_TIMER_CNT_BASE = 0x2A720000
local SI_CL0_TIMER_CNT_SIZE = 0x00010000
local SI_CL0_WDOG_CONTROL_BASE = 0x2A700000
local SI_CL0_WDOG_REFRESH_BASE = 0x2A710000
local SI_CL0_WDOG_SIZE = 0x00010000
local SI_CL0_PLL_SIZE = 0x00001000
local SI_CL0_ATW4_SYSTEM_ID_SIZE = 0x00010000
local SI_CL0_SYSTEM_ID_PHYS_BASE = 0x20000D0400000

local module = {}

function module.create(ctx, runtime)


    local si_gic_trace = runtime.si_gic_trace
    local si_gic_trace_limit = runtime.si_gic_trace_limit

    local block = {}

    function block.define_interrupt_controller(
        ctx,
        platform,
        qemu_instance)
        local gic = {
            moduletype = "arm_gicv3";
            args = {qemu_instance};
            dist_iface = {
                address = SI_CL0_GICD_VIEW1_BASE;
                size = SI_GICD_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 10;
                aliases = {
                    view0_functional = {
                        address = SI_CL0_GICD_VIEW0_BASE;
                        size = SI_GICD_SIZE;
                    };
                };
            };
            redist_iface_0 = {
                address = SI_CL0_GICR_VIEW1_BASE;
                size = SI_GICR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 10;
                aliases = {
                    view0_functional = {
                        address = SI_CL0_GICR_VIEW0_BASES[1];
                        size = SI_GICR_SIZE;
                    };
                };
            };
            redist_region = {1};
            num_cpus = 1;
            num_spi = 384;
        }
        assert(
            platform.si_cl0_gic == nil,
            "duplicate object: platform.si_cl0_gic")
        platform.si_cl0_gic = gic
    end

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_scr == nil,
            "duplicate object: platform.si_cl0_scr")
        platform.si_cl0_scr = {
            moduletype = "host_scr";
            cl1_present = true;
            cl0_config_0 = 0x01201717;
            cl0_config_1 = 0x01100002;
            cl0_config_2 = 0x00000034;
            cl0_c0_config_0 = 0x01000001;
            cl0_c0_config_1 = 0x01001000;
            cl0_c0_config_2 = 0x01200000;
            cl0_c0_config_3 = 0x00000000;
            target_socket = {
                address = SI_CL0_SCR_BASE;
                size = SI_CL0_SCR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        assert(
            platform.si_cl0_timer_cntctl == nil,
            "duplicate object: platform.si_cl0_timer_cntctl")
        platform.si_cl0_timer_cntctl = {
            moduletype = "host_gtimer";
            args = {"&platform.css_system_counter"};
            target_socket = {
                address = SI_CL0_TIMER_CNTCTL_BASE;
                size = SI_CL0_TIMER_CNTCTL_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        assert(
            platform.si_cl0_timer_cntbase == nil,
            "duplicate object: platform.si_cl0_timer_cntbase")
        platform.si_cl0_timer_cntbase = {
            moduletype = "host_gtimer";
            args = {"&platform.css_system_counter"};
            counter_base = true;
            irq = {bind = "&si_cl0_gic.spi_in_" .. SI_CL0_SYSTEM_TIMER_SPI};
            target_socket = {
                address = SI_CL0_TIMER_CNT_BASE;
                size = SI_CL0_TIMER_CNT_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        assert(
            platform.si_cl0_watchdog == nil,
            "duplicate object: platform.si_cl0_watchdog")
        platform.si_cl0_watchdog = {
            moduletype = "zena_watchdog";
            clock_frequency = SI_CL0_WDOG_CLOCK_HZ;
            control = {
                address = SI_CL0_WDOG_CONTROL_BASE;
                size = SI_CL0_WDOG_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            refresh = {
                address = SI_CL0_WDOG_REFRESH_BASE;
                size = SI_CL0_WDOG_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            ws0 = {bind = "&si_cl0_gic.spi_in_" .. SI_CL0_WATCHDOG_WS0_SPI};
            ws1 = {bind = "&host_reset_ctrl.si_watchdog_reset"};
            log_level = 0;
        }
        assert(
            platform.si_cl0_pll == nil,
            "duplicate object: platform.si_cl0_pll")
        platform.si_cl0_pll = {
            moduletype = "host_system_pll";
            lock_mask = 0x00000001;
            target_socket = {
                address = SI_CL0_SMD_EXPANSION_PHYS_BASE;
                size = SI_CL0_PLL_SIZE;
                bind = "&smd_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        assert(
            platform.si_cl0_system_id == nil,
            "duplicate object: platform.si_cl0_system_id")
        platform.si_cl0_system_id = {
            moduletype = "host_scr";
            system_id = 0x0000073c;
            soc_id = 0x0000073c;
            pidr4 = 0x00000004;
            pidr0 = 0x0000003c;
            pidr1 = 0x000000b7;
            pidr2 = 0x0000000b;
            pidr3 = 0x00000000;
            cidr0 = 0x0000000d;
            cidr1 = 0x000000f0;
            cidr2 = 0x00000005;
            cidr3 = 0x000000b1;
            target_socket = {
                address = SI_CL0_SYSTEM_ID_PHYS_BASE;
                size = SI_CL0_ATW4_SYSTEM_ID_SIZE;
                bind = "&smd_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
    end

    function block.define_multiview(ctx, platform)
        assert(
            platform.si_gic_multiview == nil,
            "duplicate object: platform.si_gic_multiview")
        platform.si_gic_multiview = {
            moduletype = "gicx00_multiview";
            trace = si_gic_trace;
            trace_limit = si_gic_trace_limit;
            backend_dist_base = SI_GIC_BACKEND_DIST_BASE;
            backend_redist_base = SI_GIC_BACKEND_REDIST_BASE;
            backend_redist_stride = SI_GICR_SIZE;
            backend_redist_count = SI_GIC_BACKEND_REDIST_COUNT;
            view_redist_stride = SI_GICR_SIZE;
            view1_redist_first = SI_GIC_VIEW1_REDIST_FIRST;
            view2_redist_first = SI_GIC_VIEW2_REDIST_FIRST;
            spi_count = SI_GIC_SPI_COUNT;
            view0_dist_cfgid = {
                address = SI_CL0_GICD_VIEW0_BASE + SI_GIC_DIST_CFGID_OFFSET;
                size = SI_GIC_DIST_CFGID_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
            view0_dist_iviewr = {
                address = SI_CL0_GICD_VIEW0_BASE + SI_GIC_DIST_IVIEWR_OFFSET;
                size = SI_GIC_DIST_IVIEWR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 0;
            };
        }

        for i = 1,4 do
            platform.si_gic_multiview["view0_redist_" .. i] = {
                address = SI_CL0_GICR_VIEW0_BASES[i + 1];
                size = SI_GICR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 20;
            }
        end

        platform.si_gic_multiview.view0_redist_0_pwrr = {
            address = SI_CL0_GICR_VIEW0_BASES[1] + SI_GIC_REDIST_PWRR_OFFSET;
            size = SI_GIC_REDIST_REGISTER_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        }
        platform.si_gic_multiview.view0_redist_0_viewr = {
            address = SI_CL0_GICR_VIEW0_BASES[1] + SI_GIC_REDIST_VIEWR_OFFSET;
            size = SI_GIC_REDIST_REGISTER_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        }
        platform.si_gic_multiview.view0_redist_0_flushr = {
            address = SI_CL0_GICR_VIEW0_BASES[1] + SI_GIC_REDIST_FLUSHR_OFFSET;
            size = SI_GIC_REDIST_REGISTER_SIZE;
            bind = "&si_cl0_router.initiator_socket";
            priority = 0;
        }
    end

    return block
end

return module
