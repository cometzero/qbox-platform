/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <utility>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <signal_or.h>
#include <systemc>

namespace {

class SignalSource : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(SignalSource);

    InitiatorSignalSocket<bool> signal0;
    InitiatorSignalSocket<bool> signal1;
    InitiatorSignalSocket<bool> signal2;

    explicit SignalSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name), signal0("signal0"), signal1("signal1"), signal2("signal2")
    {
        SC_THREAD(run);
    }

    void run()
    {
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        signal0->write(true);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        signal1->write(true);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        signal0->write(false);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        signal1->write(false);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        signal2->write(true);
        signal2->write(false);
    }
};

class SignalSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> signal;
    std::vector<std::pair<sc_core::sc_time, bool>> observed;

    explicit SignalSink(sc_core::sc_module_name name): sc_core::sc_module(name), signal("signal")
    {
        signal.register_value_changed_cb(
            [this](bool value) { observed.emplace_back(sc_core::sc_time_stamp(), value); });
    }
};

} // namespace

TEST(SignalOrTest, CombinesInputsWithoutSpuriousTransitions)
{
    signal_or dut("dut");
    SignalSource source("source");
    SignalSink sink("sink");

    ASSERT_EQ(dut.signal_in.size(), 3u);
    source.signal0.bind(dut.signal_in[0]);
    source.signal1.bind(dut.signal_in[1]);
    source.signal2.bind(dut.signal_in[2]);
    dut.signal_out.bind(sink.signal);

    sc_core::sc_start(sc_core::sc_time(10, sc_core::SC_PS));

    ASSERT_EQ(sink.observed.size(), 2u);
    EXPECT_EQ(sink.observed[0], std::make_pair(sc_core::sc_time(1, sc_core::SC_PS), true));
    EXPECT_EQ(sink.observed[1], std::make_pair(sc_core::sc_time(4, sc_core::SC_PS), false));
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    auto global_broker = cci::cci_get_global_broker(cci::cci_originator("signal_or_test"));
    global_broker.set_preset_cci_value("dut.num_inputs", cci::cci_value(3u));

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
