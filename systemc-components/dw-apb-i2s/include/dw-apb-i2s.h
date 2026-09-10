/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <array>
#include <cstdint>
#include <deque>

#include <cci_configuration>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <dma-trigger.h>
#include <module_factory_registery.h>
#include <ports/biflow-socket.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <tlm_sockets_buswidth.h>

class dw_apb_i2s : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(dw_apb_i2s);

    static constexpr uint32_t FIFO_DEPTH = 16;

    enum Register : uint32_t {
        IER = 0x000,
        IRER = 0x004,
        ITER = 0x008,
        CER = 0x00c,
        CCR = 0x010,
        RXFFR = 0x014,
        TXFFR = 0x018,
        LRBR_LTHR0 = 0x020,
        RRBR_RTHR0 = 0x024,
        RER0 = 0x028,
        TER0 = 0x02c,
        RCR0 = 0x030,
        TCR0 = 0x034,
        ISR0 = 0x038,
        IMR0 = 0x03c,
        ROR0 = 0x040,
        TOR0 = 0x044,
        RFCR0 = 0x048,
        TFCR0 = 0x04c,
        RFF0 = 0x050,
        TFF0 = 0x054,
        RXDMA = 0x1c0,
        RRXDMA = 0x1c4,
        TXDMA = 0x1c8,
        RTXDMA = 0x1cc,
        COMP_PARAM_2 = 0x1f0,
        COMP_PARAM_1 = 0x1f4,
        COMP_VERSION = 0x1f8,
        COMP_TYPE = 0x1fc,
        DMACR = 0x200,
    };

    cci::cci_param<bool> p_master_mode;
    cci::cci_param<bool> p_transmitter_enabled;
    cci::cci_param<bool> p_receiver_enabled;
    cci::cci_param<bool> p_functional_pacing;
    cci::cci_param<uint64_t> p_access_latency_ns;
    cci::cci_param<uint64_t> p_frame_period_ns;

    tlm_utils::simple_target_socket<dw_apb_i2s, DEFAULT_TLM_BUSWIDTH> target_socket;
    InitiatorSignalSocket<bool> irq;
    TargetSignalSocket<bool> reset;
    TargetSignalSocket<bool> pinmux_enable;
    gs::biflow_socket<dw_apb_i2s> audio_socket;
    InitiatorSignalSocket<uint32_t> dma_tx_req;
    InitiatorSignalSocket<uint32_t> dma_rx_req;
    TargetSignalSocket<uint32_t> dma_tx_ack;
    TargetSignalSocket<uint32_t> dma_rx_ack;

    explicit dw_apb_i2s(sc_core::sc_module_name name);

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void before_end_of_elaboration() override;
    void end_of_elaboration() override;

private:
    struct Frame {
        uint32_t left;
        uint32_t right;
    };

    static constexpr uint32_t ISR_RXDA = 1u << 0;
    static constexpr uint32_t ISR_RXFO = 1u << 1;
    static constexpr uint32_t ISR_TXFE = 1u << 4;
    static constexpr uint32_t ISR_TXFO = 1u << 5;
    static constexpr uint32_t DMACR_RXBLOCK = 1u << 16;
    static constexpr uint32_t DMACR_TXBLOCK = 1u << 17;
    static constexpr uint32_t COMPONENT_VERSION = 0x3131312a;
    static constexpr uint32_t COMPONENT_TYPE = 0x445701a0;

    std::deque<Frame> m_tx_fifo;
    std::deque<Frame> m_rx_fifo;
    uint32_t m_ccr = 0;
    uint32_t m_rcr = 5;
    uint32_t m_tcr = 5;
    uint32_t m_imr = 0x33;
    uint32_t m_rfcr = 0;
    uint32_t m_tfcr = 0;
    uint32_t m_dmacr = 0;
    uint32_t m_tx_pio_left = 0;
    uint32_t m_tx_dma_left = 0;
    bool m_tx_left_valid = false;
    bool m_tx_pio_drop_right = false;
    bool m_rx_pio_left = true;
    bool m_tx_dma_left_valid = false;
    bool m_tx_dma_drop_right = false;
    bool m_rx_dma_left = true;
    bool m_tx_overrun = false;
    bool m_rx_overrun = false;
    bool m_global_enabled = false;
    bool m_rx_block_enabled = false;
    bool m_tx_block_enabled = false;
    bool m_clock_enabled = false;
    bool m_rx_channel_enabled = false;
    bool m_tx_channel_enabled = false;
    bool m_reset_asserted = false;
    bool m_pinmux_enabled = true;
    bool m_dma_tx_force_idle = true;
    bool m_dma_rx_force_idle = true;
    std::array<uint8_t, sizeof(Frame)> m_rx_frame_bytes{};
    size_t m_rx_frame_byte_count = 0;

    sc_core::sc_event m_state_event;
    sc_core::sc_event m_output_event;
    sc_core::sc_signal<bool> m_irq_stub;
    sc_core::sc_signal<uint32_t> m_dma_tx_stub;
    sc_core::sc_signal<uint32_t> m_dma_rx_stub;
    gs::dma_trigger_request m_dma_tx;
    gs::dma_trigger_request m_dma_rx;

    void tx_thread();
    bool send_audio_frame(const Frame& frame);
    void receive_audio(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void drive_outputs();
    void reset_changed(bool asserted);
    void reset_registers();
    void state_changed();
    bool tx_active() const;
    bool rx_active() const;
    uint32_t interrupt_status() const;
    uint32_t component_parameter_1() const;
    void write_register(uint32_t offset, uint32_t value);
    uint32_t read_register(uint32_t offset);
    void write_tx_left(uint32_t value);
    void write_tx_right(uint32_t value);
    void write_tx_dma(uint32_t value);
    uint32_t read_rx_sample(bool dma);
};

extern "C" void module_register();
