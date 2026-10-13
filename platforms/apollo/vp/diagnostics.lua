-- SPDX-License-Identifier: BSD-3-Clause
-- Apply host diagnostics after the selected profile has assembled its devices.
local diagnostics = {}

function diagnostics.configure(ctx, platform)
    local options = ctx.options
    for _, name in ipairs({
        "rse_mhu0_sender_s";
        "rse_mhu0_receiver_s";
        "rse_mhu2_sender_s";
        "rse_mhu2_receiver_s";
        "host_rse_si_mhu_pbx";
        "host_rse_si_mhu_mbx";
        "host_ap_rse_mhu_pbx";
        "host_ap_rse_mhu_mbx";
        "host_ap_si_ns_scmi_mhu_pbx";
        "host_ap_si_ns_scmi_mhu_mbx";
        "host_ap_si_scmi_mhu_pbx";
        "host_ap_si_scmi_mhu_mbx";
        "host_ap_si_cl1_mhu_pbx";
        "host_ap_si_cl1_mhu_mbx";
        "host_ap_si_pfdi_monitor_mhu_pbx";
        "host_ap_si_pfdi_monitor_mhu_mbx";
        "si_cl0_ap_ns_mhu_pbx";
        "si_cl0_ap_ns_mhu_mbx";
        "si_cl0_ap_scmi_mhu_pbx";
        "si_cl0_ap_scmi_mhu_mbx";
        "si_cl0_ap_pfdi_monitor_mhu_pbx";
        "si_cl0_ap_pfdi_monitor_mhu_mbx";
        "si_cl0_rse_mhu_pbx";
        "si_cl0_rse_mhu_mbx";
        "si_cl0_pfdi_mhu_pbx";
        "si_cl0_pfdi_mhu_mbx";
    }) do
        if platform[name] then
            platform[name].trace_file = options.mhu_trace_file
        end
    end
    for _, name in ipairs({
        "si_cl1_hipc_mhu_pbx";
        "si_cl1_hipc_mhu_mbx";
        "si_cl1_pfdi_mhu_pbx";
        "si_cl1_pfdi_reply_mhu_mbx";
    }) do
        if platform[name] then
            platform[name].trace_file = ctx.config.si_cl1.mhu_trace_file
        end
    end
    if platform.rse_cc3xx then
        platform.rse_cc3xx.stats_file = options.cc3xx_stats_file
    end
    if platform.rse_cpu_pass and platform.rse_cpu_pass.rse_cc3xx then
        platform.rse_cpu_pass.rse_cc3xx.stats_file = options.cc3xx_stats_file
    end
    for i = 0, options.AP_NUM_CPUS - 1 do
        local cpu = platform["ap_cpu_" .. i]
        if cpu then
            cpu.trace_pc = options.ap_pc_trace
            cpu.trace_pc_file = options.ap_pc_trace_file
            cpu.trace_pc_interval = options.ap_pc_trace_interval
            cpu.trace_pc_limit = options.ap_pc_trace_limit
            cpu.trace_exception_state = options.ap_exception_trace
        end
    end
end

return diagnostics
