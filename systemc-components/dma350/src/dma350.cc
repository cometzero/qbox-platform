/* SPDX-License-Identifier: BSD-3-Clause */

#include <dma350.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>

namespace {
constexpr unsigned int MAX_CHANNELS = 16;
constexpr unsigned int MAX_TRIGGERS = 256;
constexpr unsigned int MODEL_DATA_WIDTH_LOG2 = 4; /* 128-bit */
constexpr unsigned int MODEL_ADDRESS_WIDTH = 48;
constexpr uint32_t IIDR_VALUE = 0x3a00043b; /* Arm DMA-350 r0p0 */
}

dma350::dma350(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , p_channel_count("channel_count", 4)
    , p_trigger_count("trigger_count", 28)
    , p_burst_bytes("burst_bytes", 256)
    , p_burst_latency("burst_latency", sc_core::sc_time(1, sc_core::SC_NS))
    , p_trace("trace", false)
    , p_trace_limit("trace_limit", 64)
    , p_trace_filter("trace_filter", "all")
    , p_trace_address_min("trace_address_min", 0)
    , initiator_socket("initiator_socket")
    , target_socket("target_socket")
    , reset("reset")
    , trig_in("trig_in", p_trigger_count.get_value())
    , trig_ack("trig_ack", p_trigger_count.get_value())
    , irq("irq", p_channel_count.get_value())
    , irq_comb_nonsec("irq_comb_nonsec")
    , m_channels(p_channel_count.get_value())
    , m_trigger_owner(p_trigger_count.get_value(), -1)
    , m_ack_values(p_trigger_count.get_value(), 0)
    , m_ack_stubs("ack_stub", p_trigger_count.get_value())
    , m_irq_stubs("irq_stub", p_channel_count.get_value())
    , m_irq_comb_nonsec_stub("irq_comb_nonsec_stub")
    , m_work_event(false)
    , m_output_event(false)
{
    if (p_channel_count.get_value() == 0 || p_channel_count.get_value() > MAX_CHANNELS)
        SC_REPORT_FATAL(name, "channel_count must be between 1 and 16");
    if (p_trigger_count.get_value() > MAX_TRIGGERS)
        SC_REPORT_FATAL(name, "trigger_count must not exceed 256");
    if (p_burst_bytes.get_value() == 0)
        SC_REPORT_FATAL(name, "burst_bytes must be non-zero");

    target_socket.register_b_transport(this, &dma350::b_transport);
    target_socket.register_transport_dbg(this, &dma350::transport_dbg);
    reset.register_value_changed_cb([this](bool asserted) {
        if (asserted) {
            m_reset_requested.store(true);
            m_work_event.notify();
        }
    });
    for (unsigned int trigger = 0; trigger < trig_in.size(); ++trigger) {
        trig_in[trigger].register_value_changed_cb(
            [this](uint32_t) { m_work_event.notify(); });
    }

    SC_METHOD(drive_outputs);
    sensitive << m_output_event;
    SC_THREAD(worker_thread);
    reset_registers();
}

void dma350::before_end_of_elaboration()
{
    for (unsigned int trigger = 0; trigger < trig_ack.size(); ++trigger) {
        if (!trig_ack[trigger].get_interface())
            trig_ack[trigger].bind(m_ack_stubs[trigger]);
    }
    for (unsigned int channel = 0; channel < irq.size(); ++channel) {
        if (!irq[channel].get_interface()) irq[channel].bind(m_irq_stubs[channel]);
    }
    if (!irq_comb_nonsec.get_interface())
        irq_comb_nonsec.bind(m_irq_comb_nonsec_stub);
}

bool dma350::is_supported_length(unsigned int len)
{
    return len == 1 || len == 2 || len == 4 || len == 8;
}

int16_t dma350::sign_extend16(uint32_t value)
{
    return static_cast<int16_t>(value & 0xffffu);
}

unsigned int dma350::transfer_bytes_from_ctrl(uint32_t ctrl)
{
    return 1u << (ctrl & 0x7u);
}

unsigned int dma350::trigger_mode(uint32_t config)
{
    return (config & TRIGIN_MODE_MASK) >> TRIGIN_MODE_SHIFT;
}

unsigned int dma350::trigger_type(uint32_t config)
{
    return (config & TRIGIN_TYPE_MASK) >> TRIGIN_TYPE_SHIFT;
}

bool dma350::request_active(uint32_t request)
{
    return (request & TRIGGER_ACTIVE) != 0;
}

unsigned int dma350::request_type(uint32_t request)
{
    return request & TRIGGER_TYPE_MASK;
}

void dma350::store32(uint32_t offset, uint32_t value)
{
    std::memcpy(&m_regs[offset], &value, sizeof(value));
}

uint32_t dma350::load32(uint32_t offset) const
{
    uint32_t value = 0;
    std::memcpy(&value, &m_regs[offset], sizeof(value));
    return value;
}

uint32_t dma350::channel_base(unsigned int channel) const
{
    return CH_BASE + channel * CH_STRIDE;
}

uint64_t dma350::channel_address(uint32_t base, uint32_t low_offset) const
{
    return load32(base + low_offset) |
           (static_cast<uint64_t>(load32(base + low_offset + 4)) << 32);
}

uint32_t dma350::source_xsize(uint32_t base) const
{
    return (load32(base + CH_XSIZE) & 0xffffu) |
           ((load32(base + CH_XSIZEHI) & 0xffffu) << 16);
}

uint32_t dma350::dest_xsize(uint32_t base) const
{
    return ((load32(base + CH_XSIZE) >> 16) & 0xffffu) |
           (((load32(base + CH_XSIZEHI) >> 16) & 0xffffu) << 16);
}

void dma350::store_address(uint32_t base, uint32_t low_offset, uint64_t value)
{
    store32(base + low_offset, static_cast<uint32_t>(value));
    store32(base + low_offset + 4, static_cast<uint32_t>(value >> 32));
}

void dma350::store_xsize(uint32_t base, uint32_t source, uint32_t dest)
{
    store32(base + CH_XSIZE, (source & 0xffffu) | ((dest & 0xffffu) << 16));
    store32(base + CH_XSIZEHI, (source >> 16) | ((dest >> 16) << 16));
}

void dma350::reset_channel(unsigned int channel)
{
    const uint32_t base = channel_base(channel);
    std::fill(m_regs.begin() + base, m_regs.begin() + base + CH_STRIDE, 0);
    store32(base + CH_CTRL, 0x00200200);
    store32(base + CH_DESTRANSCFG, 0x000f0400);
    store32(base + CH_IIDR, IIDR_VALUE);
    store32(base + CH_AIDR, 0);
    store32(base + CH_BUILDCFG0,
            (15u << 26) | (MODEL_DATA_WIDTH_LOG2 << 22) |
                ((MODEL_ADDRESS_WIDTH - 1) << 16) | 15u);
    /* 1D/fill, 32-bit XSIZE and selectable external trigger input support. */
    store32(base + CH_BUILDCFG1,
            (1u << 7) | (1u << 5) | (1u << 4) | (1u << 1) | 1u);
    m_channels[channel] = ChannelState{};
}

void dma350::reset_registers()
{
    for (unsigned int channel = 0; channel < m_channels.size(); ++channel)
        release_triggers(channel);

    m_regs.fill(0);
    std::fill(m_trigger_owner.begin(), m_trigger_owner.end(), -1);
    std::fill(m_ack_values.begin(), m_ack_values.end(), 0);
    for (unsigned int channel = 0; channel < m_channels.size(); ++channel)
        reset_channel(channel);

    store32(DMA_BUILDCFG0,
            (MODEL_DATA_WIDTH_LOG2 << 16) |
                ((MODEL_ADDRESS_WIDTH - 1) << 10) |
                ((static_cast<uint32_t>(m_channels.size()) - 1) << 4));
    store32(DMA_BUILDCFG1,
            (1u << 16) | static_cast<uint32_t>(m_trigger_owner.size()));
    store32(DMAINFO_IIDR, IIDR_VALUE);
    store32(DMAINFO_AIDR, 0);
    update_interrupt_summary();
    for (unsigned int channel = 0; channel < m_channels.size(); ++channel)
        update_irq(channel);
}

void dma350::release_triggers(unsigned int channel)
{
    if (channel >= m_channels.size()) return;
    ChannelState& state = m_channels[channel];
    TriggerSide* sides[] = { &state.source_trigger, &state.dest_trigger };
    for (auto* side : sides) {
        if (!side->used || side->select >= m_trigger_owner.size()) continue;
        if (side->ack_active) drive_trigger_ack(*side, false, ACK_OKAY);
        if (m_trigger_owner[side->select] == static_cast<int>(channel))
            m_trigger_owner[side->select] = -1;
    }
}

bool dma350::configure_trigger(unsigned int channel, uint32_t config, bool used,
                               TriggerSide& side, uint32_t error_bit)
{
    side = TriggerSide{};
    if (!used) return true;

    side.used = true;
    side.select = config & TRIGIN_SELECT_MASK;
    side.command = trigger_mode(config) == TRIGIN_MODE_COMMAND;
    side.peripheral_flow = trigger_mode(config) == TRIGIN_MODE_PERIPHERAL_FLOW;
    side.block_size = ((config >> TRIGIN_BLOCK_SHIFT) & 0xffu) + 1;

    if (trigger_type(config) != TRIGIN_TYPE_HW ||
        (trigger_mode(config) != TRIGIN_MODE_COMMAND &&
         trigger_mode(config) != TRIGIN_MODE_DMA_FLOW &&
         trigger_mode(config) != TRIGIN_MODE_PERIPHERAL_FLOW)) {
        set_error(channel, ERRINFO_CFG | ERRINFO_REG_VALUE);
        return false;
    }
    if (side.select >= m_trigger_owner.size()) {
        set_error(channel, ERRINFO_CFG | ERRINFO_REG_VALUE | error_bit);
        return false;
    }
    if (m_trigger_owner[side.select] != -1) {
        set_error(channel, error_bit);
        return false;
    }

    m_trigger_owner[side.select] = static_cast<int>(channel);
    return true;
}

bool dma350::configure_channel(unsigned int channel)
{
    const uint32_t base = channel_base(channel);
    const uint32_t ctrl = load32(base + CH_CTRL);
    const uint32_t xtype = (ctrl & CH_CTRL_XTYPE_MASK) >> CH_CTRL_XTYPE_SHIFT;
    ChannelState state{};

    state.transfer_bytes = transfer_bytes_from_ctrl(ctrl);
    if (state.transfer_bytes > (1u << MODEL_DATA_WIDTH_LOG2) ||
        (xtype != XTYPE_CONTINUE && xtype != XTYPE_WRAP &&
         xtype != XTYPE_FILL)) {
        set_error(channel, ERRINFO_CFG | ERRINFO_REG_VALUE);
        return false;
    }

    state.fill = xtype == XTYPE_FILL;
    state.wrap = xtype == XTYPE_WRAP;
    const uint64_t alignment_mask =
        ~(static_cast<uint64_t>(state.transfer_bytes) - 1u);
    state.source = channel_address(base, CH_SRCADDR) & alignment_mask;
    state.dest = channel_address(base, CH_DESADDR) & alignment_mask;
    state.start_source = state.source;
    state.start_dest = state.dest;
    state.source_remaining = state.fill ? 0 : source_xsize(base);
    state.dest_remaining = dest_xsize(base);
    state.source_length = state.source_remaining;
    state.wrap_remaining = state.wrap ?
        std::max(state.source_remaining, state.dest_remaining) : 0;
    state.source_inc = sign_extend16(load32(base + CH_XADDRINC));
    state.dest_inc = sign_extend16(load32(base + CH_XADDRINC) >> 16);

    if (state.wrap && state.source_remaining == 0 &&
        state.dest_remaining != 0) {
        m_channels[channel] = state;
        set_error(channel, ERRINFO_CFG | ERRINFO_REG_VALUE);
        return false;
    }

    m_channels[channel] = state;
    if (!configure_trigger(channel, load32(base + CH_SRCTRIGINCFG),
                           (ctrl & USE_SRC_TRIGGER) != 0,
                           m_channels[channel].source_trigger,
                           ERRINFO_SRC_TRIGGER) ||
        !configure_trigger(channel, load32(base + CH_DESTRIGINCFG),
                           (ctrl & USE_DEST_TRIGGER) != 0,
                           m_channels[channel].dest_trigger,
                           ERRINFO_DEST_TRIGGER)) {
        release_triggers(channel);
        return false;
    }

    ChannelState& configured = m_channels[channel];
    if (state.fill && configured.source_trigger.used) {
        set_error(channel, ERRINFO_CFG | ERRINFO_CFG_CONFLICT);
        release_triggers(channel);
        return false;
    }

    configured.enabled = true;
    configured.command_started =
        !(configured.source_trigger.used && configured.source_trigger.command) &&
        !(configured.dest_trigger.used && configured.dest_trigger.command);
    store_address(base, CH_SRCADDR, configured.source);
    store_address(base, CH_DESADDR, configured.dest);
    store32(base + CH_STATUS, 0);
    store32(base + CH_ERRINFO, 0);
    store32(base + CH_CMD, CH_CMD_ENABLE);
    update_irq(channel);
    update_interrupt_summary();
    return true;
}

void dma350::set_status_event(unsigned int channel, uint32_t status_bit,
                              uint32_t interrupt_bit)
{
    const uint32_t base = channel_base(channel);
    uint32_t status = load32(base + CH_STATUS) | status_bit;
    if ((load32(base + CH_INTREN) & interrupt_bit) != 0) status |= interrupt_bit;
    store32(base + CH_STATUS, status);
    update_irq(channel);
    update_interrupt_summary();
}

void dma350::set_error(unsigned int channel, uint32_t error_info)
{
    const uint32_t base = channel_base(channel);
    store32(base + CH_ERRINFO, error_info);
    set_status_event(channel, STAT_ERR, INTR_ERR);
    trace_operation(channel, m_channels[channel], "error");
    m_channels[channel].enabled = false;
    store32(base + CH_CMD, 0);
}

void dma350::finish_channel(unsigned int channel, uint32_t status_bit,
                            uint32_t interrupt_bit)
{
    ChannelState& state = m_channels[channel];
    const bool clear = state.clear_requested;
    trace_operation(channel, state,
                    status_bit == STAT_STOPPED ? "stopped" :
                    status_bit == STAT_DISABLED ? "disabled" : "done");
    release_triggers(channel);
    state.enabled = false;
    state.paused = false;
    store32(channel_base(channel) + CH_CMD, 0);
    set_status_event(channel, status_bit, interrupt_bit);
    if (clear) reset_channel(channel);
    update_irq(channel);
    update_interrupt_summary();
}

void dma350::write_command(unsigned int channel, uint32_t value)
{
    ChannelState& state = m_channels[channel];
    const uint32_t base = channel_base(channel);
    if ((value & CH_CMD_ENABLE) != 0) {
        if (!state.enabled && configure_channel(channel)) m_work_event.notify();
        return;
    }
    if ((value & CH_CMD_CLEAR) != 0) {
        if (state.enabled) {
            state.clear_requested = true;
            store32(base + CH_CMD, load32(base + CH_CMD) | CH_CMD_CLEAR);
        } else {
            release_triggers(channel);
            reset_channel(channel);
            update_irq(channel);
            update_interrupt_summary();
        }
    }
    if (!state.enabled) return;
    uint32_t command = load32(base + CH_CMD);
    if ((value & CH_CMD_STOP) != 0) {
        state.stop_requested = true;
        command |= CH_CMD_STOP;
    }
    if ((value & CH_CMD_DISABLE) != 0) {
        state.disable_requested = true;
        command |= CH_CMD_DISABLE;
    }
    if ((value & CH_CMD_PAUSE) != 0 && !state.paused) {
        state.pause_requested = true;
        command |= CH_CMD_PAUSE;
    }
    if ((value & CH_CMD_RESUME) != 0 && state.paused) {
        state.paused = false;
        state.pause_requested = false;
        uint32_t status = load32(channel_base(channel) + CH_STATUS);
        status &= ~(STAT_PAUSED | STAT_RESUMEWAIT);
        store32(base + CH_STATUS, status);
        command &= ~CH_CMD_PAUSE;
    }
    store32(base + CH_CMD, command);
    m_work_event.notify();
}

void dma350::clear_channel_status(unsigned int channel, uint32_t value)
{
    const uint32_t base = channel_base(channel);
    uint32_t clear = value & 0x7ffu;
    if ((value & STAT_DONE) != 0) clear |= STAT_DONE | INTR_DONE;
    if ((value & STAT_ERR) != 0) {
        clear |= STAT_ERR | INTR_ERR;
        store32(base + CH_ERRINFO, 0);
    }
    if ((value & STAT_DISABLED) != 0) clear |= STAT_DISABLED | INTR_DISABLED;
    if ((value & STAT_STOPPED) != 0) clear |= STAT_STOPPED | INTR_STOPPED;
    store32(base + CH_STATUS, load32(base + CH_STATUS) & ~clear);
    update_irq(channel);
    update_interrupt_summary();
}

void dma350::update_irq(unsigned int channel)
{
    (void)channel;
    m_output_event.notify(sc_core::SC_ZERO_TIME);
}

void dma350::update_interrupt_summary()
{
    uint32_t pending = 0;
    for (unsigned int channel = 0; channel < m_channels.size(); ++channel) {
        if ((load32(channel_base(channel) + CH_STATUS) & 0x7ffu) != 0)
            pending |= 1u << channel;
    }
    store32(SEC_CHINTRSTATUS0, 0);
    store32(NSEC_CHINTRSTATUS0, pending);
    store32(NSEC_STATUS,
            pending != 0 && (load32(NSEC_CTRL) & INTREN_ANYCHINTR) != 0 ?
                INTR_ANYCHINTR : 0);
}

void dma350::drive_outputs()
{
    for (unsigned int trigger = 0; trigger < trig_ack.size(); ++trigger) {
        if (trig_ack[trigger].get_interface())
            trig_ack[trigger]->write(m_ack_values[trigger]);
    }
    for (unsigned int channel = 0; channel < irq.size(); ++channel) {
        if (irq[channel].get_interface()) {
            irq[channel]->write(
                (load32(channel_base(channel) + CH_STATUS) & 0x7ffu) != 0);
        }
    }
    if (irq_comb_nonsec.get_interface())
        irq_comb_nonsec->write((load32(NSEC_STATUS) & INTR_ANYCHINTR) != 0);
}

bool dma350::clear_completed_handshakes(ChannelState& state)
{
    bool changed = false;
    TriggerSide* sides[] = { &state.source_trigger, &state.dest_trigger };
    for (auto* side : sides) {
        if (!side->used || !side->ack_active) continue;
        if (!request_active(trig_in[side->select].read())) {
            drive_trigger_ack(*side, false, ACK_OKAY);
            changed = true;
        }
    }
    return changed;
}

bool dma350::service_command_triggers(unsigned int channel, ChannelState& state)
{
    if (state.command_started) return false;
    bool ready = true;
    if (state.source_trigger.used && state.source_trigger.command) {
        if (!request_active(trig_in[state.source_trigger.select].read())) {
            set_status_event(channel, STAT_SRCWAIT, INTR_SRCWAIT);
            ready = false;
        }
    }
    if (state.dest_trigger.used && state.dest_trigger.command) {
        if (!request_active(trig_in[state.dest_trigger.select].read())) {
            set_status_event(channel, STAT_DESTWAIT, INTR_DESTWAIT);
            ready = false;
        }
    }
    if (!ready) return false;

    uint32_t status = load32(channel_base(channel) + CH_STATUS);
    status &= ~(STAT_SRCWAIT | STAT_DESTWAIT | INTR_SRCWAIT | INTR_DESTWAIT);
    store32(channel_base(channel) + CH_STATUS, status);
    if (state.source_trigger.used && state.source_trigger.command)
        drive_trigger_ack(state.source_trigger, true, ACK_OKAY);
    if (state.dest_trigger.used && state.dest_trigger.command)
        drive_trigger_ack(state.dest_trigger, true, ACK_OKAY);
    state.command_started = true;
    update_irq(channel);
    update_interrupt_summary();
    return true;
}

unsigned int dma350::flow_request_limit(const TriggerSide& side,
                                        uint32_t request) const
{
    const unsigned int type = request_type(request);
    return type == TRIGGER_BLOCK || type == TRIGGER_LAST_BLOCK ?
               side.block_size : 1u;
}

void dma350::drive_trigger_ack(TriggerSide& side, bool active, unsigned int type)
{
    side.ack_active = active;
    if (side.select < m_ack_values.size()) {
        m_ack_values[side.select] = (active ? TRIGGER_ACTIVE : 0u) |
                                    (type & TRIGGER_TYPE_MASK);
        m_output_event.notify(sc_core::SC_ZERO_TIME);
    }
}

bool dma350::execute_fill_burst(ChannelState& state, unsigned int units,
                                sc_core::sc_time& delay)
{
    const uint32_t fill = load32(channel_base(&state - m_channels.data()) + CH_FILLVAL);
    const unsigned int bytes = units * state.transfer_bytes;
    if (state.dest_inc == 1) {
        std::vector<uint8_t> data(bytes);
        for (unsigned int i = 0; i < bytes; ++i)
            data[i] = static_cast<uint8_t>(fill >> ((i & 3u) * 8));
        if (!mem_write(state.dest, data.data(), bytes, delay)) return false;
        state.dest += bytes;
    } else {
        std::array<uint8_t, 16> data{};
        for (unsigned int i = 0; i < state.transfer_bytes; ++i)
            data[i] = static_cast<uint8_t>(fill >> ((i & 3u) * 8));
        for (unsigned int i = 0; i < units; ++i) {
            if (!mem_write(state.dest, data.data(), state.transfer_bytes, delay)) return false;
            state.dest = static_cast<uint64_t>(
                static_cast<int64_t>(state.dest) +
                static_cast<int64_t>(state.dest_inc) * state.transfer_bytes);
        }
    }
    state.dest_remaining -= units;
    state.completed_bytes += static_cast<uint64_t>(units) * state.transfer_bytes;
    return true;
}

bool dma350::execute_copy_burst(ChannelState& state, unsigned int units,
                                sc_core::sc_time& delay, bool& read_error)
{
    const unsigned int write_units = std::min(units, state.dest_remaining);
    const unsigned int bytes = units * state.transfer_bytes;
    if (state.source_inc == 1 && state.dest_inc == 1 &&
        write_units == units) {
        std::vector<uint8_t> data(bytes);
        if (!mem_read(state.source, data.data(), bytes, delay)) {
            read_error = true;
            return false;
        }
        if (!mem_write(state.dest, data.data(), bytes, delay)) return false;
        state.source += bytes;
        state.dest += bytes;
    } else if (state.source_inc == 1 && write_units == 0) {
        std::vector<uint8_t> data(bytes);
        if (!mem_read(state.source, data.data(), bytes, delay)) {
            read_error = true;
            return false;
        }
        state.source += bytes;
    } else {
        std::array<uint8_t, 16> data{};
        for (unsigned int i = 0; i < units; ++i) {
            if (!mem_read(state.source, data.data(), state.transfer_bytes, delay)) {
                read_error = true;
                return false;
            }
            if (i < write_units &&
                !mem_write(state.dest, data.data(), state.transfer_bytes, delay))
                return false;
            state.source = static_cast<uint64_t>(
                static_cast<int64_t>(state.source) +
                static_cast<int64_t>(state.source_inc) * state.transfer_bytes);
            if (i < write_units) {
                state.dest = static_cast<uint64_t>(
                    static_cast<int64_t>(state.dest) +
                    static_cast<int64_t>(state.dest_inc) * state.transfer_bytes);
            }
        }
    }
    state.source_remaining -= units;
    state.dest_remaining -= write_units;
    state.completed_bytes +=
        static_cast<uint64_t>(write_units) * state.transfer_bytes;
    if (state.wrap) {
        state.wrap_remaining -= units;
        if (state.source_remaining == 0 && state.wrap_remaining != 0) {
            state.source = state.start_source;
            state.source_remaining = state.source_length;
        }
    }
    return true;
}

bool dma350::execute_burst(unsigned int channel, ChannelState& state,
                           unsigned int units, sc_core::sc_time& delay)
{
    bool read_error = false;
    const bool ok = state.fill ? execute_fill_burst(state, units, delay) :
                                 execute_copy_burst(state, units, delay, read_error);
    if (!ok) {
        set_error(channel, ERRINFO_BUS |
                               (read_error ? ERRINFO_READ_RESPONSE :
                                             ERRINFO_WRITE_RESPONSE));
        release_triggers(channel);
        return false;
    }
    update_progress(channel, state);
    return true;
}

void dma350::update_progress(unsigned int channel, const ChannelState& state)
{
    const uint32_t base = channel_base(channel);
    store_address(base, CH_SRCADDR, state.source);
    store_address(base, CH_DESADDR, state.dest);
    store_xsize(base, state.source_remaining, state.dest_remaining);
}

bool dma350::service_channel(unsigned int channel)
{
    ChannelState& state = m_channels[channel];
    if (!state.enabled) return false;

    if (state.stop_requested) {
        finish_channel(channel, STAT_STOPPED, INTR_STOPPED);
        return true;
    }
    if (state.pause_requested && !state.paused) {
        state.paused = true;
        store32(channel_base(channel) + CH_CMD,
                load32(channel_base(channel) + CH_CMD) |
                    CH_CMD_ENABLE | CH_CMD_PAUSE);
        set_status_event(channel, STAT_PAUSED | STAT_RESUMEWAIT, 0);
        return true;
    }
    if (state.paused) return false;

    const bool handshake_changed = clear_completed_handshakes(state);
    if (state.finish_after_ack) {
        if (state.source_trigger.ack_active || state.dest_trigger.ack_active)
            return handshake_changed;
        finish_channel(channel,
                       state.disable_requested ? STAT_DISABLED : STAT_DONE,
                       state.disable_requested ? INTR_DISABLED : INTR_DONE);
        return true;
    }
    if (!state.command_started) {
        const bool started = service_command_triggers(channel, state);
        if (!state.command_started) return handshake_changed || started;
    }

    const bool source_flow = state.source_trigger.used && !state.source_trigger.command;
    const bool dest_flow = state.dest_trigger.used && !state.dest_trigger.command;
    if ((source_flow && state.source_trigger.ack_active) ||
        (dest_flow && state.dest_trigger.ack_active))
        return handshake_changed;

    uint32_t source_request = 0;
    uint32_t dest_request = 0;
    unsigned int units = std::max(1u, p_burst_bytes.get_value() /
                                          state.transfer_bytes);
    bool last_request = false;
    if (source_flow) {
        source_request = trig_in[state.source_trigger.select].read();
        if (!request_active(source_request)) {
            set_status_event(channel, STAT_SRCWAIT, INTR_SRCWAIT);
            return handshake_changed;
        }
        units = std::min(units,
                         flow_request_limit(state.source_trigger, source_request));
        last_request |= state.source_trigger.peripheral_flow &&
                        (request_type(source_request) == TRIGGER_LAST_SINGLE ||
                         request_type(source_request) == TRIGGER_LAST_BLOCK);
    }
    if (dest_flow) {
        dest_request = trig_in[state.dest_trigger.select].read();
        if (!request_active(dest_request)) {
            set_status_event(channel, STAT_DESTWAIT, INTR_DESTWAIT);
            return handshake_changed;
        }
        units = std::min(units,
                         flow_request_limit(state.dest_trigger, dest_request));
        last_request |= state.dest_trigger.peripheral_flow &&
                        (request_type(dest_request) == TRIGGER_LAST_SINGLE ||
                         request_type(dest_request) == TRIGGER_LAST_BLOCK);
    }

    unsigned int remaining = state.fill ? state.dest_remaining :
        state.wrap ? state.wrap_remaining :
                     std::min(state.source_remaining, state.dest_remaining);
    if (state.wrap && state.source_remaining != 0)
        remaining = std::min(remaining, state.source_remaining);
    units = std::min(units, remaining);
    if (units == 0) {
        state.finish_after_ack = true;
        return true;
    }

    uint32_t status = load32(channel_base(channel) + CH_STATUS);
    status &= ~(STAT_SRCWAIT | STAT_DESTWAIT | INTR_SRCWAIT | INTR_DESTWAIT);
    store32(channel_base(channel) + CH_STATUS, status);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (!execute_burst(channel, state, units, delay)) return true;
    if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
    if (p_burst_latency.get_value() != sc_core::SC_ZERO_TIME)
        sc_core::wait(p_burst_latency.get_value());

    if (last_request) {
        if (source_flow && state.source_trigger.peripheral_flow)
            state.source_remaining = 0;
        if (dest_flow && state.dest_trigger.peripheral_flow)
            state.dest_remaining = 0;
        update_progress(channel, state);
    }
    const bool exhausted = state.wrap ? state.wrap_remaining == 0 :
        state.dest_remaining == 0 ||
        (!state.fill && state.source_remaining == 0);
    const bool peripheral_flow =
        (source_flow && state.source_trigger.peripheral_flow) ||
        (dest_flow && state.dest_trigger.peripheral_flow);
    if (peripheral_flow && exhausted && !last_request) {
        set_error(channel, ERRINFO_CFG | ERRINFO_CFG_CONFLICT);
        release_triggers(channel);
        return true;
    }
    const bool complete = last_request || exhausted;
    if (source_flow)
        drive_trigger_ack(state.source_trigger, true,
                          complete ? ACK_LAST_OKAY : ACK_OKAY);
    if (dest_flow)
        drive_trigger_ack(state.dest_trigger, true,
                          complete ? ACK_LAST_OKAY : ACK_OKAY);

    if (complete) {
        state.finish_after_ack =
            state.source_trigger.ack_active || state.dest_trigger.ack_active;
        if (!state.finish_after_ack)
            finish_channel(channel,
                           state.disable_requested ? STAT_DISABLED : STAT_DONE,
                           state.disable_requested ? INTR_DISABLED : INTR_DONE);
    }
    return true;
}

void dma350::worker_thread()
{
    while (true) {
        sc_core::wait(m_work_event);
        if (m_reset_requested.exchange(false)) reset_registers();

        bool progressed;
        do {
            progressed = false;
            for (unsigned int checked = 0; checked < m_channels.size(); ++checked) {
                const unsigned int channel =
                    (m_rr_channel + checked) % m_channels.size();
                if (service_channel(channel)) progressed = true;
            }
            m_rr_channel = (m_rr_channel + 1) % m_channels.size();
            if (progressed) sc_core::wait(sc_core::SC_ZERO_TIME);
            if (m_reset_requested.exchange(false)) {
                reset_registers();
                progressed = true;
            }
        } while (progressed);
    }
}

bool dma350::mem_read(uint64_t address, uint8_t* data, unsigned int len,
                      sc_core::sc_time& delay)
{
    if (initiator_socket.size() == 0) return false;
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(data);
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
    return trans.is_response_ok();
}

bool dma350::mem_write(uint64_t address, const uint8_t* data, unsigned int len,
                       sc_core::sc_time& delay)
{
    if (initiator_socket.size() == 0) return false;
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(const_cast<uint8_t*>(data));
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    initiator_socket->b_transport(trans, delay);
    return trans.is_response_ok();
}

bool dma350::writable_while_enabled(uint32_t offset) const
{
    return offset == CH_CMD || offset == CH_STATUS;
}

void dma350::write32(uint32_t offset, uint32_t value, bool execute_side_effects)
{
    if (offset >= CH_BASE) {
        const unsigned int channel = (offset - CH_BASE) / CH_STRIDE;
        if (channel >= m_channels.size()) return;
        const uint32_t ch_offset = (offset - CH_BASE) % CH_STRIDE;
        if (ch_offset == CH_STATUS) {
            if (execute_side_effects) clear_channel_status(channel, value);
            return;
        }
        if (ch_offset == CH_CMD) {
            if (execute_side_effects) write_command(channel, value);
            return;
        }
        if (m_channels[channel].enabled && !writable_while_enabled(ch_offset))
            return;
        if (ch_offset == CH_ERRINFO || ch_offset >= CH_IIDR) return;
    } else if (offset == NSEC_CTRL) {
        store32(NSEC_CTRL, value & INTREN_ANYCHINTR);
        update_interrupt_summary();
        m_output_event.notify(sc_core::SC_ZERO_TIME);
        return;
    } else if (offset == SEC_CHINTRSTATUS0 || offset == NSEC_CHINTRSTATUS0 ||
               offset == NSEC_STATUS ||
               (offset >= DMA_BUILDCFG0 && offset <= DMAINFO_AIDR)) {
        return;
    }
    store32(offset, value);
}

bool dma350::access(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay,
                    bool execute_side_effects)
{
    (void)delay;
    const uint64_t offset = trans.get_address();
    const unsigned int len = trans.get_data_length();
    uint8_t* data = trans.get_data_ptr();

    if (!data || !is_supported_length(len) || offset + len > m_regs.size()) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_byte_enable_ptr()) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_streaming_width() < len) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (offset >= CH_BASE) {
        const unsigned int channel = (offset - CH_BASE) / CH_STRIDE;
        if (channel >= m_channels.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::memcpy(data, &m_regs[offset], len);
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        if (len == sizeof(uint32_t) && (offset % sizeof(uint32_t)) == 0) {
            uint32_t value = 0;
            std::memcpy(&value, data, sizeof(value));
            write32(static_cast<uint32_t>(offset), value, execute_side_effects);
        } else {
            std::memcpy(&m_regs[offset], data, len);
        }
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    trace_access(trans, offset, len);
    return true;
}

void dma350::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    access(trans, delay, true);
}

unsigned int dma350::transport_dbg(tlm::tlm_generic_payload& trans)
{
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    return access(trans, delay, false) ? trans.get_data_length() : 0;
}

void dma350::trace_access(tlm::tlm_generic_payload& trans, uint64_t offset,
                          unsigned int len)
{
    if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value() ||
        !trace_access_filter_matches(offset)) return;
    ++m_trace_count;
    uint32_t value = 0;
    std::memcpy(&value, trans.get_data_ptr(), std::min<unsigned int>(len, 4));
    std::cerr << name() << ' '
              << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" : "write")
              << " offset=0x" << std::hex << offset << " len=0x" << len
              << " value=0x" << value << std::dec << std::endl;
}

void dma350::trace_operation(unsigned int channel, const ChannelState& state,
                             const char* status)
{
    if (!trace_operation_filter_matches(channel, !state.fill)) return;
    ++m_trace_count;
    std::cerr << name() << (state.fill ? " fill" : " copy")
              << " channel=0x" << std::hex << channel
              << " source=0x" << state.start_source
              << " dest=0x" << state.start_dest
              << " bytes=0x" << state.completed_bytes
              << std::dec
              << " src_trigger="
              << (state.source_trigger.used ?
                      static_cast<int>(state.source_trigger.select) : -1)
              << " dest_trigger="
              << (state.dest_trigger.used ?
                      static_cast<int>(state.dest_trigger.select) : -1)
              << " status=" << status << std::endl;
}

bool dma350::trace_access_filter_matches(uint64_t offset) const
{
    const auto filter = p_trace_filter.get_value();
    if (filter == "operation") return false;
    if (offset < CH_BASE)
        return p_trace_address_min.get_value() == 0 &&
               (filter.empty() || filter == "all");
    const uint32_t base = CH_BASE + ((offset - CH_BASE) / CH_STRIDE) * CH_STRIDE;
    const uint32_t ch_offset = (offset - CH_BASE) % CH_STRIDE;
    if (!trace_address_matches(base)) return false;
    if (filter.empty() || filter == "all") return true;
    if (filter == "copy") return is_copy_trace_offset(ch_offset);
    if (filter == "fill") return is_fill_trace_offset(ch_offset);
    return false;
}

bool dma350::trace_operation_filter_matches(unsigned int channel, bool copy) const
{
    if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value() ||
        !trace_address_matches(channel_base(channel))) return false;
    const auto filter = p_trace_filter.get_value();
    return filter.empty() || filter == "all" || filter == "operation" ||
           (copy ? filter == "copy" : filter == "fill");
}

bool dma350::trace_address_matches(uint32_t base) const
{
    const uint64_t min_address = p_trace_address_min.get_value();
    return min_address == 0 || channel_address(base, CH_SRCADDR) >= min_address ||
           channel_address(base, CH_DESADDR) >= min_address;
}

bool dma350::is_copy_trace_offset(uint32_t offset)
{
    return offset == CH_CMD || offset == CH_STATUS || offset == CH_CTRL ||
           (offset >= CH_SRCADDR && offset <= CH_XADDRINC) ||
           offset == CH_SRCTRIGINCFG || offset == CH_DESTRIGINCFG;
}

bool dma350::is_fill_trace_offset(uint32_t offset)
{
    return offset == CH_CMD || offset == CH_STATUS || offset == CH_CTRL ||
           (offset >= CH_DESADDR && offset <= CH_XADDRINC) ||
           offset == CH_FILLVAL || offset == CH_DESTRIGINCFG;
}

void module_register()
{
    GSC_MODULE_REGISTER_C(dma350);
}
