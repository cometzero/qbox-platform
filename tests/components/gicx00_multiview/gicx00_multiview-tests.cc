/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <cci/utils/broker.h>
#include <gicx00_multiview.h>
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

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
