local options = {}

function options.resolve(ctx)
    local si_cl0_image = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_IMAGE",
        ctx.apollo_root .. "build/tmp_baremetal/deploy/images/apollo-qvp/si0_ramfw.bin")
    local si_cl0_log = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_LOG",
        ctx.getenv_or(
            "QBOX_RDASPEN_SCP_LOG",
            ctx.apollo_root .. "build/qbox-apollo-fvp/full-system/qbox-safety-island-cl0.log"))
    local si_cl0_uart_read_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL0_UART_READ_FILE",
        "/dev/null")
    local si_cl0_uart_poll_read = si_cl0_uart_read_file ~= "/dev/null"
    local si_gic_trace = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_GIC_MULTIVIEW_TRACE", false)
    local si_gic_trace_limit =
        ctx.getenv_number_or("QBOX_APOLLO_FULL_SI_GIC_MULTIVIEW_TRACE_LIMIT", "256")
    local si_cmn_trace = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_CMN_TRACE", false)
    local si_cmn_trace_limit =
        ctx.getenv_number_or("QBOX_APOLLO_FULL_SI_CMN_TRACE_LIMIT", "512")
    return {
        entry = 0x120000000; -- Current SCP RAM firmware reset entry.
        si_cl0_image = si_cl0_image;
        si_cl0_log = si_cl0_log;
        si_cl0_uart_read_file = si_cl0_uart_read_file;
        si_cl0_uart_poll_read = si_cl0_uart_poll_read;
        si_gic_trace = si_gic_trace;
        si_gic_trace_limit = si_gic_trace_limit;
        si_cmn_trace = si_cmn_trace;
        si_cmn_trace_limit = si_cmn_trace_limit;
    }
end

return options
