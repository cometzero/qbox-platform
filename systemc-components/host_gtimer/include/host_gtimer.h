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

class host_gtimer : public sc_core::sc_module
{
    static constexpr uint64_t FRAME_BYTES = 0x10000;
    static constexpr uint64_t REG_BYTES = 0x1000;
    static constexpr uint32_t PCTL = 0x000;
    static constexpr uint32_t PCTH = 0x004;
    static constexpr uint32_t FRQ = 0x010;
    static constexpr uint32_t CNTFID0 = 0x020;
    static constexpr uint32_t SYNC_RW_LAST = 0x040;
    static constexpr uint32_t PIDR4 = 0xfd0;
    static constexpr uint32_t PIDR0 = 0xfe0;
    static constexpr uint32_t PIDR1 = 0xfe4;
    static constexpr uint32_t PIDR2 = 0xfe8;
    static constexpr uint32_t PIDR3 = 0xfec;
    static constexpr uint32_t CIDR0 = 0xff0;
    static constexpr uint32_t CIDR1 = 0xff4;
    static constexpr uint32_t CIDR2 = 0xff8;
    static constexpr uint32_t CIDR3 = 0xffc;

    std::array<uint8_t, REG_BYTES> m_regs{};
    uint64_t m_counter = 0;
    unsigned int m_trace_count = 0;

    static bool is_supported_length(unsigned int len)
    {
        return len == 1 || len == 2 || len == 4 || len == 8;
    }

    static bool is_aligned32_access(uint64_t offset, unsigned int len)
    {
        return len == sizeof(uint32_t) && (offset % sizeof(uint32_t)) == 0;
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

    static bool is_sync_id_offset(uint32_t offset)
    {
        switch (offset) {
        case PIDR4:
        case PIDR0:
        case PIDR1:
        case PIDR2:
        case PIDR3:
        case CIDR0:
        case CIDR1:
        case CIDR2:
        case CIDR3:
            return true;
        default:
            return false;
        }
    }

    static bool is_sync_rw_offset(uint32_t offset)
    {
        return offset <= SYNC_RW_LAST && (offset % sizeof(uint32_t)) == 0;
    }

    static uint32_t sync_id_value(uint32_t offset)
    {
        /*
         * Zena CSS programmer model Table 9-466 documents these
         * System_Generic_Timer_Synchronization PID/COMP_ID reset values.
         * They follow the Arm memory-mapped component identification
         * convention used by CoreSight-style peripheral ID registers.
         */
        switch (offset) {
        case PIDR4:
            return 0x00000004;
        case PIDR0:
            return 0x000000e8;
        case PIDR1:
            return 0x000000b0;
        case PIDR2:
            return 0x0000001b;
        case PIDR3:
            return 0x00000000;
        case CIDR0:
            return 0x0000000d;
        case CIDR1:
            return 0x000000f0;
        case CIDR2:
            return 0x00000005;
        case CIDR3:
            return 0x000000b1;
        default:
            return 0;
        }
    }

    uint64_t next_counter_value()
    {
        m_counter += p_counter_increment.get_value();
        return m_counter;
    }

    uint32_t read32(uint32_t offset)
    {
        if (p_sync_frame.get_value() && is_sync_id_offset(offset)) {
            return sync_id_value(offset);
        }

        if (p_counter_read.get_value()) {
            if (offset == PCTL) {
                return static_cast<uint32_t>(next_counter_value());
            }
            if (offset == PCTH) {
                return static_cast<uint32_t>(m_counter >> 32);
            }
        }

        if (p_counter_base.get_value()) {
            if (offset == PCTL) {
                return static_cast<uint32_t>(next_counter_value());
            }
            if (offset == PCTH) {
                return static_cast<uint32_t>(m_counter >> 32);
            }
            if (offset == FRQ) {
                return p_frequency.get_value();
            }
        }

        return load32(offset);
    }

    void reset_registers()
    {
        m_regs.fill(0);
        m_counter = 0;
        if (!p_sync_frame.get_value()) {
            store32(FRQ, p_frequency.get_value());
        }
        if (p_counter_control.get_value()) {
            store32(CNTFID0, p_frequency.get_value());
        }
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || !is_supported_length(len) ||
            len > FRAME_BYTES || offset > FRAME_BYTES - len) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (offset >= REG_BYTES) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::memset(data, 0, len);
            } else if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
                trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                return false;
            }

            trace_access(trans, offset, len, debug);
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }

        if (offset + len > REG_BYTES) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (p_sync_frame.get_value()) {
            if (!is_aligned32_access(offset, len)) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return false;
            }

            const auto offset32 = static_cast<uint32_t>(offset);
            if (!is_sync_rw_offset(offset32) && !is_sync_id_offset(offset32)) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return false;
            }
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            if (len == sizeof(uint32_t) && (offset % sizeof(uint32_t)) == 0) {
                const auto value = read32(static_cast<uint32_t>(offset));
                std::memcpy(data, &value, sizeof(value));
            } else {
                std::memcpy(data, &m_regs[offset], len);
            }
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            if (!(p_sync_frame.get_value() &&
                  is_sync_id_offset(static_cast<uint32_t>(offset)))) {
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

    void trace_access(tlm::tlm_generic_payload& trans, uint64_t offset,
                      unsigned int len, bool debug)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value())
            return;

        ++m_trace_count;
        uint32_t value = 0;
        if (len <= sizeof(value))
            std::memcpy(&value, trans.get_data_ptr(), len);

        std::cerr << name() << " "
                  << (debug ? "dbg_" : "")
                  << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" :
                                                                      "write")
                  << " offset=0x" << std::hex << offset
                  << " len=0x" << len
                  << " value=0x" << value
                  << std::dec << std::endl;
    }

public:
    cci::cci_param<bool> p_counter_base;
    cci::cci_param<bool> p_counter_control;
    cci::cci_param<bool> p_counter_read;
    cci::cci_param<bool> p_sync_frame;
    cci::cci_param<uint32_t> p_frequency;
    cci::cci_param<uint64_t> p_counter_increment;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    tlm_utils::simple_target_socket<host_gtimer, DEFAULT_TLM_BUSWIDTH>
        target_socket;

    explicit host_gtimer(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_counter_base("counter_base", false)
        , p_counter_control("counter_control", false)
        , p_counter_read("counter_read", false)
        , p_sync_frame("sync_frame", false)
        , p_frequency("frequency", 125000000)
        , p_counter_increment("counter_increment", 4096)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 64)
        , target_socket("target_socket")
    {
        reset_registers();
        target_socket.register_b_transport(this, &host_gtimer::b_transport);
        target_socket.register_transport_dbg(this, &host_gtimer::transport_dbg);
    }

    void before_end_of_elaboration() override
    {
        reset_registers();
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        trans.set_dmi_allowed(false);
        access(trans, false);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, true) ? trans.get_data_length() : 0;
    }
};

extern "C" void module_register();
