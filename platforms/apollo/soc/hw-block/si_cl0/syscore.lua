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
local SI_CL0_CLUSTER_UTILITY_MGI_STRIDE = 0x04000000
local SI_CL0_CLUSTER_UTILITY_MGI_SIZE = 0x00010000
local SI_CL0_ATW16_SMCF_SMD_MGI_SIZE = 0x00010000
local SI_CL0_SMCF_SMD_MGI_PHYS_BASE = 0x20000D8100000
local SI_CL0_AP_CLUSTER_MGI_PHYS_BASE = 0x140200000
local SI_CL0_SYSTEM_ID_PHYS_BASE = 0x20000D0400000

local module = {}

function module.create(ctx, hardware, _runtime, interrupts)
    local SI_CONST = hardware
    local irq_routes = interrupts

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
                        address = SI_CONST.SI_CL0_GICD_VIEW0_BASE;
                        size = SI_GICD_SIZE;
                    };
                };
            };
            redist_iface_0 = {
                address = SI_CL0_GICR_VIEW1_BASE;
                size = SI_CONST.SI_GICR_SIZE;
                bind = "&si_cl0_router.initiator_socket";
                priority = 10;
                aliases = {
                    view0_functional = {
                        address = SI_CONST.SI_CL0_GICR_VIEW0_BASES[1];
                        size = SI_CONST.SI_GICR_SIZE;
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
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_system_timer", "View1")};
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
            clock_frequency = SI_CONST.SI_CL0_WDOG_CLOCK_HZ;
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
            ws0 = {bind = irq_routes.spi_target(
                ctx, "si_cl0_watchdog_ws0", "View1")};
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
                address = SI_CONST.SI_CL0_SMD_EXPANSION_PHYS_BASE;
                size = SI_CL0_PLL_SIZE;
                bind = "&smd_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }

        assert(
            platform.si_cl0_smcf_smd_mgi == nil,
            "duplicate object: platform.si_cl0_smcf_smd_mgi")
        platform.si_cl0_smcf_smd_mgi = {
            moduletype = "host_smcf_mgi";
            group_id = 0x00000000;
            monitor_count = 1;
            data_values_per_monitor = 12;
            data_width_bits = 32;
            target_socket = {
                address = SI_CL0_SMCF_SMD_MGI_PHYS_BASE;
                size = SI_CL0_ATW16_SMCF_SMD_MGI_SIZE;
                bind = "&smd_router.initiator_socket";
                priority = 0;
            };
            log_level = 0;
        }
        for i = 0,3 do
            assert(
                platform["si_cl0_smcf_ap_cluster_mgi_" .. i] == nil,
                "duplicate object: platform." .. ("si_cl0_smcf_ap_cluster_mgi_" .. i))
            platform["si_cl0_smcf_ap_cluster_mgi_" .. i] = {
                moduletype = "host_smcf_mgi";
                group_id = i + 1;
                monitor_count = 1;
                data_values_per_monitor = 12;
                data_width_bits = 32;
                target_socket = {
                    address = SI_CL0_AP_CLUSTER_MGI_PHYS_BASE +
                        (i * SI_CL0_CLUSTER_UTILITY_MGI_STRIDE);
                    size = SI_CL0_CLUSTER_UTILITY_MGI_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                log_level = 0;
            }
        end

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

    return block
end

return module
