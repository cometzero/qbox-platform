/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class zena_device_fmu : public sc_core::sc_module
{
    static constexpr unsigned int BLOCK_TYPES = 8;
    static constexpr unsigned int SAFETY_MECHANISMS = 256;
    static constexpr unsigned int RECORDS = BLOCK_TYPES * 2;
    static constexpr uint32_t ERRSTATUS_BASE = 0x010;
    static constexpr uint32_t ERRSTATUS_STRIDE = 0x40;
    static constexpr uint32_t ERRGSR = 0xe00;
    static constexpr uint32_t SMEN = 0xf00;
    static constexpr uint32_t SMERR = 0xf04;
    static constexpr uint32_t SMCR = 0xf08;
    static constexpr uint32_t STATUS = 0xf1c;
    static constexpr uint32_t KEY = 0xf20;
    static constexpr uint32_t STATUS_V = 1u << 30;

    std::array<std::array<bool, SAFETY_MECHANISMS>, BLOCK_TYPES> m_enabled {};
    std::array<std::array<bool, SAFETY_MECHANISMS>, BLOCK_TYPES> m_critical {};
    std::array<uint64_t, RECORDS> m_status {};

    static unsigned int block_type(uint32_t value)
    {
        return (value >> 28) & 0x7u;
    }

    static unsigned int safety_mechanism(uint32_t value)
    {
        return (value >> 8) & 0xffu;
    }

    uint32_t parent_access(uint64_t address, tlm::tlm_command command,
                           uint32_t value = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        fault_socket->b_transport(trans, delay);
        return value;
    }

    void notify_parent(bool critical)
    {
        const uint64_t bank =
            static_cast<uint64_t>(p_parent_bank.get_value()) * 0x10000;
        const unsigned int record = critical ?
            p_parent_critical_record.get_value() :
            p_parent_non_critical_record.get_value();
        const uint64_t impdef = p_parent_base.get_value() + bank + 0x8000 +
            static_cast<uint64_t>(record) * 8;
        uint32_t value = parent_access(impdef, tlm::TLM_READ_COMMAND);
        value |= 1u << 9;
        if (critical) {
            value |= 1u << 15;
        }
        parent_access(p_parent_base.get_value() + bank + 0x8bfc,
                      tlm::TLM_WRITE_COMMAND, 0xbe);
        parent_access(impdef, tlm::TLM_WRITE_COMMAND, value);
    }

    uint64_t read_register(uint64_t offset) const
    {
        if (offset >= ERRSTATUS_BASE &&
            offset < ERRSTATUS_BASE + RECORDS * ERRSTATUS_STRIDE &&
            ((offset - ERRSTATUS_BASE) % ERRSTATUS_STRIDE) == 0) {
            return m_status[(offset - ERRSTATUS_BASE) / ERRSTATUS_STRIDE];
        }
        if (offset == ERRGSR) {
            uint64_t value = 0;
            for (unsigned int index = 0; index < RECORDS; ++index) {
                if ((m_status[index] & STATUS_V) != 0) {
                    value |= 1ull << index;
                }
            }
            return value;
        }
        return 0;
    }

    void write_register(uint64_t offset, uint64_t value)
    {
        if (offset >= ERRSTATUS_BASE &&
            offset < ERRSTATUS_BASE + RECORDS * ERRSTATUS_STRIDE &&
            ((offset - ERRSTATUS_BASE) % ERRSTATUS_STRIDE) == 0) {
            const unsigned int record =
                (offset - ERRSTATUS_BASE) / ERRSTATUS_STRIDE;
            if ((value & STATUS_V) == 0) {
                m_status[record] = 0;
            }
            return;
        }

        const uint32_t word = static_cast<uint32_t>(value);
        const unsigned int block = block_type(word);
        const unsigned int mechanism = safety_mechanism(word);
        if (offset == SMEN) {
            m_enabled[block][mechanism] = (word & 1u) != 0;
        } else if (offset == SMCR) {
            m_critical[block][mechanism] = (word & 1u) != 0;
        } else if (offset == SMERR && m_enabled[block][mechanism]) {
            const bool critical = m_critical[block][mechanism];
            const unsigned int record = block * 2 + (critical ? 0 : 1);
            m_status[record] = STATUS_V |
                (static_cast<uint64_t>(block) << 32) |
                (static_cast<uint64_t>(mechanism) << 8);
            notify_parent(critical);
        }
    }

    bool access(tlm::tlm_generic_payload& trans)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int length = trans.get_data_length();
        unsigned char* const data = trans.get_data_ptr();
        if (data == nullptr || (length != 4 && length != 8) ||
            offset + length > 0x10000) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            const uint64_t value = read_register(offset);
            std::memcpy(data, &value, length);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            uint64_t value = 0;
            std::memcpy(&value, data, length);
            if (offset != KEY && offset != STATUS) {
                write_register(offset, value);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

public:
    cci::cci_param<uint64_t> p_parent_base;
    cci::cci_param<unsigned int> p_parent_bank;
    cci::cci_param<unsigned int> p_parent_critical_record;
    cci::cci_param<unsigned int> p_parent_non_critical_record;
    tlm_utils::simple_target_socket<zena_device_fmu, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    tlm_utils::simple_initiator_socket<zena_device_fmu,
                                       DEFAULT_TLM_BUSWIDTH>
        fault_socket;

    explicit zena_device_fmu(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_parent_base("parent_base", 0x2a510000)
        , p_parent_bank("parent_bank", 4)
        , p_parent_critical_record("parent_critical_record", 0)
        , p_parent_non_critical_record("parent_non_critical_record", 1)
        , target_socket("target_socket")
        , fault_socket("fault_socket")
    {
        target_socket.register_b_transport(this, &zena_device_fmu::b_transport);
        target_socket.register_transport_dbg(this, &zena_device_fmu::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        trans.set_dmi_allowed(false);
        access(trans);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans) ? trans.get_data_length() : 0;
    }
};

extern "C" void module_register();
