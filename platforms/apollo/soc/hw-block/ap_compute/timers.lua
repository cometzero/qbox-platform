-- GIC SPI constants are zero-based SPI socket indices (not architectural INTIDs).
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

local timers = {}

function timers.define_watchdog(ctx, platform, hw)
    local options = ctx.options
    local AP_SIZE = hw.AP_SIZE
    local AP_HW = hw.AP_HW

    assert(platform.ap_watchdog_0 == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_watchdog_0 = {
            moduletype = "zena_watchdog";
            trace = options.getenv_bool_or("QBOX_APOLLO_RESET_TRACE", false);
            clock_frequency = AP_HW.arch_timer_frequency_hz;
            control = {
                address = AP_NS_WDOG_CONTROL_BASE;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            refresh = {
                address = AP_NS_WDOG_REFRESH_BASE;
                size = AP_SIZE.register_window;
                bind = "&system_router.initiator_socket";
            };
            ws0 = {bind = "&ap_gic.spi_in_" .. AP_NS_WDOG_WS0_SPI};
            ws1 = {bind = "&ap_ns_watchdog_ws1_fanout.signal_in"};
        }
    end
end

function timers.define(ctx, platform, hw)
    local options = ctx.options
    local AP_SIZE = hw.AP_SIZE
    local AP_HW = hw.AP_HW

    assert(platform.ap_timer_mem == nil, "duplicate Apollo object")
    if options.enable_ap_cpus then
        platform.ap_timer_mem = {
            moduletype = "qemu_arm_arch_timer_mmio";
            args = {"&platform.ap_qemu_inst"};
            cntfrq = AP_HW.arch_timer_frequency_hz;
            nr_frames = AP_HW.arch_timer_frame_count;
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
            trace = options.getenv_bool_or("QBOX_APOLLO_RESET_TRACE", false);
            clock_frequency = AP_HW.arch_timer_frequency_hz;
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

return timers
