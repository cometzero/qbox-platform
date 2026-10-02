local options = {}

function options.resolve(ctx)
    local si_cl1_image = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_IMAGE",
        ctx.apollo_root .. "build/tmp_baremetal/deploy/images/apollo-qvp/zephyr-demos-cl1.bin")
    local si_cl1_log = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_LOG",
        ctx.apollo_root .. "build/qbox-apollo-fvp/full-system/qbox-safety-island-cl1.log")
    local si_cl1_uart_read_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_UART_READ_FILE",
        "/dev/null")
    local si_cl1_uart_poll_read = si_cl1_uart_read_file ~= "/dev/null"
    local mhu_trace = ctx.getenv_bool_or("QBOX_APOLLO_FULL_SI_CL1_MHU_TRACE", false)
    local mhu_trace_file = ctx.getenv_or(
        "QBOX_APOLLO_FULL_SI_CL1_MHU_TRACE_FILE",
        ctx.apollo_root .. "build/qbox-apollo-fvp/full-system/si-cl1-mhuv3-trace.log")
    local mhu_trace_limit =
        ctx.getenv_number_or("QBOX_APOLLO_FULL_SI_CL1_MHU_TRACE_LIMIT", "4096")
    return {
        entry = 0x14000647c; -- Current Zephyr reset entry.
        si_cl1_image = si_cl1_image;
        si_cl1_log = si_cl1_log;
        si_cl1_uart_read_file = si_cl1_uart_read_file;
        si_cl1_uart_poll_read = si_cl1_uart_poll_read;
        mhu_trace = mhu_trace;
        mhu_trace_file = mhu_trace_file;
        mhu_trace_limit = mhu_trace_limit;
    }
end

return options
