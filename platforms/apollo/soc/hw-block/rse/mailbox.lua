local module = {}

function module.create(ctx, hardware, _runtime, _interrupts)
    local mhu_trace = ctx.options.mhu_trace
    local mhu_trace_file = ctx.options.mhu_trace_file
    local mhu_trace_limit = ctx.options.mhu_trace_limit
    local RSE_ADDRESS = hardware.RSE_ADDRESS
    local RSE_SIZE = hardware.RSE_SIZE
    local RSE_IRQ = hardware.RSE_IRQ

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.rse_mhu0_sender_s == nil,
            "duplicate object: platform.rse_mhu0_sender_s")
        platform.rse_mhu0_sender_s = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "rse_ap_monitor_local";
            protocol = "doorbell";
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = RSE_ADDRESS.mhu0_sender_secure;
                size = RSE_SIZE.local_mhu_frame;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.rse_mhu0_receiver_s == nil,
            "duplicate object: platform.rse_mhu0_receiver_s")
        platform.rse_mhu0_receiver_s = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "rse_ap_monitor_local";
            protocol = "doorbell";
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = RSE_ADDRESS.mhu0_receiver_secure;
                size = RSE_SIZE.local_mhu_frame;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = "&rse_cpu_pass.target_signal_socket_" ..
                RSE_IRQ.cmu_mhu0_receiver};
            log_level = 0;
        }

        assert(
            platform.rse_mhu2_sender_s == nil,
            "duplicate object: platform.rse_mhu2_sender_s")
        platform.rse_mhu2_sender_s = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "rse_to_ap_s";
            protocol = "doorbell-bridge";
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = RSE_ADDRESS.mhu2_sender_secure;
                size = RSE_SIZE.local_mhu_frame;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.rse_mhu2_receiver_s == nil,
            "duplicate object: platform.rse_mhu2_receiver_s")
        platform.rse_mhu2_receiver_s = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "ap_s_to_rse";
            protocol = "doorbell-bridge";
            doorbell_commit_on_notify = true;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = RSE_ADDRESS.mhu2_receiver_secure;
                size = RSE_SIZE.local_mhu_frame;
                bind = "&rse_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = "&rse_cpu_pass.target_signal_socket_" ..
                RSE_IRQ.cmu_mhu2_receiver};
            log_level = 0;
        }
    end

    return block
end

return module
