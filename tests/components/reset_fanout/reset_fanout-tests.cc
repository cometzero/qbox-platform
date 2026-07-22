/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/initiator-signal-socket.h>
#include <reset_fanout.h>
#include <systemc>
#include <vector>

namespace {

class ResetSource : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(ResetSource);

    InitiatorSignalSocket<bool> reset;

    explicit ResetSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , reset("reset")
    {
        SC_THREAD(run);
    }

    void run()
    {
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        reset->write(true);
        wait(sc_core::sc_time(4, sc_core::SC_PS));
        reset->write(false);
    }
};

class SameTimestampResetSource : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(SameTimestampResetSource);

    InitiatorSignalSocket<bool> reset;

    explicit SameTimestampResetSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , reset("reset")
    {
        SC_THREAD(run);
    }

    void run()
    {
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        reset->write(true);
        reset->write(false);
    }
};

class DelayedResetSource : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(DelayedResetSource);

    InitiatorSignalSocket<bool> reset;

    explicit DelayedResetSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , reset("reset")
    {
        SC_THREAD(run);
    }

    void run()
    {
        wait(sc_core::sc_time(7, sc_core::SC_PS));
        reset->write(true);
        wait(sc_core::sc_time(2, sc_core::SC_PS));
        reset->write(false);
    }
};

class ResetSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> reset;
    std::vector<bool> observed;
    std::vector<sc_core::sc_time> observed_at;

    explicit ResetSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , reset("reset")
    {
        reset.register_value_changed_cb([this](bool value) {
            observed.push_back(value);
            observed_at.push_back(sc_core::sc_time_stamp());
        });
    }
};

} // namespace

TEST(ResetFanoutTest, BroadcastsResetValueToEveryTarget)
{
    reset_fanout dut("reset_fanout");
    ResetSource source("source");
    ResetSink sink0("sink0");
    ResetSink sink1("sink1");
    ResetSink sink2("sink2");

    source.reset.bind(dut.reset_in);
    dut.reset_out.bind(sink0.reset);
    dut.reset_out.bind(sink1.reset);
    dut.reset_out.bind(sink2.reset);

    SameTimestampResetSource pulse_source("pulse_source");
    reset_fanout pulse_dut("pulse_reset_fanout");
    ResetSink pulse_sink0("pulse_sink0");
    ResetSink pulse_sink1("pulse_sink1");

    pulse_source.reset.bind(pulse_dut.reset_in);
    pulse_dut.reset_out.bind(pulse_sink0.reset);
    pulse_dut.reset_out.bind(pulse_sink1.reset);

    reset_fanout sys_rss("sys_rss_reset_fanout");
    reset_fanout aon("aon_reset_fanout");
    ResetSource sys_rss_source("sys_rss_source");
    DelayedResetSource aon_source("aon_source");
    ResetSink timer0("timer0");
    ResetSink timer3("timer3");
    ResetSink lsc("lsc");

    sys_rss_source.reset.bind(sys_rss.reset_in);
    sys_rss.reset_out.bind(timer0.reset);
    aon_source.reset.bind(aon.reset_in);
    aon.reset_out.bind(timer3.reset);
    aon.reset_out.bind(lsc.reset);

    sc_core::sc_start(sc_core::sc_time(11, sc_core::SC_PS));

    ASSERT_GE(sink0.observed.size(), 2u);
    EXPECT_EQ(sink0.observed, sink1.observed);
    EXPECT_EQ(sink0.observed, sink2.observed);
    EXPECT_TRUE(sink0.observed.front());
    EXPECT_FALSE(sink0.observed.back());

    ASSERT_EQ(pulse_sink0.observed, (std::vector<bool>{true, false}));
    EXPECT_EQ(pulse_sink0.observed, pulse_sink1.observed);

    ASSERT_FALSE(timer0.observed_at.empty());
    ASSERT_FALSE(timer3.observed_at.empty());
    EXPECT_EQ(timer0.observed, (std::vector<bool>{true, false}));
    EXPECT_EQ(timer3.observed, (std::vector<bool>{true, false}));
    EXPECT_LT(timer0.observed_at.front(), timer3.observed_at.front());
    EXPECT_EQ(timer3.observed, lsc.observed);
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
