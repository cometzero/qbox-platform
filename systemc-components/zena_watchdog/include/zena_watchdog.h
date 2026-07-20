/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class zena_watchdog : public sc_core::sc_module
{
    static constexpr uint32_t WRR = 0x000;
    static constexpr uint32_t WCS = 0x000;
    static constexpr uint32_t WOR = 0x008;
    static constexpr uint32_t WORU = 0x00c;
    static constexpr uint32_t WCV = 0x010;
    static constexpr uint32_t WCVU = 0x014;
    static constexpr uint32_t W_IIDR = 0xfcc;
    static constexpr uint32_t WCS_EN = 1u << 0;
    static constexpr uint32_t WCS_WS0 = 1u << 1;
    static constexpr uint32_t WCS_WS1 = 1u << 2;
    static constexpr uint32_t WORU_MASK = 0x0000ffffu;
    static constexpr uint32_t IIDR = 0x0001043bu;
    static constexpr uint64_t FRAME_BYTES = 0x1000;

    uint32_t m_wcs = 0;
    uint32_t m_worl = 0;
    uint32_t m_woru = 0;
    uint32_t m_wcvl = 0;
    uint32_t m_wcvu = 0;
    uint64_t m_generation = 0;
    bool m_reset = false;
    unsigned int m_trace_count = 0;
    sc_core::sc_event m_rearm;

    uint64_t offset_ticks() const
    {
        return (static_cast<uint64_t>(m_woru) << 32) | m_worl;
    }

    uint64_t current_ticks() const
    {
        const long double seconds = sc_core::sc_time_stamp().to_seconds();
        return static_cast<uint64_t>(seconds * p_clock_frequency.get_value());
    }

    sc_core::sc_time stage_delay() const
    {
        const uint64_t frequency = p_clock_frequency.get_value();
        if (frequency == 0 || offset_ticks() == 0) {
            return sc_core::SC_ZERO_TIME;
        }
        return sc_core::sc_time(
            static_cast<double>(offset_ticks()) / frequency,
            sc_core::SC_SEC);
    }

    void write_outputs()
    {
        if (ws0.size() != 0) {
            ws0->write((m_wcs & WCS_WS0) != 0);
        }
        if (ws1.size() != 0) {
            ws1->write((m_wcs & WCS_WS1) != 0);
        }
    }

    void rearm(bool update_compare)
    {
        ++m_generation;
        if (update_compare && (m_wcs & WCS_EN) != 0) {
            const uint64_t compare = current_ticks() + offset_ticks();
            m_wcvl = static_cast<uint32_t>(compare);
            m_wcvu = static_cast<uint32_t>(compare >> 32);
        }
        m_rearm.notify(sc_core::SC_ZERO_TIME);
    }

    void clear_stages()
    {
        m_wcs &= ~(WCS_WS0 | WCS_WS1);
        write_outputs();
    }

    void reset_registers()
    {
        m_wcs = 0;
        m_worl = 0;
        m_woru = 0;
        m_wcvl = 0;
        m_wcvu = 0;
        clear_stages();
        rearm(false);
    }

    void timer_thread()
    {
        for (;;) {
            if (m_reset || (m_wcs & WCS_EN) == 0 ||
                (m_wcs & WCS_WS1) != 0) {
                sc_core::wait(m_rearm);
                continue;
            }

            const uint64_t generation = m_generation;
            sc_core::wait(stage_delay(), m_rearm);
            if (generation != m_generation || m_reset ||
                (m_wcs & WCS_EN) == 0) {
                continue;
            }
            if ((m_wcs & WCS_WS0) == 0) {
                m_wcs |= WCS_WS0;
            } else {
                m_wcs |= WCS_WS1;
            }
            write_outputs();
        }
    }

    uint32_t read_control(uint32_t offset) const
    {
        switch (offset) {
        case WCS:
            return m_wcs;
        case WOR:
            return m_worl;
        case WORU:
            return m_woru;
        case WCV:
            return m_wcvl;
        case WCVU:
            return m_wcvu;
        case W_IIDR:
            return IIDR;
        default:
            return 0;
        }
    }

    uint32_t read_refresh(uint32_t offset) const
    {
        return offset == W_IIDR ? IIDR : 0;
    }

    void write_control(uint32_t offset, uint32_t value)
    {
        switch (offset) {
        case WCS:
            m_wcs = value & WCS_EN;
            write_outputs();
            rearm(true);
            break;
        case WOR:
            m_worl = value;
            clear_stages();
            rearm(true);
            break;
        case WORU:
            m_woru = value & WORU_MASK;
            clear_stages();
            rearm(true);
            break;
        case WCV:
            m_wcvl = value;
            break;
        case WCVU:
            m_wcvu = value;
            break;
        default:
            break;
        }
    }

    void write_refresh(uint32_t offset)
    {
        if (offset == WRR) {
            clear_stages();
            rearm(true);
        }
    }

    bool access(tlm::tlm_generic_payload& trans, bool refresh_frame,
                bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (data == nullptr || len != sizeof(uint32_t) ||
            (offset & 0x3u) != 0 || offset + len > FRAME_BYTES) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        uint32_t value = 0;
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            value = refresh_frame
                ? read_refresh(static_cast<uint32_t>(offset))
                : read_control(static_cast<uint32_t>(offset));
            std::memcpy(data, &value, sizeof(value));
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&value, data, sizeof(value));
            if (refresh_frame) {
                write_refresh(static_cast<uint32_t>(offset));
            } else {
                write_control(static_cast<uint32_t>(offset), value);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        if (p_trace.get_value() && m_trace_count < p_trace_limit.get_value()) {
            ++m_trace_count;
            std::cerr << name() << " " << (debug ? "dbg_" : "")
                      << (trans.is_read() ? "read" : "write")
                      << " frame=" << (refresh_frame ? "refresh" : "control")
                      << " offset=0x" << std::hex << offset
                      << " value=0x" << value << std::dec << std::endl;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    void reset_changed(bool asserted)
    {
        m_reset = asserted;
        if (asserted) {
            reset_registers();
        }
    }

public:
    SC_HAS_PROCESS(zena_watchdog);

    cci::cci_param<uint64_t> p_clock_frequency;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    tlm_utils::simple_target_socket<zena_watchdog, DEFAULT_TLM_BUSWIDTH> control;
    tlm_utils::simple_target_socket<zena_watchdog, DEFAULT_TLM_BUSWIDTH> refresh;
    InitiatorSignalSocket<bool> ws0;
    InitiatorSignalSocket<bool> ws1;
    TargetSignalSocket<bool> reset;

    explicit zena_watchdog(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_clock_frequency("clock_frequency", 62500000ull)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 64)
        , control("control")
        , refresh("refresh")
        , ws0("ws0")
        , ws1("ws1")
        , reset("reset")
    {
        control.register_b_transport(this, &zena_watchdog::control_b_transport);
        control.register_transport_dbg(this, &zena_watchdog::control_transport_dbg);
        refresh.register_b_transport(this, &zena_watchdog::refresh_b_transport);
        refresh.register_transport_dbg(this, &zena_watchdog::refresh_transport_dbg);
        reset.register_value_changed_cb(
            [this](bool asserted) { reset_changed(asserted); });
        SC_THREAD(timer_thread);
    }

    void control_b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access(trans, false, false);
    }

    unsigned int control_transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, false, true) ? trans.get_data_length() : 0;
    }

    void refresh_b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access(trans, true, false);
    }

    unsigned int refresh_transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, true, true) ? trans.get_data_length() : 0;
    }
};

extern "C" void module_register();
