/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
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

class zena_device_fmu : public sc_core::sc_module
{
    static constexpr uint64_t V = 1ull << 30;
    static constexpr uint64_t UE = 1ull << 29;
    static constexpr uint64_t OF = 1ull << 27;
    static constexpr uint64_t CI = 1ull << 19;
    static constexpr uint64_t W = 1ull << 45;
    static constexpr uint64_t INJ = 1ull << 44;
    static constexpr uint64_t OFB = 1ull << 46;
    static constexpr uint64_t OFX = 1ull << 47;
    static constexpr uint32_t CTLR_CI = 1u << 13;
    static constexpr uint32_t CTLR_UI = 1u << 2;

    struct Mechanism {
        bool enabled = true;
        bool critical = true;
        bool inserted = false;
        uint32_t timeout = 0;
    };
    struct Block {
        std::array<Mechanism, 256> mechanism;
        bool wires = true;
        bool powered = true;
        unsigned int response_cycles = 1;
        std::array<bool, 2> external_wire{{false, false}};
    };
    struct Fault {
        unsigned int block;
        unsigned int mechanism;
        bool injected;
        bool overflow = false;
        bool wire = true;
    };
    struct Record {
        uint32_t control = 3;
        std::deque<Fault> pending;
        bool visible = false;
        bool wire_only = false;
        bool overflow_block = false;
    };

    struct Response {
        uint64_t id;
        sc_core::sc_time due;
        uint32_t offset;
        uint32_t value;
        uint32_t write_data;
        uint32_t identity;
    };

    std::array<std::vector<Block>, 6> m_blocks;
    std::array<Record, 12> m_records;
    uint32_t m_status = 0;
    uint32_t m_write_data = 0;
    uint32_t m_read_data = 0;
    uint32_t m_timeout = ~0u;
    uint32_t m_fctlr = 0;
    bool m_unlocked = false;
    bool m_critical = false;
    bool m_non_critical = false;
    uint64_t m_command_id = 0;
    sc_core::sc_time m_deadline = sc_core::SC_ZERO_TIME;
    sc_core::sc_time m_access_time = sc_core::SC_ZERO_TIME;
    std::deque<Response> m_responses;
    sc_core::sc_event m_response_event;

    unsigned int types() const { return p_mhu_profile.get_value() ? 3 : 6; }
    unsigned int records() const { return types() * 2; }
    unsigned int fmu_type() const { return types() - 1; }

    unsigned int mechanism_limit(unsigned int type) const
    {
        static constexpr unsigned int gic[] = {76, 10, 12, 37, 24, 14};
        static constexpr unsigned int mhu[] = {21, 30, 14};
        return p_mhu_profile.get_value() ? mhu[type] : gic[type];
    }

    bool corrected(unsigned int type, unsigned int id) const
    {
        if (p_mhu_profile.get_value()) {
            return (type == 0 && (id == 17 || id == 18)) ||
                   (type == 1 && id >= 17 && id <= 27 && (id - 17) % 3 != 2);
        }
        return (type == 0 && id >= 24 && id <= 69 && id % 3 == 0) ||
               (type == 3 && id == 13) ||
               (type == 4 && id >= 12 && id <= 18 && id % 3 == 0);
    }

    unsigned int pages(unsigned int type, unsigned int id) const
    {
        if (p_mhu_profile.get_value()) {
            return type < 2 && id == 8 ? 3 : 1;
        }
        if (type == 3 && id >= 22 && id <= 37) {
            return 5;
        }
        if ((type == 0 && id >= 12 && id <= 16) ||
            (type == 1 && id == 5) || (type == 2 && id == 5) ||
            (type == 3 && (id == 6 || id == 7)) ||
            (type == 4 && id == 8)) {
            return 3;
        }
        if ((type == 0 && id == 20) || (type == 2 && id == 8) ||
            (type == 3 && id == 9)) {
            return 3;
        }
        return 1;
    }

    bool valid_block(unsigned int type, unsigned int block) const
    {
        return type < types() && block < m_blocks[type].size();
    }

    bool wire_asserted(unsigned int index) const
    {
        if (!(m_records[index].control & 2)) return false;
        for (const auto& block : m_blocks[index / 2]) {
            if (block.wires && block.external_wire[index & 1]) return true;
        }
        for (const auto& fault : m_records[index].pending) {
            if (fault.wire && m_blocks[index / 2][fault.block].wires) return true;
        }
        return false;
    }

    uint64_t record_status(unsigned int index) const
    {
        const auto& record = m_records[index];
        const uint64_t wire = wire_asserted(index) ? W : 0;
        const uint64_t overflow = record.overflow_block ? OFX : 0;
        if (!record.visible || record.pending.empty()) {
            if (record.wire_only && (record.control & 3) == 3) {
                return V | UE | (3ull << 20) | 1 | wire | overflow |
                    ((index & 1) ? 0 : CI);
            }
            // MHU-320AE ERRnSTATUS.SERR is the constant primary error code.
            return (p_mhu_profile.get_value() ? 1 : 0) | wire | overflow;
        }
        const auto& fault = record.pending.front();
        uint64_t value = V | UE | (3ull << 20) | 1 |
            (static_cast<uint64_t>(fault.block) << 32) |
            (static_cast<uint64_t>(fault.mechanism) << 8) | wire | overflow;
        if ((index & 1) == 0) {
            value |= CI;
        }
        if (fault.injected) {
            value |= INJ;
        }
        if (fault.overflow) {
            value |= OF;
        }
        for (const auto& pending : record.pending) {
            if (pending.block == fault.block && pending.mechanism != fault.mechanism) {
                value |= OFB;
            }
        }
        return value;
    }

    void notify_parent(bool critical, bool level)
    {
        if (fault_socket.size() == 0) {
            return;
        }
        uint32_t value = (level ? 1u : 0u) | (critical ? 2u : 0u);
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(static_cast<uint64_t>(p_parent_bank.get_value()) * 0x10000 +
            static_cast<uint64_t>(critical ? p_parent_critical_record.get_value() :
                                            p_parent_non_critical_record.get_value()) * 0x40);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        fault_socket->b_transport(trans, delay);
        if (trans.is_response_error()) {
            SC_REPORT_ERROR(name(), "System FMU rejected device fault input");
        }
    }

    void update_irqs()
    {
        bool critical = false;
        bool non_critical = false;
        for (unsigned int i = 0; i < records(); ++i) {
            if (!(record_status(i) & V) || !(m_records[i].control & 1)) {
                continue;
            }
            if (i & 1) {
                non_critical |= (m_records[i].control & CTLR_UI) != 0;
            } else {
                critical |= (m_records[i].control & CTLR_CI) != 0;
            }
        }
        if (critical != m_critical) {
            m_critical = critical;
            if (critical_irq.size()) {
                critical_irq->write(critical);
            }
            notify_parent(true, critical);
        }
        if (non_critical != m_non_critical) {
            m_non_critical = non_critical;
            if (non_critical_irq.size()) {
                non_critical_irq->write(non_critical);
            }
            notify_parent(false, non_critical);
        }
    }

    void record_fault(unsigned int type, unsigned int block, unsigned int id,
                      bool injected, bool wire = true)
    {
        if (!valid_block(type, block) || id == 0 || id > mechanism_limit(type)) {
            return;
        }
        auto& mechanism = m_blocks[type][block].mechanism[id];
        const unsigned int index = type * 2 + (mechanism.critical ? 0 : 1);
        auto& record = m_records[index];
        if (!mechanism.enabled || !(record.control & 1)) {
            return;
        }
        mechanism.inserted |= injected;
        if (record.visible && !record.pending.empty() &&
            record.pending.front().block != block) {
            record.overflow_block = true;
        }
        auto found = std::find_if(record.pending.begin(), record.pending.end(),
            [block, id, injected](const Fault& fault) {
                return fault.block == block && fault.mechanism == id &&
                       fault.injected == injected;
            });
        if (found != record.pending.end()) {
            found->overflow = true;
            found->wire |= wire;
        } else {
            for (auto& fault : record.pending) {
                if (p_mhu_profile.get_value() ||
                    (fault.block == block && fault.mechanism == id)) {
                    fault.overflow = true;
                }
            }
            record.pending.push_back({block, id, injected, false, wire});
        }
        record.visible = true;
        update_irqs();
    }

    void clear_injected(unsigned int type, unsigned int block, unsigned int id)
    {
        for (unsigned int index = type * 2; index < type * 2 + 2; ++index) {
            auto& record = m_records[index];
            record.pending.erase(std::remove_if(record.pending.begin(), record.pending.end(),
                [block, id](const Fault& fault) {
                    return fault.block == block && fault.mechanism == id && fault.injected;
                }), record.pending.end());
            record.visible = record.visible && !record.pending.empty();
        }
        m_blocks[type][block].mechanism[id].inserted = false;
    }

    void internal_fault(unsigned int id)
    {
        record_fault(fmu_type(), 0, id, false);
    }

    void clear_record(unsigned int index, uint32_t value)
    {
        auto& record = m_records[index];
        const uint64_t status = record_status(index);
        if (!(status & V) || (value & (V | UE)) != (V | UE) ||
            ((status & OF) && !(value & OF)) ||
            ((record.control & 2) && !(status & W)) ||
            record.pending.empty() || !record.visible) {
            return;
        }
        const auto old = record.pending.front();
        record.pending.pop_front();
        record.wire_only = false;
        bool inserted = false;
        for (unsigned int i = (index / 2) * 2; i < (index / 2) * 2 + 2; ++i) {
            for (const auto& fault : m_records[i].pending) {
                inserted |= fault.block == old.block && fault.mechanism == old.mechanism && fault.injected;
            }
        }
        m_blocks[index / 2][old.block].mechanism[old.mechanism].inserted = inserted;
        auto next = std::find_if(record.pending.begin(), record.pending.end(),
            [&old](const Fault& fault) { return fault.block == old.block; });
        record.visible = next != record.pending.end();
        if (record.visible) {
            std::iter_swap(record.pending.begin(), next);
        }
        update_irqs();
    }

    void command(uint32_t offset, uint32_t value)
    {
        const unsigned int type = (value >> 28) & 7;
        const unsigned int block = (value >> 16) & 0xfff;
        const unsigned int id = (value >> 8) & 0xff;
        const unsigned int page = value & 0xff;
        m_status = value & 0x7fffff00;
        if (!valid_block(type, block)) {
            m_status |= 1u << 3;
            internal_fault(11);
            return;
        }
        if (offset == 0xf28) {
            unsigned int end = m_blocks[type].size();
            if (value & 1) {
                for (unsigned int n = block; n < end; ++n) {
                    if (!m_blocks[type][n].powered) {
                        end = n;
                        break;
                    }
                }
            }
            for (unsigned int i = type * 2; i < type * 2 + 2; ++i) {
                auto& record = m_records[i];
                if (value & ((i & 1) ? 2u : 4u)) record.overflow_block = false;
                const auto first = std::find_if(record.pending.begin(), record.pending.end(),
                    [&](const Fault& fault) {
                        return fault.block >= block && fault.block < end &&
                               m_blocks[type][fault.block].powered;
                    });
                if (!record.visible && first != record.pending.end()) {
                    std::iter_swap(record.pending.begin(), first);
                    record.visible = true;
                }
                if (record.visible) {
                    for (const auto& fault : record.pending) {
                        if (fault.block >= block && fault.block < end &&
                            m_blocks[type][fault.block].powered &&
                            fault.block != record.pending.front().block) {
                            record.overflow_block = true;
                        }
                    }
                }
            }
            m_status = (type << 28) |
                (static_cast<uint32_t>(end < m_blocks[type].size() ?
                                      end : end - 1) << 16);
            update_irqs();
            return;
        }
        if (id == 0 || (id != 255 && id > mechanism_limit(type)) ||
            ((offset == 0xf0c || offset == 0xf14) && page >= pages(type, id))) {
            m_status |= 1u << 4;
            if (id == 0 || page > 15) {
                internal_fault(11);
            }
            return;
        }
        auto& state = m_blocks[type][block];
        auto& mechanism = state.mechanism[id];
        if (offset == 0xf00) {
            if (id == 255) {
                state.wires = value & 1;
            } else {
                mechanism.enabled = value & 1;
            }
        } else if (offset == 0xf08 && id != 255) {
            mechanism.critical = value & 1;
        } else if (offset == 0xf04 && id != 255) {
            record_fault(type, block, id, true);
        } else if (offset == 0xf0c || offset == 0xf14) {
            if (page == 0) {
                if (offset == 0xf0c) {
                    if (id == 255) {
                        state.wires = m_write_data & 2;
                    } else {
                        mechanism.enabled = m_write_data & 2;
                        mechanism.critical = m_write_data & 4;
                        if (m_write_data & 16) {
                            record_fault(type, block, id, true);
                        } else {
                            clear_injected(type, block, id);
                        }
                    }
                }
                m_read_data = (id == 255 ? state.wires : mechanism.enabled) ? 2 : 0;
                if (id != 255) {
                    m_read_data |= (mechanism.critical ? 4 : 0) |
                                   (mechanism.inserted ? 16 : 0);
                }
            } else if (page == 1 && p_mhu_profile.get_value()) {
                if (offset == 0xf0c) {
                    mechanism.timeout = m_write_data;
                }
                m_read_data = mechanism.timeout;
            } else {
                // shortcut: no AXI wire protocol is modeled; add CRC/ping timing with a stream model.
                m_read_data = 0;
            }
        }
        if (id == 255) {
            for (unsigned int i = type * 2; i < type * 2 + 2; ++i) {
                auto& record = m_records[i];
                const auto found = std::find_if(record.pending.begin(), record.pending.end(),
                    [block](const Fault& fault) { return fault.block == block; });
                if (found == record.pending.end()) continue;
                if (!record.visible) {
                    std::iter_swap(record.pending.begin(), found);
                    record.visible = true;
                } else if (record.pending.front().block != block) {
                    record.overflow_block = true;
                }
            }
        }
        update_irqs();
    }

    void schedule_response()
    {
        m_response_event.cancel();
        if (m_responses.empty() && !(m_status & 1)) return;
        sc_core::sc_time next = (m_status & 1) ? m_deadline : m_responses.front().due;
        for (const auto& response : m_responses) next = std::min(next, response.due);
        m_response_event.notify(next > sc_core::sc_time_stamp() ?
                               next - sc_core::sc_time_stamp() : sc_core::SC_ZERO_TIME);
    }

    void process_responses()
    {
        const auto now = sc_core::sc_time_stamp();
        for (auto it = m_responses.begin(); it != m_responses.end();) {
            if (it->due > now) { ++it; continue; }
            if (it->id == m_command_id && (m_status & 1) && it->due <= m_deadline) {
                const uint32_t current_write_data = m_write_data;
                m_write_data = it->write_data;
                execute_operation(it->offset, it->value);
                m_write_data = current_write_data;
            } else {
                m_status = (m_status & 0xff) | it->identity | (1u << 5);
            }
            it = m_responses.erase(it);
        }
        if ((m_status & 1) && now >= m_deadline) {
            m_status = (m_status & ~1u) | 2u;
        }
        schedule_response();
    }

    void execute_operation(uint32_t offset, uint32_t value)
    {
        unsigned int type = (value >> 28) & 7;
        unsigned int block = (value >> 16) & 0xfff;
        if (offset < records() * 0x40) {
            const auto& record = m_records[offset / 0x40];
            type = offset / 0x80;
            block = record.pending.empty() ? 0 : record.pending.front().block;
        }
        m_status = response_identity(offset, value);
        const bool exists = valid_block(type, block);
        if (exists && !m_blocks[type][block].powered) {
            m_status |= 4u;
        } else if (offset < records() * 0x40) {
            clear_record(offset / 0x40, value);
        } else {
            command(offset, value);
        }
    }

    uint32_t response_identity(uint32_t offset, uint32_t value) const
    {
        if (offset >= records() * 0x40) return value & 0x7fffff00;
        const auto& record = m_records[offset / 0x40];
        const uint32_t identity = (offset / 0x80) << 28;
        if (record.pending.empty()) return identity;
        const auto& fault = record.pending.front();
        return identity | (fault.block << 16) | (fault.mechanism << 8);
    }

    void start_operation(uint32_t offset, uint32_t value)
    {
        if (m_status & 1) {
            internal_fault(13);
            return;
        }
        const uint64_t period = p_clock_period_ns.get_value();
        if (period == 0) {
            execute_operation(offset, value);
            return;
        }
        unsigned int type = (value >> 28) & 7;
        unsigned int block = (value >> 16) & 0xfff;
        if (offset < records() * 0x40) {
            const auto& record = m_records[offset / 0x40];
            type = offset / 0x80;
            block = record.pending.empty() ? 0 : record.pending.front().block;
        }
        const unsigned int cycles = valid_block(type, block) ?
                                    m_blocks[type][block].response_cycles : 1;
        m_deadline = m_access_time +
            sc_core::sc_time(double(m_timeout) * double(period), sc_core::SC_NS);
        m_responses.push_back({++m_command_id,
            m_access_time + sc_core::sc_time(double(cycles) * double(period), sc_core::SC_NS),
            offset, value, m_write_data, response_identity(offset, value)});
        m_status = (m_status & 0x7fffff00) | 1u;
        schedule_response();
    }

    uint32_t read_word(uint32_t offset) const
    {
        if (offset < records() * 0x40) {
            const unsigned int index = offset / 0x40;
            if (m_blocks[index / 2].empty()) {
                return 0;
            }
            switch (offset % 0x40) {
            case 0x00: return (index & 1) ? 0x22 : 0x800002;
            case 0x04: return m_blocks[index / 2].size() - 1;
            case 0x08: return m_records[index].control;
            case 0x10: return record_status(index);
            case 0x14: return record_status(index) >> 32;
            default: return 0;
            }
        }
        switch (offset) {
        case 0xe00: {
            uint32_t result = 0;
            for (unsigned int i = 0; i < records(); ++i) {
                if (record_status(i) & V) {
                    result |= 1u << i;
                }
            }
            return result;
        }
        case 0xe10: return p_mhu_profile.get_value() ? 0x49d0143b : 0x49a0043b;
        case 0xf10: return m_write_data;
        case 0xf18: return m_read_data;
        case 0xf1c: return m_status;
        case 0xf20: return m_unlocked ? 0xbe : 0;
        case 0xf24: return m_timeout;
        case 0xf2c: return m_fctlr;
        case 0xfc8: return records();
        case 0xfd0: return 4;
        case 0xfe0: return p_mhu_profile.get_value() ? 0x9d : 0x9a;
        case 0xfe4: return 0xb4;
        case 0xfe8: return p_mhu_profile.get_value() ? 0x1b : 0x0b;
        case 0xff0: return 0xd;
        case 0xff4: return 0xf0;
        case 0xff8: return 5;
        case 0xffc: return 0xb1;
        default: return 0;
        }
    }

    void write_word(uint32_t offset, uint32_t value)
    {
        if (offset < records() * 0x40) {
            const unsigned int index = offset / 0x40;
            if (m_blocks[index / 2].empty()) {
                return;
            }
            switch (offset % 0x40) {
            case 0x08:
                m_records[index].control = value & (3 | ((index & 1) ? CTLR_UI : CTLR_CI));
                update_irqs();
                return;
            case 0x10: start_operation(offset, value); return;
            case 0x0c:
            case 0x14: return;
            default: internal_fault(10); return;
            }
        }
        switch (offset) {
        case 0xf00: case 0xf04: case 0xf08: case 0xf0c: case 0xf14: case 0xf28:
            start_operation(offset, value); return;
        case 0xf10: m_write_data = value; return;
        case 0xf24: m_timeout = value; return;
        case 0xf2c: m_fctlr = value & 5; return;
        default: internal_fault(10); return;
        }
    }

    bool access(tlm::tlm_generic_payload& trans)
    {
        trans.set_dmi_allowed(false);
        const uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        auto* data = trans.get_data_ptr();
        RequestContextTlmExtension* context = nullptr;
        trans.get_extension(context);
        const bool non_secure = context && context->get_context().secure_valid &&
                                !context->get_context().secure;
        const bool upper = length == 4 && address < records() * 0x40 &&
            (address % 0x40 == 0x0c || address % 0x40 == 0x14);
        const bool ras64 = address == 0xe00 || (address < 0xe00 &&
            (address % 0x40 == 0 || address % 0x40 == 8 || address % 0x40 == 16));
        if (!data || (length != 4 && length != 8) || (address & (length - 1)) ||
            address > 0x10000 - length || (length == 8 && !ras64)) {
            if (non_secure) {
                internal_fault(9);
            } else if (trans.is_write()) {
                if (!upper) m_unlocked = false;
                internal_fault(12);
            }
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (non_secure) {
            internal_fault(9);
            if (trans.is_read()) {
                std::memset(data, 0, length);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }
        if (trans.get_streaming_width() < length) {
            if (trans.is_write()) {
                if (!upper) m_unlocked = false;
                internal_fault(12);
            }
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return false;
        }
        const auto* enables = trans.get_byte_enable_ptr();
        const unsigned int enable_length = trans.get_byte_enable_length();
        bool full_strobes = !enables || enable_length != 0;
        for (unsigned int i = 0; enables && full_strobes && i < length; ++i) {
            full_strobes = enables[i % enable_length] == 0xff;
        }
        if (!full_strobes) {
            if (trans.is_write()) {
                if (!upper) m_unlocked = false;
                internal_fault(12);
            }
            trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return false;
        }
        if (trans.is_read()) {
            for (unsigned int i = 0; i < length; i += 4) {
                const uint32_t value = read_word(address + i);
                std::memcpy(data + i, &value, 4);
            }
        } else if (trans.is_write()) {
            uint32_t value;
            std::memcpy(&value, data, 4);
            if (address == 0xf20) {
                m_unlocked = length == 4 && (value & 0xff) == 0xbe;
            } else {
                const bool unlocked = m_unlocked;
                if (!upper) {
                    m_unlocked = false;
                }
                if (!unlocked && !upper) {
                    internal_fault(8);
                } else {
                    for (unsigned int i = 0; i < length; i += 4) {
                        std::memcpy(&value, data + i, 4);
                        write_word(address + i, value);
                    }
                }
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

public:
    SC_HAS_PROCESS(zena_device_fmu);
    cci::cci_param<bool> p_mhu_profile;
    cci::cci_param<uint64_t> p_clock_period_ns;
    cci::cci_param<unsigned int> p_gci_count;
    cci::cci_param<unsigned int> p_spi_collator_count;
    cci::cci_param<unsigned int> p_its_count;
    cci::cci_param<uint64_t> p_parent_base;
    cci::cci_param<unsigned int> p_parent_bank;
    cci::cci_param<unsigned int> p_parent_critical_record;
    cci::cci_param<unsigned int> p_parent_non_critical_record;
    tlm_utils::simple_target_socket<zena_device_fmu, DEFAULT_TLM_BUSWIDTH> target_socket;
    tlm_utils::simple_target_socket_optional<zena_device_fmu, DEFAULT_TLM_BUSWIDTH> error_socket;
    tlm_utils::simple_initiator_socket_optional<zena_device_fmu, DEFAULT_TLM_BUSWIDTH> fault_socket;
    InitiatorSignalSocket<bool> critical_irq;
    InitiatorSignalSocket<bool> non_critical_irq;
    TargetSignalSocket<bool> reset_in;

    explicit zena_device_fmu(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_mhu_profile("mhu_profile", false)
        , p_clock_period_ns("clock_period_ns", 0)
        , p_gci_count("gci_count", 1)
        , p_spi_collator_count("spi_collator_count", 1)
        , p_its_count("its_count", 1)
        , p_parent_base("parent_base", 0x2a510000)
        , p_parent_bank("parent_bank", 4)
        , p_parent_critical_record("parent_critical_record", 0)
        , p_parent_non_critical_record("parent_non_critical_record", 1)
        , target_socket("target_socket")
        , error_socket("error_socket")
        , fault_socket("fault_socket")
        , critical_irq("critical_irq")
        , non_critical_irq("non_critical_irq")
        , reset_in("reset_in")
    {
        target_socket.register_b_transport(this, &zena_device_fmu::b_transport);
        target_socket.register_transport_dbg(this, &zena_device_fmu::transport_dbg);
        error_socket.register_b_transport(this, &zena_device_fmu::error_transport);
        reset_in.register_value_changed_cb([this](bool value) { if (value) reset(); });
        SC_METHOD(process_responses);
        sensitive << m_response_event;
        dont_initialize();
        reset();
    }

    void reset()
    {
        m_response_event.cancel();
        m_responses.clear();
        m_command_id = 0;
        for (auto& record : m_records) {
            record = Record{};
        }
        for (unsigned int type = 0; type < 6; ++type) {
            unsigned int count = type < types() ? 1 : 0;
            if (!p_mhu_profile.get_value()) {
                if (type == 2) count = p_spi_collator_count.get_value();
                if (type == 3) count = p_gci_count.get_value();
                if (type == 4) count = p_its_count.get_value();
            }
            if (count > 4096) {
                SC_REPORT_FATAL(name(), "FMU block count exceeds the 12-bit BLKID");
            }
            m_blocks[type].assign(count, Block{});
            for (auto& block : m_blocks[type]) {
                for (unsigned int id = 1; id < 255; ++id) {
                    block.mechanism[id].critical = !corrected(type, id);
                }
            }
        }
        m_status = m_write_data = m_read_data = m_fctlr = 0;
        m_timeout = ~0u;
        m_unlocked = false;
        update_irqs();
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const auto period = p_clock_period_ns.get_value();
        const auto process = sc_core::sc_get_current_process_handle();
        const bool can_wait = process.valid() && process.proc_kind() == sc_core::SC_THREAD_PROC_;
        if (period != 0) {
            if (delay != sc_core::SC_ZERO_TIME) {
                if (!can_wait) {
                    trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                    return;
                }
                // Observe timed MMIO at its arrival, not at the CPU's earlier global time.
                sc_core::wait(delay);
                delay = sc_core::SC_ZERO_TIME;
            }
            process_responses();
        }
        m_access_time = sc_core::sc_time_stamp() + delay;
        if (access(trans) && period != 0 && can_wait && trans.is_read() &&
            trans.get_address() == 0xf1c && trans.get_data_length() == 4) {
            uint32_t sampled_status;
            std::memcpy(&sampled_status, trans.get_data_ptr(), sizeof(sampled_status));
            if (sampled_status & 1u) {
                // A zero-delay polling loop must allow timed completion to run.
                sc_core::wait(sc_core::sc_time(double(period), sc_core::SC_NS));
            }
        }
    }

    bool runtime_set_block_response(unsigned int type, unsigned int block,
                                    unsigned int cycles, bool powered)
    {
        if (!valid_block(type, block) || cycles == 0) return false;
        m_blocks[type][block].response_cycles = cycles;
        m_blocks[type][block].powered = powered;
        return true;
    }

    bool runtime_set_error_wire(unsigned int type, unsigned int block,
                                bool critical, bool level)
    {
        if (!valid_block(type, block)) return false;
        const unsigned int index = type * 2 + (critical ? 0 : 1);
        auto& input = m_blocks[type][block].external_wire[index & 1];
        if (!input && level && (m_records[index].control & 3) == 3) {
            m_records[index].wire_only = true;
        }
        input = level;
        update_irqs();
        return true;
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        m_access_time = sc_core::sc_time_stamp();
        return access(trans) ? trans.get_data_length() : 0;
    }

    void error_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        trans.set_dmi_allowed(false);
        if (!trans.is_write() ||
            (trans.get_address() != 0 && trans.get_address() != 4 && trans.get_address() != 8) ||
            trans.get_data_length() != 4 || !trans.get_data_ptr() ||
            trans.get_streaming_width() < 4 || trans.get_byte_enable_ptr()) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        uint32_t value;
        std::memcpy(&value, trans.get_data_ptr(), 4);
        const unsigned int type = (value >> 28) & 7;
        const unsigned int block = (value >> 16) & 0xfff;
        const unsigned int id = (value >> 8) & 0xff;
        if (trans.get_address() == 8) {
            trans.set_response_status(runtime_set_error_wire(type, block, value & 2, value & 1) ?
                                      tlm::TLM_OK_RESPONSE : tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (!valid_block(type, block) || !id || id > mechanism_limit(type)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        record_fault(type, block, id, false, trans.get_address() == 0);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

extern "C" void module_register();
