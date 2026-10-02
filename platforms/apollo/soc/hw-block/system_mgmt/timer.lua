-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    assert(platform.css_system_counter == nil, "duplicate Apollo object: css_system_counter")
    platform.css_system_counter = {
        moduletype = "arm_system_counter";
        input_frequency_hz = hw.SYSTEM_COUNTER_FREQUENCY_HZ;
        integer_increment = hw.SYSTEM_COUNTER_INTEGER_INCREMENT;
        reported_frequency_hz = hw.SYSTEM_COUNTER_FREQUENCY_HZ;
        construction_priority = -298;
    }

    assert(platform.host_css_counters_timers == nil, "duplicate Apollo object: host_css_counters_timers")
    platform.host_css_counters_timers = {
        moduletype = "host_gtimer";
        args = {"&platform.css_system_counter"};
        counter_control = true;
        target_socket = {
            address = hw.HOST_CSS_COUNTERS_TIMERS_PHYS_BASE +
                hw.HOST_CSS_COUNTER_CONTROL_OFFSET;
            size = hw.HOST_CSS_COUNTER_FRAME_SIZE;
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
            address = hw.HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + hw.HOST_CSS_COUNTER_READ_OFFSET;
            size = hw.HOST_CSS_COUNTER_FRAME_SIZE;
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
            address = hw.HOST_CSS_COUNTERS_TIMERS_PHYS_BASE + hw.HOST_CSS_COUNTER_SYNC_OFFSET;
            size = hw.HOST_CSS_COUNTER_FRAME_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }


end
