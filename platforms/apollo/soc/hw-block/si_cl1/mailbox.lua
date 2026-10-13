local SI_CL1_HIPC_PBX_BASE = 0x39000000
local SI_CL1_HIPC_MBX_BASE = 0x39040000
local SI_CL1_PFDI_PBX_BASE = 0x39200000
local SI_CL1_PFDI_MBX_BASE = 0x39240000
local SI_CL1_HIPC_MHU_SIZE = 0x00030000
local SI_CL1_PFDI_MHU_SIZE = 0x00020000
local SI_CL1_PFDI_MHU_CHANNEL_BASE = 2

local module = {}

function module.create(ctx, hardware, runtime, interrupts)
    local SI_CL1_SCMI_SHMEM_BASE = hardware.SI_CL1_SCMI_SHMEM_BASE
    local SI_CL1_MHU_CHANNELS = hardware.SI_CL1_MHU_CHANNELS
    local SI_CL1_SCMI_MSG_SIZE_PER_CORE = hardware.SI_CL1_SCMI_MSG_SIZE_PER_CORE
    local SI_CL1_PFDI_CORE_COUNT = hardware.SI_CL1_PFDI_CORE_COUNT
    local irq_routes = interrupts
    local mhu_trace = runtime.mhu_trace
    local mhu_trace_file = runtime.mhu_trace_file
    local mhu_trace_limit = runtime.mhu_trace_limit

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl1_hipc_mhu_pbx == nil,
            "duplicate object: platform.si_cl1_hipc_mhu_pbx")
        platform.si_cl1_hipc_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "apollo_si_cl1_to_ap";
            protocol = "doorbell-bridge";
            channel_count = SI_CL1_MHU_CHANNELS;
            trace = mhu_trace;
            trace_file = mhu_trace_file;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_HIPC_PBX_BASE;
                size = SI_CL1_HIPC_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl1_hipc_pbx", "View2")};
            log_level = 0;
        }

        assert(
            platform.si_cl1_hipc_mhu_mbx == nil,
            "duplicate object: platform.si_cl1_hipc_mhu_mbx")
        platform.si_cl1_hipc_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "apollo_ap_to_si_cl1";
            protocol = "doorbell-bridge";
            channel_count = SI_CL1_MHU_CHANNELS;
            trace = mhu_trace;
            trace_file = mhu_trace_file;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_HIPC_MBX_BASE;
                size = SI_CL1_HIPC_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl1_hipc_mbx", "View2")};
            log_level = 0;
        }

        assert(
            platform.si_cl1_pfdi_mhu_pbx == nil,
            "duplicate object: platform.si_cl1_pfdi_mhu_pbx")
        platform.si_cl1_pfdi_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "apollo_si_cl1_pfdi";
            protocol = "doorbell-bridge";
            scmi_transport = "pfdi-monitor";
            channel_count = SI_CL1_MHU_CHANNELS;
            tx_shmem = SI_CL1_SCMI_SHMEM_BASE;
            rx_shmem = SI_CL1_SCMI_SHMEM_BASE;
            scmi_channel_stride = SI_CL1_SCMI_MSG_SIZE_PER_CORE;
            scmi_channel_base_index = SI_CL1_PFDI_MHU_CHANNEL_BASE;
            scmi_channel_count = SI_CL1_PFDI_CORE_COUNT;
            init_shmem = true;
            requester_hold_enable = true;
            trace = mhu_trace;
            trace_file = mhu_trace_file;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_PFDI_PBX_BASE;
                size = SI_CL1_PFDI_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl1_pfdi", "View2")};
            log_level = 0;
        }

        for i = 0,(SI_CL1_PFDI_CORE_COUNT-1) do
            platform.si_cl1_pfdi_mhu_pbx["requester_hold_" .. i] = {
                bind = "&si_cl1_cpu_" .. i .. ".sync_hold";
            }
        end

        assert(
            platform.si_cl1_pfdi_reply_mhu_mbx == nil,
            "duplicate object: platform.si_cl1_pfdi_reply_mhu_mbx")
        platform.si_cl1_pfdi_reply_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "apollo_si_cl0_pfdi_reply";
            protocol = "doorbell-bridge";
            scmi_transport = "pfdi-monitor";
            channel_count = SI_CL1_MHU_CHANNELS;
            tx_shmem = SI_CL1_SCMI_SHMEM_BASE;
            rx_shmem = SI_CL1_SCMI_SHMEM_BASE;
            scmi_channel_stride = SI_CL1_SCMI_MSG_SIZE_PER_CORE;
            scmi_channel_base_index = SI_CL1_PFDI_MHU_CHANNEL_BASE;
            scmi_channel_count = SI_CL1_PFDI_CORE_COUNT;
            init_shmem = false;
            trace = mhu_trace;
            trace_file = mhu_trace_file;
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_PFDI_MBX_BASE;
                size = SI_CL1_PFDI_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            log_level = 0;
        }
    end

    return block
end

return module
