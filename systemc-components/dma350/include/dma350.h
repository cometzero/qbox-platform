/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <async_event.h>
#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class dma350 : public sc_core::sc_module
{
public:
    using initiator_socket_type = tlm_utils::simple_initiator_socket_b<
        dma350, DEFAULT_TLM_BUSWIDTH, tlm::tlm_base_protocol_types,
        sc_core::SC_ZERO_OR_MORE_BOUND>;

    cci::cci_param<unsigned int> p_channel_count;
    cci::cci_param<unsigned int> p_trigger_count;
    cci::cci_param<unsigned int> p_burst_bytes;
    cci::cci_param<sc_core::sc_time> p_burst_latency;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    cci::cci_param<std::string> p_trace_filter;
    cci::cci_param<uint64_t> p_trace_address_min;

    initiator_socket_type initiator_socket;
    tlm_utils::simple_target_socket<dma350, DEFAULT_TLM_BUSWIDTH> target_socket;
    TargetSignalSocket<bool> reset;
    sc_core::sc_vector<TargetSignalSocket<uint32_t>> trig_in;
    sc_core::sc_vector<InitiatorSignalSocket<uint32_t>> trig_ack;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> irq;

    SC_HAS_PROCESS(dma350);
    explicit dma350(sc_core::sc_module_name name);

    void before_end_of_elaboration() override;
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

private:
    static constexpr uint64_t REG_BYTES = 0x2000;
    static constexpr uint32_t SEC_CHINTRSTATUS0 = 0x100;
    static constexpr uint32_t NSEC_CHINTRSTATUS0 = 0x200;
    static constexpr uint32_t DMA_BUILDCFG0 = 0xfb0;
    static constexpr uint32_t DMA_BUILDCFG1 = 0xfb4;
    static constexpr uint32_t DMAINFO_IIDR = 0xfc8;
    static constexpr uint32_t DMAINFO_AIDR = 0xfcc;
    static constexpr uint32_t CH_BASE = 0x1000;
    static constexpr uint32_t CH_STRIDE = 0x100;
    static constexpr uint32_t CH_CMD = 0x000;
    static constexpr uint32_t CH_STATUS = 0x004;
    static constexpr uint32_t CH_INTREN = 0x008;
    static constexpr uint32_t CH_CTRL = 0x00c;
    static constexpr uint32_t CH_SRCADDR = 0x010;
    static constexpr uint32_t CH_SRCADDRHI = 0x014;
    static constexpr uint32_t CH_DESADDR = 0x018;
    static constexpr uint32_t CH_DESADDRHI = 0x01c;
    static constexpr uint32_t CH_XSIZE = 0x020;
    static constexpr uint32_t CH_XSIZEHI = 0x024;
    static constexpr uint32_t CH_SRCTRANSCFG = 0x028;
    static constexpr uint32_t CH_DESTRANSCFG = 0x02c;
    static constexpr uint32_t CH_XADDRINC = 0x030;
    static constexpr uint32_t CH_FILLVAL = 0x038;
    static constexpr uint32_t CH_SRCTRIGINCFG = 0x04c;
    static constexpr uint32_t CH_DESTRIGINCFG = 0x050;
    static constexpr uint32_t CH_ERRINFO = 0x090;
    static constexpr uint32_t CH_IIDR = 0x0f0;
    static constexpr uint32_t CH_AIDR = 0x0f4;
    static constexpr uint32_t CH_BUILDCFG0 = 0x0f8;
    static constexpr uint32_t CH_BUILDCFG1 = 0x0fc;

    static constexpr uint32_t CH_CMD_ENABLE = 1u << 0;
    static constexpr uint32_t CH_CMD_CLEAR = 1u << 1;
    static constexpr uint32_t CH_CMD_DISABLE = 1u << 2;
    static constexpr uint32_t CH_CMD_STOP = 1u << 3;
    static constexpr uint32_t CH_CMD_PAUSE = 1u << 4;
    static constexpr uint32_t CH_CMD_RESUME = 1u << 5;

    static constexpr uint32_t INTR_DONE = 1u << 0;
    static constexpr uint32_t INTR_ERR = 1u << 1;
    static constexpr uint32_t INTR_DISABLED = 1u << 2;
    static constexpr uint32_t INTR_STOPPED = 1u << 3;
    static constexpr uint32_t INTR_SRCWAIT = 1u << 8;
    static constexpr uint32_t INTR_DESTWAIT = 1u << 9;
    static constexpr uint32_t INTR_OUTWAIT = 1u << 10;
    static constexpr uint32_t STAT_DONE = 1u << 16;
    static constexpr uint32_t STAT_ERR = 1u << 17;
    static constexpr uint32_t STAT_DISABLED = 1u << 18;
    static constexpr uint32_t STAT_STOPPED = 1u << 19;
    static constexpr uint32_t STAT_PAUSED = 1u << 20;
    static constexpr uint32_t STAT_RESUMEWAIT = 1u << 21;
    static constexpr uint32_t STAT_SRCWAIT = 1u << 24;
    static constexpr uint32_t STAT_DESTWAIT = 1u << 25;
    static constexpr uint32_t STAT_OUTWAIT = 1u << 26;

    static constexpr uint32_t CH_CTRL_XTYPE_SHIFT = 9;
    static constexpr uint32_t CH_CTRL_XTYPE_MASK = 0x7u << CH_CTRL_XTYPE_SHIFT;
    static constexpr uint32_t XTYPE_CONTINUE = 0x1u;
    static constexpr uint32_t XTYPE_WRAP = 0x2u;
    static constexpr uint32_t XTYPE_FILL = 0x3u;
    static constexpr uint32_t USE_SRC_TRIGGER = 1u << 25;
    static constexpr uint32_t USE_DEST_TRIGGER = 1u << 26;

    static constexpr uint32_t TRIGGER_ACTIVE = 1u << 2;
    static constexpr uint32_t TRIGGER_TYPE_MASK = 0x3u;
    static constexpr uint32_t TRIGGER_LAST_SINGLE = 1u;
    static constexpr uint32_t TRIGGER_BLOCK = 2u;
    static constexpr uint32_t TRIGGER_LAST_BLOCK = 3u;
    static constexpr uint32_t ACK_OKAY = 0u;
    static constexpr uint32_t ACK_DENY = 1u;
    static constexpr uint32_t ACK_LAST_OKAY = 2u;

    static constexpr uint32_t TRIGIN_SELECT_MASK = 0xffu;
    static constexpr uint32_t TRIGIN_TYPE_SHIFT = 8;
    static constexpr uint32_t TRIGIN_TYPE_MASK = 0x3u << TRIGIN_TYPE_SHIFT;
    static constexpr uint32_t TRIGIN_TYPE_HW = 2u;
    static constexpr uint32_t TRIGIN_MODE_SHIFT = 10;
    static constexpr uint32_t TRIGIN_MODE_MASK = 0x3u << TRIGIN_MODE_SHIFT;
    static constexpr uint32_t TRIGIN_MODE_COMMAND = 0u;
    static constexpr uint32_t TRIGIN_MODE_DMA_FLOW = 2u;
    static constexpr uint32_t TRIGIN_MODE_PERIPHERAL_FLOW = 3u;
    static constexpr uint32_t TRIGIN_BLOCK_SHIFT = 16;

    static constexpr uint32_t ERRINFO_BUS = 1u << 0;
    static constexpr uint32_t ERRINFO_CFG = 1u << 1;
    static constexpr uint32_t ERRINFO_SRC_TRIGGER = 1u << 2;
    static constexpr uint32_t ERRINFO_DEST_TRIGGER = 1u << 3;
    static constexpr uint32_t ERRINFO_REG_VALUE = 1u << 25;
    static constexpr uint32_t ERRINFO_CFG_CONFLICT = 1u << 26;
    static constexpr uint32_t ERRINFO_READ_RESPONSE = 1u << 16;
    static constexpr uint32_t ERRINFO_WRITE_RESPONSE = 1u << 17;

    struct TriggerSide {
        bool used = false;
        bool command = false;
        bool peripheral_flow = false;
        bool ack_active = false;
        unsigned int select = 0;
        unsigned int block_size = 1;
    };

    struct ChannelState {
        bool enabled = false;
        bool paused = false;
        bool pause_requested = false;
        bool stop_requested = false;
        bool disable_requested = false;
        bool clear_requested = false;
        bool command_started = false;
        bool finish_after_ack = false;
        bool fill = false;
        bool wrap = false;
        uint64_t start_source = 0;
        uint64_t start_dest = 0;
        uint64_t source = 0;
        uint64_t dest = 0;
        uint64_t completed_bytes = 0;
        uint32_t source_remaining = 0;
        uint32_t dest_remaining = 0;
        uint32_t source_length = 0;
        uint32_t wrap_remaining = 0;
        int16_t source_inc = 0;
        int16_t dest_inc = 0;
        unsigned int transfer_bytes = 1;
        TriggerSide source_trigger;
        TriggerSide dest_trigger;
    };

    std::array<uint8_t, REG_BYTES> m_regs{};
    std::vector<ChannelState> m_channels;
    std::vector<int> m_trigger_owner;
    std::vector<uint32_t> m_ack_values;
    sc_core::sc_vector<sc_core::sc_signal<uint32_t>> m_ack_stubs;
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_irq_stubs;
    gs::async_event m_work_event;
    gs::async_event m_output_event;
    std::atomic<bool> m_reset_requested{ false };
    unsigned int m_trace_count = 0;
    unsigned int m_rr_channel = 0;

    static bool is_supported_length(unsigned int len);
    static int16_t sign_extend16(uint32_t value);
    static unsigned int transfer_bytes_from_ctrl(uint32_t ctrl);
    static unsigned int trigger_mode(uint32_t config);
    static unsigned int trigger_type(uint32_t config);
    static bool request_active(uint32_t request);
    static unsigned int request_type(uint32_t request);

    void store32(uint32_t offset, uint32_t value);
    uint32_t load32(uint32_t offset) const;
    uint32_t channel_base(unsigned int channel) const;
    uint64_t channel_address(uint32_t base, uint32_t low_offset) const;
    uint32_t source_xsize(uint32_t base) const;
    uint32_t dest_xsize(uint32_t base) const;
    void store_address(uint32_t base, uint32_t low_offset, uint64_t value);
    void store_xsize(uint32_t base, uint32_t source, uint32_t dest);

    void reset_registers();
    void reset_channel(unsigned int channel);
    void release_triggers(unsigned int channel);
    bool configure_channel(unsigned int channel);
    bool configure_trigger(unsigned int channel, uint32_t config, bool used,
                           TriggerSide& side, uint32_t error_bit);
    void write_command(unsigned int channel, uint32_t value);
    void clear_channel_status(unsigned int channel, uint32_t value);
    void set_status_event(unsigned int channel, uint32_t status_bit,
                          uint32_t interrupt_bit);
    void set_error(unsigned int channel, uint32_t error_info);
    void finish_channel(unsigned int channel, uint32_t status_bit,
                        uint32_t interrupt_bit);
    void update_irq(unsigned int channel);
    void update_interrupt_summary();
    void drive_outputs();

    void worker_thread();
    bool service_channel(unsigned int channel);
    bool service_command_triggers(unsigned int channel, ChannelState& state);
    bool clear_completed_handshakes(ChannelState& state);
    unsigned int flow_request_limit(const TriggerSide& side,
                                    uint32_t request) const;
    bool execute_burst(unsigned int channel, ChannelState& state,
                       unsigned int units, sc_core::sc_time& delay);
    bool execute_fill_burst(ChannelState& state, unsigned int units,
                            sc_core::sc_time& delay);
    bool execute_copy_burst(ChannelState& state, unsigned int units,
                            sc_core::sc_time& delay, bool& read_error);
    void update_progress(unsigned int channel, const ChannelState& state);
    void drive_trigger_ack(TriggerSide& side, bool active, unsigned int type);

    bool mem_read(uint64_t address, uint8_t* data, unsigned int len,
                  sc_core::sc_time& delay);
    bool mem_write(uint64_t address, const uint8_t* data, unsigned int len,
                   sc_core::sc_time& delay);
    bool access(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay,
                bool execute_side_effects);
    void write32(uint32_t offset, uint32_t value, bool execute_side_effects);
    bool writable_while_enabled(uint32_t channel_offset) const;

    void trace_access(tlm::tlm_generic_payload& trans, uint64_t offset,
                      unsigned int len);
    void trace_operation(unsigned int channel, const ChannelState& state,
                         const char* status);
    bool trace_access_filter_matches(uint64_t offset) const;
    bool trace_operation_filter_matches(unsigned int channel, bool copy) const;
    bool trace_address_matches(uint32_t base) const;
    static bool is_copy_trace_offset(uint32_t offset);
    static bool is_fill_trace_offset(uint32_t offset);
};

extern "C" void module_register();
