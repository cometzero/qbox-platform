/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include <async_event.h>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class apollo_cpu_ras : public sc_core::sc_module
{
    static constexpr unsigned int CORE_COUNT = 4;
    static constexpr unsigned int REGISTER_COUNT = 8;
    static constexpr uint64_t RECORD_SIZE = REGISTER_COUNT * sizeof(uint64_t);
    static constexpr unsigned int STATUS_INDEX = 2;
    static constexpr uint64_t STATUS_V = 1ull << 30;
    static constexpr uint64_t STATUS_UE = 1ull << 29;

    using target_socket_t = tlm_utils::simple_target_socket_tagged_b<
        apollo_cpu_ras, DEFAULT_TLM_BUSWIDTH, tlm::tlm_base_protocol_types,
        sc_core::SC_ZERO_OR_MORE_BOUND>;

    std::array<std::array<uint64_t, REGISTER_COUNT>, CORE_COUNT> m_records {};
    std::atomic<uint32_t> m_pending_faults {0};
    gs::async_event m_fault_event;

    void update_cluster_irq()
    {
        bool asserted = false;
        for (const auto& record : m_records) {
            asserted |= (record[STATUS_INDEX] & STATUS_V) != 0;
        }
        if (cluster_irq.size() != 0) {
            cluster_irq->write(asserted);
        }
    }

    void inject_fault(unsigned int core)
    {
        m_records[core][STATUS_INDEX] = STATUS_V | STATUS_UE;
        update_cluster_irq();
    }

    void fault_changed(unsigned int core, bool value)
    {
        if (value) {
            m_pending_faults.fetch_or(1u << core, std::memory_order_relaxed);
            m_fault_event.notify(sc_core::SC_ZERO_TIME);
        }
    }

    void inject_pending_faults()
    {
        const uint32_t pending =
            m_pending_faults.exchange(0, std::memory_order_relaxed);
        for (unsigned int core = 0; core < CORE_COUNT; ++core) {
            if ((pending & (1u << core)) != 0) {
                inject_fault(core);
            }
        }
    }

    bool access(unsigned int core, tlm::tlm_generic_payload& trans)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int length = trans.get_data_length();
        unsigned char* data = trans.get_data_ptr();
        if (data == nullptr || (length != 4 && length != 8) ||
            offset + length > RECORD_SIZE || (offset % length) != 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        const unsigned int index = offset / sizeof(uint64_t);
        const unsigned int byte_offset = offset % sizeof(uint64_t);
        uint64_t& value = m_records[core][index];
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data,
                        reinterpret_cast<unsigned char*>(&value) + byte_offset,
                        length);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            uint64_t written = 0;
            std::memcpy(&written, data, length);
            if (index == STATUS_INDEX) {
                value &= ~(written << (byte_offset * 8));
                update_cluster_irq();
            } else {
                std::memcpy(
                    reinterpret_cast<unsigned char*>(&value) + byte_offset,
                    data, length);
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    void b_transport(int core, tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        (void)delay;
        access(static_cast<unsigned int>(core), trans);
    }

    unsigned int transport_dbg(int core, tlm::tlm_generic_payload& trans)
    {
        return access(static_cast<unsigned int>(core), trans)
            ? trans.get_data_length()
            : 0;
    }

public:
    SC_HAS_PROCESS(apollo_cpu_ras);

    target_socket_t record_0;
    target_socket_t record_1;
    target_socket_t record_2;
    target_socket_t record_3;
    TargetSignalSocket<bool> cpu_fault_0;
    TargetSignalSocket<bool> cpu_fault_1;
    TargetSignalSocket<bool> cpu_fault_2;
    TargetSignalSocket<bool> cpu_fault_3;
    InitiatorSignalSocket<bool> cluster_irq;

    explicit apollo_cpu_ras(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , m_fault_event(false)
        , record_0("record_0")
        , record_1("record_1")
        , record_2("record_2")
        , record_3("record_3")
        , cpu_fault_0("cpu_fault_0")
        , cpu_fault_1("cpu_fault_1")
        , cpu_fault_2("cpu_fault_2")
        , cpu_fault_3("cpu_fault_3")
        , cluster_irq("cluster_irq")
    {
        target_socket_t* records[] = {
            &record_0, &record_1, &record_2, &record_3};
        for (unsigned int core = 0; core < CORE_COUNT; ++core) {
            records[core]->register_b_transport(
                this, &apollo_cpu_ras::b_transport, core);
            records[core]->register_transport_dbg(
                this, &apollo_cpu_ras::transport_dbg, core);
        }
        cpu_fault_0.register_value_changed_cb(
            [this](bool value) { fault_changed(0, value); });
        cpu_fault_1.register_value_changed_cb(
            [this](bool value) { fault_changed(1, value); });
        cpu_fault_2.register_value_changed_cb(
            [this](bool value) { fault_changed(2, value); });
        cpu_fault_3.register_value_changed_cb(
            [this](bool value) { fault_changed(3, value); });

        SC_METHOD(inject_pending_faults);
        sensitive << m_fault_event;
        dont_initialize();

    }
};

extern "C" void module_register();
