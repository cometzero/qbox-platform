-- GIC SPI constants are zero-based SPI socket indices (not architectural INTIDs).
local AP_SID_BASE = 0x1A4A0000
local AP_RGIC2LGIC_MESSREG_BASE = 0x5FFF0000
local AP_SBIST_CLUSTER0_BASE = 0x140100000
local AP_SI_CL0_FMU_BASE = 0x2A510000
local AP_SID_SIZE = 0x00010000
local AP_RGIC2LGIC_MESSREG_SIZE = 0x00010000
local AP_SBIST_SIZE = 0x00100000
local AP_NS_WDOG_WS1_SPI = 51

local syscore = {}

function syscore.define_reset(ctx, platform, hw)
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

    assert(platform.ap_cold_reset_fanout == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cold_reset_fanout = {
            moduletype = "reset_fanout";
            trace = options.getenv_bool_or("QBOX_APOLLO_RESET_TRACE", false);
            reset_out = {bind = options.ap_cold_reset_bind_targets()};
            log_level = 0;
        }
    end

    assert(platform.apollo_system_reset_fanout == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.apollo_system_reset_fanout = {
            moduletype = "reset_fanout";
            trace = options.getenv_bool_or("QBOX_APOLLO_RESET_TRACE", false);
            reset_out = {bind = options.apollo_system_reset_bind_targets()};
            log_level = 0;
        }
    end

    assert(platform.ap_ns_watchdog_ws1_fanout == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_ns_watchdog_ws1_fanout = {
            moduletype = "signal_fanout";
            signal_out = {
                bind = "&ap_gic.spi_in_" .. AP_NS_WDOG_WS1_SPI ..
                    ";" .. ctx.modules.si_cl0.spi_target(ctx, "si_cl0_ap_ns_watchdog_ws1", "View1");
            };
        }
    end
end

function syscore.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_HW = hw.AP_HW

    assert(platform.ap_sid == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_sid = {
            moduletype = "host_scr";
            system_id = AP_HW.sid_system_id;
            soc_id = AP_HW.sid_soc_id;
            chip_id = AP_HW.sid_chip_id;
            pidr4 = AP_HW.sid_pidr4;
            pidr0 = AP_HW.sid_pidr0;
            pidr1 = AP_HW.sid_pidr1;
            pidr2 = AP_HW.sid_pidr2;
            pidr3 = AP_HW.sid_pidr3;
            cidr0 = AP_HW.sid_cidr0;
            cidr1 = AP_HW.sid_cidr1;
            cidr2 = AP_HW.sid_cidr2;
            cidr3 = AP_HW.sid_cidr3;
            target_socket = {
                address = AP_SID_BASE;
                size = AP_SID_SIZE;
                bind = "&system_router.initiator_socket";
            };
            log_level = 0;
        }
    end

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

    assert(platform.ap_cl0_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl0_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 3;
            parent_non_critical_record = 1;
            target_socket = {
                address = AP_ADDRESS.fmu_cl0;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    assert(platform.ap_cl1_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl1_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 18;
            parent_non_critical_record = 16;
            target_socket = {
                address = AP_ADDRESS.fmu_cl1;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    assert(platform.ap_cl2_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl2_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 33;
            parent_non_critical_record = 31;
            target_socket = {
                address = AP_ADDRESS.fmu_cl2;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    assert(platform.ap_cl3_ni710ae_fmu == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_cl3_ni710ae_fmu = {
            moduletype = "zena_ni710ae_fmu";
            node_index = 0xb;
            parent_critical_record = 48;
            parent_non_critical_record = 46;
            target_socket = {
                address = AP_ADDRESS.fmu_cl3;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            fault_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }
    end

    if options.enable_ap_cpus then
        for cluster = 0, 3 do
            assert(platform["ap_sbist_cluster" .. cluster] == nil, "duplicate Apollo object")
            platform["ap_sbist_cluster" .. cluster] = {
                moduletype = "apollo_sbist";
                cpu_base = cluster * 4;
                fmu_base = AP_SI_CL0_FMU_BASE;
                target_socket = {
                    address = AP_SBIST_CLUSTER0_BASE + (cluster * 0x04000000);
                    size = AP_SBIST_SIZE;
                    bind = "&system_router.initiator_socket";
                    priority = 0;
                };
                fmu_initiator = {bind = "&si_cl0_router.target_socket"};
                log_level = 0;
            }
        end
    end
end

return syscore
