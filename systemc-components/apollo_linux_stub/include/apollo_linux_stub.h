/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <array>
#include <cstdint>
#include <mutex>

#include <module_factory_registery.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

/*
 * Linux-only SMCCC service, not a secure firmware implementation.
 * 0x00..0x18: x0..x3; 0x20: execute; 0x28: signed x0 response;
 * 0x30: completed call count; 0x38: last function ID. Each of 16 CPUs owns
 * a separate 0x40-byte bank, indexed by QEMU cpu_index.
 * PSCI is owned by QEMU. All other services return NOT_SUPPORTED except
 * SMCCC_VERSION, which advertises the 1.1 calling convention.
 * Debug writes never execute a service. No DMI is provided.
 */
class apollo_linux_stub : public sc_core::sc_module
{
    struct Bank {
        std::array<uint64_t, 4> args {};
        uint64_t result = UINT64_MAX;
        uint64_t calls = 0;
        uint64_t last_function = 0;
    };
    std::array<Bank, 16> m_banks {};
    std::mutex m_mutex;

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const uint64_t address = trans.get_address();
        const uint64_t offset = address % 0x40;
        const unsigned int size = trans.get_data_length();
        auto* data = trans.get_data_ptr();
        trans.set_dmi_allowed(false);
        if (trans.get_byte_enable_ptr()) {
            trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return false;
        }
        if (size != 8 || trans.get_streaming_width() < size) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return false;
        }
        if (!data || address >= 0x400 || (offset & 7)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        auto& bank = m_banks[address / 0x40];
        uint64_t value = 0;
        if (trans.is_write()) {
            for (unsigned int i = 0; i < size; ++i)
                value |= uint64_t(data[i]) << (i * 8);
            if (offset < 0x20) {
                bank.args[offset / 8] = value;
            } else if (offset == 0x20 && !debug && value) {
                bank.last_function = bank.args[0];
                ++bank.calls;
                bank.result = bank.args[0] == 0x80000000 ? 0x10001 : UINT64_MAX;
            }
        } else if (trans.is_read()) {
            if (offset < 0x20)
                value = bank.args[offset / 8];
            else if (offset == 0x28)
                value = bank.result;
            else if (offset == 0x30)
                value = bank.calls;
            else if (offset == 0x38)
                value = bank.last_function;
            for (unsigned int i = 0; i < size; ++i)
                data[i] = static_cast<uint8_t>(value >> (i * 8));
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

public:
    tlm_utils::simple_target_socket_b<apollo_linux_stub, DEFAULT_TLM_BUSWIDTH,
        tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND> target_socket;

    explicit apollo_linux_stub(sc_core::sc_module_name name)
        : sc_core::sc_module(name), target_socket("target_socket")
    {
        target_socket.register_b_transport(this, &apollo_linux_stub::b_transport);
        target_socket.register_transport_dbg(this, &apollo_linux_stub::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay; // Synchronous service; no simulated firmware latency.
        access(trans, false);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans, true) ? trans.get_data_length() : 0;
    }
};

extern "C" void module_register();
