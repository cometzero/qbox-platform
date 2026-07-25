/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>

#include <cci_configuration>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

namespace gs {
class arm_system_counter;
}

class host_gtimer : public sc_core::sc_module
{
    static constexpr uint64_t FRAME_BYTES = 0x10000;
    static constexpr uint64_t REG_BYTES = 0x1000;
    static constexpr uint32_t PCTL = 0x000;
    static constexpr uint32_t PCTH = 0x004;
    static constexpr uint32_t CNTCR = 0x000;
    static constexpr uint32_t CNTSR = 0x004;
    static constexpr uint32_t CNTCV_L = 0x008;
    static constexpr uint32_t CNTCV_H = 0x00c;
    static constexpr uint32_t FRQ = 0x010;
    static constexpr uint32_t CNTSCR = 0x010;
    static constexpr uint32_t CNTFID0 = 0x020;
    static constexpr uint32_t CNTINCR = 0x0d0;
    static constexpr uint32_t IMPDEF_FIRST = 0x0c0;
    static constexpr uint32_t IMPDEF_LAST = 0x0fc;
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

    gs::arm_system_counter& m_counter;
    std::array<uint8_t, REG_BYTES> m_regs{};
    unsigned int m_trace_count = 0;

    uint32_t load32(uint32_t offset) const;
    void store32(uint32_t offset, uint32_t value);
    static bool is_sync_id_offset(uint32_t offset);
    static bool is_sync_rw_offset(uint32_t offset);
    bool is_wide_impl_defined_write(
        const tlm::tlm_generic_payload& trans) const;
    static uint32_t sync_id_value(uint32_t offset);
    uint32_t read32(uint32_t offset,
                    const sc_core::sc_time& effective_time) const;
    void write32(uint32_t offset, uint32_t value,
                 const sc_core::sc_time& effective_time);
    void reset_registers();
    bool valid_access_shape(tlm::tlm_generic_payload& trans) const;
    bool access(tlm::tlm_generic_payload& trans, bool debug,
                const sc_core::sc_time& delay);
    void trace_access(tlm::tlm_generic_payload& trans, uint64_t offset,
                      unsigned int len, bool debug);

public:
    struct FrontendSnapshot {
        uint64_t counter = 0;
        uint64_t input_frequency_hz = 0;
        uint64_t reported_frequency_hz = 0;
        uint64_t increment_8_24 = 0;
        uint64_t generation = 0;
        bool observed = false;
    };

    cci::cci_param<bool> p_counter_base;
    cci::cci_param<bool> p_counter_control;
    cci::cci_param<bool> p_counter_read;
    cci::cci_param<bool> p_sync_frame;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    tlm_utils::simple_target_socket<host_gtimer, DEFAULT_TLM_BUSWIDTH>
        target_socket;

    host_gtimer(sc_core::sc_module_name name,
                gs::arm_system_counter& counter);
    host_gtimer(sc_core::sc_module_name name, sc_core::sc_object* counter);

    void before_end_of_elaboration() override;
    FrontendSnapshot snapshot_at(
        const sc_core::sc_time& effective_time) const;
    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                            tlm::tlm_dmi& dmi);
};

extern "C" void module_register();
