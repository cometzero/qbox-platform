/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <gic720ae_ras.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm-extensions/request-context.h>
#include <tlm-extensions/qemu-cpu-hint.h>
#include <tlm-extensions/qemu-memtx-attrs.h>
#include <tlm-extensions/qemu-mr-hint.h>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class gicx00_multiview : public sc_core::sc_module
{
    static constexpr uint64_t DIST_BYTES = 0x10000;
    static constexpr uint64_t DIST_FRAME_BYTES = 0x80000;
    static constexpr uint64_t REDIST_BYTES = 0x20000;
    static constexpr uint64_t REDIST_FRAME_BYTES = 0x40000;
    static constexpr unsigned int REDIST_COUNT = 16;
    static constexpr uint64_t DEFAULT_BACKEND_DIST_BASE = 0x20800000;
    static constexpr uint64_t DEFAULT_BACKEND_REDIST_BASE = 0x20880000;
    static constexpr uint64_t DEFAULT_BACKEND_REDIST_STRIDE = 0x40000;
    static constexpr unsigned int DEFAULT_BACKEND_REDIST_COUNT = 4;
    static constexpr unsigned int DEFAULT_SPI_COUNT = 960;
    static constexpr uint32_t SPI_BASE_INTID = 32;
    static constexpr uint32_t SPI_LIMIT_INTID = 992;
    static constexpr uint32_t GICD_CTLR = 0x0000;
    static constexpr uint32_t GICD_CTLR_GROUP_ENABLE_MASK = 0x7;
    static constexpr uint32_t GICD_CFGID = 0xf000;
    static constexpr uint32_t GICD_IVIEWR_BASE = 0xf600;
    static constexpr uint32_t GICD_IVIEWR_LIMIT = 0xfa00;
    static constexpr uint32_t GICD_IVIEWR_FIRST = 2;
    static constexpr uint32_t GICD_IVIEWR_LAST = 61;
    static constexpr uint64_t GICD_CFGID_VIEW = 1ull << 53;

    static constexpr uint32_t GICR_PWRR = 0x0024;
    static constexpr uint32_t GICR_VIEWR = 0x002c;
    static constexpr uint32_t GICR_FLUSHR = 0x0030;
    static constexpr uint32_t GICR_IIDR = 0x0004;
    static constexpr uint32_t GICR_TYPER = 0x0008;
    static constexpr uint32_t GICR_IDREGS = 0xffd0;
    static constexpr uint32_t GICR_VIEWR_MASK = 0x3;
    static constexpr uint64_t GICR_TYPER_PLPIS = 1ull << 0;
    static constexpr uint64_t GICR_TYPER_VLPIS = 1ull << 1;
    static constexpr uint64_t GICR_TYPER_DIRTY = 1ull << 2;
    static constexpr uint64_t GICR_TYPER_DIRECTLPI = 1ull << 3;
    static constexpr uint64_t GICR_TYPER_LAST = 1ull << 4;
    static constexpr uint64_t GICR_TYPER_RVPEID = 1ull << 7;
    static constexpr uint64_t GICR_TYPER_COMMONLPIAFF = 1ull << 24;

    using target_socket_t =
        tlm_utils::simple_target_socket_b<
            gicx00_multiview, DEFAULT_TLM_BUSWIDTH,
            tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND>;
    using initiator_socket_t =
        tlm_utils::simple_initiator_socket_b<
            gicx00_multiview, DEFAULT_TLM_BUSWIDTH,
            tlm::tlm_base_protocol_types, sc_core::SC_ZERO_OR_MORE_BOUND>;

    std::array<uint32_t, GICD_IVIEWR_LAST + 1> m_iviewr {};
    std::array<uint32_t, REDIST_COUNT> m_viewr {};
    std::array<uint32_t, 3> m_ctlr_view {};
    std::array<uint32_t, 3> m_dprir {};
    // Wire levels are retained only to make ownership mux handoff coherent;
    // architectural pending/active/enable state remains in the backend GIC.
    std::array<std::vector<bool>, 2> m_injection_levels;
    std::array<uint32_t, 32> m_spi_error {};
    std::array<uint32_t, 32> m_spi_group_error {};
    std::array<uint32_t, 32> m_spi_view_error {};
    std::array<uint32_t, REDIST_COUNT> m_ppi_error {};
    std::array<uint32_t, REDIST_COUNT> m_sgi_distribution_error {};
    std::array<uint64_t, 3> m_sgidr {};
    std::array<uint32_t, REDIST_COUNT> m_redist_fctlr {};
    uint32_t m_dist_fctlr = 0;
    unsigned int m_trace_count = 0;
    bool m_reset_asserted = false;
    bool m_policy_dirty = true;
    gic720ae_ras m_ras;
    std::array<uint32_t, 2> m_ras_asserted {};

    static bool is_supported_length(unsigned int len)
    {
        return len == 1 || len == 2 || len == 4 || len == 8;
    }

    static uint32_t redist_affinity(unsigned int cpu)
    {
        return ((cpu / 4) << 16) | ((cpu % 4) << 8);
    }

    static uint64_t redist_typer(unsigned int cpu)
    {
        uint64_t value = static_cast<uint64_t>(redist_affinity(cpu)) << 32;

        value |= GICR_TYPER_COMMONLPIAFF |
                 (static_cast<uint64_t>(cpu) << 8) |
                 GICR_TYPER_PLPIS | GICR_TYPER_VLPIS |
                 GICR_TYPER_DIRTY | GICR_TYPER_DIRECTLPI |
                 GICR_TYPER_RVPEID;
        if (cpu == REDIST_COUNT - 1) {
            value |= GICR_TYPER_LAST;
        }
        return value;
    }

    void reset_registers()
    {
        m_ras.reset();
        update_ras_irqs();
        m_iviewr.fill(0);
        m_viewr.fill(0);
        m_ctlr_view.fill(0);
        m_dprir.fill(0);
        m_policy_dirty = true;
        m_spi_error.fill(0);
        m_spi_group_error.fill(0);
        m_spi_view_error.fill(0);
        m_ppi_error.fill(0);
        m_sgi_distribution_error.fill(0);
        m_sgidr.fill(0);
        m_redist_fctlr.fill(0);
        m_dist_fctlr = 0;
    }

    void reset_model()
    {
        reset_registers();
        for (auto& levels : m_injection_levels) {
            std::fill(levels.begin(), levels.end(), false);
        }
        for (auto& output : spi_out) {
            if (output.size() != 0) {
                output->write(false);
            }
        }
    }

    void trace_access(const char* region, unsigned int index,
                      tlm::tlm_generic_payload& trans, uint64_t offset,
                      unsigned int len, bool debug)
    {
        if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value()) {
            return;
        }

        ++m_trace_count;
        uint64_t value = 0;
        if (len <= sizeof(value)) {
            std::memcpy(&value, trans.get_data_ptr(), len);
        }

        std::cerr << name() << " " << region;
        if (index != UINT32_MAX) {
            std::cerr << "[" << index << "]";
        }
        std::cerr << " " << (debug ? "dbg_" : "")
                  << (trans.get_command() == tlm::TLM_READ_COMMAND ? "read" : "write")
                  << " offset=0x" << std::hex << offset
                  << " len=0x" << len
                  << " value=0x" << value
                  << std::dec << std::endl;
    }

    bool access_reserved(const char* region, unsigned int index,
                         uint64_t frame_bytes,
                         tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || !is_supported_length(len) ||
            offset >= frame_bytes || len > frame_bytes - offset ||
            (offset % len) != 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memset(data, 0, len);
        } else if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access(region, index, trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    template <size_t N>
    bool access_array(std::array<uint8_t, N>& regs, const char* region,
                      unsigned int index, tlm::tlm_generic_payload& trans,
                      bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || !is_supported_length(len) ||
            offset > regs.size() || len > regs.size() - offset ||
            (offset % len) != 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &regs[offset], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&regs[offset], data, len);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access(region, index, trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_synthetic_redist(unsigned int index,
                                 tlm::tlm_generic_payload& trans,
                                 bool debug,
                                 bool terminate_region = false)
    {
        static constexpr std::array<uint8_t, 12> redist_ids {{
            0x44, 0x00, 0x00, 0x00, 0x93, 0xb4,
            0x4b, 0x00, 0x0d, 0xf0, 0x05, 0xb1,
        }};
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (index >= REDIST_COUNT || data == nullptr ||
            !is_supported_length(len) || offset >= REDIST_BYTES ||
            len > REDIST_BYTES - offset || (offset % len) != 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memset(data, 0, len);
            const uint64_t end = offset + len;
            const auto copy_overlap =
                [offset, end, data](uint64_t reg_offset,
                                    const void* value, size_t value_size) {
                    const uint64_t first =
                        std::max<uint64_t>(offset, reg_offset);
                    const uint64_t last =
                        std::min<uint64_t>(end, reg_offset + value_size);
                    if (first < last) {
                        std::memcpy(
                            data + first - offset,
                            static_cast<const uint8_t*>(value) +
                                first - reg_offset,
                            last - first);
                    }
                };
            const uint32_t iidr = 0x43b;
            const uint64_t typer = redist_typer(index) |
                (terminate_region ? GICR_TYPER_LAST : 0);
            copy_overlap(GICR_IIDR, &iidr, sizeof(iidr));
            copy_overlap(GICR_TYPER, &typer, sizeof(typer));
            for (unsigned int id = 0; id < redist_ids.size(); ++id) {
                const uint32_t value = redist_ids[id];
                copy_overlap(
                    GICR_IDREGS + id * sizeof(uint32_t),
                    &value, sizeof(value));
            }
        } else if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access("redist-discovery", index, trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool validate_backend_access(tlm::tlm_generic_payload& trans,
                                 uint64_t aperture_bytes)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();

        if (trans.get_data_ptr() == nullptr || !is_supported_length(len) ||
            offset >= aperture_bytes || len > aperture_bytes - offset ||
            (offset % len) != 0) {
            if (!(m_ras.records[0].control & (1ull << 38))) {
                const uint32_t data = (trans.is_read() ? 1u << 12 : 0) |
                    (trans.get_byte_enable_ptr() ? 1u << 11 : 0) | (len & 0xff);
                m_ras.report(0, data, false, 0x0e, offset);
                update_ras_irqs();
            }
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (trans.get_command() != tlm::TLM_READ_COMMAND &&
            trans.get_command() != tlm::TLM_WRITE_COMMAND) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        return true;
    }

    bool forward_backend(const char* region, unsigned int index,
                         tlm::tlm_generic_payload& trans,
                         sc_core::sc_time& delay, uint64_t address,
                         bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();

        if (backend_socket.size() == 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (!sync_backend_policy(trans, delay, debug)) return false;
        std::array<uint8_t, 8> original_data {};
        if (trans.is_write()) {
            std::memcpy(original_data.data(), trans.get_data_ptr(), len);
            uint64_t register_offset = address - p_backend_dist_base.get_value();
            if (p_backend_view2_dist_base.get_value() &&
                address >= p_backend_view2_dist_base.get_value() &&
                address < p_backend_view2_dist_base.get_value() + DIST_BYTES)
                register_offset = address - p_backend_view2_dist_base.get_value();
            uint32_t* errors = m_spi_error.data();
            uint64_t enable_base = 0x100;
            if (index < REDIST_COUNT) {
                register_offset = address - p_backend_redist_base.get_value() -
                    index * p_backend_redist_stride.get_value();
                errors = &m_ppi_error[index];
                enable_base = 0x10100;
            }
            const uint64_t words_bytes = index < REDIST_COUNT ? 4 : 0x80;
            const bool is_set = register_offset >= enable_base &&
                register_offset + len <= enable_base + words_bytes;
            const bool is_clear = register_offset >= enable_base + 0x80 &&
                register_offset + len <= enable_base + 0x80 + words_bytes;
            if (is_set || is_clear) {
                const uint64_t first_bit = (register_offset - enable_base -
                    (is_clear ? 0x80 : 0)) * 8;
                for (unsigned int bit = 0; bit < len * 8; ++bit) {
                    const unsigned int word = (first_bit + bit) / 32;
                    const uint32_t mask = 1u << ((first_bit + bit) % 32);
                    const uint32_t blocked = index < REDIST_COUNT ? errors[word] | m_sgi_distribution_error[index] : spi_contained(word);
                    if ((blocked & mask) && (original_data[bit / 8] & (1u << (bit % 8)))) {
                        trans.get_data_ptr()[bit / 8] &= ~(1u << (bit % 8));
                    }
                }
            }
        }
        trans.set_address(address);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        if (debug) {
            const unsigned int transferred =
                backend_socket->transport_dbg(trans);
            if (transferred == len &&
                trans.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE) {
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
            } else if (transferred != len &&
                       trans.get_response_status() ==
                           tlm::TLM_INCOMPLETE_RESPONSE) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            }
        } else {
            backend_socket->b_transport(trans, delay);
        }
        // A QEMU target hint would install a raw MemoryRegion alias in the CPU
        // and bypass this model's view and error containment on later accesses.
        if (auto* hint = trans.get_extension<QemuMrHintTlmExtension>()) {
            // release_extension() defers removal on managed payloads. The
            // transport bridge allocated this hint, so detach and free it now.
            trans.clear_extension(hint);
            hint->free();
        }
        trans.set_dmi_allowed(false);
        trans.set_address(offset);
        if (trans.is_write()) {
            std::memcpy(trans.get_data_ptr(), original_data.data(), len);
        }

        if (!trans.is_response_ok()) {
            return false;
        }
        trace_access(region, index, trans, offset, len, debug);
        return true;
    }

    bool is_supported_iviewr(uint64_t offset, unsigned int len) const
    {
        if (offset < GICD_IVIEWR_BASE || offset >= GICD_IVIEWR_LIMIT ||
            len != sizeof(uint32_t) ||
            ((offset - GICD_IVIEWR_BASE) % sizeof(uint32_t)) != 0) {
            return false;
        }

        const uint32_t index =
            static_cast<uint32_t>((offset - GICD_IVIEWR_BASE) / sizeof(uint32_t));
        return index >= GICD_IVIEWR_FIRST && index <= GICD_IVIEWR_LAST;
    }

    bool is_iviewr_window(uint64_t offset) const
    {
        return offset >= GICD_IVIEWR_BASE && offset < GICD_IVIEWR_LIMIT;
    }

    uint32_t owner_for_intid(uint32_t intid) const
    {
        if (intid < SPI_BASE_INTID || intid >= SPI_LIMIT_INTID) {
            return 0;
        }
        if (m_spi_view_error[intid / 32] & (1u << (intid % 32))) return 0;
        const uint32_t index = intid / 16;
        const uint32_t shift = (intid % 16) * 2;
        return (m_iviewr[index] >> shift) & 0x3u;
    }

    void refresh_spi_owner(uint32_t intid)
    {
        if (intid < SPI_BASE_INTID) {
            return;
        }
        const unsigned int index = intid - SPI_BASE_INTID;
        if (index >= spi_out.size() || spi_out[index].size() == 0) {
            return;
        }

        const uint32_t owner = owner_for_intid(intid);
        const bool level = owner >= 1 && owner <= 2 &&
            !(spi_contained(intid / 32) & (1u << (intid % 32))) ?
            m_injection_levels[owner - 1][index] : false;
        spi_out[index]->write(level);
    }

    bool access_iviewr(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();

        if (data == nullptr || len != sizeof(uint32_t) ||
            (offset % sizeof(uint32_t)) != 0 ||
            !is_iviewr_window(offset)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (!is_supported_iviewr(offset, len)) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::memset(data, 0, len);
            } else if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
                trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                return false;
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }

        const uint32_t index =
            static_cast<uint32_t>((offset - GICD_IVIEWR_BASE) /
                                  sizeof(uint32_t));
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_iviewr[index], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            const uint32_t previous = m_iviewr[index];
            std::memcpy(&m_iviewr[index], data, len);
            m_policy_dirty = true;
            if (previous != m_iviewr[index]) {
                for (uint32_t field = 0; field < 16; ++field) {
                    refresh_spi_owner(index * 16 + field);
                }
            }
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        trace_access("dist-iviewr", UINT32_MAX, trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_cfgid(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (data == nullptr || !is_supported_length(len) ||
            offset < GICD_CFGID ||
            len > GICD_CFGID + sizeof(uint64_t) - offset ||
            (offset % len) != 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            const uint64_t value = GICD_CFGID_VIEW;
            std::memcpy(
                data,
                reinterpret_cast<const uint8_t*>(&value) +
                    offset - GICD_CFGID,
                len);
        } else if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trace_access("dist-cfgid", UINT32_MAX, trans, offset, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_ctlr(unsigned int view, tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay, bool debug)
    {
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (view >= m_ctlr_view.size() || trans.get_address() != GICD_CTLR ||
            data == nullptr || len != sizeof(uint32_t)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        if (p_backend_view2_dist_base.get_value())
            return access_split_ctlr(view, trans, delay, debug);

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            if (backend_socket.size() == 0) {
                if (view == 0) {
                    std::memset(data, 0, len);
                } else {
                    std::memcpy(data, &m_ctlr_view[view], len);
                }
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                return true;
            }
            if (!forward_backend(
                    "dist-ctlr", UINT32_MAX, trans, delay,
                    p_backend_dist_base.get_value() + GICD_CTLR, debug)) {
                return false;
            }
            if (view != 0) {
                uint32_t value = 0;
                std::memcpy(&value, data, len);
                value &= ~GICD_CTLR_GROUP_ENABLE_MASK;
                value |= (m_ctlr_view[0] & m_ctlr_view[view]) &
                    GICD_CTLR_GROUP_ENABLE_MASK;
                std::memcpy(data, &value, len);
            }
            return true;
        }
        if (trans.get_command() != tlm::TLM_WRITE_COMMAND) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }

        uint32_t requested = 0;
        std::memcpy(&requested, data, len);
        m_ctlr_view[view] = requested;
        m_policy_dirty = true;
        if (backend_socket.size() == 0) {
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }

        trans.set_command(tlm::TLM_READ_COMMAND);
        bool success = forward_backend(
            "dist-ctlr", UINT32_MAX, trans, delay,
            p_backend_dist_base.get_value() + GICD_CTLR, debug);
        if (success) {
            uint32_t backend_value = 0;
            std::memcpy(&backend_value, data, len);
            const uint32_t group_enable =
                (m_ctlr_view[0] &
                 (m_ctlr_view[1] | m_ctlr_view[2])) &
                GICD_CTLR_GROUP_ENABLE_MASK;
            backend_value |= requested & ~GICD_CTLR_GROUP_ENABLE_MASK;
            backend_value &= ~GICD_CTLR_GROUP_ENABLE_MASK;
            backend_value |= group_enable;
            std::memcpy(data, &backend_value, len);
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            success = forward_backend(
                "dist-ctlr", UINT32_MAX, trans, delay,
                p_backend_dist_base.get_value() + GICD_CTLR, debug);
        }
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        std::memcpy(data, &requested, len);
        return success;
    }

    bool access_viewr(unsigned int index, tlm::tlm_generic_payload& trans,
                      bool debug)
    {
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (index >= m_viewr.size() || data == nullptr ||
            len != sizeof(uint32_t) || trans.get_address() != GICR_VIEWR) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, &m_viewr[index], len);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            uint32_t value = 0;
            std::memcpy(&value, data, len);
            m_viewr[index] = value & GICR_VIEWR_MASK;
            m_policy_dirty = true;
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return false;
        }
        trace_access("redist-viewr", index, trans, GICR_VIEWR, len, debug);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    struct DistFieldPolicy {
        bool recognized = false;
        bool replacement = false;
        uint32_t first_intid = 0;
        unsigned int bits_per_intid = 0;
    };

    struct FieldOwnership {
        bool any_owned = false;
        bool all_owned = true;
    };

    static FieldOwnership mask_fields(
        unsigned int view, uint8_t* data, unsigned int len,
        const DistFieldPolicy& policy, const gicx00_multiview& self)
    {
        FieldOwnership ownership;
        const unsigned int fields =
            (len * 8) / policy.bits_per_intid;
        for (unsigned int field = 0; field < fields; ++field) {
            const bool owned =
                self.owner_for_intid(policy.first_intid + field) == view;
            ownership.any_owned |= owned;
            ownership.all_owned &= owned;
            if (owned) {
                continue;
            }
            for (unsigned int bit = 0;
                 bit < policy.bits_per_intid; ++bit) {
                const unsigned int data_bit =
                    field * policy.bits_per_intid + bit;
                data[data_bit / 8] &=
                    static_cast<uint8_t>(~(1u << (data_bit % 8)));
            }
        }
        return ownership;
    }

    static void merge_owned_fields(
        unsigned int view, uint8_t* destination, const uint8_t* source,
        unsigned int len, const DistFieldPolicy& policy,
        const gicx00_multiview& self)
    {
        const unsigned int fields =
            (len * 8) / policy.bits_per_intid;
        for (unsigned int field = 0; field < fields; ++field) {
            if (self.owner_for_intid(policy.first_intid + field) != view) {
                continue;
            }
            for (unsigned int bit = 0;
                 bit < policy.bits_per_intid; ++bit) {
                const unsigned int data_bit =
                    field * policy.bits_per_intid + bit;
                const uint8_t mask =
                    static_cast<uint8_t>(1u << (data_bit % 8));
                destination[data_bit / 8] =
                    static_cast<uint8_t>(
                        (destination[data_bit / 8] & ~mask) |
                        (source[data_bit / 8] & mask));
            }
        }
    }

    DistFieldPolicy classify_dist_fields(
        uint64_t offset, unsigned int len) const
    {
        struct BitmapRange {
            uint64_t base;
            uint64_t limit;
        };
        static constexpr std::array<BitmapRange, 2> replacement_bitmaps {{
            { 0x0080, 0x0100 },
            { 0x0d00, 0x0d80 },
        }};
        static constexpr std::array<BitmapRange, 6> command_bitmaps {{
            { 0x0100, 0x0180 },
            { 0x0180, 0x0200 },
            { 0x0200, 0x0280 },
            { 0x0280, 0x0300 },
            { 0x0300, 0x0380 },
            { 0x0380, 0x0400 },
        }};

        for (const auto& range : replacement_bitmaps) {
            if (offset >= range.base && offset + len <= range.limit) {
                return {
                    true, true,
                    static_cast<uint32_t>((offset - range.base) * 8), 1
                };
            }
        }
        for (const auto& range : command_bitmaps) {
            if (offset >= range.base && offset + len <= range.limit) {
                return {
                    true, false,
                    static_cast<uint32_t>((offset - range.base) * 8), 1
                };
            }
        }
        if (offset >= 0x0400 && offset + len <= 0x0c00) {
            const uint64_t base = offset < 0x0800 ? 0x0400 : 0x0800;
            return {
                true, true, static_cast<uint32_t>(offset - base), 8
            };
        }
        if (offset >= 0x0c00 && offset + len <= 0x0d00) {
            return {
                true, true,
                static_cast<uint32_t>((offset - 0x0c00) * 4), 2
            };
        }
        if (offset >= 0x0e00 && offset + len <= 0x0f00) {
            return {
                true, true,
                static_cast<uint32_t>((offset - 0x0e00) * 4), 2
            };
        }
        if (offset >= 0x6000 && offset + len <= 0x7f00 &&
            (offset % sizeof(uint64_t)) == 0 &&
            (len % sizeof(uint64_t)) == 0) {
            return {
                true, true,
                static_cast<uint32_t>((offset - 0x6000) / 8), 64
            };
        }
        return {};
    }

    static bool secure_access(tlm::tlm_generic_payload& trans)
    {
        RequestContextTlmExtension* extension = nullptr;
        trans.get_extension(extension);
        return !extension || !extension->get_context().secure_valid ||
               extension->get_context().secure;
    }

    uint32_t spi_contained(unsigned int word) const
    {
        return m_spi_error[word] | m_spi_group_error[word] | m_spi_view_error[word];
    }

    bool backend_word(tlm::tlm_generic_payload& original, sc_core::sc_time& delay,
                      bool debug, uint64_t address, uint32_t& value,
                      tlm::tlm_command command)
    {
        if (!backend_socket.size()) {
            if (command == tlm::TLM_READ_COMMAND) value = 0;
            return true;
        }
        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(command);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        RequestContextTlmExtension* extension = nullptr;
        original.get_extension(extension);
        if (extension) trans.set_extension(extension);
        auto* attrs = original.get_extension<QemuMemTxAttrsTlmExtension>();
        auto* cpu = original.get_extension<QemuCpuHintTlmExtension>();
        if (attrs) trans.set_extension(attrs);
        if (cpu) trans.set_extension(cpu);
        if (debug) {
            const unsigned int length = backend_socket->transport_dbg(trans);
            if (length != 4) trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        } else {
            backend_socket->b_transport(trans, delay);
        }
        if (extension) trans.clear_extension(extension);
        if (attrs) trans.clear_extension(attrs);
        if (cpu) trans.clear_extension(cpu);
        if (!trans.is_response_ok()) {
            original.set_response_status(trans.get_response_status());
            return false;
        }
        return true;
    }

    bool sync_backend_policy(tlm::tlm_generic_payload& original,
                             sc_core::sc_time& delay, bool debug)
    {
        if (!p_backend_multi_view.get_value() || !m_policy_dirty ||
            !backend_socket.size()) return true;
        // This is synchronization of trusted configuration already accepted by
        // the front end, not an elevation of the triggering guest transaction.
        // It also replays a full-system reset before the first backend access.
        tlm::tlm_generic_payload config;
        qemu::MemoryRegionOps::MemTxAttrs attrs {};
        attrs.secure = true;
        QemuMemTxAttrsTlmExtension extension(attrs);
        config.set_extension(&extension);
        bool success = true;
        for (unsigned int index = 0; success &&
             index < std::min(p_backend_redist_count.get_value(), REDIST_COUNT); ++index) {
            uint32_t value = m_viewr[index];
            success = backend_word(config, delay, debug,
                p_backend_redist_base.get_value() + index * p_backend_redist_stride.get_value() +
                GICR_VIEWR, value, tlm::TLM_WRITE_COMMAND);
        }
        for (unsigned int index = GICD_IVIEWR_FIRST;
             success && index <= GICD_IVIEWR_LAST; ++index) {
            uint32_t value = m_iviewr[index];
            // An errored view routes nowhere until control firmware clears and
            // reprograms IVIEWR. The backend must observe the same containment.
            for (unsigned int bit = 0; bit < 16; ++bit) {
                if (m_spi_view_error[index / 2] & (1u << ((index % 2) * 16 + bit)))
                    value &= ~(3u << (bit * 2));
            }
            success = backend_word(config, delay, debug,
                p_backend_dist_base.get_value() + GICD_IVIEWR_BASE + index * 4,
                value, tlm::TLM_WRITE_COMMAND);
            if (success && p_backend_view2_dist_base.get_value())
                success = backend_word(config, delay, debug,
                    dist_base(2) + GICD_IVIEWR_BASE + index * 4,
                    value, tlm::TLM_WRITE_COMMAND);
        }
        for (unsigned int view = 1; success && view <= 2; ++view) {
            uint32_t value = m_ctlr_view[0] & m_ctlr_view[view] & 7u;
            success = backend_word(config, delay, debug,
                dist_base(view) + view * 0x80000, value, tlm::TLM_WRITE_COMMAND);
        }
        config.clear_extension(&extension);
        if (!success) original.set_response_status(config.get_response_status());
        else m_policy_dirty = false;
        return success;
    }

    bool replace_backend_bits(tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay, bool debug,
                               uint64_t address, uint32_t mask, uint32_t bits)
    {
        if (!mask) return true;
        uint32_t value = 0;
        if (!backend_word(trans, delay, debug, address, value,
                          tlm::TLM_READ_COMMAND)) return false;
        value = (value & ~mask) | (bits & mask);
        return backend_word(trans, delay, debug, address, value,
                            tlm::TLM_WRITE_COMMAND);
    }

    bool spi_fault_defaults_one(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay, bool debug,
                             unsigned int word, uint32_t group_mask,
                             uint32_t config_mask, bool set_error, uint64_t base)
    {
        if (!replace_backend_bits(trans, delay, debug, base + 0x80 + word * 4,
                                  group_mask, 0) ||
            !replace_backend_bits(trans, delay, debug, base + 0xd00 + word * 4,
                                  group_mask, 0)) return false;
        for (unsigned int half = 0; half < 2; ++half) {
            uint32_t groups = 0;
            uint32_t configs = 0;
            uint32_t nsacr = 0;
            uint32_t trigger = 0;
            for (unsigned int bit = 0; bit < 16; ++bit) {
                const uint32_t mask = 1u << (half * 16 + bit);
                if (group_mask & mask) {
                    groups |= 3u << (bit * 2);
                    if (set_error) nsacr |= ((m_dist_fctlr >> 16) & 3u) << (bit * 2);
                }
                if (config_mask & mask) {
                    configs |= 3u << (bit * 2);
                    if (set_error) trigger |= 2u << (bit * 2);
                }
            }
            if (!replace_backend_bits(trans, delay, debug,
                    base + 0xe00 + word * 8 + half * 4, groups, nsacr) ||
                !replace_backend_bits(trans, delay, debug,
                    base + 0xc00 + word * 8 + half * 4, configs, trigger)) return false;
        }
        if (set_error && config_mask) {
            return backend_word(trans, delay, debug, base + 0x200 + word * 4,
                                config_mask, tlm::TLM_WRITE_COMMAND);
        }
        return true;
    }

    uint64_t dist_base(unsigned int view) const
    {
        return view == 2 && p_backend_view2_dist_base.get_value() ?
            p_backend_view2_dist_base.get_value() : p_backend_dist_base.get_value();
    }

    uint32_t backend_owner_mask(unsigned int word, unsigned int view) const
    {
        if (!p_backend_view2_dist_base.get_value()) return ~0u;
        uint32_t mask = 0;
        for (unsigned int bit = 0; bit < 32; ++bit) {
            const unsigned int id = word * 32 + bit;
            if (id >= SPI_LIMIT_INTID) continue;
            const unsigned int owner = (m_iviewr[id / 16] >> ((id % 16) * 2)) & 3;
            if ((owner == 2 ? 2u : 1u) == view) mask |= 1u << bit;
        }
        return mask;
    }

    bool spi_fault_defaults(tlm::tlm_generic_payload& trans,
                            sc_core::sc_time& delay, bool debug,
                            unsigned int word, uint32_t groups,
                            uint32_t configs, bool set_error)
    {
        const unsigned int last = p_backend_view2_dist_base.get_value() ? 2 : 1;
        for (unsigned int view = 1; view <= last; ++view) {
            const uint32_t mask = backend_owner_mask(word, view);
            if (!spi_fault_defaults_one(trans, delay, debug, word,
                    groups & mask, configs & mask, set_error, dist_base(view))) return false;
        }
        return true;
    }

    bool disable_faulted_spis(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay, bool debug,
                             unsigned int word, uint32_t bits)
    {
        const unsigned int last = p_backend_view2_dist_base.get_value() ? 2 : 1;
        for (unsigned int view = 1; view <= last; ++view) {
            uint32_t value = bits & backend_owner_mask(word, view);
            if (value && !backend_word(trans, delay, debug,
                    dist_base(view) + 0x180 + word * 4,
                    value, tlm::TLM_WRITE_COMMAND)) return false;
        }
        return true;
    }

    bool access_split_dist(tlm::tlm_generic_payload& trans,
                           sc_core::sc_time& delay, bool debug)
    {
        const unsigned int len = trans.get_data_length();
        if (!validate_backend_access(trans, DIST_BYTES)) return false;
        std::array<uint8_t, 8> original {}, merged {};
        if (trans.is_write()) std::memcpy(original.data(), trans.get_data_ptr(), len);
        bool success = true;
        for (unsigned int view = 1; success && view <= 2; ++view) {
            if (trans.is_write()) std::memcpy(trans.get_data_ptr(), original.data(), len);
            success = access_dist_view(view, trans, delay, debug);
            if (success && trans.is_read()) {
                for (unsigned int byte = 0; byte < len; ++byte)
                    merged[byte] |= trans.get_data_ptr()[byte];
            }
        }
        std::memcpy(trans.get_data_ptr(), trans.is_read() ? merged.data() : original.data(), len);
        return success;
    }

    bool access_split_ctlr(unsigned int view, tlm::tlm_generic_payload& trans,
                           sc_core::sc_time& delay, bool debug)
    {
        if (!sync_backend_policy(trans, delay, debug)) return false;
        uint32_t requested = 0;
        if (trans.is_write()) {
            std::memcpy(&requested, trans.get_data_ptr(), 4);
            m_ctlr_view[view] = requested;
        m_policy_dirty = true;
        }
        if (!sync_backend_policy(trans, delay, debug)) return false;
        if (trans.is_read()) {
            uint32_t value = 0;
            if (!backend_word(trans, delay, debug, dist_base(view), value,
                              tlm::TLM_READ_COMMAND)) return false;
            value = (value & ~7u) | (m_ctlr_view[view] & (view ? m_ctlr_view[0] : ~0u) & 7u);
            std::memcpy(trans.get_data_ptr(), &value, 4);
        } else {
            for (unsigned int bank = 1; bank <= 2; ++bank) {
                if (view && view != bank) continue;
                uint32_t value = 0;
                if (!backend_word(trans, delay, debug, dist_base(bank), value,
                                  tlm::TLM_READ_COMMAND)) return false;
                value |= requested & ~7u;
                value = (value & ~7u) | (m_ctlr_view[0] & m_ctlr_view[bank] & 7u);
                if (!backend_word(trans, delay, debug, dist_base(bank), value,
                                  tlm::TLM_WRITE_COMMAND)) return false;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool is_dist_safety(uint64_t offset) const
    {
        return offset == 0x20 || (offset >= 0xe100 && offset < 0xe280) ||
               (offset >= 0xf200 && offset < 0xf280);
    }

    bool access_dist_safety(unsigned int view, tlm::tlm_generic_payload& trans,
                            sc_core::sc_time& delay, bool debug)
    {
        const uint64_t offset = trans.get_address();
        if (!validate_backend_access(trans, DIST_BYTES) ||
            trans.get_data_length() != 4 || trans.get_byte_enable_ptr() ||
            trans.get_streaming_width() < 4) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        uint32_t value = 0;
        if (trans.is_write()) std::memcpy(&value, trans.get_data_ptr(), 4);
        const bool secure = secure_access(trans);
        if (offset == 0x20) {
            if (view == 0 && secure) {
                if (trans.is_write()) {
                    // No modeled ECC RAM requires a scrub, so SIP completes synchronously.
                    m_dist_fctlr = value & 0x04f30000;
                }
                value = m_dist_fctlr;
            } else value = 0;
        } else {
            const uint64_t base = offset & ~0x7full;
            const unsigned int word = (offset - base) / 4;
            uint32_t allowed = 0;
            for (unsigned int bit = 0; bit < 32; ++bit) {
                const unsigned int intid = word * 32 + bit;
                if (intid >= 32 && intid < 32 + p_spi_count.get_value() &&
                    intid < SPI_LIMIT_INTID &&
                    (view == 0 || owner_for_intid(intid) == view)) {
                    allowed |= 1u << bit;
                }
            }
            if ((base == 0xf200 && view != 0) || (!secure && base != 0xe100)) {
                allowed = 0;
            }
            if (!secure && allowed) {
                uint32_t group = 0;
                if (!backend_word(trans, delay, debug,
                        dist_base(view) + 0x80 + word * 4,
                        group, tlm::TLM_READ_COMMAND)) return false;
                allowed &= group & ~m_spi_group_error[word] & ~m_spi_view_error[word];
            }
            uint32_t& state = base == 0xe180 ? m_spi_group_error[word] :
                (base == 0xf200 ? m_spi_view_error[word] : m_spi_error[word]);
            if (trans.is_write()) {
                value &= allowed;
                if (base == 0xe200) {
                    if (!disable_faulted_spis(trans, delay, debug, word, value)) return false;
                    if (!spi_fault_defaults(trans, delay, debug, word,
                                            value, value, true)) return false;
                    m_spi_error[word] |= value;
                    m_spi_group_error[word] |= value;
                    if (view == 0) { m_spi_view_error[word] |= value; m_policy_dirty = true; }
                } else {
                    const uint32_t group_mask = base == 0xe180 || base == 0xe100 ?
                        value & m_spi_group_error[word] : 0;
                    const uint32_t config_mask = base == 0xe100 ? value & m_spi_error[word] : 0;
                    if (!spi_fault_defaults(trans, delay, debug, word,
                                            group_mask, config_mask, false)) return false;
                    if (base == 0xf200) {
                        for (unsigned int bit = 0; bit < 32; ++bit) {
                            if (value & state & (1u << bit)) {
                                const unsigned int intid = word * 32 + bit;
                                m_iviewr[intid / 16] &= ~(3u << ((intid % 16) * 2));
                                m_policy_dirty = true;
                            }
                        }
                    }
                    state &= ~value;
                    if (base == 0xe100) m_spi_group_error[word] &= ~value;
                }
                for (unsigned int bit = 0; bit < 32; ++bit) {
                    if (value & (1u << bit)) refresh_spi_owner(word * 32 + bit);
                }
            }
            value = state & allowed;
        }
        if (trans.is_read()) std::memcpy(trans.get_data_ptr(), &value, 4);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool sgi_defaults(unsigned index, uint32_t bits,
                      tlm::tlm_generic_payload& trans, sc_core::sc_time& delay, bool debug)
    {
        const uint64_t base = p_backend_redist_base.get_value() + index * p_backend_redist_stride.get_value();
        const unsigned view = m_viewr[index] <= 2 ? m_viewr[index] : 0;
        uint32_t group = 0, modifier = 0, nsacr = 0, ns_mask = 0;
        for (unsigned bit = 0; bit < 16; ++bit) {
            if (!(bits & (1u << bit))) continue;
            unsigned setting = (m_sgidr[view] >> (bit * 4)) & 7;
            if (setting & 2) group |= 1u << bit;
            if (setting & 4) modifier |= 1u << bit;
            if (setting & 1) nsacr |= 1u << (bit * 2);
            ns_mask |= 3u << (bit * 2);
        }
        return replace_backend_bits(trans, delay, debug, base + 0x10080, bits, group) &&
            replace_backend_bits(trans, delay, debug, base + 0x10d00, bits, modifier) &&
            replace_backend_bits(trans, delay, debug, base + 0x10e00, ns_mask, nsacr);
    }

    bool access_sgi_distribution(unsigned index, unsigned view,
                                 tlm::tlm_generic_payload& trans,
                                 sc_core::sc_time& delay, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned len = trans.get_data_length();
        if ((len != 4 && len != 8) || (offset == 0x1c008 && len != 4) ||
            trans.get_byte_enable_ptr() || trans.get_streaming_width() < len) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (!secure_access(trans))
            return access_reserved("sgi-default-non-secure", index, REDIST_BYTES, trans, debug);
        uint64_t value = 0;
        if (trans.is_write()) std::memcpy(&value, trans.get_data_ptr(), len);
        if (offset == 0x1c008) {
            if (trans.is_write()) {
                const uint32_t bits = value & m_sgi_distribution_error[index];
                if (!sgi_defaults(index, bits, trans, delay, debug)) return false;
                m_sgi_distribution_error[index] &= ~bits;
            }
            value = m_sgi_distribution_error[index];
        } else {
            const unsigned shift = (offset & 4) * 8;
            if (trans.is_write()) {
                const uint64_t mask = (len == 8 ? ~0ull : 0xffffffffull << shift) & 0x7777777777777777ull;
                m_sgidr[view] = (m_sgidr[view] & ~mask) | ((value << shift) & mask);
            }
            value = m_sgidr[view] >> shift;
        }
        if (trans.is_read()) std::memcpy(trans.get_data_ptr(), &value, len);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_redist_safety(unsigned int index, unsigned int view,
                              tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay, bool debug)
    {
        const uint64_t offset = trans.get_address();
        if (trans.get_data_length() != 4 || trans.get_byte_enable_ptr() ||
            trans.get_streaming_width() < 4) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        uint32_t value = 0;
        if (trans.is_write()) std::memcpy(&value, trans.get_data_ptr(), 4);
        if (offset == 0x1c018) {
            // DPRIR is banked by access view. Secure G0/G1S fields are WI for
            // Non-secure callers; the G1NS field remains writable.
            const uint32_t mask = secure_access(trans) ? 0x00f8f8f8 : 0x0000f800;
            if (trans.is_write()) m_dprir[view] = (m_dprir[view] & ~mask) | (value & mask);
            value = m_dprir[view];
        } else if (!secure_access(trans)) value = 0;
        else if (offset == 0x20) {
            if (trans.is_write()) m_redist_fctlr[index] = value & 0x61e;
            value = m_redist_fctlr[index];
        } else {
            const uint64_t base = p_backend_redist_base.get_value() +
                index * p_backend_redist_stride.get_value();
            if (trans.is_write()) {
                if (offset == 0x1c180) {
                    uint32_t disable = value;
                    if (disable && !backend_word(trans, delay, debug,
                            base + 0x10180, disable, tlm::TLM_WRITE_COMMAND)) return false;
                    uint32_t pending = value;
                    if (pending && !backend_word(trans, delay, debug,
                            base + 0x10200, pending, tlm::TLM_WRITE_COMMAND)) return false;
                    m_ppi_error[index] |= value;
                } else {
                    const uint32_t clear = value & m_ppi_error[index];
                    // GCI RAM recovery changes the affected priority only;
                    // the interrupt's group and trigger configuration survive.
                    uint32_t group = 0, modifier = 0;
                    if (!backend_word(trans, delay, debug, base + 0x10080,
                                      group, tlm::TLM_READ_COMMAND) ||
                        !backend_word(trans, delay, debug, base + 0x10d00,
                                      modifier, tlm::TLM_READ_COMMAND)) return false;
                    const unsigned int owner = m_viewr[index];
                    const uint32_t defaults = m_dprir[owner <= 2 ? owner : 0];
                    for (unsigned int first = 0; first < 32; first += 4) {
                        uint32_t mask = 0, priorities = 0;
                        for (unsigned int bit = first; bit < first + 4; ++bit) {
                            if (!(clear & (1u << bit))) continue;
                            const unsigned int shift = (group & (1u << bit)) ? 8 :
                                (modifier & (1u << bit)) ? 16 : 0;
                            const unsigned int lane = (bit - first) * 8;
                            mask |= 0xffu << lane;
                            priorities |= ((defaults >> shift) & 0xf8) << lane;
                        }
                        if (!replace_backend_bits(trans, delay, debug,
                                base + 0x10400 + first, mask, priorities)) return false;
                    }
                    m_ppi_error[index] &= ~value;
                }
            }
            value = m_ppi_error[index];
        }
        if (trans.is_read()) std::memcpy(trans.get_data_ptr(), &value, 4);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_its(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay, bool debug)
    {
        const uint64_t offset = trans.get_address();
        if (!validate_backend_access(trans, 0x40000)) return false;
        // GIC-720AE Issue 0201-12, Register map pages: the fourth page
        // of each v4.1 ITS frame is reserved. The raw QEMU ITS only maps
        // its control/translation pages, so this allocated tail must end
        // here rather than becoming an external abort at the raw backend.
        if (offset >= 0x30000)
            return access_reserved("its-reserved", UINT32_MAX, 0x40000, trans, debug);
        if (offset >= 0xc000 && offset < 0xc018)
            return access_errins(32 + (offset - 0xc000) / 8, 0, trans, debug);
        if (!backend_its_socket.size())
            return access_reserved("its-unbound", UINT32_MAX, 0x40000, trans, debug);
        if (debug) {
            unsigned length = backend_its_socket->transport_dbg(trans);
            if (length != trans.get_data_length()) trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        } else backend_its_socket->b_transport(trans, delay);
        // ITS control/translation shares the policy front end: never export a
        // raw alias that would bypass ERRINS interception on subsequent accesses.
        if (auto* hint = trans.get_extension<QemuMrHintTlmExtension>()) {
            trans.clear_extension(hint);
            hint->free();
        }
        trans.set_dmi_allowed(false);
        return trans.is_response_ok();
    }

    bool ras_record_present(unsigned int record) const
    {
        return record <= 8 || (p_ras_lpi.get_value() && record <= 27);
    }

    void update_ras_irqs()
    {
        std::array<uint32_t, 2> next {};
        for (unsigned kind = 0; kind < 2; ++kind)
            if (m_ras.level(kind)) next[kind] = m_ras.irq_ids[kind];
        auto emit = [this](uint32_t id, bool level) {
            if (id < 32 || id >= 32 + p_spi_count.get_value()) return;
            const uint64_t command = uint64_t(id - 32) | (uint64_t(level) << 32);
            auto& output = owner_for_intid(id) == 2 && p_backend_view2_dist_base.get_value() ?
                internal_spi_view2 : internal_spi;
            if (output.size()) output->write(command);
        };
        for (auto id : m_ras_asserted)
            if (id && id != next[0] && id != next[1]) emit(id, false);
        for (auto id : next)
            if (id && id != m_ras_asserted[0] && id != m_ras_asserted[1]) emit(id, true);
        m_ras_asserted = next;
        if (ras_fault.size()) ras_fault->write(m_ras.level(0));
        if (ras_error.size()) ras_error->write(m_ras.level(1));
    }

    bool access_gict(unsigned view, tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t address = trans.get_address();
        const uint64_t offset = address - 0x20000;
        const unsigned length = trans.get_data_length();
        if (!validate_backend_access(trans, DIST_FRAME_BYTES) ||
            (length != 4 && length != 8) || trans.get_byte_enable_ptr() ||
            trans.get_streaming_width() < length) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (!secure_access(trans) && !(m_ras.sac & 2))
            return access_reserved("gict-non-secure", UINT32_MAX, DIST_FRAME_BYTES, trans, debug);
        uint64_t value = 0;
        if (trans.is_write()) std::memcpy(&value, trans.get_data_ptr(), length);
        const unsigned shift = (offset & 4) * 8;
        const uint64_t mask = length == 8 ? ~0ull : 0xffffffffull << shift;
        value <<= shift;
        const unsigned record = offset / 64;
        if (offset < 64 * 64 && ras_record_present(record) && (!view || record == 0)) {
            const unsigned reg = (offset % 64) & ~7u;
            if (trans.is_write()) m_ras.write(record, reg, value, mask);
            value = m_ras.read(record, reg);
        } else if ((offset & ~7ull) == 0xe000) {
            value = 0;
            for (unsigned rec = 0; rec < 64; ++rec)
                if (ras_record_present(rec) && (!view || rec == 0) &&
                    (m_ras.records[rec].status & gic720ae_ras::V)) value |= 1ull << rec;
        } else if (!view && ((offset & ~7ull) == 0xe800 || (offset & ~7ull) == 0xe808)) {
            unsigned kind = (offset - 0xe800) / 8;
            if (trans.is_write() && (mask & 0xffffffffull)) {
                unsigned id = value & 0x7ff;
                m_ras.irq_ids[kind] = id >= 32 && id < 32 + p_spi_count.get_value() ? id : 0;
            }
            value = m_ras.irq_ids[kind];
        } else if (offset == 0xe100) value = 0x0702143b;
        else value = 0;
        if (trans.is_read()) {
            value >>= shift;
            std::memcpy(trans.get_data_ptr(), &value, length);
        }
        update_ras_irqs();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_errins(unsigned ram, unsigned view, tlm::tlm_generic_payload& trans,
                       bool debug)
    {
        const unsigned length = trans.get_data_length();
        if ((length != 4 && length != 8) || !trans.get_data_ptr() ||
            trans.get_address() % length || trans.get_byte_enable_ptr() ||
            trans.get_streaming_width() < length) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (view || (!secure_access(trans) && !(m_ras.sac & 2)))
            return access_reserved("errins-denied", UINT32_MAX, DIST_FRAME_BYTES, trans, debug);
        const bool present = ram < 4 || (ram >= 16 && ram < 32) ||
            (p_ras_lpi.get_value() && ram != 15);
        unsigned max = ram == 1 || ram == 2 ? (p_spi_count.get_value() + 1) / 2 - 1 :
            ram >= 16 && ram < 32 ? 15 : ram == 0 ? 15 : p_ras_ram_entries.get_value() - 1;
        const unsigned width = p_ras_ram_width.get_value() - 1;
        auto& state = m_ras.rams[ram];
        const unsigned shift = (trans.get_address() & 4) * 8;
        const uint64_t mask = length == 8 ? ~0ull : 0xffffffffull << shift;
        if (trans.is_write()) {
            uint64_t value = 0;
            std::memcpy(&value, trans.get_data_ptr(), length);
            state.request = (state.request & ~mask) | ((value << shift) & mask);
            if (state.request & (1ull << 63)) {
                const unsigned address = (state.request >> 32) & 0xffff;
                const unsigned bit1 = state.request & 0x1ff;
                const unsigned bit2 = (state.request >> 16) & 0x1ff;
                const bool valid1 = state.request & (1ull << 15);
                const bool valid2 = state.request & (1ull << 31);
                state.status = !present || address > max ||
                    (valid1 && bit1 > width) || (valid2 && bit2 > width) ? 1 : 0;
                if (!state.status && (valid1 || valid2) &&
                    !(valid1 && valid2 && bit1 == bit2)) {
                    const bool ue = valid1 && valid2;
                    static const unsigned records[16] = {3, 1, 1, 5, 13, 9, 9, 9, 9, 11, 15, 17, 21, 19, 23, 62};
                    unsigned record = ram < 16 ? records[ram] : ram < 32 ? 7 : 25;
                    uint32_t data = address;
                    const unsigned bit = valid1 ? bit1 : bit2;
                    if (ram == 1 || ram == 2) {
                        unsigned bits = 0, count = p_spi_count.get_value() - 1;
                        while (count) { ++bits; count >>= 1; }
                        data = address * 2 + (ram == 2);
                        if (!ue) data |= bit << bits;
                    } else if (ram >= 16 && ram < 32) {
                        data = ((address & 15) << 8) | (ram - 16);
                        if (!ue) data |= bit << 12;
                    } else if (!ue) data |= bit << 16;
                    m_ras.report(record + ue, data);
                }
                state.request &= ~(1ull << 63);
            }
        } else {
            uint64_t value = (uint64_t(state.status) << 61) |
                (present ? (1ull << 60) | (uint64_t(max) << 32) | width : 0);
            value >>= shift;
            std::memcpy(trans.get_data_ptr(), &value, length);
        }
        update_ras_irqs();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return true;
    }

    bool access_dist(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay, bool debug)
    {
        // GIC-720AE multi-view configuration view is Secure-only, including
        // canonical registers and the narrow platform aliases into this frame.
        if (!secure_access(trans)) {
            return access_reserved("dist-view0-non-secure", UINT32_MAX,
                                   DIST_FRAME_BYTES, trans, debug);
        }
        const uint64_t offset = trans.get_address();
        if (offset >= 0x40000 && offset < 0x80000 && p_ras_lpi.get_value()) {
            trans.set_address(offset - 0x40000);
            const bool success = access_its(trans, delay, debug);
            trans.set_address(offset);
            return success;
        }
        if (offset >= 0x20000 && offset < 0x30000) return access_gict(0, trans, debug);
        if (offset >= 0xea00 && offset < 0xea80)
            return access_errins((offset - 0xea00) / 8, 0, trans, debug);
        if (offset == 0x24 && trans.get_data_length() == 4) {
            if (trans.is_write()) { uint32_t value; std::memcpy(&value, trans.get_data_ptr(), 4); m_ras.sac = value & 2; }
            else std::memcpy(trans.get_data_ptr(), &m_ras.sac, 4);
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }
        if (offset >= DIST_BYTES) {
            return access_reserved("dist-reserved", UINT32_MAX,
                                   DIST_FRAME_BYTES, trans, debug);
        }

        if (is_dist_safety(offset)) return access_dist_safety(0, trans, delay, debug);
        if (offset == GICD_CTLR) {
            return access_ctlr(0, trans, delay, debug);
        }

        if (is_iviewr_window(offset)) {
            const bool success = access_iviewr(trans, debug);
            return success && sync_backend_policy(trans, delay, debug);
        }

        if (offset >= GICD_CFGID &&
            offset < GICD_CFGID + sizeof(uint64_t)) {
            return access_cfgid(trans, debug);
        }

        if (p_backend_view2_dist_base.get_value() &&
            classify_dist_fields(offset, trans.get_data_length()).recognized)
            return access_split_dist(trans, delay, debug);

        if (backend_socket.size() == 0) {
            return access_reserved(
                "dist-unbound", UINT32_MAX, DIST_BYTES, trans, debug);
        }
        if (!validate_backend_access(trans, DIST_BYTES)) {
            return false;
        }
        return forward_backend(
            "dist-backend", UINT32_MAX, trans, delay,
            p_backend_dist_base.get_value() + offset, debug);
    }

    bool access_dist_view(unsigned int view,
                          tlm::tlm_generic_payload& trans,
                          sc_core::sc_time& delay, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (offset >= 0x20000 && offset < 0x30000) return access_gict(view, trans, debug);
        if (offset >= 0xea00 && offset < 0xea80)
            return access_errins((offset - 0xea00) / 8, view, trans, debug);
        if (view < 1 || view > 2 ||
            !validate_backend_access(trans, DIST_BYTES)) {
            if (view < 1 || view > 2) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            }
            return false;
        }

        if (is_dist_safety(offset)) return access_dist_safety(view, trans, delay, debug);
        if (offset == GICD_CTLR) {
            return access_ctlr(view, trans, delay, debug);
        }

        std::array<uint8_t, sizeof(uint64_t)> original {};
        std::array<uint8_t, sizeof(uint64_t)> filtered {};
        const DistFieldPolicy policy = classify_dist_fields(offset, len);
        FieldOwnership ownership;
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(original.data(), data, len);
            std::memcpy(filtered.data(), data, len);
        }
        if (policy.recognized) {
            ownership = mask_fields(
                view, filtered.data(), len, policy, *this);
        } else {
            ownership.any_owned = true;
            ownership.all_owned = true;
        }
        if (policy.recognized && !ownership.any_owned) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::memset(data, 0, len);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }

        if (backend_socket.size() == 0) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::memset(data, 0, len);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return true;
        }

        const char* region = view == 1 ? "dist-view1" : "dist-view2";
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND &&
            policy.replacement && !ownership.all_owned) {
            trans.set_command(tlm::TLM_READ_COMMAND);
            bool success = forward_backend(
                region, UINT32_MAX, trans, delay,
                dist_base(view) + offset, debug);
            if (success) {
                merge_owned_fields(
                    view, data, original.data(), len, policy, *this);
                trans.set_command(tlm::TLM_WRITE_COMMAND);
                success = forward_backend(
                    region, UINT32_MAX, trans, delay,
                    dist_base(view) + offset, debug);
            }
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            std::memcpy(data, original.data(), len);
            return success;
        }

        if (trans.get_command() == tlm::TLM_WRITE_COMMAND &&
            policy.recognized) {
            std::memcpy(data, filtered.data(), len);
        }
        const bool success = forward_backend(
            region, UINT32_MAX, trans, delay,
            dist_base(view) + offset, debug);
        if (success && trans.get_command() == tlm::TLM_READ_COMMAND &&
            policy.recognized) {
            mask_fields(view, data, len, policy, *this);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(data, original.data(), len);
        }
        return success;
    }

    bool access_redist_backend(unsigned int index,
                               tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay, bool debug,
                               const char* region, unsigned int view = 0)
    {
        const uint64_t offset = trans.get_address();
        if (index >= REDIST_COUNT || offset >= REDIST_FRAME_BYTES) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (offset >= REDIST_BYTES) {
            return access_reserved(
                "redist-reserved", index, REDIST_FRAME_BYTES, trans, debug);
        }
        if (!validate_backend_access(trans, REDIST_BYTES)) {
            return false;
        }
        if (offset == 0x1c008 || (offset >= 0x1c010 && offset < 0x1c018))
            return access_sgi_distribution(index, view, trans, delay, debug);
        if (offset >= 0x1f010 && offset < 0x1f018)
            return access_errins(16 + index, view, trans, debug);
        if (offset == 0x20 || offset == 0x1c018 || offset == 0x1c100 || offset == 0x1c180) {
            return access_redist_safety(index, view, trans, delay, debug);
        }
        if (backend_socket.size() == 0) {
            return access_reserved(
                "redist-unbound", index, REDIST_BYTES, trans, debug);
        }
        if (index >= p_backend_redist_count.get_value()) {
            return access_synthetic_redist(index, trans, debug);
        }
        const bool priority_write = trans.is_write() && offset >= 0x10400 &&
            offset + trans.get_data_length() <= 0x10420;
        uint32_t clear = 0;
        if (priority_write) {
            uint32_t allowed = ~0u;
            if (!secure_access(trans) && !backend_word(trans, delay, debug,
                    p_backend_redist_base.get_value() +
                        index * p_backend_redist_stride.get_value() + 0x10080,
                    allowed, tlm::TLM_READ_COMMAND)) return false;
            for (unsigned int byte = 0; byte < trans.get_data_length(); ++byte) {
                const auto* enables = trans.get_byte_enable_ptr();
                if (!enables || (trans.get_byte_enable_length() &&
                    enables[byte % trans.get_byte_enable_length()] != TLM_BYTE_DISABLED)) {
                    clear |= (1u << (offset - 0x10400 + byte)) & allowed;
                }
            }
        }
        const bool success = forward_backend(
            region, index, trans, delay,
            p_backend_redist_base.get_value() +
                (index * p_backend_redist_stride.get_value()) + offset,
            debug);
        if (success && trans.is_write() && m_sgi_distribution_error[index] &&
            (offset == 0x10080 || offset == 0x10d00 || offset == 0x10e00))
            return sgi_defaults(index, m_sgi_distribution_error[index], trans, delay, debug);
        if (success && priority_write) m_ppi_error[index] &= ~clear;
        return success;
    }

    bool access_redist(unsigned int index, tlm::tlm_generic_payload& trans,
                       sc_core::sc_time& delay, bool debug)
    {
        if (!secure_access(trans)) {
            return access_reserved("redist-view0-non-secure", index,
                                   REDIST_FRAME_BYTES, trans, debug);
        }
        if (index >= REDIST_COUNT) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        const uint64_t offset = trans.get_address();
        if (offset == GICR_VIEWR) {
            const bool success = access_viewr(index, trans, debug);
            return success && sync_backend_policy(trans, delay, debug);
        }
        return access_redist_backend(
            index, trans, delay, debug, "redist-backend");
    }

    bool access_arch_redist_view(unsigned int view, unsigned int index,
                                 tlm::tlm_generic_payload& trans,
                                 sc_core::sc_time& delay, bool debug)
    {
        if (view < 1 || view > 2 || index >= REDIST_COUNT) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        if (m_viewr[index] != view) {
            return access_reserved(
                view == 1 ? "redist-view1-non-owner" :
                            "redist-view2-non-owner",
                index, REDIST_FRAME_BYTES, trans, debug);
        }
        return access_redist_backend(
            index, trans, delay, debug,
            view == 1 ? "redist-view1" : "redist-view2", view);
    }

    bool access_inactive_redists(tlm::tlm_generic_payload& trans, bool debug)
    {
        const uint64_t offset = trans.get_address();
        const unsigned int active = std::min(
            p_backend_redist_count.get_value(), REDIST_COUNT);
        const uint64_t aperture_bytes =
            (REDIST_COUNT - active) * REDIST_FRAME_BYTES;

        if (trans.get_data_ptr() == nullptr ||
            !is_supported_length(trans.get_data_length()) ||
            offset >= aperture_bytes ||
            trans.get_data_length() > aperture_bytes - offset) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        const unsigned int index = active +
            static_cast<unsigned int>(offset / REDIST_FRAME_BYTES);
        const uint64_t frame_offset = offset % REDIST_FRAME_BYTES;
        trans.set_address(frame_offset);
        const bool success = frame_offset < REDIST_BYTES ?
            access_synthetic_redist(index, trans, debug, true) :
            access_reserved("inactive-redist-reserved", index,
                            REDIST_FRAME_BYTES, trans, debug);
        trans.set_address(offset);
        return success;
    }

    bool access_dist_window(tlm::tlm_generic_payload& trans, bool debug,
                            sc_core::sc_time& delay, uint64_t base)
    {
        const uint64_t offset = trans.get_address();
        trans.set_address(base + offset);
        const bool success = access_dist(trans, delay, debug);
        trans.set_address(offset);
        return success;
    }

    bool access_redist_window(tlm::tlm_generic_payload& trans, bool debug,
                              sc_core::sc_time& delay, uint64_t base)
    {
        const uint64_t offset = trans.get_address();
        trans.set_address(base + offset);
        const bool success = access_redist(0, trans, delay, debug);
        trans.set_address(offset);
        return success;
    }

    bool access_view_redists_window(unsigned int view,
                                    unsigned int first_redist,
                                    tlm::tlm_generic_payload& trans,
                                    bool debug,
                                    sc_core::sc_time& delay)
    {
        const uint64_t offset = trans.get_address();
        const uint64_t stride = p_view_redist_stride.get_value();
        if (stride < REDIST_BYTES || stride == 0) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        const unsigned int index = first_redist +
            static_cast<unsigned int>(offset / stride);
        trans.set_address(offset % stride);
        const bool success =
            access_arch_redist_view(view, index, trans, delay, debug);
        trans.set_address(offset);
        return success;
    }

public:
    // Internal fault-source protocol, not guest MMIO: address selects a PE
    // in an SGI RAM row (up to 16 PEs); the 32-bit payload selects SGI rows
    // with an uncorrectable/DED fault. The entire row uses its view defaults.
    // ERRINS deliberately does not use this destructive containment ingress.
    void b_transport_sgi_ram_fault(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const unsigned index = trans.get_address();
        if (!trans.is_write() || !trans.get_data_ptr() || trans.get_data_length() != 4 ||
            trans.get_streaming_width() < 4 || trans.get_byte_enable_ptr() ||
            index >= std::min(p_backend_redist_count.get_value(), REDIST_COUNT)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        uint32_t bits;
        std::memcpy(&bits, trans.get_data_ptr(), 4);
        bits &= 0xffff;
        tlm::tlm_generic_payload config;
        qemu::MemoryRegionOps::MemTxAttrs attrs {};
        attrs.secure = true;
        config.set_extension(new QemuMemTxAttrsTlmExtension(attrs));
        const unsigned first = index & ~15u;
        const unsigned last = std::min(first + 16, std::min(p_backend_redist_count.get_value(), REDIST_COUNT));
        for (unsigned pe = first; pe < last; ++pe) {
            const uint64_t base = p_backend_redist_base.get_value() + pe * p_backend_redist_stride.get_value();
            if (!sgi_defaults(pe, bits, config, delay, false) ||
                !backend_word(config, delay, false, base + 0x10180, bits, tlm::TLM_WRITE_COMMAND)) {
                trans.set_response_status(config.get_response_status());
                return;
            }
            m_sgi_distribution_error[pe] |= bits;
        }
        for (unsigned bit = 0; bit < 16; ++bit)
            if (bits & (1u << bit)) m_ras.report(4, (index / 16) * 16 + bit, false);
        update_ras_irqs();
        if (bits && fmu_error_socket.size()) {
            // Issue 0201-12 Protection mechanism IDs, Table 3:
            // BLKTYPE=GICD(0), BLKID=0, SM_DED_GICD_SGI=50.
            // This source event is not a GICT ERRINS diagnostic or FMU SMERR.
            uint32_t source = 50u << 8;
            tlm::tlm_generic_payload report;
            report.set_command(tlm::TLM_WRITE_COMMAND);
            report.set_address(0);
            report.set_data_ptr(reinterpret_cast<unsigned char*>(&source));
            report.set_data_length(4);
            report.set_streaming_width(4);
            fmu_error_socket->b_transport(report, delay);
            if (!report.is_response_ok()) {
                trans.set_response_status(report.get_response_status());
                return;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    void b_transport_its(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    { access_its(trans, delay, false); }
    unsigned transport_dbg_its(tlm::tlm_generic_payload& trans)
    { sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
      return access_its(trans, delay, true) ? trans.get_data_length() : 0; }
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    cci::cci_param<bool> p_ras_lpi;
    cci::cci_param<unsigned> p_ras_ram_width;
    cci::cci_param<unsigned> p_ras_ram_entries;
    cci::cci_param<bool> p_backend_multi_view;
    cci::cci_param<uint64_t> p_backend_view2_dist_base;
    cci::cci_param<uint64_t> p_backend_dist_base;
    cci::cci_param<uint64_t> p_backend_redist_base;
    cci::cci_param<uint64_t> p_backend_redist_stride;
    cci::cci_param<unsigned int> p_backend_redist_count;
    cci::cci_param<uint64_t> p_view_redist_stride;
    cci::cci_param<unsigned int> p_view1_redist_first;
    cci::cci_param<unsigned int> p_view2_redist_first;
    cci::cci_param<unsigned int> p_spi_count;

    initiator_socket_t backend_socket;
    initiator_socket_t backend_its_socket;
    initiator_socket_t fmu_error_socket;
    target_socket_t its;
    target_socket_t sgi_ram_fault;
    InitiatorSignalSocket<uint64_t> internal_spi;
    InitiatorSignalSocket<uint64_t> internal_spi_view2;
    InitiatorSignalSocket<bool> ras_fault;
    InitiatorSignalSocket<bool> ras_error;

    target_socket_t view0_dist;
    target_socket_t view1_dist;
    target_socket_t view2_dist;
    target_socket_t view1_redists;
    target_socket_t view2_redists;
    target_socket_t view0_dist_cfgid;
    target_socket_t view0_dist_iviewr;
    target_socket_t view0_redist_0;
    target_socket_t view0_redist_0_pwrr;
    target_socket_t view0_redist_0_viewr;
    target_socket_t view0_redist_0_flushr;
    target_socket_t view0_redist_1;
    target_socket_t view0_redist_2;
    target_socket_t view0_redist_3;
    target_socket_t view0_redist_4;
    target_socket_t view0_redist_5;
    target_socket_t view0_redist_6;
    target_socket_t view0_redist_7;
    target_socket_t view0_redist_8;
    target_socket_t view0_redist_9;
    target_socket_t view0_redist_10;
    target_socket_t view0_redist_11;
    target_socket_t view0_redist_12;
    target_socket_t view0_redist_13;
    target_socket_t view0_redist_14;
    target_socket_t view0_redist_15;
    target_socket_t inactive_redists;
    sc_core::sc_vector<TargetSignalSocket<bool>> view1_spi_in;
    sc_core::sc_vector<TargetSignalSocket<bool>> view2_spi_in;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> spi_out;
    TargetSignalSocket<bool> reset;

    explicit gicx00_multiview(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_trace("trace", false)
        , p_trace_limit("trace_limit", 128)
        , p_ras_lpi("ras_lpi", false)
        , p_ras_ram_width("ras_ram_width", 128)
        , p_ras_ram_entries("ras_ram_entries", 256)
        , p_backend_multi_view("backend_multi_view", false)
        , p_backend_view2_dist_base("backend_view2_dist_base", 0)
        , p_backend_dist_base("backend_dist_base", DEFAULT_BACKEND_DIST_BASE)
        , p_backend_redist_base(
              "backend_redist_base", DEFAULT_BACKEND_REDIST_BASE)
        , p_backend_redist_stride(
              "backend_redist_stride", DEFAULT_BACKEND_REDIST_STRIDE)
        , p_backend_redist_count(
              "backend_redist_count", DEFAULT_BACKEND_REDIST_COUNT)
        , p_view_redist_stride("view_redist_stride", REDIST_BYTES)
        , p_view1_redist_first("view1_redist_first", 0)
        , p_view2_redist_first("view2_redist_first", 0)
        , p_spi_count("spi_count", DEFAULT_SPI_COUNT)
        , backend_socket("backend_socket")
        , backend_its_socket("backend_its_socket")
        , fmu_error_socket("fmu_error_socket")
        , its("its")
        , sgi_ram_fault("sgi_ram_fault")
        , internal_spi("internal_spi")
        , internal_spi_view2("internal_spi_view2")
        , ras_fault("ras_fault")
        , ras_error("ras_error")
        , view0_dist("view0_dist")
        , view1_dist("view1_dist")
        , view2_dist("view2_dist")
        , view1_redists("view1_redists")
        , view2_redists("view2_redists")
        , view0_dist_cfgid("view0_dist_cfgid")
        , view0_dist_iviewr("view0_dist_iviewr")
        , view0_redist_0("view0_redist_0")
        , view0_redist_0_pwrr("view0_redist_0_pwrr")
        , view0_redist_0_viewr("view0_redist_0_viewr")
        , view0_redist_0_flushr("view0_redist_0_flushr")
        , view0_redist_1("view0_redist_1")
        , view0_redist_2("view0_redist_2")
        , view0_redist_3("view0_redist_3")
        , view0_redist_4("view0_redist_4")
        , view0_redist_5("view0_redist_5")
        , view0_redist_6("view0_redist_6")
        , view0_redist_7("view0_redist_7")
        , view0_redist_8("view0_redist_8")
        , view0_redist_9("view0_redist_9")
        , view0_redist_10("view0_redist_10")
        , view0_redist_11("view0_redist_11")
        , view0_redist_12("view0_redist_12")
        , view0_redist_13("view0_redist_13")
        , view0_redist_14("view0_redist_14")
        , view0_redist_15("view0_redist_15")
        , inactive_redists("inactive_redists")
        , view1_spi_in(
              "view1_spi_in", p_spi_count.get_value(),
              [](const char* n, size_t) {
                  return new TargetSignalSocket<bool>(n);
              })
        , view2_spi_in(
              "view2_spi_in", p_spi_count.get_value(),
              [](const char* n, size_t) {
                  return new TargetSignalSocket<bool>(n);
              })
        , spi_out(
              "spi_out", p_spi_count.get_value(),
              [](const char* n, size_t) {
                  return new InitiatorSignalSocket<bool>(n);
              })
        , reset("reset")
    {
        if (!p_ras_ram_width.get_value() || p_ras_ram_width.get_value() > 512 ||
            !p_ras_ram_entries.get_value() || p_ras_ram_entries.get_value() > 65536)
            SC_REPORT_FATAL(name, "Invalid GIC logical diagnostic RAM geometry");
        m_injection_levels[0].assign(p_spi_count.get_value(), false);
        m_injection_levels[1].assign(p_spi_count.get_value(), false);
        reset_registers();
        sgi_ram_fault.register_b_transport(this, &gicx00_multiview::b_transport_sgi_ram_fault);
        its.register_b_transport(this, &gicx00_multiview::b_transport_its);
        its.register_transport_dbg(this, &gicx00_multiview::transport_dbg_its);
        view0_dist.register_b_transport(this, &gicx00_multiview::b_transport_dist);
        view0_dist.register_transport_dbg(this, &gicx00_multiview::transport_dbg_dist);
        view1_dist.register_b_transport(
            this, &gicx00_multiview::b_transport_dist_view1);
        view1_dist.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_dist_view1);
        view2_dist.register_b_transport(
            this, &gicx00_multiview::b_transport_dist_view2);
        view2_dist.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_dist_view2);
        view1_redists.register_b_transport(
            this, &gicx00_multiview::b_transport_view1_redists);
        view1_redists.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_view1_redists);
        view2_redists.register_b_transport(
            this, &gicx00_multiview::b_transport_view2_redists);
        view2_redists.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_view2_redists);
        view0_dist_cfgid.register_b_transport(
            this, &gicx00_multiview::b_transport_dist_cfgid);
        view0_dist_cfgid.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_dist_cfgid);
        view0_dist_iviewr.register_b_transport(
            this, &gicx00_multiview::b_transport_dist_iviewr);
        view0_dist_iviewr.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_dist_iviewr);
        view0_redist_0.register_b_transport(this, &gicx00_multiview::b_transport_redist0);
        view0_redist_0.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist0);
        view0_redist_0_pwrr.register_b_transport(
            this, &gicx00_multiview::b_transport_redist0_pwrr);
        view0_redist_0_pwrr.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_redist0_pwrr);
        view0_redist_0_viewr.register_b_transport(
            this, &gicx00_multiview::b_transport_redist0_viewr);
        view0_redist_0_viewr.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_redist0_viewr);
        view0_redist_0_flushr.register_b_transport(
            this, &gicx00_multiview::b_transport_redist0_flushr);
        view0_redist_0_flushr.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_redist0_flushr);
        view0_redist_1.register_b_transport(this, &gicx00_multiview::b_transport_redist1);
        view0_redist_1.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist1);
        view0_redist_2.register_b_transport(this, &gicx00_multiview::b_transport_redist2);
        view0_redist_2.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist2);
        view0_redist_3.register_b_transport(this, &gicx00_multiview::b_transport_redist3);
        view0_redist_3.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist3);
        view0_redist_4.register_b_transport(this, &gicx00_multiview::b_transport_redist4);
        view0_redist_4.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist4);
        view0_redist_5.register_b_transport(this, &gicx00_multiview::b_transport_redist5);
        view0_redist_5.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist5);
        view0_redist_6.register_b_transport(this, &gicx00_multiview::b_transport_redist6);
        view0_redist_6.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist6);
        view0_redist_7.register_b_transport(this, &gicx00_multiview::b_transport_redist7);
        view0_redist_7.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist7);
        view0_redist_8.register_b_transport(this, &gicx00_multiview::b_transport_redist8);
        view0_redist_8.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist8);
        view0_redist_9.register_b_transport(this, &gicx00_multiview::b_transport_redist9);
        view0_redist_9.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist9);
        view0_redist_10.register_b_transport(this, &gicx00_multiview::b_transport_redist10);
        view0_redist_10.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist10);
        view0_redist_11.register_b_transport(this, &gicx00_multiview::b_transport_redist11);
        view0_redist_11.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist11);
        view0_redist_12.register_b_transport(this, &gicx00_multiview::b_transport_redist12);
        view0_redist_12.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist12);
        view0_redist_13.register_b_transport(this, &gicx00_multiview::b_transport_redist13);
        view0_redist_13.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist13);
        view0_redist_14.register_b_transport(this, &gicx00_multiview::b_transport_redist14);
        view0_redist_14.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist14);
        view0_redist_15.register_b_transport(this, &gicx00_multiview::b_transport_redist15);
        view0_redist_15.register_transport_dbg(this, &gicx00_multiview::transport_dbg_redist15);
        inactive_redists.register_b_transport(
            this, &gicx00_multiview::b_transport_inactive_redists);
        inactive_redists.register_transport_dbg(
            this, &gicx00_multiview::transport_dbg_inactive_redists);
        for (unsigned int spi = 0; spi < p_spi_count.get_value(); ++spi) {
            view1_spi_in[spi].register_value_changed_cb(
                [this, spi](bool value) {
                    inject_spi(1, SPI_BASE_INTID + spi, value);
                });
            view2_spi_in[spi].register_value_changed_cb(
                [this, spi](bool value) {
                    inject_spi(2, SPI_BASE_INTID + spi, value);
                });
        }
        reset.register_value_changed_cb([this](bool asserted) {
            m_reset_asserted = asserted;
            if (asserted) {
                reset_model();
            }
        });
    }

    void b_transport_dist(tlm::tlm_generic_payload& trans,
                          sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_dist(trans, delay, false);
    }

    unsigned int transport_dbg_dist(tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_dist(trans, delay, true) ? trans.get_data_length() : 0;
    }

    void b_transport_dist_view(unsigned int view,
                               tlm::tlm_generic_payload& trans,
                               sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_dist_view(view, trans, delay, false);
    }

    unsigned int transport_dbg_dist_view(
        unsigned int view, tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_dist_view(view, trans, delay, true) ?
            trans.get_data_length() : 0;
    }

    void b_transport_dist_view1(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
    {
        b_transport_dist_view(1, trans, delay);
    }

    unsigned int transport_dbg_dist_view1(
        tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_dist_view(1, trans);
    }

    void b_transport_dist_view2(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
    {
        b_transport_dist_view(2, trans, delay);
    }

    unsigned int transport_dbg_dist_view2(
        tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_dist_view(2, trans);
    }

    bool inject_spi(unsigned int view, uint32_t intid, bool value)
    {
        if (view < 1 || view > 2 || intid < SPI_BASE_INTID) {
            return false;
        }
        const unsigned int index = intid - SPI_BASE_INTID;
        if (index >= spi_out.size()) {
            return false;
        }
        if (m_reset_asserted) {
            m_injection_levels[view - 1][index] = false;
            return false;
        }
        m_injection_levels[view - 1][index] = value;
        if (owner_for_intid(intid) != view || spi_out[index].size() == 0) {
            return false;
        }
        spi_out[index]->write(value);
        return true;
    }

    uint32_t runtime_spi_owner(uint32_t intid) const
    {
        return owner_for_intid(intid);
    }

    void b_transport_view1_redists(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_view_redists_window(
            1, p_view1_redist_first.get_value(), trans, false, delay);
    }

    unsigned int transport_dbg_view1_redists(
        tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_view_redists_window(
                   1, p_view1_redist_first.get_value(), trans, true, delay) ?
            trans.get_data_length() : 0;
    }

    void b_transport_view2_redists(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_view_redists_window(
            2, p_view2_redist_first.get_value(), trans, false, delay);
    }

    unsigned int transport_dbg_view2_redists(
        tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_view_redists_window(
                   2, p_view2_redist_first.get_value(), trans, true, delay) ?
            trans.get_data_length() : 0;
    }

    void b_transport_dist_cfgid(tlm::tlm_generic_payload& trans,
                                sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_dist_window(trans, false, delay, GICD_CFGID);
    }

    unsigned int transport_dbg_dist_cfgid(tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_dist_window(trans, true, delay, GICD_CFGID) ?
            trans.get_data_length() : 0;
    }

    void b_transport_dist_iviewr(tlm::tlm_generic_payload& trans,
                                 sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_dist_window(trans, false, delay, GICD_IVIEWR_BASE);
    }

    unsigned int transport_dbg_dist_iviewr(tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_dist_window(trans, true, delay, GICD_IVIEWR_BASE) ?
            trans.get_data_length() : 0;
    }

    void b_transport_redist(unsigned int index, tlm::tlm_generic_payload& trans,
                            sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_redist(index, trans, delay, false);
    }

    unsigned int transport_dbg_redist(unsigned int index,
                                      tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_redist(index, trans, delay, true) ?
            trans.get_data_length() : 0;
    }

    void b_transport_redist_view(unsigned int view, unsigned int index,
                                 tlm::tlm_generic_payload& trans,
                                 sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_arch_redist_view(view, index, trans, delay, false);
    }

    unsigned int transport_dbg_redist_view(
        unsigned int view, unsigned int index,
        tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_arch_redist_view(view, index, trans, delay, true) ?
            trans.get_data_length() : 0;
    }

    void b_transport_inactive_redists(tlm::tlm_generic_payload& trans,
                                      sc_core::sc_time& delay)
    {
        (void)delay;
        trans.set_dmi_allowed(false);
        access_inactive_redists(trans, false);
    }

    unsigned int transport_dbg_inactive_redists(
        tlm::tlm_generic_payload& trans)
    {
        return access_inactive_redists(trans, true) ?
            trans.get_data_length() : 0;
    }

    void b_transport_redist0_pwrr(tlm::tlm_generic_payload& trans,
                                  sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_redist_window(trans, false, delay, GICR_PWRR);
    }

    unsigned int transport_dbg_redist0_pwrr(
        tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_redist_window(trans, true, delay, GICR_PWRR) ?
            trans.get_data_length() : 0;
    }

    void b_transport_redist0_viewr(tlm::tlm_generic_payload& trans,
                                   sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_redist_window(trans, false, delay, GICR_VIEWR);
    }

    unsigned int transport_dbg_redist0_viewr(
        tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_redist_window(trans, true, delay, GICR_VIEWR) ?
            trans.get_data_length() : 0;
    }

    void b_transport_redist0_flushr(tlm::tlm_generic_payload& trans,
                                    sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        access_redist_window(trans, false, delay, GICR_FLUSHR);
    }

    unsigned int transport_dbg_redist0_flushr(
        tlm::tlm_generic_payload& trans)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        return access_redist_window(trans, true, delay, GICR_FLUSHR) ?
            trans.get_data_length() : 0;
    }

    void b_transport_redist0(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(0, trans, delay);
    }
    void b_transport_redist1(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(1, trans, delay);
    }
    void b_transport_redist2(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(2, trans, delay);
    }
    void b_transport_redist3(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(3, trans, delay);
    }
    void b_transport_redist4(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(4, trans, delay);
    }
    void b_transport_redist5(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(5, trans, delay);
    }
    void b_transport_redist6(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(6, trans, delay);
    }
    void b_transport_redist7(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(7, trans, delay);
    }
    void b_transport_redist8(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(8, trans, delay);
    }
    void b_transport_redist9(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
    {
        b_transport_redist(9, trans, delay);
    }
    void b_transport_redist10(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay)
    {
        b_transport_redist(10, trans, delay);
    }
    void b_transport_redist11(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay)
    {
        b_transport_redist(11, trans, delay);
    }
    void b_transport_redist12(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay)
    {
        b_transport_redist(12, trans, delay);
    }
    void b_transport_redist13(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay)
    {
        b_transport_redist(13, trans, delay);
    }
    void b_transport_redist14(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay)
    {
        b_transport_redist(14, trans, delay);
    }
    void b_transport_redist15(tlm::tlm_generic_payload& trans,
                              sc_core::sc_time& delay)
    {
        b_transport_redist(15, trans, delay);
    }

    unsigned int transport_dbg_redist0(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(0, trans);
    }
    unsigned int transport_dbg_redist1(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(1, trans);
    }
    unsigned int transport_dbg_redist2(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(2, trans);
    }
    unsigned int transport_dbg_redist3(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(3, trans);
    }
    unsigned int transport_dbg_redist4(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(4, trans);
    }
    unsigned int transport_dbg_redist5(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(5, trans);
    }
    unsigned int transport_dbg_redist6(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(6, trans);
    }
    unsigned int transport_dbg_redist7(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(7, trans);
    }
    unsigned int transport_dbg_redist8(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(8, trans);
    }
    unsigned int transport_dbg_redist9(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(9, trans);
    }
    unsigned int transport_dbg_redist10(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(10, trans);
    }
    unsigned int transport_dbg_redist11(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(11, trans);
    }
    unsigned int transport_dbg_redist12(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(12, trans);
    }
    unsigned int transport_dbg_redist13(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(13, trans);
    }
    unsigned int transport_dbg_redist14(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(14, trans);
    }
    unsigned int transport_dbg_redist15(tlm::tlm_generic_payload& trans)
    {
        return transport_dbg_redist(15, trans);
    }
};

extern "C" void module_register();
