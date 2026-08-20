/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <runonsysc.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class apollo_sbist : public sc_core::sc_module
{
    static constexpr uint64_t WINDOW_BYTES = 0x100000;
    static constexpr uint64_t CORE_STRIDE = 0x10000;
    static constexpr unsigned int CORES_PER_CLUSTER = 4;
    static constexpr uint64_t FCTLR_OFFSET = 0;
    static constexpr uint64_t FCTLR_FAIL = 1u << 2;
    static constexpr uint64_t FMU_BANK1_OFFSET = 0x10000;
    static constexpr uint64_t FMU_SYS_KEY_OFFSET = 0x8bfc;
    static constexpr uint32_t FMU_SYS_KEY_VALUE = 0xbe;
    static constexpr uint64_t FMU_IMPDEF_BASE = 0x8000;
    static constexpr uint64_t FMU_IMPDEF_STRIDE = 0x8;
    static constexpr uint32_t FMU_IMPDEF_INJECT = 1u << 9;
    static constexpr unsigned int SBIST_EQ_FAIL_RECORD0 = 210;

    std::array<uint8_t, WINDOW_BYTES> m_regs {};
    gs::runonsysc m_inject_run_on_sysc;

    static bool supported_length(unsigned int length)
    {
        return length == 1 || length == 2 || length == 4 || length == 8;
    }

    bool valid_access(uint64_t offset, unsigned int length,
                      const uint8_t* data) const
    {
        return data != nullptr && supported_length(length) &&
               offset <= m_regs.size() && length <= m_regs.size() - offset;
    }

    void write_fmu32(uint64_t address, uint32_t value)
    {
        if (fmu_initiator.size() == 0)
            return;

        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_dmi_allowed(false);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        fmu_initiator->b_transport(trans, delay);
    }

    void inject_fault(unsigned int cpu)
    {
        const uint64_t bank = p_fmu_base.get_value() + FMU_BANK1_OFFSET;
        const uint64_t record = SBIST_EQ_FAIL_RECORD0 + cpu;
        write_fmu32(bank + FMU_SYS_KEY_OFFSET, FMU_SYS_KEY_VALUE);
        write_fmu32(bank + FMU_IMPDEF_BASE + record * FMU_IMPDEF_STRIDE,
                    FMU_IMPDEF_INJECT);
    }

    void schedule_fault(unsigned int local_cpu)
    {
        const unsigned int cpu = p_cpu_base.get_value() + local_cpu;
        m_inject_run_on_sysc.run_on_sysc([this, cpu] { inject_fault(cpu); });
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int length = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (!valid_access(offset, length, data)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_regs[offset], length);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&m_regs[offset], data, length);
            const uint64_t local_cpu = offset / CORE_STRIDE;
            const uint64_t register_offset = offset % CORE_STRIDE;
            uint64_t value = 0;
            std::memcpy(&value, data, length);
            if (!debug && local_cpu < CORES_PER_CLUSTER &&
                register_offset == FCTLR_OFFSET && (value & FCTLR_FAIL) != 0)
                schedule_fault(static_cast<unsigned int>(local_cpu));
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

public:
    cci::cci_param<unsigned int> p_cpu_base;
    cci::cci_param<uint64_t> p_fmu_base;
    tlm_utils::simple_target_socket_b<
        apollo_sbist, DEFAULT_TLM_BUSWIDTH, tlm::tlm_base_protocol_types,
        sc_core::SC_ZERO_OR_MORE_BOUND>
        target_socket;
    tlm_utils::simple_initiator_socket_b<
        apollo_sbist, DEFAULT_TLM_BUSWIDTH, tlm::tlm_base_protocol_types,
        sc_core::SC_ZERO_OR_MORE_BOUND>
        fmu_initiator;

    explicit apollo_sbist(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , m_inject_run_on_sysc("inject_run_on_sysc")
        , p_cpu_base("cpu_base", 0)
        , p_fmu_base("fmu_base", 0x2a510000)
        , target_socket("target_socket")
        , fmu_initiator("fmu_initiator")
    {
        m_regs.fill(0);
        target_socket.register_b_transport(this, &apollo_sbist::b_transport);
        target_socket.register_transport_dbg(this, &apollo_sbist::transport_dbg);
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
