-- SPDX-License-Identifier: BSD-3-Clause
-- Optional host monitor, QMP endpoints and cross-domain timer snapshot.
return function(ctx, platform)
    dofile(ctx.apollo_dir.."vp/test-wiring/runtime-injection.lua")(ctx, platform)
    dofile(ctx.apollo_dir.."vp/debug/qmp.lua")(platform, true)
    if ctx.config.monitor.enabled then
        platform.qbox_monitor = {
            moduletype = "monitor";
            bind_address = ctx.config.monitor.bind_address;
            server_port = ctx.config.monitor.port;
            runtime_mutation = ctx.config.runtime_injection.enabled;
            injection_service = ctx.config.runtime_injection.enabled and
                "platform.apollo_runtime_injection" or "";
            use_html_presentation = true;
        }
        print("QBox monitor dashboard: http://127.0.0.1:"..
              ctx.config.monitor.port.."/")
    end

    if ctx.options.enable_ap_cpus then
        platform.apollo_timer_snapshot = {
            moduletype = "apollo_timer_snapshot";
            args = {
                "&platform.host_css_counters_timers_read";
                "&platform.ap_cpu_counter_mirror_0";
                "&platform.ap_timer_counter_mirror";
                "&platform.si_cl0_cpu_counter_mirror";
                "&platform.si_cl0_timer_cntbase";
                "&platform.si_cl1_cpu_counter_mirror_0";
                "&platform.rse_cpu_pass.rse_lsc_counter";
                "&platform.rse_cpu_pass.rse_timer_0";
                "&platform.rse_cpu_pass.rse_timer_1";
                "&platform.rse_cpu_pass.rse_timer_2";
                "&platform.rse_cpu_pass.rse_timer_3";
            };
        }
    end
end
