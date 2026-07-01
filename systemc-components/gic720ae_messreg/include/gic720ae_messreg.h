/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class gic720ae_messreg : public sc_core::sc_module
{
    static constexpr uint64_t WINDOW_BYTES = 0x10000;

    using target_socket_t =
        tlm_utils::simple_target_socket_b<
            gic720ae_messreg, DEFAULT_TLM_BUSWIDTH,
            tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND>;

    std::array<uint8_t, WINDOW_BYTES> m_regs {};
    unsigned int m_trace_count = 0;

    static bool is_supported_length(unsigned int len)
    {
        return len == 1 || len == 2 || len == 4 || len == 8;
    }

    uint64_t window_size() const
    {
        const uint64_t configured = p_window_size.get_value();
        return configured < WINDOW_BYTES ? configured : WINDOW_BYTES;
    }

    bool is_valid_access(uint64_t offset, unsigned int len,
                         const uint8_t* data) const
    {
        return data != nullptr && is_supported_length(len) &&
               offset <= window_size() && len <= window_size() - offset;
    }

    void trace_access(tlm::tlm_generic_payload& trans, uint64_t offset,
                      unsigned int len, bool debug)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value()) {
            return;
        }

        uint64_t value = 0;
        if (len <= sizeof(value)) {
            std::memcpy(&value, trans.get_data_ptr(), len);
        }

        ++m_trace_count;
        std::cerr << name() << " "
                  << (debug ? "dbg_" : "")
                  << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" : "write")
                  << " offset=0x" << std::hex << offset
                  << " len=0x" << len
                  << " value=0x" << value
                  << std::dec << std::endl;
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (!is_valid_access(offset, len, data)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_regs[offset], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&m_regs[offset], data, len);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access(trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

public:
    cci::cci_param<uint64_t> p_window_size;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;

    target_socket_t target_socket;

    explicit gic720ae_messreg(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_window_size("window_size", WINDOW_BYTES)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 128)
        , target_socket("target_socket")
    {
        m_regs.fill(0);
        target_socket.register_b_transport(this, &gic720ae_messreg::b_transport);
        target_socket.register_transport_dbg(this, &gic720ae_messreg::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        access(trans, false);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, true) ? trans.get_data_length() : 0;
    }
};

extern "C" void module_register();
