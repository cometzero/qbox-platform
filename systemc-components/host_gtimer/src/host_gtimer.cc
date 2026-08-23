/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <host_gtimer.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

#include <arm_system_counter.h>
#include <module_factory_registery.h>

namespace {

gs::arm_system_counter& require_counter(sc_core::sc_object* object)
{
    auto* counter = dynamic_cast<gs::arm_system_counter*>(object);
    if (counter == nullptr) {
        throw std::invalid_argument(
            "host_gtimer requires an arm_system_counter");
    }
    return *counter;
}

}

uint32_t host_gtimer::load32(uint32_t offset) const
{
    uint32_t value = 0;
    std::memcpy(&value, &m_regs[offset], sizeof(value));
    return value;
}

void host_gtimer::store32(uint32_t offset, uint32_t value)
{
    std::memcpy(&m_regs[offset], &value, sizeof(value));
}

bool host_gtimer::is_sync_id_offset(uint32_t offset)
{
    return offset == PIDR4 ||
           (offset >= PIDR0 && offset <= CIDR3 &&
            (offset - PIDR0) % sizeof(uint32_t) == 0);
}

bool host_gtimer::is_sync_rw_offset(uint32_t offset)
{
    return offset <= SYNC_RW_LAST &&
           (offset % sizeof(uint32_t)) == 0;
}

bool host_gtimer::is_wide_impl_defined_write(
    const tlm::tlm_generic_payload& trans) const
{
    const uint64_t offset = trans.get_address();
    return p_counter_control.get_value() && trans.is_write() &&
           trans.get_data_length() == sizeof(uint64_t) &&
           offset >= IMPDEF_FIRST &&
           offset <= IMPDEF_LAST - sizeof(uint32_t) &&
           offset % sizeof(uint64_t) == 0;
}

uint32_t host_gtimer::sync_id_value(uint32_t offset)
{
    static constexpr std::array<uint32_t, 8> peripheral_ids = {
        0x000000e8, 0x000000b0, 0x0000001b, 0x00000000,
        0x0000000d, 0x000000f0, 0x00000005, 0x000000b1,
    };
    if (offset == PIDR4) {
        return 0x00000004;
    }
    return peripheral_ids[(offset - PIDR0) / sizeof(uint32_t)];
}

uint32_t host_gtimer::read32(
    uint32_t offset, const sc_core::sc_time& effective_time) const
{
    if (p_sync_frame.get_value() && is_sync_id_offset(offset)) {
        return sync_id_value(offset);
    }

    const bool counter_view =
        p_counter_read.get_value() || p_counter_base.get_value();
    if (counter_view && (offset == PCTL || offset == PCTH)) {
        const uint64_t count = m_counter.count_at(effective_time);
        return offset == PCTL ? static_cast<uint32_t>(count) :
                                static_cast<uint32_t>(count >> 32);
    }
    if (p_counter_base.get_value() && offset == FRQ) {
        return static_cast<uint32_t>(
            m_counter.snapshot().reported_frequency_hz);
    }
    if (p_counter_base.get_value() && offset == P_CTL) {
        const uint32_t control =
            load32(offset) & (P_CTL_ENABLE | P_CTL_IMASK);
        return timer_expired(effective_time) ?
            control | P_CTL_ISTATUS : control;
    }
    if (p_counter_control.get_value()) {
        if (offset == CNTCR) {
            const auto state = m_counter.snapshot();
            return (load32(offset) & ~3u) |
                   (state.enabled ? 1u : 0u) |
                   (state.halt_on_debug ? 2u : 0u);
        }
        if (offset == CNTCV_L || offset == CNTCV_H) {
            const uint64_t count = m_counter.count_at(effective_time);
            return offset == CNTCV_L ?
                static_cast<uint32_t>(count) :
                static_cast<uint32_t>(count >> 32);
        }
        if (offset == CNTFID0) {
            return static_cast<uint32_t>(
                m_counter.snapshot().reported_frequency_hz);
        }
    }
    return load32(offset);
}

void host_gtimer::write32(
    uint32_t offset, uint32_t value,
    const sc_core::sc_time& effective_time)
{
    if (p_counter_base.get_value() &&
        (offset == P_CVALL || offset == P_CVALH ||
         offset == P_CTL)) {
        store32(offset, offset == P_CTL ?
            value & (P_CTL_ENABLE | P_CTL_IMASK) : value);
        m_timer_rearm.notify(sc_core::SC_ZERO_TIME);
        return;
    }
    if (!p_counter_control.get_value()) {
        store32(offset, value);
        return;
    }
    if (offset == CNTCR) {
        store32(offset, value);
        m_counter.set_control_at(
            (value & 1u) != 0, (value & 2u) != 0, effective_time);
    } else if (offset == CNTCV_L) {
        m_counter.write_count_low_at(value, effective_time);
    } else if (offset == CNTCV_H) {
        m_counter.write_count_high_at(value, effective_time);
    } else if (offset == CNTFID0) {
        store32(offset, value);
        m_counter.set_reported_frequency_at(value, effective_time);
    } else if (offset == CNTSCR) {
        store32(offset, value);
        m_counter.set_scale_8_24_at(value, effective_time);
    } else if (offset == CNTINCR) {
        store32(offset, value);
        m_counter.set_integer_increment_at(
            value == 0 ? 1 : value, effective_time);
    } else if (offset != CNTSR) {
        store32(offset, value);
    }
}

void host_gtimer::reset_registers()
{
    m_regs.fill(0);
    if (p_counter_control.get_value()) {
        const auto state = m_counter.snapshot();
        store32(CNTCR, (state.enabled ? 1u : 0u) |
                          (state.halt_on_debug ? 2u : 0u));
        store32(
            CNTFID0,
            static_cast<uint32_t>(state.reported_frequency_hz));
    }
}

uint64_t host_gtimer::compare_value() const
{
    return (static_cast<uint64_t>(load32(P_CVALH)) << 32) |
           load32(P_CVALL);
}

bool host_gtimer::timer_expired(const sc_core::sc_time& time) const
{
    return p_counter_base.get_value() &&
           (load32(P_CTL) & P_CTL_ENABLE) != 0 &&
           m_counter.count_at(time) >= compare_value();
}

bool host_gtimer::timer_delay(sc_core::sc_time& delay) const
{
    using Wide = unsigned __int128;

    if (!p_counter_base.get_value() ||
        (load32(P_CTL) & P_CTL_ENABLE) == 0) {
        return false;
    }

    const sc_core::sc_time now = sc_core::sc_time_stamp();
    const auto state = m_counter.snapshot_at(now);
    const uint64_t compare = compare_value();
    if (state.anchor_count >= compare || !state.running() ||
        state.increment_8_24 == 0) {
        return false;
    }

    const Wide count_delta =
        (Wide(compare - state.anchor_count) <<
         gs::arm_system_counter::fractional_bits) -
        state.fractional_count;
    const Wide input_ticks =
        (count_delta + state.increment_8_24 - 1) /
        state.increment_8_24;
    const Wide ticks_per_second =
        sc_core::sc_time(1, sc_core::SC_SEC).value();
    const Wide required =
        input_ticks * ticks_per_second;
    const Wide remaining = required > state.input_tick_remainder ?
        required - state.input_tick_remainder : 0;
    Wide delay_ticks =
        (remaining + state.input_frequency_hz - 1) /
        state.input_frequency_hz;
    const Wide max_delay_ticks =
        std::numeric_limits<sc_dt::uint64>::max() - now.value();
    if (delay_ticks > max_delay_ticks) {
        return false;
    }
    delay = sc_core::sc_time::from_value(
        static_cast<sc_dt::uint64>(delay_ticks));
    return true;
}

void host_gtimer::update_timer_irq()
{
    const uint32_t control = load32(P_CTL);
    const bool asserted =
        timer_expired(sc_core::sc_time_stamp()) &&
        (control & P_CTL_IMASK) == 0;
    if (asserted == m_irq_asserted) {
        return;
    }
    m_irq_asserted = asserted;
    if (irq.size() != 0) {
        irq->write(asserted);
    }
}

void host_gtimer::timer_thread()
{
    for (;;) {
        const auto events =
            m_timer_rearm | m_counter.state_changed_event();
        const auto counter_state = m_counter.snapshot();
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        if (counter_state.anchor_time_ticks > now.value()) {
            sc_core::wait(
                sc_core::sc_time::from_value(
                    counter_state.anchor_time_ticks - now.value()),
                events);
            continue;
        }

        update_timer_irq();
        sc_core::sc_time delay;
        if (timer_delay(delay)) {
            sc_core::wait(delay, events);
        } else {
            sc_core::wait(events);
        }
    }
}

bool host_gtimer::valid_access_shape(
    tlm::tlm_generic_payload& trans) const
{
    const uint64_t offset = trans.get_address();
    const unsigned int len = trans.get_data_length();
    if (trans.get_data_ptr() == nullptr || len == 0 ||
        len > FRAME_BYTES || offset > FRAME_BYTES - len) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_streaming_width() != 0 &&
        trans.get_streaming_width() < len) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (is_wide_impl_defined_write(trans)) {
        return true;
    }
    if (offset < REG_BYTES &&
        (len != sizeof(uint32_t) ||
         offset % sizeof(uint32_t) != 0)) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    return true;
}

bool host_gtimer::access(
    tlm::tlm_generic_payload& trans, bool debug,
    const sc_core::sc_time& delay)
{
    if (!valid_access_shape(trans)) {
        return false;
    }

    const uint64_t offset = trans.get_address();
    const unsigned int len = trans.get_data_length();
    uint8_t* data = trans.get_data_ptr();
    if (debug && trans.is_write()) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    if (offset >= REG_BYTES) {
        if (trans.is_read()) {
            std::memset(data, 0, len);
        } else if (!trans.is_write()) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trace_access(trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    const auto offset32 = static_cast<uint32_t>(offset);
    if (is_wide_impl_defined_write(trans)) {
        trace_access(trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }
    if (p_sync_frame.get_value() &&
        !is_sync_rw_offset(offset32) &&
        !is_sync_id_offset(offset32)) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }

    const sc_core::sc_time effective_time =
        sc_core::sc_time_stamp() + delay;
    if (trans.is_read()) {
        const uint32_t value = read32(offset32, effective_time);
        std::memcpy(data, &value, sizeof(value));
    } else if (trans.is_write()) {
        if (!(p_sync_frame.get_value() &&
              is_sync_id_offset(offset32))) {
            uint32_t value = 0;
            std::memcpy(&value, data, sizeof(value));
            write32(offset32, value, effective_time);
        }
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }

    trace_access(trans, offset, len, debug);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return true;
}

void host_gtimer::trace_access(
    tlm::tlm_generic_payload& trans, uint64_t offset,
    unsigned int len, bool debug)
{
    if (!p_trace.get_value() ||
        m_trace_count >= p_trace_limit.get_value()) {
        return;
    }

    ++m_trace_count;
    uint32_t value = 0;
    if (len <= sizeof(value)) {
        std::memcpy(&value, trans.get_data_ptr(), len);
    }

    std::cerr << name() << " "
              << (debug ? "dbg_" : "")
              << (trans.is_read() ? "read" : "write")
              << " offset=0x" << std::hex << offset
              << " len=0x" << len
              << " value=0x" << value
              << std::dec << std::endl;
}

host_gtimer::host_gtimer(
    sc_core::sc_module_name name, gs::arm_system_counter& counter)
    : sc_core::sc_module(name)
    , m_counter(counter)
    , p_counter_base("counter_base", false)
    , p_counter_control("counter_control", false)
    , p_counter_read("counter_read", false)
    , p_sync_frame("sync_frame", false)
    , p_trace("trace", false)
    , p_trace_limit("trace_limit", 64)
    , target_socket("target_socket")
    , irq("irq")
{
    reset_registers();
    target_socket.register_b_transport(
        this, &host_gtimer::b_transport);
    target_socket.register_transport_dbg(
        this, &host_gtimer::transport_dbg);
    target_socket.register_get_direct_mem_ptr(
        this, &host_gtimer::get_direct_mem_ptr);
}

host_gtimer::host_gtimer(
    sc_core::sc_module_name name, sc_core::sc_object* counter)
    : host_gtimer(name, require_counter(counter))
{
}

void host_gtimer::before_end_of_elaboration()
{
    reset_registers();
    if (p_counter_base.get_value() && irq.size() != 0 &&
        !m_timer_process_registered) {
        SC_THREAD(timer_thread);
        m_timer_process_registered = true;
    }
}

host_gtimer::FrontendSnapshot host_gtimer::snapshot_at(
    const sc_core::sc_time& effective_time) const
{
    FrontendSnapshot view;
    const auto state = m_counter.snapshot_at(effective_time);
    view.counter = state.anchor_count;
    view.input_frequency_hz = state.input_frequency_hz;
    view.reported_frequency_hz = state.reported_frequency_hz;
    view.increment_8_24 = state.increment_8_24;
    view.generation = state.generation;
    view.observed = true;
    return view;
}

void host_gtimer::b_transport(
    tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    access(trans, false, delay);
}

unsigned int host_gtimer::transport_dbg(
    tlm::tlm_generic_payload& trans)
{
    return access(trans, true, sc_core::SC_ZERO_TIME) ?
        trans.get_data_length() : 0;
}

bool host_gtimer::get_direct_mem_ptr(
    tlm::tlm_generic_payload& trans, tlm::tlm_dmi&)
{
    trans.set_dmi_allowed(false);
    return false;
}

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(host_gtimer, sc_core::sc_object*);
}
