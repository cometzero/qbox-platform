/* SPDX-License-Identifier: BSD-3-Clause */

#include <algorithm>
#include <cstring>

#include <dw-apb-i2s.h>

dw_apb_i2s::dw_apb_i2s(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , p_master_mode("master_mode", false)
    , p_transmitter_enabled("transmitter_enabled", true)
    , p_receiver_enabled("receiver_enabled", true)
    , p_functional_pacing("functional_pacing", false)
    , p_access_latency_ns("access_latency_ns", 10)
    , p_frame_period_ns("frame_period_ns", 20833)
    , target_socket("target_socket")
    , irq("irq")
    , reset("reset")
    , pinmux_enable("pinmux_enable")
    , audio_socket("audio_socket")
    , dma_tx_req("dma_tx_req")
    , dma_rx_req("dma_rx_req")
    , dma_tx_ack("dma_tx_ack")
    , dma_rx_ack("dma_rx_ack")
    , m_irq_stub("irq_stub")
    , m_dma_tx_stub("dma_tx_stub")
    , m_dma_rx_stub("dma_rx_stub")
    , m_dma_tx(dma_tx_req)
    , m_dma_rx(dma_rx_req)
{
    target_socket.register_b_transport(this, &dw_apb_i2s::b_transport);
    audio_socket.register_b_transport(this, &dw_apb_i2s::receive_audio);
    reset.register_value_changed_cb([this](bool value) { reset_changed(value); });
    pinmux_enable.register_value_changed_cb([this](bool value) {
        m_pinmux_enabled = value;
        if (!value) {
            m_dma_tx_force_idle = true;
            m_dma_rx_force_idle = true;
        }
        state_changed();
    });
    dma_tx_ack.register_value_changed_cb([this](uint32_t value) {
        m_dma_tx.acknowledge(value);
        m_output_event.notify(sc_core::SC_ZERO_TIME);
    });
    dma_rx_ack.register_value_changed_cb([this](uint32_t value) {
        m_dma_rx.acknowledge(value);
        m_output_event.notify(sc_core::SC_ZERO_TIME);
    });

    SC_THREAD(tx_thread);
    SC_METHOD(drive_outputs);
    sensitive << m_output_event;

    reset_registers();
}

void dw_apb_i2s::before_end_of_elaboration()
{
    if (!irq.get_interface()) irq.bind(m_irq_stub);
    if (!dma_tx_req.get_interface()) dma_tx_req.bind(m_dma_tx_stub);
    if (!dma_rx_req.get_interface()) dma_rx_req.bind(m_dma_rx_stub);
}

void dw_apb_i2s::end_of_elaboration() { audio_socket.can_receive_any(); }

bool dw_apb_i2s::tx_active() const
{
    return p_transmitter_enabled.get_value() && !m_reset_asserted && m_pinmux_enabled && m_global_enabled &&
           m_tx_block_enabled && m_tx_channel_enabled && (!p_master_mode.get_value() || m_clock_enabled);
}

bool dw_apb_i2s::rx_active() const
{
    return p_receiver_enabled.get_value() && !m_reset_asserted && m_pinmux_enabled && m_global_enabled &&
           m_rx_block_enabled && m_rx_channel_enabled;
}

void dw_apb_i2s::state_changed()
{
    m_state_event.notify(sc_core::SC_ZERO_TIME);
    m_output_event.notify(sc_core::SC_ZERO_TIME);
}

void dw_apb_i2s::reset_registers()
{
    m_tx_fifo.clear();
    m_rx_fifo.clear();
    m_ccr = 0;
    m_rcr = 5;
    m_tcr = 5;
    m_imr = 0x33;
    m_rfcr = 0;
    m_tfcr = 0;
    m_dmacr = 0;
    m_tx_pio_left = 0;
    m_tx_dma_left = 0;
    m_tx_left_valid = false;
    m_tx_pio_drop_right = false;
    m_rx_pio_left = true;
    m_tx_dma_left_valid = false;
    m_tx_dma_drop_right = false;
    m_rx_dma_left = true;
    m_tx_overrun = false;
    m_rx_overrun = false;
    m_global_enabled = false;
    m_rx_block_enabled = false;
    m_tx_block_enabled = false;
    m_clock_enabled = false;
    m_rx_channel_enabled = false;
    m_tx_channel_enabled = false;
    m_dma_tx_force_idle = true;
    m_dma_rx_force_idle = true;
    m_rx_frame_byte_count = 0;
    audio_socket.reset();
    state_changed();
}

void dw_apb_i2s::reset_changed(bool asserted)
{
    m_reset_asserted = asserted;
    if (asserted)
        reset_registers();
    else
        state_changed();
}

uint32_t dw_apb_i2s::component_parameter_1() const
{
    return (4u << 16) | (p_receiver_enabled.get_value() ? 1u << 6 : 0) |
           (p_transmitter_enabled.get_value() ? 1u << 5 : 0) | (p_master_mode.get_value() ? 1u << 4 : 0) | (3u << 2) |
           2u;
}

uint32_t dw_apb_i2s::interrupt_status() const
{
    uint32_t status = 0;
    if (p_receiver_enabled.get_value() && m_rx_fifo.size() >= std::min<size_t>(FIFO_DEPTH, (m_rfcr & 0xf) + 1))
        status |= ISR_RXDA;
    if (m_rx_overrun) status |= ISR_RXFO;
    if (p_transmitter_enabled.get_value() && m_tx_fifo.size() <= std::min<size_t>(FIFO_DEPTH - 1, m_tfcr & 0xf))
        status |= ISR_TXFE;
    if (m_tx_overrun) status |= ISR_TXFO;
    return status;
}

void dw_apb_i2s::drive_outputs()
{
    if (m_dma_tx_force_idle) {
        m_dma_tx.force_idle();
        m_dma_tx_force_idle = false;
    }
    if (m_dma_rx_force_idle) {
        m_dma_rx.force_idle();
        m_dma_rx_force_idle = false;
    }

    uint32_t enabled_status = 0;
    if (m_global_enabled && m_rx_block_enabled && m_rx_channel_enabled) enabled_status |= ISR_RXDA | ISR_RXFO;
    if (m_global_enabled && m_tx_block_enabled && m_tx_channel_enabled) enabled_status |= ISR_TXFE | ISR_TXFO;
    irq->write((interrupt_status() & enabled_status & ~m_imr) != 0);
    m_dma_tx.update(tx_active() && (m_dmacr & DMACR_TXBLOCK) && m_tx_fifo.size() < FIFO_DEPTH);
    m_dma_rx.update(rx_active() && (m_dmacr & DMACR_RXBLOCK) && !m_rx_fifo.empty());
}

void dw_apb_i2s::receive_audio(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    (void)delay;
    auto* bytes = trans.get_data_ptr();
    if (!bytes) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // Functional retries must be atomic: one complete stereo frame per call.
    if (p_functional_pacing.get_value() && trans.get_data_length() != sizeof(Frame)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    for (unsigned int i = 0; i < trans.get_data_length(); ++i) {
        if (!rx_active()) {
            m_rx_frame_byte_count = 0;
            continue;
        }
        m_rx_frame_bytes[m_rx_frame_byte_count++] = bytes[i];
        if (m_rx_frame_byte_count == sizeof(Frame)) {
            if (m_rx_fifo.size() == FIFO_DEPTH) {
                if (p_functional_pacing.get_value()) {
                    m_rx_frame_byte_count = 0;
                    trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                    return;
                }
                m_rx_overrun = true;
                m_rx_frame_byte_count = 0;
                break;
            }
            Frame frame{};
            std::memcpy(&frame, m_rx_frame_bytes.data(), sizeof(frame));
            m_rx_fifo.push_back(frame);
            m_rx_frame_byte_count = 0;
        }
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    state_changed();
}

bool dw_apb_i2s::send_audio_frame(const Frame& frame)
{
    std::array<uint8_t, sizeof(frame)> bytes{};
    std::memcpy(bytes.data(), &frame, sizeof(frame));
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_IGNORE_COMMAND);
    trans.set_data_ptr(bytes.data());
    trans.set_data_length(bytes.size());
    trans.set_streaming_width(bytes.size());
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    audio_socket.force_send(trans);
    return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
}

void dw_apb_i2s::tx_thread()
{
    while (true) {
        while (!tx_active()) wait(m_state_event);
        if (p_functional_pacing.get_value() && m_tx_fifo.empty()) {
            wait(m_state_event);
            continue;
        }

        wait(sc_core::sc_time(std::max<uint64_t>(1, p_frame_period_ns.get_value()), sc_core::SC_NS));
        if (!tx_active()) continue;

        Frame frame{};
        const bool queued = !m_tx_fifo.empty();
        if (queued) frame = m_tx_fifo.front();
        if (send_audio_frame(frame) && queued) m_tx_fifo.pop_front();
        state_changed();
    }
}

void dw_apb_i2s::write_tx_left(uint32_t value)
{
    if (m_tx_fifo.size() == FIFO_DEPTH) {
        m_tx_overrun = true;
        m_tx_left_valid = false;
        m_tx_pio_drop_right = true;
    } else {
        m_tx_pio_left = value;
        m_tx_left_valid = true;
        m_tx_pio_drop_right = false;
    }
    state_changed();
}

void dw_apb_i2s::write_tx_right(uint32_t value)
{
    if (m_tx_pio_drop_right) {
        m_tx_pio_drop_right = false;
        state_changed();
        return;
    }
    if (!m_tx_left_valid) return;

    if (m_tx_fifo.size() == FIFO_DEPTH) {
        m_tx_overrun = true;
    } else {
        m_tx_fifo.push_back({ m_tx_pio_left, value });
    }
    m_tx_left_valid = false;
    state_changed();
}

void dw_apb_i2s::write_tx_dma(uint32_t value)
{
    if (!m_tx_dma_left_valid) {
        if (m_tx_fifo.size() == FIFO_DEPTH) {
            m_tx_overrun = true;
            m_tx_dma_drop_right = true;
        } else {
            m_tx_dma_left = value;
            m_tx_dma_drop_right = false;
        }
        m_tx_dma_left_valid = true;
        state_changed();
        return;
    }

    if (!m_tx_dma_drop_right) {
        if (m_tx_fifo.size() == FIFO_DEPTH)
            m_tx_overrun = true;
        else
            m_tx_fifo.push_back({ m_tx_dma_left, value });
    }
    m_tx_dma_left_valid = false;
    m_tx_dma_drop_right = false;
    state_changed();
}

uint32_t dw_apb_i2s::read_rx_sample(bool dma)
{
    if (m_rx_fifo.empty()) return 0;

    bool& left = dma ? m_rx_dma_left : m_rx_pio_left;
    const uint32_t value = left ? m_rx_fifo.front().left : m_rx_fifo.front().right;
    if (left) {
        left = false;
    } else {
        left = true;
        m_rx_fifo.pop_front();
        state_changed();
    }
    return value;
}

void dw_apb_i2s::write_register(uint32_t offset, uint32_t value)
{
    switch (offset) {
    case IER:
        m_global_enabled = value & 1;
        if (!m_global_enabled) {
            m_tx_fifo.clear();
            m_rx_fifo.clear();
            m_rx_frame_byte_count = 0;
            m_tx_left_valid = false;
            m_tx_pio_drop_right = false;
            m_tx_dma_left_valid = false;
            m_tx_dma_drop_right = false;
            m_rx_pio_left = true;
            m_rx_dma_left = true;
            m_tx_overrun = false;
            m_rx_overrun = false;
            m_dma_tx_force_idle = true;
            m_dma_rx_force_idle = true;
        }
        break;
    case IRER:
        m_rx_block_enabled = value & 1;
        if (!m_rx_block_enabled) m_dma_rx_force_idle = true;
        break;
    case ITER:
        m_tx_block_enabled = value & 1;
        if (!m_tx_block_enabled) m_dma_tx_force_idle = true;
        break;
    case CER:
        m_clock_enabled = value & 1;
        break;
    case CCR:
        if (!m_clock_enabled) m_ccr = value & 0x1f;
        break;
    case RXFFR:
        if ((value & 1) && !m_rx_block_enabled) {
            m_rx_fifo.clear();
            m_rx_frame_byte_count = 0;
            m_rx_pio_left = true;
            m_rx_dma_left = true;
        }
        break;
    case TXFFR:
        if ((value & 1) && !m_tx_block_enabled) {
            m_tx_fifo.clear();
            m_tx_left_valid = false;
            m_tx_pio_drop_right = false;
            m_tx_dma_left_valid = false;
            m_tx_dma_drop_right = false;
        }
        break;
    case LRBR_LTHR0:
        if (p_transmitter_enabled.get_value()) write_tx_left(value);
        return;
    case RRBR_RTHR0:
        if (p_transmitter_enabled.get_value()) write_tx_right(value);
        return;
    case RER0:
        m_rx_channel_enabled = value & 1;
        if (!m_rx_channel_enabled) m_dma_rx_force_idle = true;
        break;
    case TER0:
        m_tx_channel_enabled = value & 1;
        if (!m_tx_channel_enabled) m_dma_tx_force_idle = true;
        break;
    case RCR0:
        if (!m_rx_channel_enabled) m_rcr = (value & 7) <= 5 ? value & 7 : 5;
        break;
    case TCR0:
        if (!m_tx_channel_enabled) m_tcr = (value & 7) <= 5 ? value & 7 : 5;
        break;
    case IMR0:
        m_imr = value & 0x33;
        break;
    case RFCR0:
        if (!m_rx_channel_enabled) m_rfcr = std::min(value & 0xf, 15u);
        break;
    case TFCR0:
        if (!m_tx_channel_enabled) m_tfcr = std::min(value & 0xf, 15u);
        break;
    case RFF0:
        if ((value & 1) && (!m_rx_channel_enabled || !m_rx_block_enabled)) {
            m_rx_fifo.clear();
            m_rx_frame_byte_count = 0;
            m_rx_pio_left = true;
            m_rx_dma_left = true;
        }
        break;
    case TFF0:
        if ((value & 1) && (!m_tx_channel_enabled || !m_tx_block_enabled)) {
            m_tx_fifo.clear();
            m_tx_left_valid = false;
            m_tx_pio_drop_right = false;
            m_tx_dma_left_valid = false;
            m_tx_dma_drop_right = false;
        }
        break;
    case RRXDMA:
        if ((value & 1) && m_rx_dma_left) m_rx_dma_left = true;
        break;
    case TXDMA:
        if (p_transmitter_enabled.get_value()) write_tx_dma(value);
        return;
    case RTXDMA:
        break;
    case DMACR:
        value &= DMACR_RXBLOCK | DMACR_TXBLOCK;
        if ((value ^ m_dmacr) & DMACR_TXBLOCK) m_dma_tx_force_idle = true;
        if ((value ^ m_dmacr) & DMACR_RXBLOCK) m_dma_rx_force_idle = true;
        m_dmacr = value;
        break;
    default:
        return;
    }
    state_changed();
}

uint32_t dw_apb_i2s::read_register(uint32_t offset)
{
    switch (offset) {
    case IER:
        return m_global_enabled;
    case IRER:
        return m_rx_block_enabled;
    case ITER:
        return m_tx_block_enabled;
    case CER:
        return m_clock_enabled;
    case CCR:
        return m_ccr;
    case LRBR_LTHR0:
        return read_rx_sample(false);
    case RRBR_RTHR0:
        return read_rx_sample(false);
    case RER0:
        return m_rx_channel_enabled;
    case TER0:
        return m_tx_channel_enabled;
    case RCR0:
        return m_rcr;
    case TCR0:
        return m_tcr;
    case ISR0:
        return interrupt_status();
    case IMR0:
        return m_imr;
    case ROR0: {
        const uint32_t value = m_rx_overrun;
        m_rx_overrun = false;
        state_changed();
        return value;
    }
    case TOR0: {
        const uint32_t value = m_tx_overrun;
        m_tx_overrun = false;
        state_changed();
        return value;
    }
    case RFCR0:
        return m_rfcr;
    case TFCR0:
        return m_tfcr;
    case RXDMA:
        return read_rx_sample(true);
    case COMP_PARAM_2:
        return 4;
    case COMP_PARAM_1:
        return component_parameter_1();
    case COMP_VERSION:
        return COMPONENT_VERSION;
    case COMP_TYPE:
        return COMPONENT_TYPE;
    case DMACR:
        return m_dmacr;
    default:
        return 0;
    }
}

void dw_apb_i2s::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    const uint64_t address = trans.get_address();
    auto* data = trans.get_data_ptr();
    const unsigned int length = trans.get_data_length();
    const bool dma_halfword = length == sizeof(uint16_t) &&
                              ((trans.get_command() == tlm::TLM_WRITE_COMMAND && address == TXDMA) ||
                               (trans.get_command() == tlm::TLM_READ_COMMAND && address == RXDMA));
    if (!data || (length != sizeof(uint32_t) && !dma_halfword) || (address & (length - 1)) || address > DMACR ||
        trans.get_byte_enable_ptr()) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    uint32_t value = 0;
    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&value, data, length);
        write_register(static_cast<uint32_t>(address), value);
    } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        value = read_register(static_cast<uint32_t>(address));
        std::memcpy(data, &value, length);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    delay += sc_core::sc_time(p_access_latency_ns.get_value(), sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

extern "C" void module_register() { GSC_MODULE_REGISTER_C(dw_apb_i2s); }
