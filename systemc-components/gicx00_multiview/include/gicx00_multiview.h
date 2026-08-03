/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

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
    // Wire levels are retained only to make ownership mux handoff coherent;
    // architectural pending/active/enable state remains in the backend GIC.
    std::array<std::vector<bool>, 2> m_injection_levels;
    unsigned int m_trace_count = 0;
    bool m_reset_asserted = false;

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
        m_iviewr.fill(0);
        m_viewr.fill(0);
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
        trans.set_address(offset);

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
        const bool level = owner >= 1 && owner <= 2 ?
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

    bool access_dist(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay, bool debug)
    {
        const uint64_t offset = trans.get_address();
        if (offset >= DIST_BYTES) {
            return access_reserved("dist-reserved", UINT32_MAX,
                                   DIST_FRAME_BYTES, trans, debug);
        }

        if (is_iviewr_window(offset)) {
            return access_iviewr(trans, debug);
        }

        if (offset >= GICD_CFGID &&
            offset < GICD_CFGID + sizeof(uint64_t)) {
            return access_cfgid(trans, debug);
        }

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
        if (view < 1 || view > 2 ||
            !validate_backend_access(trans, DIST_BYTES)) {
            if (view < 1 || view > 2) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            }
            return false;
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
                p_backend_dist_base.get_value() + offset, debug);
            if (success) {
                merge_owned_fields(
                    view, data, original.data(), len, policy, *this);
                trans.set_command(tlm::TLM_WRITE_COMMAND);
                success = forward_backend(
                    region, UINT32_MAX, trans, delay,
                    p_backend_dist_base.get_value() + offset, debug);
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
            p_backend_dist_base.get_value() + offset, debug);
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
                               const char* region)
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
        if (backend_socket.size() == 0) {
            return access_reserved(
                "redist-unbound", index, REDIST_BYTES, trans, debug);
        }
        if (index >= p_backend_redist_count.get_value()) {
            return access_synthetic_redist(index, trans, debug);
        }
        return forward_backend(
            region, index, trans, delay,
            p_backend_redist_base.get_value() +
                (index * p_backend_redist_stride.get_value()) + offset,
            debug);
    }

    bool access_redist(unsigned int index, tlm::tlm_generic_payload& trans,
                       sc_core::sc_time& delay, bool debug)
    {
        if (index >= REDIST_COUNT) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }

        const uint64_t offset = trans.get_address();
        if (offset == GICR_VIEWR) {
            return access_viewr(index, trans, debug);
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
            view == 1 ? "redist-view1" : "redist-view2");
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
    cci::cci_param<bool> p_trace;
    cci::cci_param<unsigned int> p_trace_limit;
    cci::cci_param<uint64_t> p_backend_dist_base;
    cci::cci_param<uint64_t> p_backend_redist_base;
    cci::cci_param<uint64_t> p_backend_redist_stride;
    cci::cci_param<unsigned int> p_backend_redist_count;
    cci::cci_param<uint64_t> p_view_redist_stride;
    cci::cci_param<unsigned int> p_view1_redist_first;
    cci::cci_param<unsigned int> p_view2_redist_first;
    cci::cci_param<unsigned int> p_spi_count;

    initiator_socket_t backend_socket;

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
        m_injection_levels[0].assign(p_spi_count.get_value(), false);
        m_injection_levels[1].assign(p_spi_count.get_value(), false);
        reset_registers();
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
