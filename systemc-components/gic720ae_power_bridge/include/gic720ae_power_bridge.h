/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class gic720ae_power_bridge : public sc_core::sc_module
{
    static constexpr uint64_t GICR_FRAME_BYTES = 0x20000;
    static constexpr uint64_t GICR_WAKER = 0x0014;
    static constexpr uint64_t GICR_PWRR = 0x0024;
    static constexpr uint32_t WAKER_SLEEP = 1u << 0;
    static constexpr uint32_t WAKER_PROCESSOR_SLEEP = 1u << 1;
    static constexpr uint32_t WAKER_CHILDREN_ASLEEP = 1u << 2;
    static constexpr uint32_t WAKER_QUIESCENT = 1u << 31;
    static constexpr uint32_t PWRR_RDPD = 1u << 0;
    static constexpr unsigned int SIGNAL_KINDS = 4;

    using target_socket_t =
        tlm_utils::simple_target_socket_b<
            gic720ae_power_bridge, DEFAULT_TLM_BUSWIDTH,
            tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND>;
    using initiator_socket_t =
        tlm_utils::simple_initiator_socket_b<
            gic720ae_power_bridge, DEFAULT_TLM_BUSWIDTH,
            tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND>;

    struct RedistributorState {
        bool powered_down = true;
        bool processor_sleep = true;
        bool children_asleep = true;
        std::array<bool, SIGNAL_KINDS> input_level {{false, false, false, false}};
    };

    std::vector<RedistributorState> m_state;
    std::vector<bool> m_backend_sync_pending;
    sc_core::sc_event m_backend_sync_event;
    bool m_sleep = false;
    bool m_quiescent = false;

    bool decode_redistributor(
        uint64_t address, unsigned int& index, uint64_t& offset) const;
    bool validate_register_access(tlm::tlm_generic_payload& trans) const;
    bool forward(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay,
        bool debug);
    bool access_waker(
        unsigned int index, tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay, bool debug);
    bool access_pwrr(
        unsigned int index, tlm::tlm_generic_payload& trans);
    bool access(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay,
        bool debug);
    uint32_t waker_value(unsigned int index) const;
    bool all_children_asleep(
        unsigned int selected, const RedistributorState& selected_state) const;
    bool sync_backend_waker(unsigned int index);
    bool delivery_enabled(unsigned int index) const;
    void sync_pending_backend_wakers();
    void receive(unsigned int index, unsigned int kind, bool value);
    void drive(unsigned int index, unsigned int kind, bool value);
    void refresh(unsigned int index);
    void reset_model();

public:
    SC_HAS_PROCESS(gic720ae_power_bridge);

    cci::cci_param<uint64_t> p_backend_redist_base;
    cci::cci_param<uint64_t> p_backend_redist_stride;
    cci::cci_param<unsigned int> p_redistributor_count;

    target_socket_t target_socket;
    initiator_socket_t backend_socket;
    sc_core::sc_vector<TargetSignalSocket<bool>> irq_in;
    sc_core::sc_vector<TargetSignalSocket<bool>> fiq_in;
    sc_core::sc_vector<TargetSignalSocket<bool>> virq_in;
    sc_core::sc_vector<TargetSignalSocket<bool>> vfiq_in;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> irq_out;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> fiq_out;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> virq_out;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> vfiq_out;
    TargetSignalSocket<bool> reset;

    explicit gic720ae_power_bridge(sc_core::sc_module_name name);

    void b_transport(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
};

extern "C" void module_register();
