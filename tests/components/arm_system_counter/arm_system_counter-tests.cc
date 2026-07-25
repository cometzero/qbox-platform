/* SPDX-License-Identifier: BSD-3-Clause */

#include <cstdint>
#include <limits>

#include <arm_system_counter.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <systemc>

namespace {

using Counter = gs::arm_system_counter;

sc_core::sc_time ticks(sc_dt::uint64 value)
{
    return sc_core::sc_time::from_value(value);
}

TEST(ArmSystemCounterTest, UsesRawResolutionTicksAtSubNanosecondTimes)
{
    Counter counter("sub_ns_counter");
    const auto tick = sc_core::sc_get_time_resolution();

    ASSERT_LT(tick, sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(counter.count_at(ticks(999)), 0u);
    EXPECT_EQ(counter.count_at(ticks(8000)), 1u);
    EXPECT_EQ(counter.count_at(ticks(8000)), 1u);
}

TEST(ArmSystemCounterTest, FractionalScaleAndClockPhaseSurviveMutation)
{
    Counter counter("fractional_counter");
    ASSERT_TRUE(counter.set_input_frequency_at(
        1000000000, sc_core::SC_ZERO_TIME));
    ASSERT_TRUE(counter.set_scale_8_24_at(
        uint32_t(1) << 23, sc_core::SC_ZERO_TIME));

    EXPECT_EQ(counter.count_at(sc_core::sc_time(1, sc_core::SC_NS)), 0u);
    EXPECT_EQ(counter.count_at(sc_core::sc_time(2, sc_core::SC_NS)), 1u);

    ASSERT_TRUE(counter.set_enabled_at(
        false, sc_core::sc_time(3, sc_core::SC_NS)));
    const auto stopped = counter.snapshot();
    EXPECT_EQ(stopped.fractional_count, uint32_t(1) << 23);
    ASSERT_TRUE(counter.set_enabled_at(
        true, sc_core::sc_time(5, sc_core::SC_NS)));
    EXPECT_EQ(counter.count_at(sc_core::sc_time(6, sc_core::SC_NS)), 2u);
}

TEST(ArmSystemCounterTest, AllMirrorRelevantChangesAdvanceGeneration)
{
    Counter counter("generation_counter");
    uint64_t generation = counter.snapshot().generation;

    EXPECT_TRUE(counter.set_reported_frequency_at(
        100000000, sc_core::SC_ZERO_TIME));
    EXPECT_EQ(counter.snapshot().generation, ++generation);
    EXPECT_TRUE(counter.set_integer_increment_at(
        8, sc_core::SC_ZERO_TIME));
    EXPECT_EQ(counter.snapshot().generation, ++generation);
    EXPECT_TRUE(counter.set_control_at(
        false, true, sc_core::SC_ZERO_TIME));
    EXPECT_EQ(counter.snapshot().generation, ++generation);
    EXPECT_TRUE(counter.reanchor_at(
        0x123456789abcdef0ULL, 0, sc_core::SC_ZERO_TIME));
    EXPECT_EQ(counter.snapshot().generation, ++generation);
}

TEST(ArmSystemCounterTest, CountWrapsAtSixtyFourBits)
{
    Counter counter("wrap_counter");
    counter.reanchor_at(std::numeric_limits<uint64_t>::max(), 0,
                        sc_core::SC_ZERO_TIME);

    EXPECT_EQ(counter.count_at(sc_core::sc_time(8, sc_core::SC_NS)), 0u);
}

TEST(ArmSystemCounterTest, SaveRestorePreservesFractionalPhaseAndReanchors)
{
    Counter counter("checkpoint_counter");
    counter.set_input_frequency_at(1000000000, sc_core::SC_ZERO_TIME);
    counter.set_scale_8_24_at(uint32_t(1) << 23,
                              sc_core::SC_ZERO_TIME);
    const auto saved =
        counter.save_state_at(sc_core::sc_time(3, sc_core::SC_NS));

    counter.reanchor_at(99, 0, sc_core::sc_time(3, sc_core::SC_NS));
    const uint64_t generation = counter.snapshot().generation;
    counter.restore_state_at(saved, sc_core::sc_time(10, sc_core::SC_NS));

    const auto restored = counter.snapshot();
    EXPECT_EQ(restored.anchor_count, 1u);
    EXPECT_EQ(restored.fractional_count, uint32_t(1) << 23);
    EXPECT_EQ(restored.anchor_time_ticks,
              sc_core::sc_time(10, sc_core::SC_NS).value());
    EXPECT_EQ(restored.generation, generation + 1);
    EXPECT_EQ(counter.count_at(sc_core::sc_time(11, sc_core::SC_NS)), 2u);
}

TEST(ArmSystemCounterTest, ResetRestoresConfiguredStateAtEffectiveTime)
{
    Counter counter("reset_counter");
    counter.reanchor_at(0x1234, uint32_t(1) << 23,
                        sc_core::sc_time(1, sc_core::SC_NS));
    counter.set_enabled_at(false, sc_core::sc_time(2, sc_core::SC_NS));
    const uint64_t generation = counter.snapshot().generation;

    counter.reset_at(sc_core::sc_time(2501, sc_core::SC_PS));
    const auto state = counter.snapshot();
    EXPECT_EQ(state.anchor_count, 0u);
    EXPECT_EQ(state.fractional_count, 0u);
    EXPECT_EQ(state.anchor_time_ticks,
              sc_core::sc_time(2501, sc_core::SC_PS).value());
    EXPECT_TRUE(state.enabled);
    EXPECT_EQ(state.generation, generation + 1);
}

TEST(ArmSystemCounterTest, RejectsBackwardEffectiveTimes)
{
    Counter counter("backward_time_counter");
    counter.reanchor_at(0, 0, sc_core::sc_time(1, sc_core::SC_NS));

    EXPECT_THROW(counter.count_at(ticks(999)), std::out_of_range);
    EXPECT_THROW(counter.set_enabled_at(false, ticks(999)),
                 std::out_of_range);
    EXPECT_THROW(counter.restore_state_at(counter.snapshot(), ticks(999)),
                 std::out_of_range);
    EXPECT_THROW(counter.reset_at(ticks(999)), std::out_of_range);
}

TEST(ArmSystemCounterTest, FutureMutationNotifiesAtItsAnchorTime)
{
    Counter counter("future_event_counter");
    const sc_core::sc_time effective_time(2501, sc_core::SC_PS);

    ASSERT_TRUE(counter.set_enabled_at(false, effective_time));
    EXPECT_FALSE(sc_core::sc_pending_activity_at_current_time());
    EXPECT_TRUE(sc_core::sc_pending_activity_at_future_time());
    EXPECT_EQ(sc_core::sc_time_to_pending_activity(), effective_time);
    EXPECT_THROW(counter.snapshot_at(sc_core::sc_time_stamp()),
                 std::out_of_range);

    sc_core::sc_start(effective_time);
    EXPECT_EQ(sc_core::sc_time_stamp(), effective_time);
    EXPECT_NO_THROW(counter.snapshot_at(sc_core::sc_time_stamp()));
}

}

int sc_main(int argc, char** argv)
{
    sc_core::sc_set_time_resolution(1, sc_core::SC_PS);
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
