-- SPDX-License-Identifier: BSD-3-Clause
local HOST_SI_PIK_PHYS_BASE = 0x400002A600000
local HOST_SI_SCR_PHYS_BASE = 0x400002A6B0000
local HOST_SI_CONTROL_WINDOW_SIZE = 0x00010000
local HOST_CSS_RGM_PHYS_BASE = 0x20000D0010000
local HOST_SYSTOP_PIK_PHYS_BASE = 0x20000D0200000
local HOST_CSS_COUNTERS_TIMERS_PHYS_BASE = 0x20000D0100000
local HOST_CSS_COUNTER_CONTROL_OFFSET = 0x00000000
local HOST_CSS_COUNTER_READ_OFFSET = 0x00010000
local HOST_CSS_COUNTER_SYNC_OFFSET = 0x00020000
local HOST_CSS_COUNTER_FRAME_SIZE = 0x00010000
local SYSTEM_RESET_REGISTER_SIZE = 0x00010000
local SYSTEM_COUNTER_FREQUENCY_HZ = 125000000
local SYSTEM_COUNTER_INTEGER_INCREMENT = 1

local system = {}

function system.define_control(ctx, platform)
    local options = ctx.options
    assert(platform.host_si_pik == nil, "duplicate Apollo object: host_si_pik")
    platform.host_si_pik = {
        moduletype = "host_ppu";
        trace = options.host_ppu_trace;
        trace_limit = options.host_ppu_trace_limit;
        target_socket = {
            address = HOST_SI_PIK_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_si_scr == nil, "duplicate Apollo object: host_si_scr")
    platform.host_si_scr = {
        moduletype = "host_scr";
        cl1_present = true;
        target_socket = {
            address = HOST_SI_SCR_PHYS_BASE;
            size = HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&system_router.initiator_socket";
        };
        log_level = 0;
    }
end

function system.define_reset(ctx, platform)
    assert(platform.host_reset_ctrl == nil, "duplicate Apollo object: host_reset_ctrl")
    platform.host_reset_ctrl = {
        moduletype = "zena_reset_ctrl";
        rgm = {
            address = HOST_CSS_RGM_PHYS_BASE;
            size = SYSTEM_RESET_REGISTER_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        pik = {
            address = HOST_SYSTOP_PIK_PHYS_BASE;
            size = SYSTEM_RESET_REGISTER_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        ap_reset = {bind = "&ap_cold_reset_fanout.reset_in"};
        log_level = 0;
    }
end

function system.define_counter(ctx, platform)
    assert(platform.css_system_counter == nil, "duplicate Apollo object: css_system_counter")
    platform.css_system_counter = {
        moduletype = "arm_system_counter";
        input_frequency_hz = SYSTEM_COUNTER_FREQUENCY_HZ;
        integer_increment = SYSTEM_COUNTER_INTEGER_INCREMENT;
        reported_frequency_hz = SYSTEM_COUNTER_FREQUENCY_HZ;
        construction_priority = -298;
    }

    assert(platform.host_css_counters_timers == nil, "duplicate Apollo object: host_css_counters_timers")
    platform.host_css_counters_timers = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        counter_control = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE +
                HOST_CSS_COUNTER_CONTROL_OFFSET;
            size = HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_css_counters_timers_read == nil, "duplicate Apollo object: host_css_counters_timers_read")
    platform.host_css_counters_timers_read = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        counter_read = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + HOST_CSS_COUNTER_READ_OFFSET;
            size = HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_css_counters_timers_sync == nil, "duplicate Apollo object: host_css_counters_timers_sync")
    platform.host_css_counters_timers_sync = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        sync_frame = true;
        target_socket = {
            address = HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + HOST_CSS_COUNTER_SYNC_OFFSET;
            size = HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }
end

return system
