local HOST_AP_NS_MHU_SHMEM_BASE = 0x00180000
local HOST_AP_SI_MHU_FRAME_SIZE = 0x00030000
local RSE_MHU_FRAME_SIZE = 0x00020000
local SI_CL0_AP_NS_MHU_PBX_BASE = 0x38000000
local SI_CL0_AP_NS_MHU_MBX_BASE = 0x38040000
local SI_CL0_AP_SCMI_MHU_PBX_BASE = 0x38080000
local SI_CL0_AP_SCMI_MHU_MBX_BASE = 0x380C0000
local SI_CL0_AP_PFDI_MHU_PBX_BASE = 0x38380000
local SI_CL0_AP_PFDI_MHU_MBX_BASE = 0x383C0000
local SI_CL0_RSE_MHU_PBX_BASE = 0x38100000
local SI_CL0_RSE_MHU_MBX_BASE = 0x38140000
local SI_CL0_PFDI_MHU_PBX_BASE = 0x38200000
local SI_CL0_PFDI_MHU_MBX_BASE = 0x38240000
local SI_CL0_PFDI_MHU_SIZE = 0x00020000
local SI_CL0_PFDI_SHMEM_BASE = 0x48000000
local SI_CL0_PFDI_CHANNEL_STRIDE = 40
local SI_CL0_PFDI_CHANNEL_BASE = 2

local module = {}

function module.create(ctx, hardware, _runtime, interrupts)
    local mhu_trace = ctx.options.mhu_trace
    local mhu_trace_file = ctx.options.mhu_trace_file
    local mhu_trace_limit = ctx.options.mhu_trace_limit
    local SI_CONST = hardware
    local irq_routes = interrupts

    local block = {}

    function block.define(ctx, platform)
        assert(
            platform.si_cl0_ap_ns_mhu_pbx == nil,
            "duplicate object: platform.si_cl0_ap_ns_mhu_pbx")
        platform.si_cl0_ap_ns_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "si_cl0_to_ap_ns";
            protocol = "doorbell-bridge";
            tx_shmem = HOST_AP_NS_MHU_SHMEM_BASE;
            rx_shmem = HOST_AP_NS_MHU_SHMEM_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_AP_NS_MHU_PBX_BASE;
                size = HOST_AP_SI_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_ap_ns_mhu_pbx", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ap_ns_mhu_mbx == nil,
            "duplicate object: platform.si_cl0_ap_ns_mhu_mbx")
        platform.si_cl0_ap_ns_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "ap_to_si_cl0_ns";
            protocol = "doorbell-bridge";
            tx_shmem = HOST_AP_NS_MHU_SHMEM_BASE;
            rx_shmem = HOST_AP_NS_MHU_SHMEM_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_AP_NS_MHU_MBX_BASE;
                size = HOST_AP_SI_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_ap_ns_mhu_mbx", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ap_scmi_mhu_pbx == nil,
            "duplicate object: platform.si_cl0_ap_scmi_mhu_pbx")
        platform.si_cl0_ap_scmi_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "si_cl0_to_ap_scmi";
            protocol = "doorbell-bridge";
            tx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
            rx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_AP_SCMI_MHU_PBX_BASE;
                size = HOST_AP_SI_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_ap_scmi_mhu_pbx", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ap_scmi_mhu_mbx == nil,
            "duplicate object: platform.si_cl0_ap_scmi_mhu_mbx")
        platform.si_cl0_ap_scmi_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "ap_to_si_cl0_scmi";
            protocol = "doorbell-bridge";
            tx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
            rx_shmem = SI_CONST.HOST_AP_SCMI_PAYLOAD_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_AP_SCMI_MHU_MBX_BASE;
                size = HOST_AP_SI_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_ap_scmi_mhu_mbx", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ap_pfdi_monitor_mhu_pbx == nil,
            "duplicate object: platform.si_cl0_ap_pfdi_monitor_mhu_pbx")
        platform.si_cl0_ap_pfdi_monitor_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "si_cl0_to_ap_pfdi";
            protocol = "doorbell-bridge";
            tx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
            rx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_AP_PFDI_MHU_PBX_BASE;
                size = HOST_AP_SI_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_ap_pfdi_mhu_pbx", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_ap_pfdi_monitor_mhu_mbx == nil,
            "duplicate object: platform.si_cl0_ap_pfdi_monitor_mhu_mbx")
        platform.si_cl0_ap_pfdi_monitor_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "ap_to_si_cl0_pfdi";
            protocol = "doorbell-bridge";
            tx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
            rx_shmem = SI_CONST.HOST_AP_SCMI_PFDI_MONITOR_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_AP_PFDI_MHU_MBX_BASE;
                size = HOST_AP_SI_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&system_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_ap_pfdi_mhu_mbx", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_rse_mhu_pbx == nil,
            "duplicate object: platform.si_cl0_rse_mhu_pbx")
        platform.si_cl0_rse_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "si_cl0_to_rse";
            protocol = "doorbell-bridge";
            tx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
            rx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_RSE_MHU_PBX_BASE;
                size = RSE_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_rse_mhu_mbx == nil,
            "duplicate object: platform.si_cl0_rse_mhu_mbx")
        platform.si_cl0_rse_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "rse_to_si_cl0";
            protocol = "doorbell-bridge";
            tx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
            rx_shmem = SI_CONST.SI_CL0_RSE_MHU_SHMEM_BASE;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_RSE_MHU_MBX_BASE;
                size = RSE_MHU_FRAME_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl0_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_rse_mhu", "View1")};
            log_level = 0;
        }

        assert(
            platform.si_cl0_pfdi_mhu_pbx == nil,
            "duplicate object: platform.si_cl0_pfdi_mhu_pbx")
        platform.si_cl0_pfdi_mhu_pbx = {
            moduletype = "mhu320ae";
            frame = "pbx";
            pair = "apollo_si_cl0_pfdi_reply";
            protocol = "doorbell-bridge";
            scmi_transport = "pfdi-monitor";
            channel_count = SI_CONST.SI_CL0_PFDI_MHU_CHANNELS;
            tx_shmem = SI_CL0_PFDI_SHMEM_BASE;
            rx_shmem = SI_CL0_PFDI_SHMEM_BASE;
            scmi_channel_stride = SI_CL0_PFDI_CHANNEL_STRIDE;
            scmi_channel_base_index = SI_CL0_PFDI_CHANNEL_BASE;
            scmi_channel_count = SI_CONST.SI_CL0_PFDI_CHANNEL_COUNT;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_PFDI_MHU_PBX_BASE;
                size = SI_CL0_PFDI_MHU_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl0_router.target_socket"};
            log_level = 0;
        }

        assert(
            platform.si_cl0_pfdi_mhu_mbx == nil,
            "duplicate object: platform.si_cl0_pfdi_mhu_mbx")
        platform.si_cl0_pfdi_mhu_mbx = {
            moduletype = "mhu320ae";
            frame = "mbx";
            pair = "apollo_si_cl1_pfdi";
            protocol = "doorbell-bridge";
            scmi_transport = "pfdi-monitor";
            channel_count = SI_CONST.SI_CL0_PFDI_MHU_CHANNELS;
            tx_shmem = SI_CL0_PFDI_SHMEM_BASE;
            rx_shmem = SI_CL0_PFDI_SHMEM_BASE;
            scmi_channel_stride = SI_CL0_PFDI_CHANNEL_STRIDE;
            scmi_channel_base_index = SI_CL0_PFDI_CHANNEL_BASE;
            scmi_channel_count = SI_CONST.SI_CL0_PFDI_CHANNEL_COUNT;
            init_shmem = false;
            trace = mhu_trace;
            trace_limit = mhu_trace_limit;
            trace_file = mhu_trace_file;
            target_socket = {
                address = SI_CL0_PFDI_MHU_MBX_BASE;
                size = SI_CL0_PFDI_MHU_SIZE;
                bind = "&si_cl0_router.initiator_socket";
            };
            initiator_socket = {bind = "&si_cl0_router.target_socket"};
            irq = {bind = irq_routes.spi_target(
                ctx, "si_cl0_cl1_mhu", "View1")};
            log_level = 0;
        }
    end

    return block
end

return module
