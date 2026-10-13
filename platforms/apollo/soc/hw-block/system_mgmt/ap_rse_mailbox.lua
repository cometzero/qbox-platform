-- SPDX-License-Identifier: BSD-3-Clause
local HOST_AP_RSE_MHU_PHYS_BASE = 0x300001B600000
local MHU_V3_FRAME_SIZE = 0x00030000
local HOST_AP_RSE_MAILBOX_PHYS_BASE = 0xFFFFC000

local ap_rse_mailbox = {}

function ap_rse_mailbox.define(ctx, platform)
    local options = ctx.options
    local ap_rse_ps_proxy = options.rse_ps_proxy
    local ap_rse_mhu_protocol = ap_rse_ps_proxy and "rse-ps-proxy" or "doorbell-bridge"
    local ap_rse_mhu_pbx_pair = ap_rse_ps_proxy and "ap_rse_ps_proxy" or "ap_s_to_rse"
    local ap_rse_mhu_mbx_pair = ap_rse_ps_proxy and "ap_rse_ps_proxy" or "rse_to_ap_s"

    assert(platform.host_ap_rse_mhu_pbx == nil, "duplicate Apollo object: host_ap_rse_mhu_pbx")
    platform.host_ap_rse_mhu_pbx = {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = ap_rse_mhu_pbx_pair;
        protocol = ap_rse_mhu_protocol;
        tx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        rx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        target_socket = {
            address = HOST_AP_RSE_MHU_PHYS_BASE;
            size = MHU_V3_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }

    assert(platform.host_ap_rse_mhu_mbx == nil, "duplicate Apollo object: host_ap_rse_mhu_mbx")
    platform.host_ap_rse_mhu_mbx = {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = ap_rse_mhu_mbx_pair;
        protocol = ap_rse_mhu_protocol;
        doorbell_commit_on_notify = not ap_rse_ps_proxy;
        tx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        rx_shmem = HOST_AP_RSE_MAILBOX_PHYS_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        target_socket = {
            address = HOST_AP_RSE_MHU_PHYS_BASE + MHU_V3_FRAME_SIZE;
            size = MHU_V3_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        log_level = 0;
    }
end

return ap_rse_mailbox
