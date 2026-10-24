/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <cci/utils/broker.h>
#include <gicx00_multiview.h>
#include <zena_device_fmu.h>
#include <zena_fmu.h>
#include <gtest/gtest.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace {

constexpr uint64_t GICD_CTLR = 0x0000;
constexpr uint64_t GICD_IGROUPR = 0x0080;
constexpr uint64_t GICD_ISENABLER = 0x0100;
constexpr uint64_t GICD_ICENABLER = 0x0180;
constexpr uint64_t GICD_ISPENDR = 0x0200;
constexpr uint64_t GICD_ICPENDR = 0x0280;
constexpr uint64_t GICD_ISACTIVER = 0x0300;
constexpr uint64_t GICD_ICACTIVER = 0x0380;
constexpr uint64_t GICD_IPRIORITYR = 0x0400;
constexpr uint64_t GICD_ICFGR = 0x0c00;
constexpr uint64_t GICD_IGRPMODR = 0x0d00;
constexpr uint64_t GICD_NSACR = 0x0e00;
constexpr uint64_t GICD_IROUTER = 0x6000;
constexpr uint64_t GICD_CFGID = 0xf000;
constexpr uint64_t GICD_IVIEWR_BASE = 0xf600;
constexpr uint64_t AP_GIC_DIST_BASE = 0x20800000;
constexpr uint64_t AP_GIC_REDIST_BASE = 0x20880000;
constexpr uint64_t AP_GIC_REDIST_STRIDE = 0x40000;
constexpr uint64_t GICR_PWRR = 0x0024;
constexpr uint64_t GICR_VIEWR = 0x002c;
constexpr uint64_t GICR_FLUSHR = 0x0030;
constexpr uint64_t GICR_TYPER = 0x0008;
constexpr uint64_t GICR_PIDR2 = 0xffe8;
constexpr uint64_t GICD_CFGID_VIEW = 1ull << 53;
constexpr uint64_t GICR_TYPER_LAST = 1ull << 4;
constexpr uint64_t GICR_TYPER_FEATURES =
    (1ull << 24) | (1ull << 7) | (1ull << 3) |
    (1ull << 2) | (1ull << 1) | (1ull << 0);
constexpr uint16_t SPI_MIN = 32;
constexpr uint16_t SPI_LIMIT = 992;

struct SpiView {
    uint16_t spi;
    uint32_t view;
};

class FunctionalGicBackend : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<FunctionalGicBackend,
                                    DEFAULT_TLM_BUSWIDTH>
        target_socket;
    unsigned int accesses = 0;
    uint64_t last_address = 0;
    tlm::tlm_command last_command = tlm::TLM_IGNORE_COMMAND;
    uint64_t read_value = 0;
    bool export_hint = false;
    unsigned int secure_accesses = 0;

    explicit FunctionalGicBackend(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , target_socket("target_socket")
    {
        target_socket.register_b_transport(
            this, &FunctionalGicBackend::b_transport);
        target_socket.register_transport_dbg(
            this, &FunctionalGicBackend::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        delay += sc_core::sc_time(7, sc_core::SC_NS);
        handle(trans);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        handle(trans);
        return trans.is_response_ok() ? trans.get_data_length() : 0;
    }

private:
    void handle(tlm::tlm_generic_payload& trans)
    {
        ++accesses;
        last_address = trans.get_address();
        last_command = trans.get_command();
        const auto* attrs = trans.get_extension<QemuMemTxAttrsTlmExtension>();
        if (attrs && attrs->get_attrs().secure) ++secure_accesses;
        if (export_hint) {
            trans.set_extension(new QemuMrHintTlmExtension());
            trans.set_dmi_allowed(true);
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(trans.get_data_ptr(), &read_value,
                        trans.get_data_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class StatefulCanonicalGicBackend
    : public sc_core::sc_module
    , public sc_core::sc_signal_inout_if<bool>
{
public:
    tlm_utils::simple_target_socket<StatefulCanonicalGicBackend,
                                    DEFAULT_TLM_BUSWIDTH>
        target_socket;
    std::array<bool, SPI_LIMIT> enabled {};
    std::array<uint32_t, 16> local_enable {};
    std::array<uint32_t, 16> local_pending {};
    std::array<std::array<uint8_t, 0x1000>, 16> local_registers {};
    std::array<bool, SPI_LIMIT> pending {};
    std::array<bool, SPI_LIMIT> active {};
    std::array<uint8_t, 0x8000> distributor {};
    unsigned int accesses = 0;
    unsigned int mutations = 0;
    unsigned int asserted = 0;
    unsigned int deasserted = 0;
    const void* last_mmio_identity = nullptr;
    const void* last_signal_identity = nullptr;
    bool fail_next_read = false;

    explicit StatefulCanonicalGicBackend(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , target_socket("target_socket")
    {
        target_socket.register_b_transport(
            this, &StatefulCanonicalGicBackend::b_transport);
        target_socket.register_transport_dbg(
            this, &StatefulCanonicalGicBackend::transport_dbg);
    }

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        delay += sc_core::sc_time(7, sc_core::SC_NS);
        if (fail_next_read &&
            trans.get_command() == tlm::TLM_READ_COMMAND) {
            fail_next_read = false;
            ++accesses;
            last_mmio_identity = this;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        handle(trans);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        handle(trans);
        return trans.is_response_ok() ? trans.get_data_length() : 0;
    }

    const sc_core::sc_event& default_event() const override { return event_; }
    const sc_core::sc_event& value_changed_event() const override
    {
        return event_;
    }
    const sc_core::sc_event& posedge_event() const override { return event_; }
    const sc_core::sc_event& negedge_event() const override { return event_; }
    const bool& read() const override { return signal_value_; }
    const bool& get_data_ref() const override { return signal_value_; }
    bool event() const override { return false; }
    bool posedge() const override { return false; }
    bool negedge() const override { return false; }

    void write(const bool& value) override
    {
        last_signal_identity = this;
        if (value == signal_value_) {
            return;
        }
        signal_value_ = value;
        if (value) {
            ++asserted;
        } else {
            ++deasserted;
        }
    }

private:
    bool access_bitmap(tlm::tlm_generic_payload& trans, uint64_t offset,
                       uint64_t set_base, uint64_t clear_base,
                       std::array<bool, SPI_LIMIT>& state)
    {
        const uint64_t relative = offset - AP_GIC_DIST_BASE;
        const bool is_set =
            relative >= set_base && relative < set_base + 0x80;
        const bool is_clear =
            relative >= clear_base && relative < clear_base + 0x80;
        if (!is_set && !is_clear) {
            return false;
        }

        const uint64_t base = is_set ? set_base : clear_base;
        const unsigned int first =
            static_cast<unsigned int>(((relative - base) / 4) * 32);
        uint32_t value = 0;
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            for (unsigned int bit = 0; bit < 32; ++bit) {
                if (first + bit < state.size() && state[first + bit]) {
                    value |= 1u << bit;
                }
            }
            std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
        } else {
            std::memcpy(&value, trans.get_data_ptr(), sizeof(value));
            for (unsigned int bit = 0; bit < 32; ++bit) {
                if ((value & (1u << bit)) != 0 &&
                    first + bit < state.size()) {
                    state[first + bit] = is_set;
                    ++mutations;
                }
            }
        }
        return true;
    }

    void handle(tlm::tlm_generic_payload& trans)
    {
        ++accesses;
        last_mmio_identity = this;
        const uint64_t offset = trans.get_address();
        if (offset >= AP_GIC_REDIST_BASE &&
            offset < AP_GIC_REDIST_BASE + 16 * AP_GIC_REDIST_STRIDE) {
            const unsigned int cpu = (offset - AP_GIC_REDIST_BASE) / AP_GIC_REDIST_STRIDE;
            const uint64_t reg = (offset - AP_GIC_REDIST_BASE) % AP_GIC_REDIST_STRIDE;
            if (reg == 0x10100 || reg == 0x10180 || reg == 0x10200 || reg == 0x10280) {
                uint32_t& state = reg < 0x10200 ? local_enable[cpu] : local_pending[cpu];
                if (trans.is_read()) std::memcpy(trans.get_data_ptr(), &state, 4);
                else {
                    uint32_t value;
                    std::memcpy(&value, trans.get_data_ptr(), 4);
                    if (reg & 0x80) state &= ~value;
                    else state |= value;
                }
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }
            if (reg >= 0x10000 && reg + trans.get_data_length() <= 0x11000) {
                auto* data = local_registers[cpu].data() + reg - 0x10000;
                if (trans.is_read()) std::memcpy(trans.get_data_ptr(), data, trans.get_data_length());
                else std::memcpy(data, trans.get_data_ptr(), trans.get_data_length());
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }
        }
        const bool handled =
            access_bitmap(trans, offset, GICD_ISENABLER, GICD_ICENABLER,
                          enabled) ||
            access_bitmap(trans, offset, GICD_ISPENDR, GICD_ICPENDR,
                          pending) ||
            access_bitmap(trans, offset, GICD_ISACTIVER, GICD_ICACTIVER,
                          active);
        if (!handled) {
            const bool in_distributor =
                offset >= AP_GIC_DIST_BASE &&
                offset - AP_GIC_DIST_BASE + trans.get_data_length() <=
                    distributor.size();
            if (in_distributor) {
                const size_t relative =
                    static_cast<size_t>(offset - AP_GIC_DIST_BASE);
                if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                    std::memcpy(trans.get_data_ptr(),
                                distributor.data() + relative,
                                trans.get_data_length());
                } else {
                    std::memcpy(distributor.data() + relative,
                                trans.get_data_ptr(),
                                trans.get_data_length());
                    ++mutations;
                }
            } else if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::memset(trans.get_data_ptr(), 0, trans.get_data_length());
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

private:
    bool signal_value_ = false;
    sc_core::sc_event event_;
};

template <typename T>
T access(gicx00_multiview& dut, bool dist, unsigned int redist,
         uint64_t offset, tlm::tlm_command command, T value = 0)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (dist) {
        dut.b_transport_dist(trans, delay);
    } else {
        dut.b_transport_redist(redist, trans, delay);
    }

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

template <typename T>
tlm::tlm_response_status raw_access(gicx00_multiview& dut, bool dist,
                                    unsigned int redist, uint64_t offset,
                                    tlm::tlm_command command, T* data)
{
    tlm::tlm_generic_payload trans;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(*data));
    trans.set_streaming_width(sizeof(*data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (dist) {
        dut.b_transport_dist(trans, delay);
    } else {
        dut.b_transport_redist(redist, trans, delay);
    }

    return trans.get_response_status();
}

uint32_t read_dist32(gicx00_multiview& dut, uint64_t offset)
{
    return access<uint32_t>(dut, true, 0, offset, tlm::TLM_READ_COMMAND);
}

uint64_t read_dist64(gicx00_multiview& dut, uint64_t offset)
{
    return access<uint64_t>(dut, true, 0, offset, tlm::TLM_READ_COMMAND);
}

void write_dist32(gicx00_multiview& dut, uint64_t offset, uint32_t value)
{
    (void)access<uint32_t>(dut, true, 0, offset, tlm::TLM_WRITE_COMMAND, value);
}

void write_dist64(gicx00_multiview& dut, uint64_t offset, uint64_t value)
{
    (void)access<uint64_t>(dut, true, 0, offset, tlm::TLM_WRITE_COMMAND, value);
}

uint32_t read_redist32(gicx00_multiview& dut, unsigned int redist,
                       uint64_t offset)
{
    return access<uint32_t>(dut, false, redist, offset, tlm::TLM_READ_COMMAND);
}

uint64_t read_redist64(gicx00_multiview& dut, unsigned int redist,
                       uint64_t offset)
{
    return access<uint64_t>(dut, false, redist, offset,
                            tlm::TLM_READ_COMMAND);
}

void write_redist32(gicx00_multiview& dut, unsigned int redist,
                    uint64_t offset, uint32_t value)
{
    (void)access<uint32_t>(
        dut, false, redist, offset, tlm::TLM_WRITE_COMMAND, value);
}

uint64_t iviewr_offset(uint16_t spi)
{
    return GICD_IVIEWR_BASE + ((spi / 16) * sizeof(uint32_t));
}

unsigned int iviewr_shift(uint16_t spi)
{
    return (spi % 16) * 2;
}

uint32_t iviewr_field(gicx00_multiview& dut, uint16_t spi)
{
    return (read_dist32(dut, iviewr_offset(spi)) >> iviewr_shift(spi)) & 0x3u;
}

void write_spi_view(gicx00_multiview& dut, uint16_t spi, uint32_t view)
{
    const uint64_t offset = iviewr_offset(spi);
    const unsigned int shift = iviewr_shift(spi);
    uint32_t value = read_dist32(dut, offset);

    value &= ~(0x3u << shift);
    value |= (view & 0x3u) << shift;
    write_dist32(dut, offset, value);
}

uint32_t access_dist_view32(gicx00_multiview& dut, unsigned int view,
                            uint64_t offset, tlm::tlm_command command,
                            uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_dist_view(view, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint64_t access_dist_view64(gicx00_multiview& dut, unsigned int view,
                            uint64_t offset, tlm::tlm_command command,
                            uint64_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_dist_view(view, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint32_t debug_write_dist_view32(gicx00_multiview& dut, unsigned int view,
                                 uint64_t offset, uint32_t value)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));

    EXPECT_EQ(dut.transport_dbg_dist_view(view, trans), sizeof(value));
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint32_t access_redist_view32(gicx00_multiview& dut, unsigned int view,
                              unsigned int redist, uint64_t offset,
                              tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_redist_view(view, redist, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

} // namespace

TEST(Gicx00MultiviewTest, ResetValuesAdvertiseViewAndPowerOnState)
{
    gicx00_multiview dut("gicx00_multiview_reset");

    EXPECT_EQ(read_dist32(dut, GICD_CTLR), 0u);
    EXPECT_EQ(read_dist64(dut, GICD_CFGID) & GICD_CFGID_VIEW,
              GICD_CFGID_VIEW);
    EXPECT_EQ(read_dist32(dut, iviewr_offset(SPI_MIN)), 0u);
    EXPECT_EQ(read_dist32(dut, iviewr_offset(SPI_LIMIT - 1)), 0u);
    EXPECT_EQ(read_redist32(dut, 0, GICR_PWRR) & 0x1u, 0u);
    EXPECT_EQ(read_redist32(dut, 0, GICR_VIEWR), 0u);
    EXPECT_EQ(read_redist32(dut, 0, GICR_FLUSHR), 0u);
}

TEST(Gicx00MultiviewTest,
     StandardGicAccessesReachCanonicalFunctionalBackend)
{
    FunctionalGicBackend backend("gicx00_functional_backend");
    gicx00_multiview dut("gicx00_multiview_backend");
    dut.backend_socket.bind(backend.target_socket);

    write_dist32(dut, GICD_CTLR, 0x7u);
    EXPECT_EQ(backend.last_command, tlm::TLM_WRITE_COMMAND);
    EXPECT_EQ(backend.last_address, AP_GIC_DIST_BASE + GICD_CTLR);

    backend.read_value = 0xa5a55a5au;
    EXPECT_EQ(read_dist32(dut, GICD_CTLR), 0xa5a55a5au);
    EXPECT_EQ(backend.last_address, AP_GIC_DIST_BASE + GICD_CTLR);

    EXPECT_EQ(read_redist32(dut, 1, 0x0040), 0xa5a55a5au);
    EXPECT_EQ(backend.last_address,
              AP_GIC_REDIST_BASE + AP_GIC_REDIST_STRIDE + 0x0040);

    const unsigned int before_reserved = backend.accesses;
    EXPECT_EQ(read_redist32(dut, 1, 0x20008), 0u);
    EXPECT_EQ(backend.accesses, before_reserved);

    EXPECT_EQ(read_redist32(dut, 2, 0x0040), 0xa5a55a5au);
    EXPECT_EQ(backend.last_address,
              AP_GIC_REDIST_BASE + (2 * AP_GIC_REDIST_STRIDE) + 0x0040);

    EXPECT_EQ(read_redist32(dut, 3, 0x0040), 0xa5a55a5au);
    EXPECT_EQ(backend.last_address,
              AP_GIC_REDIST_BASE + (3 * AP_GIC_REDIST_STRIDE) + 0x0040);

    const unsigned int before_discovery = backend.accesses;
    const uint64_t cpu1_typer = read_redist64(dut, 1, GICR_TYPER);
    EXPECT_EQ(cpu1_typer, backend.read_value);
    EXPECT_EQ(backend.accesses, before_discovery + 1);
    EXPECT_EQ(backend.last_address,
              AP_GIC_REDIST_BASE + AP_GIC_REDIST_STRIDE + GICR_TYPER);

    const unsigned int standard_accesses = backend.accesses;
    EXPECT_NE(read_dist64(dut, GICD_CFGID) & GICD_CFGID_VIEW, 0u);
    write_dist32(dut, iviewr_offset(SPI_MIN), 0x1u);
    write_redist32(dut, 1, GICR_VIEWR, 0x2u);
    EXPECT_EQ(backend.accesses, standard_accesses);
}

TEST(Gicx00MultiviewTest,
     DistributorControlKeepsIndependentViewEnableState)
{
    constexpr uint32_t group_enable_mask = 0x7u;
    StatefulCanonicalGicBackend backend("gicx00_ctlr_backend");
    gicx00_multiview dut("gicx00_multiview_ctlr");
    dut.backend_socket.bind(backend.target_socket);

    access_dist_view32(
        dut, 1, GICD_CTLR, tlm::TLM_WRITE_COMMAND, 0x51u);
    EXPECT_EQ(read_dist32(dut, GICD_CTLR) & group_enable_mask, 0u);
    access_dist_view32(
        dut, 2, GICD_CTLR, tlm::TLM_WRITE_COMMAND, 0x12u);
    EXPECT_EQ(read_dist32(dut, GICD_CTLR) & group_enable_mask, 0u);

    write_dist32(dut, GICD_CTLR, 0x57u);

    EXPECT_EQ(read_dist32(dut, GICD_CTLR) & group_enable_mask, 0x3u);
    EXPECT_EQ(access_dist_view32(
                  dut, 1, GICD_CTLR, tlm::TLM_READ_COMMAND) &
                  group_enable_mask,
              0x1u);
    EXPECT_EQ(access_dist_view32(
                  dut, 2, GICD_CTLR, tlm::TLM_READ_COMMAND) &
                  group_enable_mask,
              0x2u);

    write_dist32(dut, GICD_CTLR, 0x52u);
    EXPECT_EQ(read_dist32(dut, GICD_CTLR) & group_enable_mask, 0x2u);
    EXPECT_EQ(access_dist_view32(
                  dut, 1, GICD_CTLR, tlm::TLM_READ_COMMAND) &
                  group_enable_mask,
              0u);
    EXPECT_EQ(access_dist_view32(
                  dut, 2, GICD_CTLR, tlm::TLM_READ_COMMAND) &
                  group_enable_mask,
              0x2u);

    access_dist_view32(
        dut, 2, GICD_CTLR, tlm::TLM_WRITE_COMMAND, 0u);
    EXPECT_EQ(read_dist32(dut, GICD_CTLR) & group_enable_mask, 0u);
}

TEST(Gicx00MultiviewTest,
     PinView0UsesOneCanonicalBackendForDistributorAndRedistributor)
{
    FunctionalGicBackend backend("gicx00_pin_backend");
    gicx00_multiview dut("gicx00_multiview_pin");
    dut.backend_socket.bind(backend.target_socket);

    backend.read_value = 0x5aa55aa5u;
    uint32_t timed_value = 0;
    tlm::tlm_generic_payload timed_trans;
    timed_trans.set_address(GICD_CTLR);
    timed_trans.set_command(tlm::TLM_READ_COMMAND);
    timed_trans.set_data_length(sizeof(timed_value));
    timed_trans.set_streaming_width(sizeof(timed_value));
    timed_trans.set_data_ptr(
        reinterpret_cast<unsigned char*>(&timed_value));
    timed_trans.set_dmi_allowed(true);
    sc_core::sc_time delay(3, sc_core::SC_NS);
    dut.b_transport_dist(timed_trans, delay);

    EXPECT_EQ(timed_value, 0x5aa55aa5u);
    EXPECT_EQ(delay, sc_core::sc_time(10, sc_core::SC_NS));
    EXPECT_FALSE(timed_trans.is_dmi_allowed());
    std::cout
        << "task11_tlm backend_delay_ns="
        << (sc_core::sc_time(7, sc_core::SC_NS) /
            sc_core::sc_time(1, sc_core::SC_NS))
        << " total_delay_ns="
        << (delay / sc_core::sc_time(1, sc_core::SC_NS))
        << " dmi_allowed=" << timed_trans.is_dmi_allowed() << '\n';
    const auto dist_value = read_dist32(dut, GICD_CTLR);
    const unsigned int after_dist = backend.accesses;
    const auto redist_value = read_redist32(dut, 1, 0x0040);

    EXPECT_EQ(dist_value, 0x5aa55aa5u);
    EXPECT_EQ(redist_value, 0x5aa55aa5u);
    EXPECT_EQ(after_dist, 2u);
    EXPECT_EQ(backend.accesses, 3u);
    EXPECT_EQ(backend.last_address,
              AP_GIC_REDIST_BASE + AP_GIC_REDIST_STRIDE + 0x0040);

    write_spi_view(dut, 105, 1u);
    EXPECT_EQ(iviewr_field(dut, 105), 1u);
    EXPECT_EQ(backend.accesses, 3u);
}

TEST(Gicx00MultiviewTest,
     OwnerMigrationKeepsCanonicalStateAndFiltersMmioAndInjection)
{
    constexpr uint16_t spi = 105;
    constexpr uint32_t bit = 1u << (spi % 32);
    constexpr uint64_t word = (spi / 32) * sizeof(uint32_t);
    StatefulCanonicalGicBackend backend("gicx00_stateful_backend");
    gicx00_multiview dut("gicx00_multiview_policy");
    dut.backend_socket.bind(backend.target_socket);
    dut.spi_out[spi - SPI_MIN].bind(backend);

    write_spi_view(dut, spi, 1u);
    access_dist_view32(
        dut, 1, GICD_ISENABLER + word, tlm::TLM_WRITE_COMMAND, bit);
    access_dist_view32(
        dut, 1, GICD_ISPENDR + word, tlm::TLM_WRITE_COMMAND, bit);
    access_dist_view32(
        dut, 1, GICD_ISACTIVER + word, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_EQ(access_dist_view32(
                  dut, 1, GICD_ISENABLER + word, tlm::TLM_READ_COMMAND) & bit,
              bit);
    EXPECT_EQ(access_dist_view32(
                  dut, 1, GICD_ISPENDR + word, tlm::TLM_READ_COMMAND) & bit,
              bit);
    EXPECT_EQ(access_dist_view32(
                  dut, 1, GICD_ISACTIVER + word, tlm::TLM_READ_COMMAND) & bit,
              bit);

    const unsigned int mutations_before_migration = backend.mutations;
    write_spi_view(dut, spi, 2u);
    EXPECT_EQ(backend.mutations, mutations_before_migration);
    EXPECT_EQ(access_dist_view32(
                  dut, 2, GICD_ISENABLER + word, tlm::TLM_READ_COMMAND) & bit,
              bit);
    EXPECT_EQ(access_dist_view32(
                  dut, 2, GICD_ISPENDR + word, tlm::TLM_READ_COMMAND) & bit,
              bit);
    EXPECT_EQ(access_dist_view32(
                  dut, 2, GICD_ISACTIVER + word, tlm::TLM_READ_COMMAND) & bit,
              bit);

    const unsigned int accesses_before_non_owner = backend.accesses;
    const unsigned int mutations_before_non_owner = backend.mutations;
    EXPECT_EQ(access_dist_view32(
                  dut, 1, GICD_ISENABLER + word, tlm::TLM_READ_COMMAND) & bit,
              0u);
    access_dist_view32(
        dut, 1, GICD_ICENABLER + word, tlm::TLM_WRITE_COMMAND, bit);
    access_dist_view32(
        dut, 1, GICD_ICPENDR + word, tlm::TLM_WRITE_COMMAND, bit);
    access_dist_view32(
        dut, 1, GICD_ICACTIVER + word, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_EQ(backend.accesses, accesses_before_non_owner);
    EXPECT_EQ(backend.mutations, mutations_before_non_owner);

    dut.inject_spi(1, spi, true);
    dut.inject_spi(1, spi, false);
    EXPECT_EQ(backend.asserted, 0u);
    EXPECT_EQ(backend.deasserted, 0u);
    dut.inject_spi(2, spi, true);
    dut.inject_spi(2, spi, false);
    EXPECT_EQ(backend.asserted, 1u);
    EXPECT_EQ(backend.deasserted, 1u);

    write_spi_view(dut, spi, 1u);
    dut.inject_spi(2, spi, true);
    dut.inject_spi(2, spi, false);
    EXPECT_EQ(backend.asserted, 1u);
    EXPECT_EQ(backend.deasserted, 1u);
    dut.inject_spi(1, spi, true);
    dut.inject_spi(1, spi, false);
    EXPECT_EQ(backend.asserted, 2u);
    EXPECT_EQ(backend.deasserted, 2u);

    const bool shared_backend_identity =
        backend.last_mmio_identity == static_cast<const void*>(&backend) &&
        backend.last_signal_identity == static_cast<const void*>(&backend) &&
        backend.last_mmio_identity == backend.last_signal_identity;
    EXPECT_TRUE(shared_backend_identity);

    std::cout
        << "task11_semantics owner_migration=1_to_2 "
        << "canonical_enable=1 canonical_pending=1 canonical_active=1 "
        << "view1_to_view2_delivery=1 view2_to_view1_delivery=1 "
        << "non_owner_delivery_count=0 non_owner_backend_access_delta=0 "
        << "shared_backend_identity=" << shared_backend_identity
        << " mmio_observed=" << (backend.last_mmio_identity != nullptr)
        << " injection_observed=" << (backend.last_signal_identity != nullptr)
        << '\n';
}

TEST(Gicx00MultiviewTest,
     MixedOwnerReplacementWritesPreserveForeignCanonicalState)
{
    constexpr uint16_t owner_spi = 104;
    constexpr uint16_t foreign_spi = 105;
    constexpr uint64_t bitmap_word =
        (owner_spi / 32) * sizeof(uint32_t);
    constexpr uint64_t pair_word =
        (owner_spi / 16) * sizeof(uint32_t);
    constexpr unsigned int bitmap_owner_shift = owner_spi % 32;
    constexpr unsigned int pair_owner_shift = (owner_spi % 16) * 2;
    StatefulCanonicalGicBackend backend("gicx00_mixed_owner_backend");
    gicx00_multiview dut("gicx00_multiview_mixed_owner");
    dut.backend_socket.bind(backend.target_socket);

    write_spi_view(dut, owner_spi, 1u);
    write_spi_view(dut, foreign_spi, 2u);

    const uint32_t bitmap_seed = 0xffffffffu;
    const uint32_t bitmap_write = 0u;
    const uint32_t bitmap_expected =
        bitmap_seed & ~(1u << bitmap_owner_shift);
    write_dist32(dut, GICD_IGROUPR + bitmap_word, bitmap_seed);
    access_dist_view32(dut, 1, GICD_IGROUPR + bitmap_word,
                       tlm::TLM_WRITE_COMMAND, bitmap_write);
    const uint32_t igroupr_result =
        read_dist32(dut, GICD_IGROUPR + bitmap_word);
    EXPECT_EQ(igroupr_result, bitmap_expected);

    write_dist32(dut, GICD_IGRPMODR + bitmap_word, bitmap_seed);
    access_dist_view32(dut, 1, GICD_IGRPMODR + bitmap_word,
                       tlm::TLM_WRITE_COMMAND, bitmap_write);
    const uint32_t igrpmodr_result =
        read_dist32(dut, GICD_IGRPMODR + bitmap_word);
    EXPECT_EQ(igrpmodr_result, bitmap_expected);

    const uint32_t priority_seed = 0x44332211u;
    const uint32_t priority_write = 0xa4a3a2a1u;
    const uint32_t priority_expected = 0x443322a1u;
    write_dist32(dut, GICD_IPRIORITYR + owner_spi, priority_seed);
    access_dist_view32(dut, 1, GICD_IPRIORITYR + owner_spi,
                       tlm::TLM_WRITE_COMMAND, priority_write);
    const uint32_t priority_result =
        read_dist32(dut, GICD_IPRIORITYR + owner_spi);
    EXPECT_EQ(priority_result, priority_expected);

    write_dist32(dut, GICD_IPRIORITYR + owner_spi, priority_seed);
    EXPECT_EQ(debug_write_dist_view32(
                  dut, 1, GICD_IPRIORITYR + owner_spi, priority_write),
              priority_write);
    const uint32_t priority_debug_result =
        read_dist32(dut, GICD_IPRIORITYR + owner_spi);
    EXPECT_EQ(priority_debug_result, priority_expected);

    const uint32_t pair_seed = 0xaaaaaaaau;
    const uint32_t pair_write = 0x55555555u;
    const uint32_t pair_mask = 0x3u << pair_owner_shift;
    const uint32_t pair_expected =
        (pair_seed & ~pair_mask) | (pair_write & pair_mask);
    write_dist32(dut, GICD_ICFGR + pair_word, pair_seed);
    access_dist_view32(dut, 1, GICD_ICFGR + pair_word,
                       tlm::TLM_WRITE_COMMAND, pair_write);
    const uint32_t icfgr_result =
        read_dist32(dut, GICD_ICFGR + pair_word);
    EXPECT_EQ(icfgr_result, pair_expected);

    write_dist32(dut, GICD_NSACR + pair_word, pair_seed);
    access_dist_view32(dut, 1, GICD_NSACR + pair_word,
                       tlm::TLM_WRITE_COMMAND, pair_write);
    const uint32_t nsacr_result =
        read_dist32(dut, GICD_NSACR + pair_word);
    EXPECT_EQ(nsacr_result, pair_expected);

    const uint64_t route_offset = GICD_IROUTER + foreign_spi * sizeof(uint64_t);
    const uint64_t route_seed = 0x1122334455667788ull;
    const uint64_t route_write = 0xaabbccddeeff0011ull;
    write_dist64(dut, route_offset, route_seed);
    const unsigned int accesses_before_foreign_route = backend.accesses;
    const unsigned int mutations_before_foreign_route = backend.mutations;
    const uint64_t route_write_buffer =
        access_dist_view64(dut, 1, route_offset,
                           tlm::TLM_WRITE_COMMAND, route_write);
    EXPECT_EQ(route_write_buffer, route_write);
    EXPECT_EQ(backend.accesses, accesses_before_foreign_route);
    EXPECT_EQ(backend.mutations, mutations_before_foreign_route);
    const uint64_t route_result = read_dist64(dut, route_offset);
    EXPECT_EQ(route_result, route_seed);

    const bool foreign_fields_preserved =
        igroupr_result == bitmap_expected &&
        igrpmodr_result == bitmap_expected &&
        priority_result == priority_expected &&
        priority_debug_result == priority_expected &&
        icfgr_result == pair_expected &&
        nsacr_result == pair_expected &&
        route_write_buffer == route_write &&
        route_result == route_seed;
    EXPECT_TRUE(foreign_fields_preserved);

    std::cout
        << "task11_mixed_owner replacement_registers=6 "
        << "foreign_fields_preserved=" << foreign_fields_preserved << ' '
        << "debug_rmw_preserved="
        << (priority_debug_result == priority_expected) << ' '
        << "foreign_irouter_backend_access_delta="
        << (backend.accesses - accesses_before_foreign_route - 1)
        << " foreign_irouter_mutation_delta="
        << (backend.mutations - mutations_before_foreign_route) << '\n';
}

TEST(Gicx00MultiviewTest,
     ReplacementRmwReadFailurePreservesPayloadAndDelay)
{
    constexpr uint16_t owner_spi = 104;
    constexpr uint16_t foreign_spi = 105;
    constexpr uint64_t offset = GICD_IPRIORITYR + owner_spi;
    constexpr uint32_t seed = 0x44332211u;
    constexpr uint32_t write_value = 0xa4a3a2a1u;
    StatefulCanonicalGicBackend backend("gicx00_rmw_failure_backend");
    gicx00_multiview dut("gicx00_multiview_rmw_failure");
    dut.backend_socket.bind(backend.target_socket);

    write_spi_view(dut, owner_spi, 1u);
    write_spi_view(dut, foreign_spi, 2u);
    write_dist32(dut, offset, seed);

    uint32_t payload_value = write_value;
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(payload_value));
    trans.set_streaming_width(sizeof(payload_value));
    trans.set_data_ptr(
        reinterpret_cast<unsigned char*>(&payload_value));
    trans.set_dmi_allowed(true);
    backend.fail_next_read = true;
    sc_core::sc_time delay(3, sc_core::SC_NS);
    dut.b_transport_dist_view(1, trans, delay);

    EXPECT_EQ(trans.get_response_status(),
              tlm::TLM_GENERIC_ERROR_RESPONSE);
    EXPECT_EQ(trans.get_command(), tlm::TLM_WRITE_COMMAND);
    EXPECT_EQ(trans.get_address(), offset);
    EXPECT_EQ(payload_value, write_value);
    EXPECT_EQ(delay, sc_core::sc_time(10, sc_core::SC_NS));
    EXPECT_FALSE(trans.is_dmi_allowed());
    const uint32_t canonical_value = read_dist32(dut, offset);
    EXPECT_EQ(canonical_value, seed);

    const bool payload_restored =
        trans.get_command() == tlm::TLM_WRITE_COMMAND &&
        trans.get_address() == offset && payload_value == write_value;
    std::cout
        << "task11_rmw_failure response=generic_error "
        << "payload_restored=" << payload_restored
        << " backend_delay_ns=7 total_delay_ns="
        << (delay / sc_core::sc_time(1, sc_core::SC_NS))
        << " dmi_allowed=" << trans.is_dmi_allowed()
        << " canonical_state_preserved=" << (canonical_value == seed)
        << '\n';
}

TEST(Gicx00MultiviewTest, StoresTwoBitDistributorViewFields)
{
    gicx00_multiview dut("gicx00_multiview_dist");

    write_dist32(dut, GICD_CTLR, 0x7u);
    write_dist32(dut, iviewr_offset(SPI_MIN), 0xffffffffu);

    EXPECT_EQ(read_dist32(dut, GICD_CTLR), 0u);
    EXPECT_EQ(iviewr_field(dut, SPI_MIN), 0x3u);
    EXPECT_EQ(iviewr_field(dut, SPI_MIN + 1), 0x3u);

    write_spi_view(dut, SPI_MIN, 1u);
    write_spi_view(dut, SPI_MIN + 1, 2u);
    write_spi_view(dut, SPI_MIN + 2, 3u);

    EXPECT_EQ(iviewr_field(dut, SPI_MIN), 1u);
    EXPECT_EQ(iviewr_field(dut, SPI_MIN + 1), 2u);
    EXPECT_EQ(iviewr_field(dut, SPI_MIN + 2), 3u);
    EXPECT_EQ(iviewr_field(dut, SPI_MIN + 3), 3u);
}

TEST(Gicx00MultiviewTest, CoversApolloSpiRangeAndRazwiOutsideIt)
{
    gicx00_multiview dut("gicx00_multiview_spi_range");

    write_spi_view(dut, SPI_MIN, 1u);
    write_spi_view(dut, SPI_LIMIT - 1, 2u);
    write_dist32(dut, GICD_IVIEWR_BASE + sizeof(uint32_t), 0xffffffffu);
    write_dist32(dut, GICD_IVIEWR_BASE + ((SPI_LIMIT / 16) * sizeof(uint32_t)),
                 0xffffffffu);

    EXPECT_EQ(iviewr_field(dut, SPI_MIN), 1u);
    EXPECT_EQ(iviewr_field(dut, SPI_LIMIT - 1), 2u);
    EXPECT_EQ(read_dist32(dut, GICD_IVIEWR_BASE + sizeof(uint32_t)), 0u);
    EXPECT_EQ(read_dist32(
                  dut, GICD_IVIEWR_BASE + ((SPI_LIMIT / 16) * sizeof(uint32_t))),
              0u);
}

TEST(Gicx00MultiviewTest, CoversApolloApAndSafetyIslandViewTables)
{
    gicx00_multiview dut("gicx00_multiview_apollo_views");

    constexpr std::array<SpiView, 20> ap_view1 {{
        { 81, 1 }, { 82, 1 }, { 84, 1 }, { 89, 1 }, { 97, 1 },
        { 144, 1 }, { 145, 1 }, { 248, 1 }, { 249, 1 }, { 250, 1 },
        { 251, 1 }, { 289, 1 }, { 290, 1 }, { 291, 1 }, { 292, 1 },
        { 293, 1 }, { 295, 1 }, { 300, 1 }, { 152, 1 }, { 153, 1 },
    }};
    constexpr std::array<SpiView, 19> si_cl0_view1 {{
        { 34, 1 }, { 37, 1 }, { 40, 1 }, { 97, 1 }, { 99, 1 },
        { 103, 1 }, { 107, 1 }, { 105, 1 }, { 128, 1 }, { 129, 1 },
        { 325, 1 }, { 327, 1 }, { 329, 1 }, { 331, 1 }, { 288, 1 },
        { 360, 1 }, { 362, 1 }, { 364, 1 }, { 366, 1 },
    }};
    constexpr std::array<SpiView, 6> si_cl1_view2 {{
        { 33, 2 }, { 36, 2 }, { 39, 2 }, { 72, 2 }, { 73, 2 }, { 82, 2 },
    }};

    for (const auto& entry : ap_view1) {
        write_spi_view(dut, entry.spi, entry.view);
        EXPECT_EQ(iviewr_field(dut, entry.spi), entry.view) << entry.spi;
    }
    for (const auto& entry : si_cl0_view1) {
        write_spi_view(dut, entry.spi, entry.view);
        EXPECT_EQ(iviewr_field(dut, entry.spi), entry.view) << entry.spi;
    }
    for (const auto& entry : si_cl1_view2) {
        write_spi_view(dut, entry.spi, entry.view);
        EXPECT_EQ(iviewr_field(dut, entry.spi), entry.view) << entry.spi;
    }
}

TEST(Gicx00MultiviewTest, RetainsOnlyRedistributorViewOwnershipMetadata)
{
    gicx00_multiview dut("gicx00_multiview_redist");

    write_redist32(dut, 0, GICR_PWRR, 0xffffffffu);
    write_redist32(dut, 0, GICR_VIEWR, 0xffffffffu);
    write_redist32(dut, 4, GICR_VIEWR, 0x2u);
    write_redist32(dut, 15, GICR_VIEWR, 0x1u);
    write_redist32(dut, 0, GICR_FLUSHR, 0xffffffffu);

    EXPECT_EQ(read_redist32(dut, 0, GICR_PWRR) & 0x1u, 0x0u);
    EXPECT_EQ(read_redist32(dut, 0, GICR_VIEWR), 0x3u);
    EXPECT_EQ(read_redist32(dut, 4, GICR_VIEWR), 0x2u);
    EXPECT_EQ(read_redist32(dut, 15, GICR_VIEWR), 0x1u);
    EXPECT_EQ(read_redist32(dut, 0, GICR_FLUSHR), 0u);
}

TEST(Gicx00MultiviewTest,
     RedistributorOwnerMigrationUsesSameCanonicalBackend)
{
    FunctionalGicBackend backend("gicx00_redist_policy_backend");
    gicx00_multiview dut("gicx00_redist_policy");
    dut.backend_socket.bind(backend.target_socket);
    backend.read_value = 0xc35aa53cu;

    write_redist32(dut, 1, GICR_VIEWR, 1u);
    EXPECT_EQ(access_redist_view32(
                  dut, 1, 1, 0x0040, tlm::TLM_READ_COMMAND),
              0xc35aa53cu);
    EXPECT_EQ(backend.accesses, 1u);

    write_redist32(dut, 1, GICR_VIEWR, 2u);
    EXPECT_EQ(backend.accesses, 1u);
    EXPECT_EQ(access_redist_view32(
                  dut, 2, 1, 0x0040, tlm::TLM_READ_COMMAND),
              0xc35aa53cu);
    EXPECT_EQ(backend.accesses, 2u);

    EXPECT_EQ(access_redist_view32(
                  dut, 1, 1, 0x0040, tlm::TLM_READ_COMMAND),
              0u);
    access_redist_view32(
        dut, 1, 1, 0x0040, tlm::TLM_WRITE_COMMAND, 0xffffffffu);
    EXPECT_EQ(backend.accesses, 2u);

    std::cout
        << "task11_redist owner_migration=1_to_2 "
        << "canonical_readback=0xc35aa53c "
        << "non_owner_backend_access_delta=0\n";
}

TEST(Gicx00MultiviewTest, ContiguousViewSocketPreservesEachRedistributorAffinity)
{
    FunctionalGicBackend backend("gic_ae_multipe_backend");
    gicx00_multiview dut("gic_ae_multipe_front");
    dut.backend_socket.bind(backend.target_socket);
    dut.p_backend_redist_count = 4;
    dut.p_view_redist_stride = AP_GIC_REDIST_STRIDE;
    for (unsigned int cpu = 0; cpu < 4; ++cpu) {
        write_redist32(dut, cpu, GICR_VIEWR, 1u);
    }
    for (bool debug : {false, true}) {
        for (unsigned int cpu = 0; cpu < 4; ++cpu) {
            const uint64_t expected = (static_cast<uint64_t>(cpu << 8) << 32) |
                (cpu << 8) | GICR_TYPER_FEATURES | GICR_TYPER_LAST;
            backend.read_value = expected;
            uint64_t value = 0;
            tlm::tlm_generic_payload trans;
            trans.set_address(cpu * AP_GIC_REDIST_STRIDE + GICR_TYPER);
            trans.set_command(tlm::TLM_READ_COMMAND);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
            trans.set_data_length(8);
            trans.set_streaming_width(8);
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            if (debug) dut.transport_dbg_view1_redists(trans);
            else dut.b_transport_view1_redists(trans, delay);
            EXPECT_TRUE(trans.is_response_ok());
            EXPECT_EQ(value, expected);
            EXPECT_EQ(backend.last_address,
                      AP_GIC_REDIST_BASE + cpu * AP_GIC_REDIST_STRIDE + GICR_TYPER);
            EXPECT_EQ(trans.get_address(), cpu * AP_GIC_REDIST_STRIDE + GICR_TYPER);
        }
    }
}

TEST(Gicx00MultiviewTest, RejectsInvalidAccesses)
{
    gicx00_multiview dut("gicx00_multiview_invalid");
    uint32_t value = 0;
    uint8_t bytes[3] {};

    EXPECT_EQ(raw_access(dut, false, 16, GICR_VIEWR, tlm::TLM_READ_COMMAND,
                         &value),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(raw_access(dut, true, 0, 0x80000, tlm::TLM_READ_COMMAND, &value),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);

    tlm::tlm_generic_payload trans;
    trans.set_address(GICD_CTLR);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(sizeof(bytes));
    trans.set_streaming_width(sizeof(bytes));
    trans.set_data_ptr(bytes);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_dist(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);

    value = 0;
    EXPECT_EQ(raw_access(dut, true, 0, GICD_CTLR, tlm::TLM_IGNORE_COMMAND,
                         &value),
              tlm::TLM_COMMAND_ERROR_RESPONSE);
}

TEST(Gicx00MultiviewTest, ViewPolicyFailsClosedForMalformedAccesses)
{
    FunctionalGicBackend backend("gicx00_malformed_backend");
    gicx00_multiview dut("gicx00_multiview_malformed");
    dut.backend_socket.bind(backend.target_socket);
    uint32_t value = 0xffffffffu;
    uint8_t short_data[3] {};
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    trans.set_address(GICD_ISENABLER);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    dut.b_transport_dist_view(3, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backend.accesses, 0u);

    trans.set_address(GICD_ISENABLER + 1);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    dut.b_transport_dist_view(1, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backend.accesses, 0u);

    trans.set_address(GICD_ISENABLER);
    trans.set_data_length(sizeof(short_data));
    trans.set_streaming_width(sizeof(short_data));
    trans.set_data_ptr(short_data);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    dut.b_transport_dist_view(1, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backend.accesses, 0u);

    value = 0xffffffffu;
    trans.set_address(GICD_ISENABLER + (31 * sizeof(uint32_t)));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    dut.b_transport_dist_view(1, trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(value, 0u);
    EXPECT_EQ(backend.accesses, 0u);

    EXPECT_FALSE(dut.inject_spi(0, SPI_MIN, true));
    EXPECT_FALSE(dut.inject_spi(3, SPI_MIN, true));
    EXPECT_FALSE(dut.inject_spi(1, SPI_LIMIT, true));

    std::cout
        << "task11_malformed invalid_view=fail_closed "
        << "unaligned=fail_closed unsupported_length=fail_closed "
        << "out_of_range_intid=raz_wi backend_access_delta=0\n";
}

TEST(Gicx00MultiviewTest, DebugTransportReadsAndWritesRegisters)
{
    gicx00_multiview dut("gicx00_multiview_debug");
    uint32_t value = 0x00000002u;
    tlm::tlm_generic_payload trans;

    trans.set_address(GICR_VIEWR);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    EXPECT_EQ(dut.transport_dbg_redist(3, trans), sizeof(value));
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(read_redist32(dut, 3, GICR_VIEWR), 2u);

    value = 0;
    trans.set_command(tlm::TLM_READ_COMMAND);
    EXPECT_EQ(dut.transport_dbg_redist(3, trans), sizeof(value));
    EXPECT_EQ(value, 2u);

    trans.set_address(0x40000);
    EXPECT_EQ(dut.transport_dbg_redist(3, trans), 0u);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
}

TEST(Gicx00MultiviewTest, AllocatedFrameReservedTailsAreRazWi)
{
    gicx00_multiview dut("gicx00_multiview_reserved_tails");

    write_dist32(dut, 0x7fffcu, 0xffffffffu);
    EXPECT_EQ(read_dist32(dut, 0x7fffcu), 0u);

    write_redist32(dut, 0, 0x3fffcu, 0xffffffffu);
    EXPECT_EQ(read_redist32(dut, 0, 0x3fffcu), 0u);
}

TEST(Gicx00MultiviewTest,
     InactiveCanonicalRegionsReportFootprintAndTerminateIndividually)
{
    gicx00_multiview dut("gicx00_multiview_inactive_redists");
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_generic_payload trans;
    uint32_t value = 0xffffffffu;

    trans.set_address(GICR_PIDR2);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    dut.b_transport_inactive_redists(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(value, 0x4bu);

    uint64_t typer = 0;
    trans.set_address(GICR_TYPER);
    trans.set_data_length(sizeof(typer));
    trans.set_streaming_width(sizeof(typer));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&typer));
    dut.b_transport_inactive_redists(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(typer >> 32, 0x10000u);
    EXPECT_EQ(typer & GICR_TYPER_FEATURES, GICR_TYPER_FEATURES);
    EXPECT_NE(typer & GICR_TYPER_LAST, 0u);

    typer = 0;
    trans.set_address((11 * AP_GIC_REDIST_STRIDE) + GICR_TYPER);
    dut.b_transport_inactive_redists(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(typer >> 32, 0x30300u);
    EXPECT_NE(typer & GICR_TYPER_LAST, 0u);

    value = 0xa5a5a5a5u;
    trans.set_address(0x2ffffcu);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    dut.b_transport_inactive_redists(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);

    value = 0xffffffffu;
    trans.set_command(tlm::TLM_READ_COMMAND);
    dut.b_transport_inactive_redists(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(value, 0u);
}

TEST(Gicx00MultiviewTest, NarrowExtensionWindowsTranslateRelativeOffsets)
{
    gicx00_multiview dut("gicx00_multiview_windows");
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_generic_payload trans;

    uint64_t cfgid = 0;
    trans.set_address(0);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(sizeof(cfgid));
    trans.set_streaming_width(sizeof(cfgid));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&cfgid));
    dut.b_transport_dist_cfgid(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(cfgid & GICD_CFGID_VIEW, GICD_CFGID_VIEW);

    uint32_t iviewr = 1u << iviewr_shift(105);
    trans.set_address(iviewr_offset(105) - GICD_IVIEWR_BASE);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(iviewr));
    trans.set_streaming_width(sizeof(iviewr));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&iviewr));
    dut.b_transport_dist_iviewr(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(iviewr_field(dut, 105), 1u);

    uint32_t viewr = 2u;
    trans.set_address(0);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(viewr));
    trans.set_streaming_width(sizeof(viewr));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&viewr));
    dut.b_transport_redist0_viewr(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(read_redist32(dut, 0, GICR_VIEWR), 2u);
}

TEST(Gicx00MultiviewTest, BackendHintsCannotBypassContainmentAndRmwRetainsSecurity)
{
    struct MemoryManager : tlm::tlm_mm_interface {
        void free(tlm::tlm_generic_payload*) override {}
    } manager;
    FunctionalGicBackend backend("gic_ae_hint_backend");
    gicx00_multiview dut("gic_ae_hint_front");
    dut.backend_socket.bind(backend.target_socket);
    backend.export_hint = true;
    uint32_t value = 0;
    qemu::MemoryRegionOps::MemTxAttrs attrs;
    attrs.secure = true;
    QemuMemTxAttrsTlmExtension attrs_ext(attrs);
    tlm::tlm_generic_payload trans;
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_extension(&attrs_ext);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    for (bool managed : {false, true}) {
        trans.set_mm(managed ? &manager : nullptr);
        for (bool debug : {false, true}) {
            for (bool dist : {false, true}) {
                trans.set_address(dist ? 0x104 : 0x10100);
                if (debug) {
                    if (dist) dut.transport_dbg_dist(trans);
                    else dut.transport_dbg_redist(0, trans);
                } else {
                    if (dist) dut.b_transport_dist(trans, delay);
                    else dut.b_transport_redist(0, trans, delay);
                }
                EXPECT_TRUE(trans.is_response_ok());
                EXPECT_FALSE(trans.is_dmi_allowed());
                EXPECT_EQ(trans.get_extension<QemuMrHintTlmExtension>(), nullptr);
            }
        }
    }
    trans.set_mm(nullptr);
    trans.set_address(0xe204);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    value = 1;
    dut.b_transport_dist(trans, delay);
    EXPECT_TRUE(trans.is_response_ok());
    EXPECT_GT(backend.accesses, 4u);
    EXPECT_EQ(backend.secure_accesses, backend.accesses);
    EXPECT_EQ(trans.get_extension<QemuMemTxAttrsTlmExtension>(), &attrs_ext);
    EXPECT_EQ(trans.get_extension<QemuMrHintTlmExtension>(), nullptr);
    trans.clear_extension(&attrs_ext);
}

TEST(Gicx00MultiviewTest, SpiErrorContainsBackendUntilSoftwareReprograms)
{
    StatefulCanonicalGicBackend backend("gic_ae_spi_backend");
    gicx00_multiview dut("gic_ae_spi");
    dut.backend_socket.bind(backend.target_socket);
    const unsigned int intid = 105;
    const uint32_t bit = 1u << (intid % 32);
    const uint64_t word = (intid / 32) * 4;
    write_spi_view(dut, intid, 1);
    write_dist32(dut, 0x100 + word, bit);
    EXPECT_TRUE(backend.enabled[intid]);
    access_dist_view32(dut, 1, 0xe200 + word, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_FALSE(backend.enabled[intid]);
    EXPECT_TRUE(backend.pending[intid]);
    EXPECT_EQ(read_dist32(dut, 0xe100 + word), bit);
    EXPECT_EQ(read_dist32(dut, 0xe180 + word), bit);
    EXPECT_EQ(read_dist32(dut, 0xf200 + word), 0u);
    write_dist32(dut, 0x100 + word, bit);
    EXPECT_FALSE(backend.enabled[intid]);
    access_dist_view32(dut, 2, 0xe100 + word, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_EQ(read_dist32(dut, 0xe100 + word), bit);
    access_dist_view32(dut, 1, 0xe100 + word, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_EQ(read_dist32(dut, 0xe100 + word), 0u);
    EXPECT_EQ(read_dist32(dut, 0xe180 + word), 0u);
    EXPECT_FALSE(backend.enabled[intid]);
    write_dist32(dut, 0x80 + word, bit);
    write_dist32(dut, 0x280 + word, bit);
    write_dist32(dut, 0x100 + word, bit);
    EXPECT_TRUE(backend.enabled[intid]);
    EXPECT_FALSE(backend.pending[intid]);
    write_dist32(dut, 0x200 + word, bit);
    EXPECT_TRUE(backend.enabled[intid] && backend.pending[intid]);
}

TEST(Gicx00MultiviewTest, ViewZeroErrorRequiresViewRecoveryAndBlocksForeignAccess)
{
    StatefulCanonicalGicBackend backend("gic_ae_view_backend");
    gicx00_multiview dut("gic_ae_view");
    dut.backend_socket.bind(backend.target_socket);
    const uint32_t bit = 1u << 9;
    write_spi_view(dut, 105, 1);
    write_dist32(dut, 0xe20c, bit);
    EXPECT_EQ(read_dist32(dut, 0xf20c), bit);
    EXPECT_EQ(access_dist_view32(dut, 1, 0xe10c, tlm::TLM_READ_COMMAND), 0u);
    access_dist_view32(dut, 1, 0xf20c, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_EQ(read_dist32(dut, 0xf20c), bit);
    write_dist32(dut, 0xf20c, bit);
    EXPECT_EQ(iviewr_field(dut, 105), 0u);
    write_spi_view(dut, 105, 1);
    EXPECT_EQ(access_dist_view32(dut, 1, 0xe10c, tlm::TLM_READ_COMMAND), bit);
    access_dist_view32(dut, 1, 0xe10c, tlm::TLM_WRITE_COMMAND, bit);
    EXPECT_EQ(read_dist32(dut, 0xe10c), 0u);
    write_dist32(dut, 0x10c, bit);
    EXPECT_TRUE(backend.enabled[105]);
}

TEST(Gicx00MultiviewTest, NonSecureErrorClearRequiresSecureGroupRecovery)
{
    StatefulCanonicalGicBackend backend("gic_ae_security_backend");
    gicx00_multiview dut("gic_ae_security");
    dut.backend_socket.bind(backend.target_socket);
    const uint32_t bit = 1u << 9;
    write_spi_view(dut, 105, 1);
    access_dist_view32(dut, 1, 0xe20c, tlm::TLM_WRITE_COMMAND, bit);
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    uint32_t value = bit;
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0xe10c);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_dist_view1(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(read_dist32(dut, 0xe10c), bit);
    access_dist_view32(dut, 1, 0xe18c, tlm::TLM_WRITE_COMMAND, bit);
    write_dist32(dut, 0x8c, bit);
    dut.b_transport_dist_view1(trans, delay);
    EXPECT_EQ(read_dist32(dut, 0xe10c), 0u);
    trans.set_address(0xe20c);
    dut.b_transport_dist_view1(trans, delay);
    EXPECT_EQ(read_dist32(dut, 0xe10c), 0u);
    trans.clear_extension(&extension);
}

TEST(Gicx00MultiviewTest, LocalInterruptErrorPreventsBackendReenable)
{
    StatefulCanonicalGicBackend backend("gic_ae_ppi_backend");
    gicx00_multiview dut("gic_ae_ppi");
    dut.backend_socket.bind(backend.target_socket);
    const uint32_t bits = (1u << 27) | (1u << 3);
    write_redist32(dut, 1, 0x10100, bits);
    EXPECT_EQ(backend.local_enable[1], bits);
    write_redist32(dut, 1, 0x1c180, bits);
    EXPECT_EQ(read_redist32(dut, 1, 0x1c100), bits);
    EXPECT_EQ(backend.local_enable[1], 0u);
    EXPECT_EQ(backend.local_pending[1], bits);
    write_redist32(dut, 1, 0x10100, bits);
    EXPECT_EQ(backend.local_enable[1], 0u);
    write_redist32(dut, 1, 0x1c100, bits);
    EXPECT_EQ(read_redist32(dut, 1, 0x1c100), 0u);
    EXPECT_EQ(backend.local_enable[1], 0u);
    write_redist32(dut, 1, 0x10100, bits);
    EXPECT_EQ(backend.local_enable[1], bits);
    write_dist32(dut, 0x20, ~0u);
    EXPECT_EQ(read_dist32(dut, 0x20), 0x04f30000u);
    write_redist32(dut, 1, 0x20, ~0u);
    EXPECT_EQ(read_redist32(dut, 1, 0x20), 0x61eu);
}

TEST(Gicx00MultiviewTest, SecureViewZeroAndOwningViewFctlrAccess)
{
    FunctionalGicBackend backend("gic_access_backend");
    gicx00_multiview dut("gic_access");
    dut.backend_socket.bind(backend.target_socket);
    write_redist32(dut, 0, GICR_VIEWR, 1);
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    uint32_t value = ~0u;
    tlm::tlm_generic_payload trans;
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    const unsigned int before = backend.accesses;
    for (uint64_t offset : {0ull, 0xf608ull, 0x104ull, 0xf000ull}) {
        trans.set_address(offset);
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        value = ~0u;
        dut.b_transport_dist(trans, delay);
        trans.set_command(tlm::TLM_READ_COMMAND);
        dut.b_transport_dist(trans, delay);
        EXPECT_EQ(value, 0u);
    }
    trans.set_address(GICR_VIEWR);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    value = 2;
    dut.b_transport_redist(0, trans, delay);
    EXPECT_EQ(read_redist32(dut, 0, GICR_VIEWR), 1u);
    EXPECT_EQ(backend.accesses, before);
    trans.set_address(0x20);
    value = ~0u;
    dut.b_transport_redist_view(1, 0, trans, delay);
    EXPECT_EQ(access_redist_view32(dut, 1, 0, 0x20, tlm::TLM_READ_COMMAND), 0u);
    trans.clear_extension(&extension);
    access_redist_view32(dut, 1, 0, 0x20, tlm::TLM_WRITE_COMMAND, 0x612);
    EXPECT_EQ(access_redist_view32(dut, 1, 0, 0x20, tlm::TLM_READ_COMMAND), 0x612u);
    EXPECT_EQ(access_redist_view32(dut, 2, 0, 0x20, tlm::TLM_READ_COMMAND), 0u);
}

TEST(Gicx00MultiviewTest, SpiErrorRecoveryResetsNsacrDespiteFaultOverride)
{
    StatefulCanonicalGicBackend backend("gic_nsacr_backend");
    gicx00_multiview dut("gic_nsacr");
    dut.backend_socket.bind(backend.target_socket);
    write_spi_view(dut, 105, 1);
    write_dist32(dut, 0x20, 3u << 16);
    access_dist_view32(dut, 1, 0xe20c, tlm::TLM_WRITE_COMMAND, 1u << 9);
    EXPECT_EQ((read_dist32(dut, 0xe18) >> 18) & 3u, 3u);
    access_dist_view32(dut, 1, 0xe18c, tlm::TLM_WRITE_COMMAND, 1u << 9);
    EXPECT_EQ((read_dist32(dut, 0xe18) >> 18) & 3u, 0u);
}

TEST(Gicx00MultiviewTest, LocalErrorRecoveryUsesOwnerDefaultPrioritiesOnly)
{
    StatefulCanonicalGicBackend backend("gic_priority_backend");
    gicx00_multiview dut("gic_priority");
    dut.backend_socket.bind(backend.target_socket);
    write_redist32(dut, 0, GICR_VIEWR, 1);
    write_redist32(dut, 1, GICR_VIEWR, 2);
    const uint32_t bits = (1u << 16) | (1u << 17) | (1u << 18);
    access_redist_view32(dut, 1, 0, 0x1c018, tlm::TLM_WRITE_COMMAND, 0x382818);
    access_redist_view32(dut, 2, 1, 0x1c018, tlm::TLM_WRITE_COMMAND, 0x685848);
    write_redist32(dut, 0, 0x10080, 1u << 17);
    write_redist32(dut, 0, 0x10d00, 1u << 18);
    write_redist32(dut, 0, 0x10c04, 0x2a);
    write_redist32(dut, 0, 0x10410, 0x88776655);
    write_redist32(dut, 0, 0x1c180, bits);
    write_redist32(dut, 0, 0x1c100, bits);
    EXPECT_EQ(read_redist32(dut, 0, 0x10410), 0x88382818u);
    EXPECT_EQ(read_redist32(dut, 0, 0x10080), 1u << 17);
    EXPECT_EQ(read_redist32(dut, 0, 0x10d00), 1u << 18);
    EXPECT_EQ(read_redist32(dut, 0, 0x10c04), 0x2au);
    write_redist32(dut, 0, 0x1c180, bits | (1u << 27));
    write_redist32(dut, 0, 0x10410, 0x40302010);
    EXPECT_EQ(read_redist32(dut, 0, 0x1c100), 1u << 27);
    EXPECT_EQ(read_redist32(dut, 0, 0x10410), 0x40302010u);
}

class SplitSiGicBackend : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<SplitSiGicBackend, DEFAULT_TLM_BUSWIDTH> target;
    StatefulCanonicalGicBackend cl0, cl1;
    explicit SplitSiGicBackend(sc_core::sc_module_name name)
        : sc_module(name), target("target"), cl0("cl0"), cl1("cl1")
    { target.register_b_transport(this, &SplitSiGicBackend::transport); }
    void transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const uint64_t original = trans.get_address();
        if (original >= 0x21800000 && original < 0x21810000) {
            trans.set_address(AP_GIC_DIST_BASE + original - 0x21800000);
            cl1.b_transport(trans, delay);
        } else if (original >= AP_GIC_REDIST_BASE + AP_GIC_REDIST_STRIDE) {
            trans.set_address(original - AP_GIC_REDIST_STRIDE);
            cl1.b_transport(trans, delay);
        } else cl0.b_transport(trans, delay);
        trans.set_address(original);
    }
};

TEST(Gicx00MultiviewTest, SplitSiBackendsContainAndRecoverTheirOwnedInterrupts)
{
    SplitSiGicBackend backend("gic_si_split_backend");
    gicx00_multiview dut("gic_si_split");
    dut.p_backend_view2_dist_base = 0x21800000;
    dut.p_backend_redist_count = 5;
    dut.backend_socket.bind(backend.target);
    write_spi_view(dut, 33, 2);
    write_spi_view(dut, 34, 1);
    write_redist32(dut, 0, GICR_VIEWR, 1);
    write_redist32(dut, 1, GICR_VIEWR, 2);
    write_dist32(dut, 0x104, 6);
    EXPECT_TRUE(backend.cl0.enabled[34]);
    EXPECT_FALSE(backend.cl0.enabled[33]);
    EXPECT_TRUE(backend.cl1.enabled[33]);
    EXPECT_FALSE(backend.cl1.enabled[34]);
    EXPECT_EQ(read_dist32(dut, 0x104), 6u);
    write_dist32(dut, 0xe204, 2);
    EXPECT_TRUE(backend.cl0.enabled[34]);
    EXPECT_FALSE(backend.cl1.enabled[33]);
    access_dist_view32(dut, 2, 0x104, tlm::TLM_WRITE_COMMAND, 2);
    EXPECT_FALSE(backend.cl1.enabled[33]);
    write_dist32(dut, 0xf204, 2);
    write_spi_view(dut, 33, 2);
    access_dist_view32(dut, 2, 0xe104, tlm::TLM_WRITE_COMMAND, 2);
    access_dist_view32(dut, 2, 0x104, tlm::TLM_WRITE_COMMAND, 2);
    EXPECT_TRUE(backend.cl1.enabled[33]);
    access_redist_view32(dut, 2, 1, 0x10100, tlm::TLM_WRITE_COMMAND, 1u << 27);
    EXPECT_EQ(backend.cl1.local_enable[0], 1u << 27);
    EXPECT_EQ(backend.cl0.local_enable[0], 0u);
    write_dist32(dut, 0, 7);
    access_dist_view32(dut, 1, 0, tlm::TLM_WRITE_COMMAND, 1);
    access_dist_view32(dut, 2, 0, tlm::TLM_WRITE_COMMAND, 2);
    EXPECT_EQ(backend.cl0.distributor[0] & 7u, 1u);
    EXPECT_EQ(backend.cl1.distributor[0] & 7u, 2u);
}

// A passive interface records commands without notifying a kernel event during
// pre-elaboration unit traffic; the CPU fixture verifies the active IRQ path.
class GicRecordedIrq : public sc_core::sc_signal_inout_if<uint64_t>
{
    uint64_t m_value = 0;
    sc_core::sc_event m_event;
public:
    std::vector<uint64_t> commands;
    const sc_core::sc_event& default_event() const override { return m_event; }
    const sc_core::sc_event& value_changed_event() const override { return m_event; }
    const uint64_t& read() const override { return m_value; }
    const uint64_t& get_data_ref() const override { return m_value; }
    bool event() const override { return false; }
    void write(const uint64_t& value) override { m_value = value; commands.push_back(value); }
};

TEST(Gicx00MultiviewTest, DiagnosticRamInjectionReportsRasWithoutContainingInterrupts)
{
    StatefulCanonicalGicBackend backend("gic_ras_backend");
    gicx00_multiview dut("gic_ras");
    dut.backend_socket.bind(backend.target_socket);
    GicRecordedIrq sink;
    auto& commands = sink.commands;
    dut.internal_spi.bind(sink);
    write_spi_view(dut, 105, 1);
    write_dist32(dut, 0x10c, 1u << 9);
    write_dist32(dut, 0x2e800, 104);
    write_dist32(dut, 0x20048, 0x100); // Correctable SPI record CFI
    const uint64_t single = (1ull << 63) | (1ull << 15) | (36ull << 32) | 7;
    write_dist64(dut, 0xea10, single); // SPI1 row 36 = INTID105
    EXPECT_EQ(read_dist64(dut, 0xea10) >> 61, 0u);
    EXPECT_EQ(read_dist32(dut, 0x20050) & 0x7e000000u, 0x46000000u);
    EXPECT_EQ(read_dist32(dut, 0x20060), (7u << 10) | 73u);
    EXPECT_EQ(read_dist32(dut, 0x2e000), 2u);
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands.back(), (1ull << 32) | (104 - 32));
    EXPECT_TRUE(backend.enabled[105]);
    EXPECT_EQ(read_dist32(dut, 0xe10c), 0u);
    write_dist32(dut, 0x20050, 0xffffffff);
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_EQ(commands.back(), 104u - 32);
    write_dist64(dut, 0xea10, single | (1ull << 31) | (8ull << 16));
    EXPECT_EQ(read_dist32(dut, 0x20090) & 0x66300000u, 0x64300000u);
    EXPECT_EQ(read_dist32(dut, 0x200a0), 73u);
    EXPECT_TRUE(backend.enabled[105]);
    EXPECT_EQ(read_dist32(dut, 0xe10c), 0u);
}

TEST(Gicx00MultiviewTest, DiagnosticRamBoundsSplitWritesCounterOverflowAndVisibility)
{
    gicx00_multiview dut("gic_ras_bounds");
    const uint64_t discovery = read_dist64(dut, 0xea08);
    EXPECT_TRUE(discovery & (1ull << 60));
    EXPECT_EQ(discovery & 0x1ff, 127u);
    EXPECT_EQ((discovery >> 32) & 0xffff, 479u);
    write_dist32(dut, 0xea08, 0x8000 | 127);
    write_dist32(dut, 0xea0c, 0x80000000);
    EXPECT_EQ(read_dist32(dut, 0x2e000), 2u);
    EXPECT_EQ(access_dist_view32(dut, 1, 0x2e000, tlm::TLM_READ_COMMAND), 0u);
    write_dist32(dut, 0x20050, 0xffffffff);
    write_dist64(dut, 0x20060, 0xffull << 32);
    write_dist64(dut, 0xea08, (1ull << 63) | 0x8000);
    EXPECT_TRUE(read_dist32(dut, 0x20050) & (1u << 27));
    EXPECT_TRUE(read_dist64(dut, 0x20060) & (1ull << 40));
    write_dist64(dut, 0xea08, (1ull << 63) | 0x8080);
    EXPECT_EQ(read_dist64(dut, 0xea08) >> 61, 1u);
    write_dist64(dut, 0xea78, (1ull << 63) | 0x8000); // absent multi-chip CC RAM
    EXPECT_EQ(read_dist64(dut, 0xea78) >> 61, 1u);
    EXPECT_FALSE(read_dist64(dut, 0xea78) & (1ull << 60));
    access<uint64_t>(dut, false, 0, 0x1f010, tlm::TLM_WRITE_COMMAND,
                     (1ull << 63) | 0x8000);
    EXPECT_TRUE(read_dist32(dut, 0x2e000) & (1u << 7));
    EXPECT_EQ(read_redist32(dut, 0, 0x1c100), 0u);
}

TEST(Gicx00MultiviewTest, SgiDistributionFaultUsesBankedDefaultsAndExplicitRecovery)
{
    StatefulCanonicalGicBackend backend("gic_sgi_dist_backend");
    gicx00_multiview dut("gic_sgi_dist");
    dut.backend_socket.bind(backend.target_socket);
    write_redist32(dut, 0, GICR_VIEWR, 1);
    access<uint64_t>(dut, false, 0, 0x1c010, tlm::TLM_WRITE_COMMAND, 0x5000);
    // The PE belongs to view 1: view 0's default must not be used.
    access_redist_view32(dut, 1, 0, 0x1c010, tlm::TLM_WRITE_COMMAND, 0x3000);
    write_redist32(dut, 0, 0x10100, 8);
    uint32_t bits = 8;
    tlm::tlm_generic_payload fault;
    fault.set_command(tlm::TLM_WRITE_COMMAND);
    fault.set_address(0);
    fault.set_data_ptr(reinterpret_cast<unsigned char*>(&bits));
    fault.set_data_length(4);
    fault.set_streaming_width(4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_sgi_ram_fault(fault, delay);
    ASSERT_EQ(fault.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(read_redist32(dut, 0, 0x1c008), 8u);
    EXPECT_EQ(read_redist32(dut, 0, 0x10080), 8u);
    EXPECT_EQ(read_redist32(dut, 0, 0x10d00), 0u);
    EXPECT_EQ(read_redist32(dut, 0, 0x10e00), 1u << 6);
    EXPECT_EQ(backend.local_enable[0], 0u);
    write_redist32(dut, 0, 0x10100, 8);
    EXPECT_EQ(backend.local_enable[0], 0u);
    EXPECT_TRUE(read_dist32(dut, 0x2e000) & (1u << 4));
    EXPECT_EQ(read_dist32(dut, 0x20110) & 0xffff, 7u); // real-origin record
    write_redist32(dut, 0, 0x1c008, 8);
    EXPECT_EQ(read_redist32(dut, 0, 0x1c008), 0u);
    write_redist32(dut, 0, 0x10100, 8);
    EXPECT_EQ(backend.local_enable[0], 8u);
}

TEST(Gicx00MultiviewTest, ItsInjectionHonorsSacAndPreservesForwardingContracts)
{
    FunctionalGicBackend backend("gic_its_ras_backend");
    gicx00_multiview dut("gic_its_ras");
    dut.p_ras_lpi = true;
    dut.backend_its_socket.bind(backend.target_socket);
    RequestContext context;
    context.secure_valid = true;
    context.secure = false;
    RequestContextTlmExtension extension(context);
    uint64_t value = (1ull << 63) | 0x8000;
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0xc000);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    trans.set_extension(&extension);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_its(trans, delay);
    EXPECT_EQ(read_dist32(dut, 0x2e000), 0u);
    write_dist32(dut, 0x24, 2);
    dut.b_transport_its(trans, delay);
    EXPECT_EQ(read_dist32(dut, 0x2e000), 1u << 25);
    EXPECT_EQ(read_dist32(dut, 0x20650) & 0xffff, 0x106u);
    trans.set_address(0x40);
    trans.set_command(tlm::TLM_READ_COMMAND);
    backend.read_value = 0x1234;
    backend.export_hint = true;
    dut.b_transport_its(trans, delay);
    EXPECT_EQ(value, 0x1234u);
    EXPECT_EQ(backend.accesses, 1u);
    EXPECT_FALSE(trans.is_dmi_allowed());
    EXPECT_EQ(trans.get_extension<QemuMrHintTlmExtension>(), nullptr);
    trans.clear_extension(&extension);
}

TEST(Gicx00MultiviewTest, ItsReservedPageTerminatesBothPublicApertures)
{
    FunctionalGicBackend backend("gic_its_tail_backend");
    gicx00_multiview dut("gic_its_tail");
    dut.p_ras_lpi = true;
    dut.backend_its_socket.bind(backend.target_socket);
    // SI0 ATU D0770000+0x7fffc aliases AP master GIC 2007fffc.
    EXPECT_EQ(read_dist32(dut, 0x7fffc), 0u);
    write_dist32(dut, 0x7fffc, 0xffffffff);
    EXPECT_EQ(read_dist32(dut, 0x7fffc), 0u);
    uint64_t value = ~0ull;
    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(0x3fff8);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(8);
    trans.set_streaming_width(8);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_its(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(value, 0u);
    trans.set_address(0x30000);
    value = ~0ull;
    EXPECT_EQ(dut.transport_dbg_its(trans), 8u);
    EXPECT_EQ(value, 0u);
    EXPECT_EQ(backend.accesses, 0u);
    trans.set_address(0x40000);
    dut.b_transport_its(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(backend.accesses, 0u);
    // The translation page still reaches the backend.
    trans.set_address(0x10040);
    backend.read_value = 0x1234;
    dut.b_transport_its(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(value, 0x1234u);
    EXPECT_EQ(backend.accesses, 1u);
}

class GicFmuInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<GicFmuInitiator, DEFAULT_TLM_BUSWIDTH> socket;
    explicit GicFmuInitiator(sc_core::sc_module_name name) : sc_module(name), socket("socket") {}
    template<typename T> T access(uint64_t address, tlm::tlm_command command, T value = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(command);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(T));
        trans.set_streaming_width(sizeof(T));
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        return value;
    }
    void protected_write(uint64_t address, uint32_t value)
    {
        access<uint32_t>(0xf20, tlm::TLM_WRITE_COMMAND, 0xbe);
        access<uint32_t>(address, tlm::TLM_WRITE_COMMAND, value);
    }
};

// Keep the elaborated path last: all modules and sockets are alive throughout
// simulation, including the optional multi-target System FMU source route.
TEST(Gicx00MultiviewTest, SgiDedSourceReachesLeafAndSystemFmuWithHardwareOrigin)
{
    cci::cci_get_global_broker(cci::cci_originator("gic_fmu_source_test")).set_preset_cci_value(
        "gic_fmu_parent.critical_records", cci::cci_value("0:6,4:0"));
    StatefulCanonicalGicBackend backend("gic_fmu_backend");
    gicx00_multiview dut("gic_fmu_source");
    zena_device_fmu leaf("gic_fmu_leaf");
    zena_fmu parent("gic_fmu_parent");
    GicFmuInitiator leaf_mmio("gic_fmu_leaf_mmio"), parent_mmio("gic_fmu_parent_mmio");
    dut.backend_socket.bind(backend.target_socket);
    dut.fmu_error_socket.bind(leaf.error_socket);
    leaf.fault_socket.bind(parent.device_fault_socket);
    leaf_mmio.socket.bind(leaf.target_socket);
    parent_mmio.socket.bind(parent.target_socket);
    sc_core::sc_signal<bool> leaf_irq("gic_fmu_leaf_irq"), parent_irq("gic_fmu_parent_irq");
    leaf.critical_irq.bind(leaf_irq);
    parent.critical_irq.bind(parent_irq);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    leaf_mmio.protected_write(8, 0x2003);
    write_dist64(dut, 0xea00, (1ull << 63) | 0x80018000);
    EXPECT_EQ(leaf_mmio.access<uint32_t>(0xe00, tlm::TLM_READ_COMMAND), 0u);
    write_dist32(dut, 0x20110, 0xffffffff);
    write_redist32(dut, 0, GICR_VIEWR, 1);
    write_redist32(dut, 0, 0x10100, 8);
    uint32_t bits = 8;
    tlm::tlm_generic_payload source;
    source.set_command(tlm::TLM_WRITE_COMMAND);
    source.set_address(0);
    source.set_data_ptr(reinterpret_cast<unsigned char*>(&bits));
    source.set_data_length(4);
    source.set_streaming_width(4);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport_sgi_ram_fault(source, delay);
    ASSERT_EQ(source.get_response_status(), tlm::TLM_OK_RESPONSE);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    const uint64_t status = leaf_mmio.access<uint64_t>(0x10, tlm::TLM_READ_COMMAND);
    EXPECT_EQ((status >> 8) & 0xff, 50u);
    EXPECT_EQ((status >> 32) & 0xfff, 0u);
    EXPECT_EQ(status & (1ull << 44), 0u);
    EXPECT_TRUE(leaf_irq.read());
    EXPECT_TRUE(parent_irq.read());
    EXPECT_EQ(read_redist32(dut, 0, 0x1c008), 8u);
    EXPECT_EQ(read_redist32(dut, 1, 0x1c008), 8u); // shared RAM row
    leaf_mmio.protected_write(0x10, (1u << 30) | (1u << 29));
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(leaf_irq.read());
    EXPECT_TRUE(parent_irq.read());
    parent_mmio.access<uint32_t>(0x48bfc, tlm::TLM_WRITE_COMMAND, 0xbe);
    parent_mmio.access<uint32_t>(0x40010, tlm::TLM_WRITE_COMMAND, (1u << 30) | (1u << 19));
    parent_mmio.access<uint32_t>(0x8bfc, tlm::TLM_WRITE_COMMAND, 0xbe);
    parent_mmio.access<uint32_t>(6 * 0x40 + 0x10, tlm::TLM_WRITE_COMMAND, (1u << 30) | (1u << 19));
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(parent_irq.read());
    EXPECT_EQ(read_redist32(dut, 0, 0x1c008), 8u);
    write_redist32(dut, 0, 0x1c008, 8);
    EXPECT_EQ(read_redist32(dut, 0, 0x1c008), 0u);
    leaf_mmio.protected_write(0xf00, 50u << 8); // mask only this protection source
    dut.b_transport_sgi_ram_fault(source, delay);
    EXPECT_EQ(leaf_mmio.access<uint32_t>(0xe00, tlm::TLM_READ_COMMAND), 0u);
    dut.reset->write(true);
    leaf.reset_in->write(true);
    parent.reset_in->write(true);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(parent_irq.read());
    EXPECT_EQ(read_dist32(dut, 0x2e000), 0u);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
