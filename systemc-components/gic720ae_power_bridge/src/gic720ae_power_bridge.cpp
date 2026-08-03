/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gic720ae_power_bridge.h>

gic720ae_power_bridge::gic720ae_power_bridge(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , p_backend_redist_base("backend_redist_base", 0x20880000)
    , p_backend_redist_stride("backend_redist_stride", GICR_FRAME_BYTES)
    , p_redistributor_count("redistributor_count", 1)
    , target_socket("target_socket")
    , backend_socket("backend_socket")
    , irq_in("irq_in", p_redistributor_count.get_value())
    , fiq_in("fiq_in", p_redistributor_count.get_value())
    , virq_in("virq_in", p_redistributor_count.get_value())
    , vfiq_in("vfiq_in", p_redistributor_count.get_value())
    , irq_out("irq_out", p_redistributor_count.get_value())
    , fiq_out("fiq_out", p_redistributor_count.get_value())
    , virq_out("virq_out", p_redistributor_count.get_value())
    , vfiq_out("vfiq_out", p_redistributor_count.get_value())
    , reset("reset")
{
    m_state.resize(p_redistributor_count.get_value());
    target_socket.register_b_transport(
        this, &gic720ae_power_bridge::b_transport);
    target_socket.register_transport_dbg(
        this, &gic720ae_power_bridge::transport_dbg);

    for (unsigned int index = 0; index < m_state.size(); ++index) {
        irq_in[index].register_value_changed_cb(
            [this, index](bool value) { receive(index, 0, value); });
        fiq_in[index].register_value_changed_cb(
            [this, index](bool value) { receive(index, 1, value); });
        virq_in[index].register_value_changed_cb(
            [this, index](bool value) { receive(index, 2, value); });
        vfiq_in[index].register_value_changed_cb(
            [this, index](bool value) { receive(index, 3, value); });
    }
    reset.register_value_changed_cb([this](bool asserted) {
        if (asserted) {
            reset_model();
        }
    });
    reset_model();
}

bool gic720ae_power_bridge::decode_redistributor(
    uint64_t address, unsigned int& index, uint64_t& offset) const
{
    const uint64_t base = p_backend_redist_base.get_value();
    const uint64_t stride = p_backend_redist_stride.get_value();
    if (address < base || stride < GICR_FRAME_BYTES || stride == 0) {
        return false;
    }
    const uint64_t relative = address - base;
    const uint64_t candidate = relative / stride;
    offset = relative % stride;
    if (candidate >= m_state.size() || offset >= GICR_FRAME_BYTES) {
        return false;
    }
    index = static_cast<unsigned int>(candidate);
    return true;
}

bool gic720ae_power_bridge::validate_register_access(
    tlm::tlm_generic_payload& trans) const
{
    if (trans.get_data_ptr() == nullptr ||
        trans.get_data_length() != sizeof(uint32_t)) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_command() != tlm::TLM_READ_COMMAND &&
        trans.get_command() != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    return true;
}

bool gic720ae_power_bridge::forward(
    tlm::tlm_generic_payload& trans, sc_core::sc_time& delay, bool debug)
{
    if (backend_socket.size() == 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    if (debug) {
        const unsigned int transferred = backend_socket->transport_dbg(trans);
        if (transferred == trans.get_data_length() &&
            trans.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE) {
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
        } else if (transferred != trans.get_data_length() &&
                   trans.get_response_status() ==
                       tlm::TLM_INCOMPLETE_RESPONSE) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
    } else {
        backend_socket->b_transport(trans, delay);
    }
    return trans.is_response_ok();
}

uint32_t gic720ae_power_bridge::waker_value(unsigned int index) const
{
    const RedistributorState& state = m_state[index];
    return (m_sleep ? WAKER_SLEEP : 0) |
           (state.processor_sleep ? WAKER_PROCESSOR_SLEEP : 0) |
           (state.children_asleep ? WAKER_CHILDREN_ASLEEP : 0) |
           (m_quiescent ? WAKER_QUIESCENT : 0);
}

bool gic720ae_power_bridge::all_children_asleep(
    unsigned int selected, const RedistributorState& selected_state) const
{
    for (unsigned int index = 0; index < m_state.size(); ++index) {
        const RedistributorState& state =
            index == selected ? selected_state : m_state[index];
        if (!state.processor_sleep || !state.children_asleep) {
            return false;
        }
    }
    return true;
}

bool gic720ae_power_bridge::access_waker(
    unsigned int index, tlm::tlm_generic_payload& trans,
    sc_core::sc_time& delay, bool debug)
{
    if (!validate_register_access(trans)) {
        return false;
    }
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const uint32_t value = waker_value(index);
        std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    uint32_t request = 0;
    std::memcpy(&request, trans.get_data_ptr(), sizeof(request));
    RedistributorState next = m_state[index];
    bool next_sleep = m_sleep;
    bool next_quiescent = m_quiescent;

    if ((request & WAKER_SLEEP) == 0) {
        next_sleep = false;
        next_quiescent = false;
    }
    if ((request & WAKER_PROCESSOR_SLEEP) != 0) {
        next.processor_sleep = true;
        next.children_asleep = true;
    } else if (!next_sleep && !next_quiescent && !next.powered_down) {
        next.processor_sleep = false;
        next.children_asleep = false;
    }
    if ((request & WAKER_SLEEP) != 0 &&
        all_children_asleep(index, next)) {
        next_sleep = true;
        next_quiescent = true;
    }

    const uint32_t forwarded =
        next.processor_sleep ? WAKER_PROCESSOR_SLEEP : 0;
    std::array<uint8_t, sizeof(uint32_t)> original;
    std::memcpy(original.data(), trans.get_data_ptr(), original.size());
    std::memcpy(trans.get_data_ptr(), &forwarded, sizeof(forwarded));
    const bool success = forward(trans, delay, debug);
    std::memcpy(trans.get_data_ptr(), original.data(), original.size());
    if (!success) {
        return false;
    }

    m_state[index] = next;
    m_sleep = next_sleep;
    m_quiescent = next_quiescent;
    refresh(index);
    return true;
}

bool gic720ae_power_bridge::access_pwrr(
    unsigned int index, tlm::tlm_generic_payload& trans)
{
    if (!validate_register_access(trans)) {
        return false;
    }
    RedistributorState& state = m_state[index];
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const uint32_t value = state.powered_down ? PWRR_RDPD : 0;
        std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
    } else {
        uint32_t value = 0;
        std::memcpy(&value, trans.get_data_ptr(), sizeof(value));
        const bool power_down = (value & PWRR_RDPD) != 0;
        if (!power_down || state.processor_sleep) {
            state.powered_down = power_down;
            refresh(index);
        }
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return true;
}

bool gic720ae_power_bridge::access(
    tlm::tlm_generic_payload& trans, sc_core::sc_time& delay, bool debug)
{
    unsigned int index = 0;
    uint64_t offset = 0;
    if (decode_redistributor(trans.get_address(), index, offset)) {
        if (offset == GICR_WAKER) {
            return access_waker(index, trans, delay, debug);
        }
        if (offset == GICR_PWRR) {
            return access_pwrr(index, trans);
        }
    }
    return forward(trans, delay, debug);
}

bool gic720ae_power_bridge::delivery_enabled(unsigned int index) const
{
    const RedistributorState& state = m_state[index];
    return !state.powered_down && !state.processor_sleep &&
           !state.children_asleep;
}

void gic720ae_power_bridge::receive(
    unsigned int index, unsigned int kind, bool value)
{
    if (index >= m_state.size() || kind >= SIGNAL_KINDS) {
        return;
    }
    m_state[index].input_level[kind] = value;
    drive(index, kind, delivery_enabled(index) ? value : false);
}

void gic720ae_power_bridge::drive(
    unsigned int index, unsigned int kind, bool value)
{
    switch (kind) {
    case 0:
        if (irq_out[index].size() != 0) {
            irq_out[index]->write(value);
        }
        break;
    case 1:
        if (fiq_out[index].size() != 0) {
            fiq_out[index]->write(value);
        }
        break;
    case 2:
        if (virq_out[index].size() != 0) {
            virq_out[index]->write(value);
        }
        break;
    case 3:
        if (vfiq_out[index].size() != 0) {
            vfiq_out[index]->write(value);
        }
        break;
    default:
        break;
    }
}

void gic720ae_power_bridge::refresh(unsigned int index)
{
    const bool enabled = delivery_enabled(index);
    for (unsigned int kind = 0; kind < SIGNAL_KINDS; ++kind) {
        drive(index, kind, enabled ? m_state[index].input_level[kind] : false);
    }
}

void gic720ae_power_bridge::reset_model()
{
    m_sleep = false;
    m_quiescent = false;
    for (unsigned int index = 0; index < m_state.size(); ++index) {
        m_state[index] = RedistributorState {};
        refresh(index);
    }
}

void gic720ae_power_bridge::b_transport(
    tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    access(trans, delay, false);
}

unsigned int gic720ae_power_bridge::transport_dbg(
    tlm::tlm_generic_payload& trans)
{
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    return access(trans, delay, true) ? trans.get_data_length() : 0;
}

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(gic720ae_power_bridge);
}
