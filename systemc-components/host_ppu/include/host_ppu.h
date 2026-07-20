/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <runonsysc.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class host_ppu : public sc_core::sc_module
{
    static constexpr uint64_t REG_BYTES = 0x1000;
    static constexpr uint32_t PPU_PWPR = 0x000;
    static constexpr uint32_t PPU_PMER = 0x004;
    static constexpr uint32_t PPU_PWSR = 0x008;
    static constexpr uint32_t PPU_POWER_MASK = 0x0000000f;
    static constexpr uint32_t PPU_POWER_DYN_ENABLE = 0x00000100;
    static constexpr uint32_t PPU_POWER_DYN_STATUS = 0x00000100;
    static constexpr uint32_t PPU_OFF_LOCK_ENABLE = 0x00001000;
    static constexpr uint32_t PPU_OFF_LOCK_STATUS = 0x00001000;
    static constexpr uint32_t PPU_OP_POLICY_MASK = 0x000f0000;
    static constexpr uint32_t PPU_OP_DYN_ENABLE = 0x01000000;
    static constexpr uint32_t PPU_OP_DYN_STATUS = 0x01000000;
    static constexpr uint32_t PPU_POWER_STATUS_OFF = 0x0;
    static constexpr uint32_t PPU_POWER_STATUS_ON = 0x8;

    std::array<uint8_t, REG_BYTES> m_regs{};
    unsigned int m_trace_count = 0;
    struct pending_power_transition {
        bool power_on;
        uint32_t final_status;
    };

    std::deque<pending_power_transition> m_pending_power_transitions;
    sc_core::sc_event m_power_on_sequence_event;
    gs::runonsysc m_power_transition_run_on_sysc;

    static bool is_supported_length(unsigned int len)
    {
        return len == 1 || len == 2 || len == 4 || len == 8;
    }

    uint32_t load32(uint32_t offset) const
    {
        uint32_t value = 0;
        std::memcpy(&value, &m_regs[offset], sizeof(value));
        return value;
    }

    void store32(uint32_t offset, uint32_t value)
    {
        std::memcpy(&m_regs[offset], &value, sizeof(value));
    }

    void reset_registers()
    {
        m_regs.fill(0);
        store32(PPU_PWSR, p_initial_power_status.get_value() & PPU_POWER_MASK);
    }

    void write32(uint32_t offset, uint32_t value)
    {
        switch (offset) {
        case PPU_PWPR:
        {
            const uint32_t previous_status = load32(PPU_PWSR);
            uint32_t status = load32(PPU_PWSR);
            const bool power_dynamic = (value & PPU_POWER_DYN_ENABLE) != 0;
            const bool op_dynamic = (value & PPU_OP_DYN_ENABLE) != 0;
            const uint32_t requested_power = value & PPU_POWER_MASK;
            const uint32_t requested_op = value & PPU_OP_POLICY_MASK;

            // Dynamic policy is a minimum; without DEVACTIVE inputs it cannot
            // lower the current status until software requests a static mode.
            const uint32_t power_status = power_dynamic
                ? std::max(previous_status & PPU_POWER_MASK, requested_power)
                : requested_power;
            const uint32_t op_status = op_dynamic
                ? std::max(previous_status & PPU_OP_POLICY_MASK, requested_op)
                : requested_op;

            store32(PPU_PWPR, value);
            status &= ~(PPU_POWER_MASK | PPU_POWER_DYN_STATUS |
                        PPU_OFF_LOCK_STATUS | PPU_OP_POLICY_MASK |
                        PPU_OP_DYN_STATUS);
            status |= power_status | op_status;
            if (power_dynamic) {
                status |= PPU_POWER_DYN_STATUS;
            }
            if ((value & PPU_OFF_LOCK_ENABLE) != 0) {
                status |= PPU_OFF_LOCK_STATUS;
            }
            if (op_dynamic) {
                status |= PPU_OP_DYN_STATUS;
            }
            const bool defer_power_on_status =
                !power_status_is_on(previous_status) &&
                power_status_is_on(status) &&
                (p_power_on_status_delay_ns.get_value() != 0 ||
                 p_assert_power_on_load.get_value() ||
                 p_assert_power_on_reset.get_value());
            if (defer_power_on_status) {
                const uint32_t transitional_status =
                    (status & ~PPU_POWER_MASK) |
                    (previous_status & PPU_POWER_MASK);
                store32(PPU_PWSR, transitional_status);
            } else {
                store32(PPU_PWSR, status);
            }
            observe_power_transition(previous_status, status);
            break;
        }
        case PPU_PMER:
            store32(PPU_PMER, value);
            break;
        case PPU_PWSR:
            break;
        default:
            store32(offset, value);
            break;
        }
    }

    static bool power_status_is_on(uint32_t status)
    {
        return (status & PPU_POWER_MASK) == PPU_POWER_STATUS_ON;
    }

    static bool power_status_is_off(uint32_t status)
    {
        return (status & PPU_POWER_MASK) == PPU_POWER_STATUS_OFF;
    }

    void observe_power_transition(uint32_t previous_status, uint32_t status)
    {
        if (!power_status_is_on(previous_status) && power_status_is_on(status)) {
            trace_signal("power-on-sequence-scheduled", true);
            queue_power_transition(true, status);
        } else if (power_status_is_on(previous_status) && power_status_is_off(status) &&
                   p_power_on_reset_assert_on_power_off.get_value()) {
            queue_power_transition(false, status);
        }
    }

    void queue_power_transition(bool power_on, uint32_t final_status)
    {
        m_power_transition_run_on_sysc.run_on_sysc(
            [this, power_on, final_status] {
                if (!power_on) {
                    write_power_on_reset(true);
                    return;
                }

                m_pending_power_transitions.push_back(
                    {true, final_status});
                m_power_on_sequence_event.notify(sc_core::SC_ZERO_TIME);
            });
    }

    static void wait_ns(uint64_t ns)
    {
        if (ns == 0) {
            sc_core::wait(sc_core::SC_ZERO_TIME);
            return;
        }
        sc_core::wait(sc_core::sc_time(ns, sc_core::SC_NS));
    }

    void emit_power_on_sequence()
    {
        if (p_assert_power_on_reset.get_value() &&
            p_power_on_reset_assert_on_power_off.get_value() &&
            power_status_is_off(load32(PPU_PWSR))) {
            write_power_on_reset(true);
        }

        for (;;) {
            while (m_pending_power_transitions.empty()) {
                sc_core::wait(m_power_on_sequence_event);
            }
            const pending_power_transition transition =
                m_pending_power_transitions.front();
            m_pending_power_transitions.pop_front();

            if (!transition.power_on) {
                write_power_on_reset(true);
                continue;
            }

            if (p_assert_power_on_load.get_value()) {
                write_power_on_load(true);
                wait_ns(p_power_on_load_pulse_width_ns.get_value());
                write_power_on_load(false);
            }

            if (p_assert_power_on_reset.get_value()) {
                wait_ns(p_power_on_load_to_reset_delay_ns.get_value());
                write_power_on_reset(false);
            }

            wait_ns(p_power_on_status_delay_ns.get_value());
            if (power_status_is_on(load32(PPU_PWPR))) {
                store32(PPU_PWSR, transition.final_status);
                trace_signal("power-on-status", true);
            }
        }
    }

    void write_power_on_reset(bool asserted)
    {
        trace_signal("power-on-reset", asserted);
        if (power_on_reset.size() != 0) {
            power_on_reset->write(asserted);
        }
    }

    void write_power_on_load(bool asserted)
    {
        trace_signal("power-on-load", asserted);
        if (power_on_load.size() != 0) {
            power_on_load->write(asserted);
        }
    }

    void trace_signal(const char* event, bool asserted)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value()) {
            return;
        }

        ++m_trace_count;
        std::cerr << name() << " " << event
                  << " asserted=" << (asserted ? "true" : "false")
                  << std::endl;
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || !is_supported_length(len) || offset + len > m_regs.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_regs[offset], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            if (len == sizeof(uint32_t) && (offset % sizeof(uint32_t)) == 0) {
                uint32_t value = 0;
                std::memcpy(&value, data, sizeof(value));
                write32(static_cast<uint32_t>(offset), value);
            } else {
                std::memcpy(&m_regs[offset], data, len);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access(trans, offset, len, debug);

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    void trace_access(tlm::tlm_generic_payload& trans, uint64_t offset, unsigned int len, bool debug)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value()) {
            return;
        }

        ++m_trace_count;
        uint32_t value = 0;
        if (len <= sizeof(value)) {
            std::memcpy(&value, trans.get_data_ptr(), len);
        }

        std::cerr << name() << " "
                  << (debug ? "dbg_" : "")
                  << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" : "write")
                  << " offset=0x" << std::hex << offset
                  << " len=0x" << len
                  << " value=0x" << value
                  << std::dec << std::endl;
    }

public:
    SC_HAS_PROCESS(host_ppu);

    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    cci::cci_param<uint32_t> p_initial_power_status;
    cci::cci_param<bool> p_assert_power_on_reset;
    cci::cci_param<bool> p_power_on_reset_assert_on_power_off;
    cci::cci_param<bool> p_assert_power_on_load;
    cci::cci_param<uint64_t> p_power_on_load_pulse_width_ns;
    cci::cci_param<uint64_t> p_power_on_load_to_reset_delay_ns;
    cci::cci_param<uint64_t> p_power_on_status_delay_ns;
    cci::cci_param<uint64_t> p_access_latency_ns;
    tlm_utils::simple_target_socket<host_ppu, DEFAULT_TLM_BUSWIDTH> target_socket;
    InitiatorSignalSocket<bool> power_on_reset;
    InitiatorSignalSocket<bool> power_on_load;
    TargetSignalSocket<bool> reset;

    explicit host_ppu(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , m_power_transition_run_on_sysc("power_transition_run_on_sysc")
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 64)
        , p_initial_power_status("initial_power_status", 0x00000000)
        , p_assert_power_on_reset("assert_power_on_reset", false)
        , p_power_on_reset_assert_on_power_off(
              "power_on_reset_assert_on_power_off", true)
        , p_assert_power_on_load("assert_power_on_load", false)
        , p_power_on_load_pulse_width_ns("power_on_load_pulse_width_ns", 1)
        , p_power_on_load_to_reset_delay_ns("power_on_load_to_reset_delay_ns", 1)
        , p_power_on_status_delay_ns("power_on_status_delay_ns", 0)
        , p_access_latency_ns("access_latency_ns", 0)
        , target_socket("target_socket")
        , power_on_reset("power_on_reset")
        , power_on_load("power_on_load")
        , reset("reset")
    {
        reset_registers();
        SC_THREAD(emit_power_on_sequence);
        target_socket.register_b_transport(this, &host_ppu::b_transport);
        target_socket.register_transport_dbg(this, &host_ppu::transport_dbg);
        reset.register_value_changed_cb([this](const bool& asserted) {
            if (!asserted) {
                return;
            }
            m_pending_power_transitions.clear();
            m_power_on_sequence_event.cancel();
            reset_registers();
            if (p_assert_power_on_reset.get_value()) {
                write_power_on_reset(true);
            }
        });
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        delay += sc_core::sc_time(p_access_latency_ns.get_value(),
                                  sc_core::SC_NS);
        trans.set_dmi_allowed(false);
        access(trans, false);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, true) ? trans.get_data_length() : 0;
    }
};

extern "C" void module_register();
