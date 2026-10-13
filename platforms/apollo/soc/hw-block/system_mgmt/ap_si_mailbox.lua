-- SPDX-License-Identifier: BSD-3-Clause
local HOST_AP_SHARED_SRAM_PHYS_BASE = 0x00000000
local HOST_AP_SDS_MEM_SIZE = 0x00000DC0
local HOST_AP_SCMI_PAYLOAD_BASE = HOST_AP_SHARED_SRAM_PHYS_BASE + HOST_AP_SDS_MEM_SIZE
local HOST_AP_NS_SCMI_TX_SHMEM_BASE = 0x00180000
local HOST_AP_NS_SCMI_RX_SHMEM_BASE = 0x00180100
local HOST_AP_SI_NS_SCMI_MHU_PBX_PHYS_BASE = 0x400003B000000
local HOST_AP_SI_NS_SCMI_MHU_MBX_PHYS_BASE = 0x400003B040000
local HOST_AP_SI_SCMI_MHU_PBX_PHYS_BASE = 0x400003B080000
local HOST_AP_SI_SCMI_MHU_MBX_PHYS_BASE = 0x400003B0C0000
local HOST_AP_SI_CL1_MHU_PBX_PHYS_BASE = 0x400003B100000
local HOST_AP_SI_CL1_MHU_MBX_PHYS_BASE = 0x400003B140000
local HOST_AP_SI_PFDI_MONITOR_MHU_PBX_PHYS_BASE = 0x400003B380000
local HOST_AP_SI_PFDI_MONITOR_MHU_MBX_PHYS_BASE = 0x400003B3C0000
local HOST_AP_SI_MHU_FRAME_SIZE = 0x00030000
local HOST_AP_SCMI_PFDI_MONITOR_OFFSET = 0x00000100
local HOST_AP_SCMI_PFDI_MONITOR_BASE = HOST_AP_SCMI_PAYLOAD_BASE + HOST_AP_SCMI_PFDI_MONITOR_OFFSET
local HOST_AP_SCMI_PFDI_MONITOR_STRIDE = 40
local HOST_AP_SCMI_PFDI_MONITOR_CHANNELS = 16
local AP_SI_NS_MHU_PBX_SPI = 112
local AP_SI_NS_MHU_MBX_SPI = 113
local AP_SI_SCMI_MHU_PBX_SPI = 114
local AP_SI_SCMI_MHU_MBX_SPI = 115
local AP_SI_PFDI_MHU_PBX_SPI = 118
local AP_SI_PFDI_MHU_MBX_SPI = 119
local AP_SI_CL1_MHU_PBX_SPI = 120
local AP_SI_CL1_MHU_MBX_SPI = 121
local AP_SI_CL1_MHU_CHANNEL_COUNT = 32
local SYSTEM_POWER_RESET_DELAY_NS = 1
local SYSTEM_POWER_RESET_PULSE_WIDTH_NS = 1
-- AP IRQ constants are zero-based GIC SPI socket indices.

local ap_si_mailbox = {}

function ap_si_mailbox.define(ctx, platform)
    local options = ctx.options
    assert(platform.host_ap_si_ns_scmi_mhu_pbx == nil, "duplicate Apollo object: host_ap_si_ns_scmi_mhu_pbx")
    platform.host_ap_si_ns_scmi_mhu_pbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "ap_to_si_cl0_ns";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_NS_SCMI_TX_SHMEM_BASE;
        rx_shmem = HOST_AP_NS_SCMI_RX_SHMEM_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_NS_SCMI_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_NS_MHU_PBX_SPI};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_ns_scmi_mhu_mbx == nil, "duplicate Apollo object: host_ap_si_ns_scmi_mhu_mbx")
    platform.host_ap_si_ns_scmi_mhu_mbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_ap_ns";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_NS_SCMI_TX_SHMEM_BASE;
        rx_shmem = HOST_AP_NS_SCMI_RX_SHMEM_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_NS_SCMI_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_NS_MHU_MBX_SPI};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_scmi_mhu_pbx == nil, "duplicate Apollo object: host_ap_si_scmi_mhu_pbx")
    platform.host_ap_si_scmi_mhu_pbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "ap_to_si_cl0_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        power_domain_reset_count = options.AP_NUM_CPUS;
        performance_domain_count = math.floor((options.AP_NUM_CPUS + 3) / 4);
        power_domain_reset_delay_ns = options.ap_power_domain_reset_delay_ns;
        power_domain_reset_assert_on_power_off = false;
        power_domain_reset_pulse_on_power_on = true;
        system_power_reset_delay_ns = SYSTEM_POWER_RESET_DELAY_NS;
        system_power_reset_pulse_width_ns = SYSTEM_POWER_RESET_PULSE_WIDTH_NS;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_SCMI_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_SCMI_MHU_PBX_SPI};
        system_reset = {bind = options.ap_system_reset_bind_targets()};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_scmi_mhu_mbx == nil, "duplicate Apollo object: host_ap_si_scmi_mhu_mbx")
    platform.host_ap_si_scmi_mhu_mbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_ap_scmi";
        protocol = "doorbell-bridge";
        tx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        rx_shmem = HOST_AP_SCMI_PAYLOAD_BASE;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_SCMI_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_SCMI_MHU_MBX_SPI};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_cl1_mhu_pbx == nil, "duplicate Apollo object: host_ap_si_cl1_mhu_pbx")
    platform.host_ap_si_cl1_mhu_pbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "apollo_ap_to_si_cl1";
        protocol = "doorbell-bridge";
        channel_count = AP_SI_CL1_MHU_CHANNEL_COUNT;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_CL1_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_CL1_MHU_PBX_SPI};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_cl1_mhu_mbx == nil, "duplicate Apollo object: host_ap_si_cl1_mhu_mbx")
    platform.host_ap_si_cl1_mhu_mbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "apollo_si_cl1_to_ap";
        protocol = "doorbell-bridge";
        channel_count = AP_SI_CL1_MHU_CHANNEL_COUNT;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_CL1_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_CL1_MHU_MBX_SPI};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_pfdi_monitor_mhu_pbx == nil, "duplicate Apollo object: host_ap_si_pfdi_monitor_mhu_pbx")
    platform.host_ap_si_pfdi_monitor_mhu_pbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "pbx";
        pair = "ap_to_si_cl0_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        scmi_channel_stride = HOST_AP_SCMI_PFDI_MONITOR_STRIDE;
        scmi_channel_count = HOST_AP_SCMI_PFDI_MONITOR_CHANNELS;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_PFDI_MONITOR_MHU_PBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_PFDI_MHU_PBX_SPI};
        log_level = 0;
    } or nil

    assert(platform.host_ap_si_pfdi_monitor_mhu_mbx == nil, "duplicate Apollo object: host_ap_si_pfdi_monitor_mhu_mbx")
    platform.host_ap_si_pfdi_monitor_mhu_mbx = options.enable_ap_cpus and {
        moduletype = "mhu320ae";
        frame = "mbx";
        pair = "si_cl0_to_ap_pfdi";
        protocol = "doorbell-bridge";
        scmi_transport = "pfdi-monitor";
        tx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        rx_shmem = HOST_AP_SCMI_PFDI_MONITOR_BASE;
        scmi_channel_stride = HOST_AP_SCMI_PFDI_MONITOR_STRIDE;
        scmi_channel_count = HOST_AP_SCMI_PFDI_MONITOR_CHANNELS;
        init_shmem = false;
        trace = options.mhu_trace;
        trace_limit = options.mhu_trace_limit;
        trace_file = options.mhu_trace_file;
        target_socket = {
            address = HOST_AP_SI_PFDI_MONITOR_MHU_MBX_PHYS_BASE;
            size = HOST_AP_SI_MHU_FRAME_SIZE;
            bind = "&system_router.initiator_socket";
        };
        initiator_socket = {bind = "&system_router.target_socket"};
        irq = {bind = "&ap_gic.spi_in_"..AP_SI_PFDI_MHU_MBX_SPI};
        log_level = 0;
    } or nil
end

return ap_si_mailbox
