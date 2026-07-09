/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <ras_ffh_stub.h>

#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/target-signal-socket.h>
#include <systemc>

namespace {

class SignalSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> signal;
    std::vector<bool> observed;

    explicit SignalSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
        signal.register_value_changed_cb([this](bool value) {
            observed.push_back(value);
        });
    }
};

} // namespace

TEST(RasFfhStubTest, ExposesOptionalIrqInitiator)
{
    ras_ffh_stub dut("ras_ffh");
    SignalSink sink("sink");

    EXPECT_STREQ(dut.name(), "ras_ffh");
    EXPECT_STREQ(dut.irq.name(), "ras_ffh.irq");
    EXPECT_EQ(dut.irq.size(), 0);

    dut.irq.bind(sink.signal);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(dut.irq.size(), 1);

    dut.irq->write(true);
    dut.irq->write(false);

    EXPECT_EQ(sink.observed, (std::vector<bool>{true, false}));
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
