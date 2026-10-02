-- SPDX-License-Identifier: BSD-3-Clause
return function(ctx, platform, hw)
    local options = ctx.options
    assert(platform.host_rse_si_mhu_pbx == nil, "duplicate Apollo object: host_rse_si_mhu_pbx")
    platform.host_rse_si_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "rse_to_si_cl0";
        protocol = "doorbell-bridge";
        tx_shmem = hw.HOST_RSE_SI_SSRAM_PHYS_BASE;
        rx_shmem = hw.HOST_RSE_SI_SSRAM_PHYS_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = hw.HOST_RSE_SI_MHU_PHYS_BASE;
            size = hw.RSE_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    assert(platform.host_rse_si_mhu_mbx == nil, "duplicate Apollo object: host_rse_si_mhu_mbx")
    platform.host_rse_si_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_rse";
        protocol = "doorbell-bridge";
        tx_shmem = hw.HOST_RSE_SI_SSRAM_PHYS_BASE;
        rx_shmem = hw.HOST_RSE_SI_SSRAM_PHYS_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = hw.HOST_RSE_SI_MHU_PHYS_BASE + hw.RSE_MHU_FRAME_SIZE;
            size = hw.RSE_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&rse_cpu_pass.target_signal_socket_"..
            hw.RSE_IRQ_SI_CL0_RSE_CMU_MHU_RECEIVER};
        log_level = 0;
    }


end
