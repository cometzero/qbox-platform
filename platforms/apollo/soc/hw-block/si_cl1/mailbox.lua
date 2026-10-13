-- SPI constants are socket indices; PPI constants are architectural INTIDs.
local SI_CL1_HIPC_PBX_SPI = 72 - 32
local SI_CL1_HIPC_MBX_SPI = 73 - 32
local SI_CL1_PFDI_SPI = 82 - 32

local SI_CL1_SCMI_SHMEM_BASE = 0x48000000
local SI_CL1_MHU_CHANNELS = 32
local SI_CL1_SCMI_MSG_SIZE_PER_CORE = 40
local SI_CL1_PFDI_CORE_COUNT = 4

local SI_CL1_HIPC_PBX_BASE = 0x39000000
local SI_CL1_HIPC_MBX_BASE = 0x39040000
local SI_CL1_PFDI_PBX_BASE = 0x39200000
local SI_CL1_PFDI_MBX_BASE = 0x39240000
local SI_CL1_HIPC_MHU_SIZE = 0x00030000
local SI_CL1_PFDI_MHU_SIZE = 0x00020000
local SI_CL1_PFDI_MHU_CHANNEL_BASE = 2

local module = {}

function module.create(ctx, runtime)

    local mhu_trace = runtime.mhu_trace
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
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_HIPC_PBX_BASE;
                size = SI_CL1_HIPC_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            irq = {bind = "&si_cl1_gic.spi_in_" .. SI_CL1_HIPC_PBX_SPI};
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
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_HIPC_MBX_BASE;
                size = SI_CL1_HIPC_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            irq = {bind = "&si_cl1_gic.spi_in_" .. SI_CL1_HIPC_MBX_SPI};
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
            trace_limit = mhu_trace_limit;
            target_socket = {
                address = SI_CL1_PFDI_PBX_BASE;
                size = SI_CL1_PFDI_MHU_SIZE;
                bind = "&si_cl1_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl1_router.target_socket"};
            irq = {bind = "&si_cl1_gic.spi_in_" .. SI_CL1_PFDI_SPI};
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
