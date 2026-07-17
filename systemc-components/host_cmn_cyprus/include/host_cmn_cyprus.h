/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <unordered_map>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class host_cmn_cyprus : public sc_core::sc_module
{
    static constexpr uint64_t DEFAULT_WINDOW_BYTES = 0x40000000;
    static constexpr uint64_t CFGM_BASE = 0x00000;
    static constexpr uint64_t MXP_BASE = 0x10000;
    static constexpr uint64_t NODE_STRIDE = 0x10000;

    static constexpr uint64_t CFGM_PERIPH_ID1 = 0x010;
    static constexpr uint64_t CFGM_CHILD_INFO = 0x080;
    static constexpr uint64_t CFGM_CHILD_POINTER = 0x100;

    static constexpr uint64_t MXP_PORT_CONNECT_INFO = 0x008;
    static constexpr uint64_t MXP_CHILD_INFO = 0x080;
    static constexpr uint64_t MXP_CHILD_POINTER = 0x100;
    static constexpr uint64_t MXP_PORT_DISABLE = 0xa70;

    static constexpr uint64_t NODE_CHILD_INFO = 0x080;
    static constexpr uint64_t HNS_UNIT_INFO0 = 0x900;
    static constexpr uint64_t HNS_UNIT_INFO1 = 0x908;
    static constexpr uint64_t RNSAM_UNIT_INFO0 = 0x900;
    static constexpr uint64_t RNSAM_UNIT_INFO1 = 0x908;
    static constexpr uint64_t RNSAM_STATUS = 0x1100;

    static constexpr uint16_t NODE_TYPE_CFG = 0x002;
    static constexpr uint16_t NODE_TYPE_DVM = 0x001;
    static constexpr uint16_t NODE_TYPE_DTC = 0x003;
    static constexpr uint16_t NODE_TYPE_HN_I = 0x004;
    static constexpr uint16_t NODE_TYPE_XP = 0x006;
    static constexpr uint16_t NODE_TYPE_SBSX = 0x007;
    static constexpr uint16_t NODE_TYPE_RN_I = 0x00a;
    static constexpr uint16_t NODE_TYPE_RN_D = 0x00d;
    static constexpr uint16_t NODE_TYPE_RN_SAM = 0x00f;
    static constexpr uint16_t NODE_TYPE_HN_P = 0x011;
    static constexpr uint16_t NODE_TYPE_CCRA = 0x103;
    static constexpr uint16_t NODE_TYPE_CCHA = 0x104;
    static constexpr uint16_t NODE_TYPE_CCLA = 0x105;
    static constexpr uint16_t NODE_TYPE_HN_S = 0x200;
    static constexpr uint16_t NODE_TYPE_HN_S_MPAM_S = 0x201;
    static constexpr uint16_t NODE_TYPE_HN_S_MPAM_NS = 0x202;

    static constexpr uint8_t DEVICE_TYPE_RN_I = 0x01;
    static constexpr uint8_t DEVICE_TYPE_RN_D = 0x02;
    static constexpr uint8_t DEVICE_TYPE_HN_I = 0x09;
    static constexpr uint8_t DEVICE_TYPE_HN_P = 0x0b;
    static constexpr uint8_t DEVICE_TYPE_SBSX = 0x0d;
    static constexpr uint8_t DEVICE_TYPE_CCG = 0x1e;
    static constexpr uint8_t DEVICE_TYPE_HN_S = 0x1a;
    static constexpr uint8_t DEVICE_TYPE_RN_F_CHIF_ESAM = 0x21;

    static constexpr uint64_t RNSAM_NONHASH_RCOMP_EN = UINT64_C(1) << 31;
    static constexpr uint64_t RNSAM_HTG_RCOMP_EN = UINT64_C(1) << 27;
    static constexpr uint64_t RNSAM_RCOMP_LSB_DEFAULT = 20;

    struct node_spec {
        uint16_t type;
        uint16_t node_id;
        uint16_t logical_id;
    };

    struct xp_spec {
        uint16_t node_id;
        uint16_t logical_id;
        uint8_t port_count;
        std::array<uint8_t, 4> port_types;
        std::array<node_spec, 6> children;
        uint8_t child_count;
    };

    static const std::array<xp_spec, 24>& topology()
    {
        static const std::array<xp_spec, 24> value = {{
            { 344, 23, 4, {{ DEVICE_TYPE_RN_I, DEVICE_TYPE_HN_P,
                             DEVICE_TYPE_HN_P, DEVICE_TYPE_RN_D }},
              {{{ NODE_TYPE_RN_SAM, 350, 0 }, { NODE_TYPE_RN_D, 350, 2 },
                { NODE_TYPE_HN_P, 348, 6 }, { NODE_TYPE_HN_P, 346, 5 },
                { NODE_TYPE_RN_SAM, 344, 0 }, { NODE_TYPE_RN_I, 344, 7 }}},
              6 },
            { 336, 17, 3, {{ DEVICE_TYPE_RN_I, DEVICE_TYPE_HN_I,
                             DEVICE_TYPE_RN_D, 0 }},
              {{{ NODE_TYPE_RN_SAM, 340, 0 }, { NODE_TYPE_RN_D, 340, 1 },
                { NODE_TYPE_HN_I, 338, 1 }, { NODE_TYPE_RN_SAM, 336, 0 },
                { NODE_TYPE_RN_I, 336, 4 }, { 0, 0, 0 }}},
              5 },
            { 328, 11, 3, {{ DEVICE_TYPE_RN_I, DEVICE_TYPE_HN_I,
                             DEVICE_TYPE_RN_D, 0 }},
              {{{ NODE_TYPE_RN_SAM, 332, 0 }, { NODE_TYPE_RN_D, 332, 0 },
                { NODE_TYPE_HN_I, 330, 0 }, { NODE_TYPE_RN_SAM, 328, 0 },
                { NODE_TYPE_RN_I, 328, 2 }, { 0, 0, 0 }}},
              5 },
            { 320, 5, 2, {{ DEVICE_TYPE_RN_I, DEVICE_TYPE_HN_P, 0, 0 }},
              {{{ NODE_TYPE_HN_P, 324, 4 }, { NODE_TYPE_RN_SAM, 320, 0 },
                { NODE_TYPE_RN_I, 320, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }}},
              3 },
            { 280, 22, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 280, 8 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 272, 16, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                             0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 276, 7 },
                { NODE_TYPE_HN_S_MPAM_S, 276, 7 },
                { NODE_TYPE_HN_S, 276, 7 }, { NODE_TYPE_RN_SAM, 272, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 264, 10, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                             0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 268, 3 },
                { NODE_TYPE_HN_S_MPAM_S, 268, 3 },
                { NODE_TYPE_HN_S, 268, 3 }, { NODE_TYPE_RN_SAM, 264, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 256, 4, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 256, 4 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 216, 21, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 216, 7 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 208, 15, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                             0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 212, 6 },
                { NODE_TYPE_HN_S_MPAM_S, 212, 6 },
                { NODE_TYPE_HN_S, 212, 6 }, { NODE_TYPE_RN_SAM, 208, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 200, 9, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                            0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 204, 2 },
                { NODE_TYPE_HN_S_MPAM_S, 204, 2 },
                { NODE_TYPE_HN_S, 204, 2 }, { NODE_TYPE_RN_SAM, 200, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 192, 3, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 192, 3 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 152, 20, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 152, 6 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 144, 14, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                             0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 148, 5 },
                { NODE_TYPE_HN_S_MPAM_S, 148, 5 },
                { NODE_TYPE_HN_S, 148, 5 }, { NODE_TYPE_RN_SAM, 144, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 136, 8, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                            0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 140, 1 },
                { NODE_TYPE_HN_S_MPAM_S, 140, 1 },
                { NODE_TYPE_HN_S, 140, 1 }, { NODE_TYPE_RN_SAM, 136, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 128, 2, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 128, 2 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 88, 19, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 88, 5 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 80, 13, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                            0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 84, 4 },
                { NODE_TYPE_HN_S_MPAM_S, 84, 4 },
                { NODE_TYPE_HN_S, 84, 4 }, { NODE_TYPE_RN_SAM, 80, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 72, 7, 2, {{ DEVICE_TYPE_RN_F_CHIF_ESAM, DEVICE_TYPE_HN_S,
                           0, 0 }},
              {{{ NODE_TYPE_HN_S_MPAM_NS, 76, 0 },
                { NODE_TYPE_HN_S_MPAM_S, 76, 0 },
                { NODE_TYPE_HN_S, 76, 0 }, { NODE_TYPE_RN_SAM, 72, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
            { 64, 1, 1, {{ DEVICE_TYPE_SBSX, 0, 0, 0 }},
              {{{ NODE_TYPE_SBSX, 64, 1 }, { 0, 0, 0 }, { 0, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }}},
              1 },
            { 24, 18, 4, {{ DEVICE_TYPE_RN_I, DEVICE_TYPE_RN_I,
                            DEVICE_TYPE_HN_I, DEVICE_TYPE_HN_I }},
              {{{ NODE_TYPE_HN_I, 30, 3 }, { NODE_TYPE_HN_I, 28, 2 },
                { NODE_TYPE_RN_SAM, 26, 0 }, { NODE_TYPE_RN_I, 26, 6 },
                { NODE_TYPE_RN_SAM, 24, 0 }, { NODE_TYPE_RN_I, 24, 5 }}},
              6 },
            { 16, 12, 1, {{ DEVICE_TYPE_CCG, 0, 0, 0 }},
              {{{ NODE_TYPE_RN_SAM, 17, 0 }, { NODE_TYPE_RN_I, 17, 3 },
                { NODE_TYPE_CCRA, 16, 1 }, { NODE_TYPE_CCLA, 16, 1 },
                { NODE_TYPE_RN_SAM, 16, 0 }, { NODE_TYPE_CCHA, 16, 1 }}},
              6 },
            { 8, 6, 1, {{ DEVICE_TYPE_CCG, 0, 0, 0 }},
              {{{ NODE_TYPE_RN_SAM, 9, 0 }, { NODE_TYPE_RN_I, 9, 1 },
                { NODE_TYPE_CCRA, 8, 0 }, { NODE_TYPE_CCLA, 8, 0 },
                { NODE_TYPE_RN_SAM, 8, 0 }, { NODE_TYPE_CCHA, 8, 0 }}},
              6 },
            { 0, 0, 2, {{ DEVICE_TYPE_SBSX, DEVICE_TYPE_HN_I, 0, 0 }},
              {{{ NODE_TYPE_HN_I, 4, 7 }, { NODE_TYPE_DTC, 4, 0 },
                { NODE_TYPE_DVM, 4, 0 }, { NODE_TYPE_SBSX, 0, 0 },
                { 0, 0, 0 }, { 0, 0, 0 }}},
              4 },
        }};
        return value;
    }

    std::unordered_map<uint64_t, uint8_t> m_regs;
    unsigned int m_trace_count = 0;

    static bool is_supported_length(unsigned int len)
    {
        return len == 1 || len == 2 || len == 4 || len == 8;
    }

    static uint64_t node_info(uint16_t type, uint16_t node_id,
                              uint16_t logical_id)
    {
        return (static_cast<uint64_t>(logical_id) << 32) |
            (static_cast<uint64_t>(node_id) << 16) | type;
    }

    static uint64_t mxp_node_info(uint16_t node_id, uint16_t logical_id,
                                  uint8_t port_count)
    {
        return (static_cast<uint64_t>(port_count) << 48) |
            node_info(NODE_TYPE_XP, node_id, logical_id);
    }

    uint8_t load8(uint64_t offset) const
    {
        const auto it = m_regs.find(offset);
        return it == m_regs.end() ? 0 : it->second;
    }

    uint64_t load_le(uint64_t offset, unsigned int len) const
    {
        uint64_t value = 0;
        for (unsigned int i = 0; i < len; ++i)
            value |= static_cast<uint64_t>(load8(offset + i)) << (i * 8);
        return value;
    }

    void store_le(uint64_t offset, uint64_t value, unsigned int len)
    {
        for (unsigned int i = 0; i < len; ++i) {
            m_regs[offset + i] = static_cast<uint8_t>(value >> (i * 8));
        }
    }

    void seed64(uint64_t offset, uint64_t value)
    {
        store_le(offset, value, sizeof(value));
    }

    static uint64_t xp_offset(unsigned int index)
    {
        return MXP_BASE + (index * NODE_STRIDE);
    }

    static uint64_t child_offset(unsigned int index)
    {
        return MXP_BASE + ((topology().size() + index) * NODE_STRIDE);
    }

    void seed_child_node(uint64_t base, const node_spec& spec)
    {
        seed64(base, node_info(spec.type, spec.node_id, spec.logical_id));
        seed64(base + NODE_CHILD_INFO, 0);

        if (spec.type == NODE_TYPE_HN_S) {
            seed64(base + HNS_UNIT_INFO0, 0);
            seed64(base + HNS_UNIT_INFO1, 0);
        } else if (spec.type == NODE_TYPE_RN_SAM) {
            const uint64_t rcomp_lsb =
                static_cast<uint64_t>(p_rcomp_lsb.get_value());
            seed64(base + RNSAM_UNIT_INFO0,
                   RNSAM_NONHASH_RCOMP_EN | RNSAM_HTG_RCOMP_EN);
            seed64(base + RNSAM_UNIT_INFO1,
                   (rcomp_lsb << 5) | rcomp_lsb);
            seed64(base + RNSAM_STATUS, 0x2);
        }
    }

    void seed_registers()
    {
        m_regs.clear();
        m_trace_count = 0;

        seed64(CFGM_BASE, node_info(NODE_TYPE_CFG, 0, 0));
        seed64(CFGM_PERIPH_ID1,
               static_cast<uint64_t>(p_revision.get_value() & 0xf) << 4);
        seed64(CFGM_CHILD_INFO, topology().size());

        unsigned int child_index = 0;
        for (unsigned int xp_index = 0; xp_index < topology().size();
             ++xp_index) {
            const xp_spec& xp = topology()[xp_index];
            const uint64_t xp_base = xp_offset(xp_index);

            seed64(CFGM_CHILD_POINTER +
                       (xp_index * sizeof(uint64_t)),
                   xp_base);
            seed64(xp_base,
                   mxp_node_info(xp.node_id, xp.logical_id, xp.port_count));
            for (unsigned int port = 0; port < xp.port_types.size(); ++port) {
                seed64(xp_base + MXP_PORT_CONNECT_INFO +
                           (port * sizeof(uint64_t)),
                       xp.port_types[port]);
            }
            seed64(xp_base + MXP_CHILD_INFO, xp.child_count);
            seed64(xp_base + MXP_PORT_DISABLE, 0);

            for (unsigned int node_index = 0;
                 node_index < xp.child_count; ++node_index, ++child_index) {
                const uint64_t base = child_offset(child_index);
                seed64(xp_base + MXP_CHILD_POINTER +
                           (node_index * sizeof(uint64_t)),
                       base);
                seed_child_node(base, xp.children[node_index]);
            }
        }
    }

    bool is_in_window(uint64_t offset, unsigned int len) const
    {
        const uint64_t window = p_window_bytes.get_value();
        return offset < window && len <= (window - offset);
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || !is_supported_length(len) ||
            !is_in_window(offset, len)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            const uint64_t value = load_le(offset, len);
            std::memcpy(data, &value, len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            uint64_t value = 0;
            std::memcpy(&value, data, len);
            store_le(offset, value, len);
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
        uint64_t value = 0;
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
    cci::cci_param<uint32_t> p_revision;
    cci::cci_param<uint32_t> p_rcomp_lsb;
    cci::cci_param<uint64_t> p_window_bytes;
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    tlm_utils::simple_target_socket<host_cmn_cyprus, DEFAULT_TLM_BUSWIDTH>
        target_socket;

    explicit host_cmn_cyprus(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_revision("revision", 3)
        , p_rcomp_lsb("rcomp_lsb", RNSAM_RCOMP_LSB_DEFAULT)
        , p_window_bytes("window_bytes", DEFAULT_WINDOW_BYTES)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 128)
        , target_socket("target_socket")
    {
        seed_registers();
        target_socket.register_b_transport(this, &host_cmn_cyprus::b_transport);
        target_socket.register_transport_dbg(this,
                                             &host_cmn_cyprus::transport_dbg);
    }

    void before_end_of_elaboration() override
    {
        seed_registers();
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
