-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform)
    if ctx.config.runtime_injection.enabled then
        -- Interpose only in the opt-in profile; preserve the existing GIC sink.
        platform.ap_i2c5_irq_fault = {
            moduletype = "signal_fault_injector";
            signal_out = platform.ap_dw_i2c_5.irq;
        }
        platform.ap_dw_i2c_5.irq = {bind = "&ap_i2c5_irq_fault.signal_in"}
        platform.apollo_runtime_injection = {
            moduletype = "apollo_runtime_injection";
            mhu_target = "platform.host_ap_si_cl1_mhu_pbx";
            signal_target = "platform.ap_i2c5_irq_fault";
            system_reset = {bind = "&apollo_system_reset_fanout.reset_in"};
            args = {
                "&platform.si_gic_multiview";
                "&platform.si_cl0_gic";
                "&platform.si_cl1_gic";
                "&platform.si_cl0_ssu";
                "&platform.css_system_counter";
                "&platform.host_smd_gpio";
                "&platform.rse_gpio_0";
                "&platform.rse_gpio_1";
            };
        }
    end

end
