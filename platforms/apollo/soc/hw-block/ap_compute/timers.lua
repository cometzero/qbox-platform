local timers = {}

function timers.define_watchdog(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_IRQ = hw.AP_IRQ
    local AP_HW = hw.AP_HW

    assert(platform.ap_watchdog_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_watchdog_0 = {
            moduletype = "zena_watchdog";
            trace = options.getenv_bool_or("QBOX_APOLLO_RESET_TRACE", false);
            clock_frequency = AP_HW.arch_timer_frequency_hz;
            control = {
                address = AP_ADDRESS.ns_wdog_control;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            refresh = {
                address = AP_ADDRESS.ns_wdog_refresh;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            ws0 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.ns_wdog_ws0};
            ws1 = {bind = "&ap_ns_watchdog_ws1_fanout.signal_in"};
        }
    end
end

function timers.define(ctx, platform, hw)
    local options = ctx.options
    local AP_ADDRESS = hw.AP_ADDRESS
    local AP_SIZE = hw.AP_SIZE
    local AP_IRQ = hw.AP_IRQ
    local AP_HW = hw.AP_HW

    assert(platform.ap_timer_mem == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_timer_mem = {
            moduletype = "qemu_arm_arch_timer_mmio";
            args = {"&platform.ap_qemu_inst"};
            cntfrq = AP_HW.arch_timer_frequency_hz;
            nr_frames = AP_HW.arch_timer_frame_count;
            view_size = AP_SIZE.sys_timer;
            frame_offset_0 = AP_ADDRESS.sys_counter_non_secure - AP_ADDRESS.sys_timctl;
            frame_id_0 = 0;
            frame_offset_1 = AP_ADDRESS.sys_counter_secure - AP_ADDRESS.sys_timctl;
            frame_id_1 = 1;
            mem = {
                address = AP_ADDRESS.sys_timctl;
                size = (AP_ADDRESS.sys_counter_non_secure - AP_ADDRESS.sys_timctl) + AP_SIZE.sys_timer;
                bind = "&system_router.initiator_socket";
            };
            -- sc_vector ports are named irq_0/irq_1, not irq.0/irq.1.
            irq_0 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.sys_timer_non_secure};
            irq_1 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.sys_timer_secure};
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
            trace = options.getenv_bool_or("QBOX_APOLLO_RESET_TRACE", false);
            clock_frequency = AP_HW.arch_timer_frequency_hz;
            control = {
                address = AP_ADDRESS.secure_wdog_control;
                size = AP_SIZE.secure_wdog;
                bind = "&system_router.initiator_socket";
            };
            refresh = {
                address = AP_ADDRESS.secure_wdog_refresh;
                size = AP_SIZE.secure_wdog;
                bind = "&system_router.initiator_socket";
            };
            ws0 = {bind = "&ap_gic.spi_in_" .. AP_IRQ.secure_wdog_ws0};
            ws1 = {bind = "&host_reset_ctrl.ap_s_watchdog_reset"};
            log_level = 0;
        }
    end
end

return timers
