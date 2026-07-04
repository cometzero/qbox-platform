/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <utility>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <host_gtimer.h>
#include <systemc>
#include <tlm>

namespace {

constexpr uint64_t PCTL = 0x000;
constexpr uint64_t FRQ = 0x010;
constexpr uint64_t P_CTL = 0x02c;
constexpr uint64_t CNTCR = 0x000;
constexpr uint64_t CNTSR = 0x004;
constexpr uint64_t CNTCV_L = 0x008;
constexpr uint64_t CNTCV_H = 0x00c;
constexpr uint64_t CNTFID0 = 0x020;
constexpr uint64_t CNTREAD_L = 0x000;
constexpr uint64_t CNTREAD_H = 0x004;
constexpr uint64_t SYNC_FIRST_UNDOCUMENTED = 0x044;
constexpr uint64_t SYNC_PIDR4 = 0xfd0;

uint32_t access32(host_gtimer& dut, uint64_t offset, tlm::tlm_command command,
                  uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t read32(host_gtimer& dut, uint64_t offset)
{
    return access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void write32(host_gtimer& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, offset, tlm::TLM_WRITE_COMMAND, value);
}

tlm::tlm_response_status access_status(host_gtimer& dut, uint64_t offset,
                                        tlm::tlm_command command,
                                        unsigned int len, uint64_t value = 0)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(len);
    trans.set_streaming_width(len);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);

    return trans.get_response_status();
}

} // namespace

TEST(HostGtimerTest, CounterBaseLowWordAdvances)
{
    host_gtimer dut("host_gtimer_counter");
    dut.p_counter_base = true;
    dut.p_counter_increment = 125u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, PCTL), 125u);
    EXPECT_EQ(read32(dut, PCTL), 250u);
}

TEST(HostGtimerTest, CounterBaseReportsFrequency)
{
    host_gtimer dut("host_gtimer_frequency");
    dut.p_counter_base = true;
    dut.p_frequency = 1000000000u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, FRQ), 1000000000u);
}

TEST(HostGtimerTest, RegistersPreserveWrites)
{
    host_gtimer dut("host_gtimer_rw");

    write32(dut, P_CTL, 0x00000003u);

    EXPECT_EQ(read32(dut, P_CTL), 0x00000003u);
}

TEST(HostGtimerTest, CntControlReportsFrequencyAndPreservesControlWrites)
{
    host_gtimer dut("host_gtimer_cntcontrol");
    dut.p_counter_control = true;
    dut.p_frequency = 125000000u;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, CNTFID0), 125000000u);
    EXPECT_EQ(read32(dut, CNTSR), 0u);

    write32(dut, CNTCR, 0x00000101u);
    write32(dut, CNTCV_L, 0x12345678u);
    write32(dut, CNTCV_H, 0x9abcdef0u);
    write32(dut, 0x0c0, 0xa5a50001u);
    write32(dut, 0x0d0, 8u);

    EXPECT_EQ(read32(dut, CNTCR), 0x00000101u);
    EXPECT_EQ(read32(dut, CNTCV_L), 0x12345678u);
    EXPECT_EQ(read32(dut, CNTCV_H), 0x9abcdef0u);
    EXPECT_EQ(read32(dut, 0x0c0), 0xa5a50001u);
    EXPECT_EQ(read32(dut, 0x0d0), 8u);
}

TEST(HostGtimerTest, CntReadLowHighReadsAreMonotonic)
{
    host_gtimer dut("host_gtimer_cntread");
    dut.p_counter_read = true;
    dut.p_counter_increment = 0x100000001ULL;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, CNTREAD_L), 1u);
    EXPECT_EQ(read32(dut, CNTREAD_H), 1u);
    EXPECT_EQ(read32(dut, CNTREAD_L), 2u);
    EXPECT_EQ(read32(dut, CNTREAD_H), 2u);
}

TEST(HostGtimerTest, SyncRegistersResetToZeroAndPreserveWrites)
{
    host_gtimer dut("host_gtimer_sync_rw");
    dut.p_sync_frame = true;
    dut.before_end_of_elaboration();

    for (uint64_t offset = 0x0; offset <= 0x40; offset += sizeof(uint32_t)) {
        EXPECT_EQ(read32(dut, offset), 0u) << "offset=0x" << std::hex
                                          << offset;
        write32(dut, offset, static_cast<uint32_t>(0xfeed0000u | offset));
        EXPECT_EQ(read32(dut, offset), static_cast<uint32_t>(0xfeed0000u |
                                                             offset))
            << "offset=0x" << std::hex << offset;
    }
}

TEST(HostGtimerTest, SyncRejectsUndocumentedOffsets)
{
    host_gtimer dut("host_gtimer_sync_decode");
    dut.p_sync_frame = true;
    dut.before_end_of_elaboration();

    EXPECT_EQ(access_status(dut, SYNC_FIRST_UNDOCUMENTED,
                            tlm::TLM_WRITE_COMMAND, sizeof(uint32_t),
                            0x12345678u),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(dut, SYNC_FIRST_UNDOCUMENTED,
                            tlm::TLM_READ_COMMAND, sizeof(uint32_t)),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
}

TEST(HostGtimerTest, SyncRejectsUnsupportedRegisterAccessSizes)
{
    host_gtimer dut("host_gtimer_sync_access_size");
    dut.p_sync_frame = true;
    dut.before_end_of_elaboration();

    EXPECT_EQ(access_status(dut, CNTCR, tlm::TLM_READ_COMMAND, 1),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(dut, CNTCR, tlm::TLM_WRITE_COMMAND, 8,
                            0xffffffffffffffffULL),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(dut, CNTCR + 1, tlm::TLM_WRITE_COMMAND,
                            sizeof(uint32_t), 0xffffffffu),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(read32(dut, CNTCR), 0u);
}

TEST(HostGtimerTest, SyncIdRegistersReportResetValuesAndIgnoreWrites)
{
    host_gtimer dut("host_gtimer_sync_id");
    dut.p_sync_frame = true;
    dut.before_end_of_elaboration();

    const std::pair<uint64_t, uint32_t> ids[] = {
        { 0xfd0, 0x00000004u }, { 0xfe0, 0x000000e8u },
        { 0xfe4, 0x000000b0u }, { 0xfe8, 0x0000001bu },
        { 0xfec, 0x00000000u }, { 0xff0, 0x0000000du },
        { 0xff4, 0x000000f0u }, { 0xff8, 0x00000005u },
        { 0xffc, 0x000000b1u },
    };

    for (const auto& reg : ids) {
        EXPECT_EQ(read32(dut, reg.first), reg.second)
            << "offset=0x" << std::hex << reg.first;
        write32(dut, reg.first, 0xffffffffu);
        EXPECT_EQ(read32(dut, reg.first), reg.second)
            << "offset=0x" << std::hex << reg.first;
    }
}

TEST(HostGtimerTest, SyncIdRegistersRejectUnsupportedAccessShapes)
{
    host_gtimer dut("host_gtimer_sync_id_access");
    dut.p_sync_frame = true;
    dut.before_end_of_elaboration();

    EXPECT_EQ(access_status(dut, SYNC_PIDR4, tlm::TLM_WRITE_COMMAND, 1, 0xff),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(dut, SYNC_PIDR4, tlm::TLM_READ_COMMAND, 1),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(read32(dut, SYNC_PIDR4), 0x00000004u);

    EXPECT_EQ(access_status(dut, SYNC_PIDR4, tlm::TLM_WRITE_COMMAND, 8,
                            0xffffffffffffffffULL),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(dut, SYNC_PIDR4, tlm::TLM_READ_COMMAND, 8),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(read32(dut, SYNC_PIDR4), 0x00000004u);

    EXPECT_EQ(access_status(dut, SYNC_PIDR4 + 1, tlm::TLM_READ_COMMAND,
                            sizeof(uint32_t)),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(dut, SYNC_PIDR4 + 1, tlm::TLM_WRITE_COMMAND,
                            sizeof(uint32_t), 0xffffffffu),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(read32(dut, SYNC_PIDR4), 0x00000004u);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
