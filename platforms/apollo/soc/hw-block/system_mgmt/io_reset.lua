-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    local smd_gpio_init_inputs = ctx.getenv_number_or(
        "QBOX_APOLLO_SMD_GPIO_INIT_INPUTS", "0")

    assert(platform.host_smdexp2smd_atu == nil, "duplicate Apollo object: host_smdexp2smd_atu")
    platform.host_smdexp2smd_atu = {
        moduletype = "rse_atu";
        trace = options.atu_trace;
        trace_limit = options.atu_trace_limit;
        trace_filter = options.atu_trace_filter;
        trace_address_min = hw.atu_trace_address_min;
        trace_address_max = hw.atu_trace_address_max;
        build_config = hw.SYSTEM_ATU_BUILD_CONFIG;
        target_socket = {
            address = hw.HOST_SMDEXP2SMD_ATU_PHYS_BASE;
            size = hw.HOST_SI_CONTROL_WINDOW_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        log_level = 0;
    }

    assert(platform.host_smd_gpio == nil, "duplicate Apollo object: host_smd_gpio")
    platform.host_smd_gpio = {
        moduletype = "qemu_pl061";
        args = {"&platform.qemu_inst"};
        init_inputs = smd_gpio_init_inputs;
        pullups = 0;
        pulldowns = 0;
        mem = {
            address = hw.SMD_GPIO.phys_base;
            size = hw.SMD_GPIO.size;
            bind = "&smd_router.initiator_socket";
        };
        irq = options.enable_ap_cpus and {
            bind = "&ap_gic.spi_in_"..hw.SMD_GPIO.ap_irq;
        } or nil;
    }

    assert(platform.host_smd_gpio_cold_reset == nil, "duplicate Apollo object: host_smd_gpio_cold_reset")
    platform.host_smd_gpio_cold_reset = {
        moduletype = "qemu_device_cold_reset";
        args = {"&platform.host_smd_gpio"};
    }

    assert(platform.host_reset_ctrl == nil, "duplicate Apollo object: host_reset_ctrl")
    platform.host_reset_ctrl = {
        moduletype = "zena_reset_ctrl";
        rgm = {
            address = hw.HOST_CSS_RGM_PHYS_BASE;
            size = hw.SYSTEM_RESET_REGISTER_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        pik = {
            address = hw.HOST_SYSTOP_PIK_PHYS_BASE;
            size = hw.SYSTEM_RESET_REGISTER_SIZE;
            bind = "&smd_router.initiator_socket";
        };
        ap_reset = {bind = "&ap_cold_reset_fanout.reset_in"};
        log_level = 0;
    }


end
