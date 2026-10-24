/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <algorithm>
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
#include <tlm-extensions/request-context.h>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

// NI-710AE r0p1 TRM 102756_0001_04, sections 3.9 and 16.11.
// Functional error packets and RAS records; no physical CRC/DLS or error-network timing.
class zena_ni710ae_fmu : public sc_core::sc_module
{
    static constexpr unsigned MAX_RECORDS = 896; // 0xe000 / 0x40
    static constexpr uint32_t V = 1u << 30, UE = 1u << 29, OF = 1u << 27;
    static constexpr uint32_t MV = 1u << 26, CE = 3u << 24, UET = 3u << 20, CI = 1u << 19;
    static constexpr uint32_t W1C = V | UE | OF | MV | CE | UET | CI;
    static constexpr uint32_t CTLR_MASK = 1 | 4 | 8 | 0x100 | 0x2000;
    static constexpr uint64_t INFO_MASK = (1ull << 62) - 1;
    std::vector<uint64_t> m_node_info;
    std::vector<uint32_t> m_status, m_enabled;
    uint32_t m_control = 1;
    uint64_t m_sminfo = 0;
    bool m_unlocked = false, m_reset = false;
    bool m_functional_reset = false, m_record_reset = false;
    bool m_half_pending = false;
    uint64_t m_half_address = 0;
    std::array<bool, 3> m_levels{{false, false, false}}; // ERI, FHI, CRI

    bool valid_node(unsigned node) const
    {
        return node < m_node_info.size() && m_node_info[node] != 0;
    }
    unsigned selected_node() const
    {
        for (unsigned i = 0; i < m_node_info.size(); ++i)
            if (valid_node(i) && m_node_info[i] == m_sminfo) return i;
        return MAX_RECORDS;
    }
    void configure_nodes()
    {
        m_node_info = p_node_info.get_value();
        const std::string base = std::string(name()) + ".node_info";
        const auto children = gs::sc_cci_children(base.c_str());
        std::vector<uint64_t> indexed;
        // Record numbers are numeric indices. Lexical ordering would put
        // record10 before record2, silently changing the hardware identity map.
        const auto broker = cci::cci_get_broker();
        if (children.size() > MAX_RECORDS)
            SC_REPORT_FATAL(name(), "NI node table exceeds register aperture");
        for (unsigned i = 0; i < children.size(); ++i) {
            uint64_t value = 0;
            if (!gs::cci_get<uint64_t>(broker, base + "." + std::to_string(i), value))
                SC_REPORT_FATAL(name(), "NI node_info requires contiguous numeric indices");
            indexed.push_back(value);
        }
        if (!indexed.empty()) {
            if (!m_node_info.empty()) SC_REPORT_FATAL(name(), "NI node_info specified twice");
            m_node_info = indexed;
        }
        if (m_node_info.empty()) {
            if (p_node_index.get_value() >= MAX_RECORDS)
                SC_REPORT_FATAL(name(), "NI FMU node_index exceeds register aperture");
            // Preserve the platform's self-node record index. Undescribed slots
            // are reserved holes, not invented interconnect endpoint identities.
            m_node_info.assign(p_node_index.get_value() + 1, 0);
            m_node_info[p_node_index.get_value()] = 0x61;
        }
        if (m_node_info.size() > MAX_RECORDS)
            SC_REPORT_FATAL(name(), "NI node table exceeds register aperture");
        for (unsigned i = 0; i < m_node_info.size(); ++i) {
            if (m_node_info[i] & ~INFO_MASK)
                SC_REPORT_FATAL(name(), "NI node_info has reserved bits");
            if (m_node_info[i] && std::find(m_node_info.begin(), m_node_info.begin() + i,
                                          m_node_info[i]) != m_node_info.begin() + i)
                SC_REPORT_FATAL(name(), "NI node_info identities must be unique");
        }
        m_status.assign(m_node_info.size(), 0);
        m_enabled.assign(m_node_info.size(), 0);
    }
    void notify_parent(unsigned output, bool level)
    {
        if (!fault_socket.size()) return;
        const unsigned record = output == 2 ? p_parent_critical_record.get_value() :
            output == 1 ? p_parent_fault_record.get_value() : p_parent_non_critical_record.get_value();
        uint32_t value = unsigned(level) | (output == 2 ? 2u : 0u);
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(uint64_t(p_parent_bank.get_value()) * 0x10000 + uint64_t(record) * 0x40);
        tx.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        fault_socket->b_transport(tx, delay);
        if (tx.is_response_error()) SC_REPORT_ERROR(name(), "System FMU rejected NI fault level");
    }
    void update_irqs()
    {
        std::array<bool, 3> levels{{false, false, false}};
        for (auto status : m_status) {
            if (!(status & V) || !(m_control & 1)) continue;
            levels[0] |= (status & UE) && (m_control & 4);
            levels[1] |= ((status & UE) && (m_control & 8)) || ((status & CE) && (m_control & 0x100));
            levels[2] |= (status & CI) && (m_control & 0x2000);
        }
        for (unsigned i = 0; i < 3; ++i) {
            if (m_levels[i] == levels[i]) continue;
            m_levels[i] = levels[i];
            if (i == 0 && non_critical_irq.size()) non_critical_irq->write(levels[i]);
            if (i == 1 && fault_irq.size()) fault_irq->write(levels[i]);
            if (i == 2 && critical_irq.size()) critical_irq->write(levels[i]);
            notify_parent(i, levels[i]);
        }
    }
    static unsigned severity(uint32_t status)
    {
        if (!(status & V)) return 0;
        if (!(status & UE)) return (status & CE) ? 1 : 0;
        static constexpr unsigned priorities[] = {5, 4, 2, 3}; // UC, UEU, UEO, UER
        return priorities[(status >> 20) & 3];
    }
    bool record_error(unsigned node, unsigned mechanism, unsigned uet)
    {
        if (!valid_node(node) || mechanism >= 18 || uet > 3 ||
            m_reset || m_functional_reset || m_record_reset) return false;
        if (!(m_enabled[node] & (1u << mechanism)) || !(m_control & 1)) return true;
        const bool corrected = mechanism == 14;
        const bool critical = !corrected && (p_critical_vector.get_value() & (1u << mechanism));
        if (critical && !(m_control & 0x2000)) uet = 0; // RAS: disabled CRI becomes UC.
        uint32_t incoming = V | MV | (mechanism << 8) | 1;
        incoming |= corrected ? (2u << 24) : UE | (uet << 20);
        if (critical) incoming |= CI;
        auto& old = m_status[node];
        const bool valid = old & V;
        if (!valid || severity(incoming) > severity(old)) {
            // RAS v1.1: overwrite with a higher severity, preserving lower class flags.
            incoming |= valid ? old & (CE | CI | OF) : 0;
            if (valid) incoming |= OF;
            old = incoming;
        } else {
            old |= OF | (incoming & (CE | UE | CI));
        }
        update_irqs();
        return true;
    }
    uint32_t read32(uint64_t address) const
    {
        if (address < m_status.size() * 0x40) {
            const unsigned node = address / 0x40, field = address % 0x40;
            if (field == 0) return node == 0 ? 0x8008a2 : 0;
            if (field == 8) return node == 0 ? m_control : 0;
            if (field == 0x10) return (m_status[node] & ~1u) | ((m_status[node] & V) ? 1u : 0u);
            if (field == 0x20) return m_node_info[node];
            if (field == 0x24) return m_node_info[node] >> 32;
            return 0;
        }
        if (address >= 0xe000 && address < 0xe080) {
            const unsigned first = ((address - 0xe000) / 4) * 32;
            uint32_t value = 0;
            for (unsigned bit = 0; bit < 32 && first + bit < m_status.size(); ++bit)
                if (m_status[first + bit] & V) value |= 1u << bit;
            return value;
        }
        switch (address) {
        case 0xe100: return 0x43d1043b; // r0p1, detailed register table 16-119.
        case 0xe200: return m_unlocked ? 0xbe : 0;
        case 0xe204: { const auto node = selected_node(); return valid_node(node) ? m_enabled[node] : 0; }
        case 0xe210: return m_sminfo;
        case 0xe214: return m_sminfo >> 32;
        case 0xffbc: return 0x47710a00;
        case 0xffc8: return m_node_info.size();
        case 0xffd0: return 0x44;
        case 0xffe0: return 0x3d;
        case 0xffe4: return 0xb4;
        case 0xffe8: return 0x0b;
        case 0xfff0: return 0xd;
        case 0xfff4: return 0xf0;
        case 0xfff8: return 5;
        case 0xfffc: return 0xb1;
        default: return 0;
        }
    }
    void write32(uint64_t address, uint32_t value)
    {
        if (address == 8) {
            m_control = value & CTLR_MASK;
            update_irqs();
        } else if (address < m_status.size() * 0x40 && address % 0x40 == 0x10) {
            auto& status = m_status[address / 0x40];
            // RAS v1.1 conditional W1C prevents a stale ACK from clearing a
            // new overflow or a different outstanding error class.
            uint32_t clear = value & W1C;
            if ((status & OF) && !(clear & OF)) clear &= ~UE;
            const bool keep_syndrome = ((status & UE) && !(clear & UE)) ||
                (!(status & UE) && (status & CE & ~clear));
            if (keep_syndrome) clear &= ~(MV | UET);
            if (status & (CE | UE) & ~clear) clear &= ~V;
            if ((status & V) && !keep_syndrome)
                status = (status & ~0xff00u) | (value & 0xff00u);
            status &= ~clear;
            update_irqs();
        } else if (address == 0xe210) {
            m_sminfo = (m_sminfo & 0xffffffff00000000ull) | value;
        } else if (address == 0xe214) {
            m_sminfo = (m_sminfo & 0xffffffffull) | (uint64_t(value & 0x3fffffff) << 32);
        } else if (address == 0xe204) {
            const auto node = selected_node();
            if (valid_node(node)) m_enabled[node] = value & 0x3ffff;
        } else if (address == 0xe208) {
            record_error(selected_node(), value & 0x3f, 0);
        }
    }
    bool ras64(uint64_t address) const
    {
        if (address == 0xe210 || (address >= 0xe000 && address < 0xe080)) return true;
        return address < m_status.size() * 0x40 &&
            (address % 0x40 == 0 || address % 0x40 == 8 ||
             address % 0x40 == 0x10 || address % 0x40 == 0x20);
    }
    bool access(tlm::tlm_generic_payload& tx)
    {
        tx.set_dmi_allowed(false);
        RequestContextTlmExtension* context = nullptr;
        tx.get_extension(context);
        if (context && context->get_context().secure_valid && !context->get_context().secure) {
            if (tx.is_read() && tx.get_data_ptr()) std::memset(tx.get_data_ptr(), 0, tx.get_data_length());
            tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false; // Never change key/paired-half state on NS accesses.
        }
        const auto address = tx.get_address();
        const auto length = tx.get_data_length();
        auto reject = [&](tlm::tlm_response_status result) {
            if (tx.is_write()) m_unlocked = m_half_pending = false;
            tx.set_response_status(result);
            return false;
        };
        if (!tx.get_data_ptr() || (length != 4 && length != 8) || address > 0x10000 - length ||
            address % length || (length == 8 && !ras64(address)))
            return reject(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        if (tx.get_streaming_width() < length) return reject(tlm::TLM_BURST_ERROR_RESPONSE);
        const auto* enables = tx.get_byte_enable_ptr();
        const auto enable_length = tx.get_byte_enable_length();
        if (enables && !enable_length) return reject(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        for (unsigned i = 0; enables && i < length; ++i)
            if (enables[i % enable_length] != 0xff) return reject(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        if (tx.is_read()) {
            for (unsigned i = 0; i < length; i += 4) {
                const auto value = read32(address + i);
                std::memcpy(tx.get_data_ptr() + i, &value, 4);
            }
        } else if (tx.is_write()) {
            uint32_t value;
            std::memcpy(&value, tx.get_data_ptr(), 4);
            if (m_reset || m_functional_reset) { tx.set_response_status(tlm::TLM_OK_RESPONSE); return true; }
            if (address == 0xe200) {
                m_unlocked = (value & 0xff) == 0xbe;
                m_half_pending = false;
            } else {
                const bool second_half = length == 4 && m_half_pending && address == (m_half_address ^ 4);
                const bool permitted = m_unlocked || second_half;
                m_half_pending = m_unlocked && length == 4 && ras64(address & ~7ull);
                m_half_address = address;
                m_unlocked = false;
                if (permitted) for (unsigned i = 0; i < length; i += 4) {
                    std::memcpy(&value, tx.get_data_ptr() + i, 4);
                    write32(address + i, value);
                }
            }
        } else return reject(tlm::TLM_COMMAND_ERROR_RESPONSE);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }
public:
    cci::cci_param<uint64_t> p_parent_base; // Retained config ABI; level input uses relative addresses.
    cci::cci_param<unsigned> p_node_index, p_parent_bank, p_parent_critical_record;
    cci::cci_param<unsigned> p_parent_non_critical_record, p_parent_fault_record;
    cci::cci_param<unsigned> p_critical_vector;
    cci::cci_param<std::vector<uint64_t>> p_node_info;
    tlm_utils::simple_target_socket<zena_ni710ae_fmu, DEFAULT_TLM_BUSWIDTH> target_socket;
    tlm_utils::simple_target_socket_optional<zena_ni710ae_fmu, DEFAULT_TLM_BUSWIDTH> error_socket;
    tlm_utils::simple_initiator_socket_optional<zena_ni710ae_fmu, DEFAULT_TLM_BUSWIDTH> fault_socket;
    InitiatorSignalSocket<bool> critical_irq, non_critical_irq, fault_irq;
    TargetSignalSocket<bool> reset_in, functional_reset_in, record_reset_in;

    explicit zena_ni710ae_fmu(sc_core::sc_module_name name)
        : sc_module(name), p_parent_base("parent_base", 0x2a510000), p_node_index("node_index", 0),
          p_parent_bank("parent_bank", 1), p_parent_critical_record("parent_critical_record", 2),
          p_parent_non_critical_record("parent_non_critical_record", 0),
          p_parent_fault_record("parent_fault_record", 1), p_critical_vector("critical_vector", 0),
          p_node_info("node_info", std::vector<uint64_t>{}), target_socket("target_socket"),
          error_socket("error_socket"), fault_socket("fault_socket"), critical_irq("critical_irq"),
          non_critical_irq("non_critical_irq"), fault_irq("fault_irq"), reset_in("reset_in"),
          functional_reset_in("functional_reset_in"), record_reset_in("record_reset_in")
    {
        target_socket.register_b_transport(this, &zena_ni710ae_fmu::b_transport);
        target_socket.register_transport_dbg(this, &zena_ni710ae_fmu::transport_dbg);
        error_socket.register_b_transport(this, &zena_ni710ae_fmu::error_transport);
        reset_in.register_value_changed_cb([this](bool level) { m_reset = level; if (level) reset(); });
        functional_reset_in.register_value_changed_cb([this](bool level) {
            m_functional_reset = level;
            if (level) functional_reset();
        });
        record_reset_in.register_value_changed_cb([this](bool level) {
            m_record_reset = level;
            if (level) { std::fill(m_status.begin(), m_status.end(), 0); update_irqs(); }
        });
        configure_nodes();
    }
    void functional_reset()
    {
        m_control = 1;
        m_sminfo = 0;
        std::fill(m_enabled.begin(), m_enabled.end(), 0);
        m_unlocked = m_half_pending = false;
        update_irqs();
    }
    void reset()
    {
        std::fill(m_status.begin(), m_status.end(), 0);
        functional_reset();
    }
    bool runtime_report_error(unsigned node, unsigned mechanism, unsigned uet = 0)
    {
        return record_error(node, mechanism, uet);
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        (void)delay;
        access(tx);
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& tx)
    {
        return access(tx) ? tx.get_data_length() : 0;
    }
    // Functional detector packet: node index[25:16], SMID[5:0], UET[7:6].
    // No INJ field exists in this NI implementation's architected STATUS.
    void error_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        (void)delay;
        tx.set_dmi_allowed(false);
        if (!tx.is_write() || tx.get_address() || tx.get_data_length() != 4 ||
            !tx.get_data_ptr() || tx.get_streaming_width() < 4) {
            tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
        }
        const auto* enables = tx.get_byte_enable_ptr();
        const auto enable_length = tx.get_byte_enable_length();
        if (enables && !enable_length) {
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
        }
        for (unsigned i = 0; enables && i < 4; ++i) {
            if (enables[i % enable_length] != 0xff) {
                tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
            }
        }
        uint32_t value;
        std::memcpy(&value, tx.get_data_ptr(), 4);
        const bool valid = !(value & 0xfc00ff00u) &&
            runtime_report_error(value >> 16, value & 0x3f, (value >> 6) & 3);
        tx.set_response_status(valid ? tlm::TLM_OK_RESPONSE : tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
};
extern "C" void module_register();
