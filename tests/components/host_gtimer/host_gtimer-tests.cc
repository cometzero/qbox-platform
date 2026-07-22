/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <utility>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <arm_system_counter.h>
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
constexpr uint64_t CNTSCR = 0x010;
constexpr uint64_t CNTFID0 = 0x020;
constexpr uint64_t CNTINCR = 0x0d0;
constexpr uint64_t CNTREAD_L = 0x000;
constexpr uint64_t CNTREAD_H = 0x004;
constexpr uint64_t SYNC_FIRST_UNDOCUMENTED = 0x044;
constexpr uint64_t SYNC_PIDR4 = 0xfd0;

uint32_t access32(host_gtimer& dut, uint64_t offset, tlm::tlm_command command,
                  uint32_t value = 0,
                  sc_core::sc_time delay = sc_core::SC_ZERO_TIME)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(data));
    trans.set_streaming_width(sizeof(data));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));

    dut.b_transport(trans, delay);

    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return data;
}

uint32_t read32(host_gtimer& dut, uint64_t offset)
{
    return access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void write32(host_gtimer& dut, uint64_t offset, uint32_t value,
             sc_core::sc_time delay = sc_core::SC_ZERO_TIME)
{
    (void)access32(dut, offset, tlm::TLM_WRITE_COMMAND, value, delay);
}

tlm::tlm_response_status access_status(host_gtimer& dut, uint64_t offset,
                                        tlm::tlm_command command,
                                        unsigned int len, uint64_t value = 0,
                                        unsigned int streaming_width = 0,
                                        unsigned char* byte_enable = nullptr)
{
    tlm::tlm_generic_payload trans;
    auto data = value;

    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(len);
    trans.set_streaming_width(streaming_width == 0 ? len : streaming_width);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_byte_enable_ptr(byte_enable);
    trans.set_byte_enable_length(byte_enable == nullptr ? 0 : len);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);

    return trans.get_response_status();
}

tlm::tlm_response_status debug_write_status(host_gtimer& dut,
                                            uint64_t offset,
                                            uint32_t value)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    (void)dut.transport_dbg(trans);
    return trans.get_response_status();
}

} // namespace

TEST(HostGtimerTest, CounterReadsAreSideEffectFreeAndShared)
{
    gs::arm_system_counter counter("shared_counter");
    host_gtimer base("host_gtimer_counter_base", counter);
    host_gtimer read("host_gtimer_counter_read", counter);
    base.p_counter_base = true;
    read.p_counter_read = true;
    base.before_end_of_elaboration();
    read.before_end_of_elaboration();

    const sc_core::sc_time at_first_tick(8, sc_core::SC_NS);
    EXPECT_EQ(access32(base, PCTL, tlm::TLM_READ_COMMAND, 0, at_first_tick),
              1u);
    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0,
                       at_first_tick),
              1u);
    EXPECT_EQ(access32(base, PCTL, tlm::TLM_READ_COMMAND, 0, at_first_tick),
              1u);
}

TEST(HostGtimerTest, FrontendSnapshotObservesExplicitTimeWithoutMutation)
{
    gs::arm_system_counter counter("frontend_snapshot_counter");
    host_gtimer control("frontend_snapshot_control", counter);
    control.p_counter_control = true;
    control.before_end_of_elaboration();
    const auto before = counter.snapshot();

    const auto view = control.snapshot_at(8);
    const auto after = counter.snapshot();

    EXPECT_TRUE(view.observed);
    EXPECT_EQ(view.counter, 1u);
    EXPECT_EQ(view.input_frequency_hz, 125000000u);
    EXPECT_EQ(view.reported_frequency_hz, 125000000u);
    EXPECT_EQ(view.increment, 1u);
    EXPECT_EQ(after.generation, before.generation);
}

TEST(HostGtimerTest, CounterBaseReportsFrequency)
{
    gs::arm_system_counter counter("frequency_counter");
    host_gtimer dut("host_gtimer_frequency", counter);
    dut.p_counter_base = true;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, FRQ), 125000000u);
}

TEST(HostGtimerTest, RegistersPreserveWrites)
{
    gs::arm_system_counter counter("rw_counter");
    host_gtimer dut("host_gtimer_rw", counter);

    write32(dut, P_CTL, 0x00000003u);

    EXPECT_EQ(read32(dut, P_CTL), 0x00000003u);
}

TEST(HostGtimerTest, CntControlReportsFrequencyAndPreservesControlWrites)
{
    gs::arm_system_counter counter("control_counter");
    host_gtimer dut("host_gtimer_cntcontrol", counter);
    dut.p_counter_control = true;
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

TEST(HostGtimerTest, CntControlWritesUpdateEverySharedView)
{
    gs::arm_system_counter counter("control_write_counter");
    host_gtimer control("host_gtimer_control", counter);
    host_gtimer read("host_gtimer_read", counter);
    control.p_counter_control = true;
    read.p_counter_read = true;
    control.before_end_of_elaboration();
    read.before_end_of_elaboration();

    write32(control, CNTCV_L, 0x12345678u);
    write32(control, CNTCV_H, 0x9abcdef0u);

    EXPECT_EQ(read32(read, CNTREAD_L), 0x12345678u);
    EXPECT_EQ(read32(read, CNTREAD_H), 0x9abcdef0u);
}

TEST(HostGtimerTest, CntcrEnableFreezesAndResumesSharedCounter)
{
    gs::arm_system_counter counter("enable_counter");
    host_gtimer control("enable_control", counter);
    host_gtimer read("enable_read", counter);
    control.p_counter_control = true;
    read.p_counter_read = true;
    control.before_end_of_elaboration();
    read.before_end_of_elaboration();

    write32(control, CNTCR, 0u, sc_core::sc_time(8, sc_core::SC_NS));
    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0,
                       sc_core::sc_time(16, sc_core::SC_NS)),
              1u);

    write32(control, CNTCR, 1u, sc_core::sc_time(16, sc_core::SC_NS));
    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0,
                       sc_core::sc_time(24, sc_core::SC_NS)),
              2u);
}

TEST(HostGtimerTest, CntcrStateIsVisibleThroughEveryControlFrontend)
{
    gs::arm_system_counter counter("shared_control_counter");
    host_gtimer smd("smd_control", counter);
    host_gtimer si0("si0_control", counter);
    smd.p_counter_control = true;
    si0.p_counter_control = true;
    smd.before_end_of_elaboration();
    si0.before_end_of_elaboration();

    write32(smd, CNTCR, 0u);
    EXPECT_EQ(read32(si0, CNTCR) & 1u, 0u);

    const uint64_t generation = counter.snapshot().generation;
    write32(si0, CNTCR, 3u);
    EXPECT_EQ(read32(smd, CNTCR) & 3u, 3u);
    EXPECT_EQ(counter.snapshot().generation, generation + 1);
}

TEST(HostGtimerTest, PartialCountWritesUseAnnotatedEffectiveTime)
{
    gs::arm_system_counter counter("partial_write_counter");
    host_gtimer control("partial_write_control", counter);
    host_gtimer read("partial_write_read", counter);
    control.p_counter_control = true;
    read.p_counter_read = true;
    control.before_end_of_elaboration();
    read.before_end_of_elaboration();
    const sc_core::sc_time effective(8, sc_core::SC_NS);

    write32(control, CNTCV_L, 0x76543210u, effective);
    write32(control, CNTCV_H, 0xfedcba98u, effective);

    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0, effective),
              0x76543210u);
    EXPECT_EQ(access32(read, CNTREAD_H, tlm::TLM_READ_COMMAND, 0, effective),
              0xfedcba98u);
}

TEST(HostGtimerTest, IncrementAndScaleWritesChangeElapsedCount)
{
    gs::arm_system_counter counter("scale_counter");
    host_gtimer control("scale_control", counter);
    host_gtimer read("scale_read", counter);
    control.p_counter_control = true;
    read.p_counter_read = true;
    control.before_end_of_elaboration();
    read.before_end_of_elaboration();

    write32(control, CNTINCR, 8u);
    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0,
                       sc_core::sc_time(8, sc_core::SC_NS)),
              8u);

    write32(control, CNTSCR, 2u << gs::arm_system_counter::fractional_bits,
            sc_core::sc_time(8, sc_core::SC_NS));
    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0,
                       sc_core::sc_time(16, sc_core::SC_NS)),
              10u);
}

TEST(HostGtimerTest, HighLowHighRetryDetectsRollover)
{
    gs::arm_system_counter counter("rollover_counter");
    host_gtimer read("rollover_read", counter);
    read.p_counter_read = true;
    read.before_end_of_elaboration();
    counter.reanchor_at(0xffffffffULL, 0, 0);

    EXPECT_EQ(read32(read, CNTREAD_H), 0u);
    EXPECT_EQ(access32(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 0,
                       sc_core::sc_time(8, sc_core::SC_NS)),
              0u);
    EXPECT_EQ(access32(read, CNTREAD_H, tlm::TLM_READ_COMMAND, 0,
                       sc_core::sc_time(8, sc_core::SC_NS)),
              1u);
}

TEST(HostGtimerTest, CounterFramesRejectMalformedAccessShapes)
{
    gs::arm_system_counter counter("malformed_counter");
    host_gtimer read("malformed_read", counter);
    read.p_counter_read = true;
    read.before_end_of_elaboration();
    unsigned char byte_enable[4] = { 0xff, 0xff, 0xff, 0xff };

    EXPECT_EQ(access_status(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 2),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(read, CNTREAD_L + 1, tlm::TLM_READ_COMMAND, 4),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(access_status(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 4, 0,
                            2),
              tlm::TLM_BURST_ERROR_RESPONSE);
    EXPECT_EQ(access_status(read, CNTREAD_L, tlm::TLM_READ_COMMAND, 4, 0,
                            4, byte_enable),
              tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
}

TEST(HostGtimerTest, Si0WideImplementationDefinedWriteIsIgnored)
{
    gs::arm_system_counter counter("si0_wide_write_counter");
    host_gtimer control("si0_wide_write_control", counter);
    control.p_counter_control = true;
    control.before_end_of_elaboration();
    const auto before = counter.snapshot();

    EXPECT_EQ(access_status(control, CNTINCR, tlm::TLM_WRITE_COMMAND,
                            sizeof(uint64_t), 8u),
              tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(access_status(control, CNTCR, tlm::TLM_WRITE_COMMAND,
                            sizeof(uint64_t), 1u),
              tlm::TLM_ADDRESS_ERROR_RESPONSE);

    const auto after = counter.snapshot();
    EXPECT_EQ(read32(control, CNTINCR), 0u);
    EXPECT_EQ(after.input_frequency_hz, before.input_frequency_hz);
    EXPECT_EQ(after.increment_8_24, before.increment_8_24);
    EXPECT_EQ(after.reported_frequency_hz, before.reported_frequency_hz);
    EXPECT_EQ(after.generation, before.generation);
}

TEST(HostGtimerTest, DebugReadsAreSideEffectFreeAndWritesAreDenied)
{
    gs::arm_system_counter counter("debug_counter");
    host_gtimer control("debug_control", counter);
    host_gtimer read("debug_read", counter);
    control.p_counter_control = true;
    read.p_counter_read = true;
    control.before_end_of_elaboration();
    read.before_end_of_elaboration();

    EXPECT_EQ(read32(read, CNTREAD_L), 0u);
    EXPECT_EQ(read32(read, CNTREAD_L), 0u);
    EXPECT_EQ(debug_write_status(control, CNTCV_L, 0xffffffffu),
              tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(read32(read, CNTREAD_L), 0u);
}

TEST(HostGtimerTest, DirectMemoryAccessIsNeverGranted)
{
    gs::arm_system_counter counter("dmi_counter");
    host_gtimer read("dmi_read", counter);
    tlm::tlm_generic_payload trans;
    tlm::tlm_dmi dmi;
    trans.set_address(CNTREAD_L);

    EXPECT_FALSE(read.get_direct_mem_ptr(trans, dmi));
    EXPECT_FALSE(trans.is_dmi_allowed());
}

TEST(HostGtimerTest, SyncRegistersResetToZeroAndPreserveWrites)
{
    gs::arm_system_counter counter("sync_rw_counter");
    host_gtimer dut("host_gtimer_sync_rw", counter);
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
    gs::arm_system_counter counter("sync_decode_counter");
    host_gtimer dut("host_gtimer_sync_decode", counter);
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

TEST(HostGtimerTest, AllocatedFrameReservedTailIsRazWi)
{
    gs::arm_system_counter counter("reserved_tail_counter");
    host_gtimer dut("host_gtimer_reserved_tail", counter);
    dut.p_sync_frame = true;
    dut.before_end_of_elaboration();

    EXPECT_EQ(read32(dut, 0xfffc), 0u);
    write32(dut, 0xfffc, 0xa5a5a5a5u);
    EXPECT_EQ(read32(dut, 0xfffc), 0u);
}

TEST(HostGtimerTest, SyncRejectsUnsupportedRegisterAccessSizes)
{
    gs::arm_system_counter counter("sync_access_counter");
    host_gtimer dut("host_gtimer_sync_access_size", counter);
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
    gs::arm_system_counter counter("sync_id_counter");
    host_gtimer dut("host_gtimer_sync_id", counter);
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
    gs::arm_system_counter counter("sync_id_access_counter");
    host_gtimer dut("host_gtimer_sync_id_access", counter);
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
