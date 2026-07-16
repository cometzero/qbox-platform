/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <array>
#include <cstdint>

#include <cci/utils/broker.h>
#include <gicx00_multiview.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>

namespace {

constexpr uint64_t GICD_CTLR = 0x0000;
constexpr uint64_t GICD_CFGID = 0xf000;
constexpr uint64_t GICD_IVIEWR_BASE = 0xf600;
constexpr uint64_t GICR_PWRR = 0x0024;
constexpr uint64_t GICR_VIEWR = 0x002c;
constexpr uint64_t GICR_FLUSHR = 0x0030;
constexpr uint64_t GICD_CFGID_VIEW = 1ull << 53;
constexpr uint32_t GICR_FLUSHR_RESET = 0x3cfffff0u;
constexpr uint32_t GICR_FLUSHR_RW_MASK = 0x3cfffff1u;
constexpr uint16_t SPI_MIN = 32;
constexpr uint16_t SPI_LIMIT = 992;

struct SpiView {
    uint16_t spi;
    uint32_t view;
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

uint32_t read_redist32(gicx00_multiview& dut, unsigned int redist,
                       uint64_t offset)
{
    return access<uint32_t>(dut, false, redist, offset, tlm::TLM_READ_COMMAND);
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
    EXPECT_EQ(read_redist32(dut, 0, GICR_FLUSHR), GICR_FLUSHR_RESET);
}

TEST(Gicx00MultiviewTest, StoresTwoBitDistributorViewFields)
{
    gicx00_multiview dut("gicx00_multiview_dist");

    write_dist32(dut, GICD_CTLR, 0x7u);
    write_dist32(dut, iviewr_offset(SPI_MIN), 0xffffffffu);

    EXPECT_EQ(read_dist32(dut, GICD_CTLR), 0x7u);
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

TEST(Gicx00MultiviewTest, StoresRedistributorViewPowerAndFlushRegisters)
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
    EXPECT_EQ(read_redist32(dut, 0, GICR_FLUSHR), GICR_FLUSHR_RW_MASK);
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

TEST(Gicx00MultiviewTest, InactiveRedistributorApertureIsRazWi)
{
    gicx00_multiview dut("gicx00_multiview_inactive_redists");
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    tlm::tlm_generic_payload trans;
    uint32_t value = 0xffffffffu;

    trans.set_address(0xffe8u);
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    dut.b_transport_inactive_redists(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(value, 0u);

    value = 0xa5a5a5a5u;
    trans.set_address(0x2ffffcu);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
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
