/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <algorithm>
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

class zena_ni710ae_fmu : public sc_core::sc_module
{
    static constexpr unsigned int MAX_RECORDS = 64;
    static constexpr uint32_t ERR_STATUS_BASE = 0x010;
    static constexpr uint32_t ERR_MISC_BASE = 0x020;
    static constexpr uint32_t ERR_STRIDE = 0x40;
    static constexpr uint32_t ERRGSR_LOW_BASE = 0xe000;
    static constexpr uint32_t SYS_KEY = 0xe200;
    static constexpr uint32_t SMEN = 0xe204;
    static constexpr uint32_t INJECT = 0xe208;
    static constexpr uint32_t SMINFO = 0xe210;
    static constexpr uint32_t ERRDEVID = 0xffc8;
    static constexpr uint32_t STATUS_V = 1u << 30;
    static constexpr uint32_t STATUS_W1C = STATUS_V | (1u << 29) |
        (1u << 27) | (1u << 26) | (3u << 24) | (3u << 20);

    std::array<uint32_t, MAX_RECORDS> m_status {};
    uint32_t m_enabled = 0;
    uint64_t m_sminfo = 0;

    unsigned int record_count() const
    {
        return std::min<unsigned int>(p_node_index.get_value() + 1,
                                      MAX_RECORDS);
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

    void inject(unsigned int mechanism)
    {
        if (mechanism >= 18 || (m_enabled & (1u << mechanism)) == 0) {
            return;
        }
        const unsigned int record = p_node_index.get_value();
        if (record >= MAX_RECORDS) {
            return;
        }
        const bool critical = mechanism != 14;
        m_status[record] = STATUS_V | (mechanism << 8);
        notify_parent(critical);
    }

    uint32_t read32(uint64_t offset) const
    {
        if (offset >= ERR_STATUS_BASE &&
            ((offset - ERR_STATUS_BASE) % ERR_STRIDE) == 0) {
            const unsigned int record =
                (offset - ERR_STATUS_BASE) / ERR_STRIDE;
            return record < MAX_RECORDS ? m_status[record] : 0;
        }
        if (offset >= ERR_MISC_BASE &&
            ((offset - ERR_MISC_BASE) % ERR_STRIDE) == 0) {
            const unsigned int record =
                (offset - ERR_MISC_BASE) / ERR_STRIDE;
            return record == p_node_index.get_value() ? 0x61u : 0;
        }
        if (offset >= ERRGSR_LOW_BASE && offset < ERRGSR_LOW_BASE + 0x30) {
            const unsigned int group = (offset - ERRGSR_LOW_BASE) / 8;
            const bool high = ((offset - ERRGSR_LOW_BASE) % 8) == 4;
            uint32_t value = 0;
            const unsigned int first = group * 64 + (high ? 32 : 0);
            for (unsigned int bit = 0; bit < 32; ++bit) {
                const unsigned int record = first + bit;
                if (record < MAX_RECORDS &&
                    (m_status[record] & STATUS_V) != 0) {
                    value |= 1u << bit;
                }
            }
            return value;
        }
        if (offset == SMEN) {
            return m_enabled;
        }
        if (offset == SMINFO) {
            return static_cast<uint32_t>(m_sminfo);
        }
        if (offset == SMINFO + 4) {
            return static_cast<uint32_t>(m_sminfo >> 32);
        }
        if (offset == ERRDEVID) {
            return record_count();
        }
        return 0;
    }

    void write32(uint64_t offset, uint32_t value)
    {
        if (offset >= ERR_STATUS_BASE &&
            ((offset - ERR_STATUS_BASE) % ERR_STRIDE) == 0) {
            const unsigned int record =
                (offset - ERR_STATUS_BASE) / ERR_STRIDE;
            if (record < MAX_RECORDS) {
                m_status[record] &= ~(value & STATUS_W1C);
            }
        } else if (offset == SMEN) {
            m_enabled = value & 0x3ffffu;
        } else if (offset == SMINFO) {
            m_sminfo = (m_sminfo & 0xffffffff00000000ull) | value;
        } else if (offset == SMINFO + 4) {
            m_sminfo = (m_sminfo & 0xffffffffull) |
                (static_cast<uint64_t>(value) << 32);
        } else if (offset == INJECT) {
            inject(value & 0xffu);
        }
    }

    bool access(tlm::tlm_generic_payload& trans)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int length = trans.get_data_length();
        unsigned char* const data = trans.get_data_ptr();
        if (data == nullptr || length != 4 || offset + length > 0x10000) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            const uint32_t value = read32(offset);
            std::memcpy(data, &value, sizeof(value));
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            uint32_t value = 0;
            std::memcpy(&value, data, sizeof(value));
            if (offset != SYS_KEY) {
                write32(offset, value);
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
    cci::cci_param<unsigned int> p_node_index;
    cci::cci_param<unsigned int> p_parent_bank;
    cci::cci_param<unsigned int> p_parent_critical_record;
    cci::cci_param<unsigned int> p_parent_non_critical_record;
    tlm_utils::simple_target_socket<zena_ni710ae_fmu,
                                    DEFAULT_TLM_BUSWIDTH>
        target_socket;
    tlm_utils::simple_initiator_socket<zena_ni710ae_fmu,
                                       DEFAULT_TLM_BUSWIDTH>
        fault_socket;

    explicit zena_ni710ae_fmu(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_parent_base("parent_base", 0x2a510000)
        , p_node_index("node_index", 0)
        , p_parent_bank("parent_bank", 1)
        , p_parent_critical_record("parent_critical_record", 0)
        , p_parent_non_critical_record("parent_non_critical_record", 1)
        , target_socket("target_socket")
        , fault_socket("fault_socket")
    {
        target_socket.register_b_transport(this, &zena_ni710ae_fmu::b_transport);
        target_socket.register_transport_dbg(this, &zena_ni710ae_fmu::transport_dbg);
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
