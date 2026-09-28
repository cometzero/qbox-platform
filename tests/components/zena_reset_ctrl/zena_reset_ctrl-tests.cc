/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <zena_reset_ctrl.h>
#include <zena_watchdog.h>
#include <reset_fanout.h>

namespace {

class SignalSource : public sc_core::sc_module
{
public:
    InitiatorSignalSocket<bool> signal;

    explicit SignalSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
    }
};

class SignalSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> signal;
    std::vector<bool> values;

    explicit SignalSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
    {
        signal.register_value_changed_cb(
            [this](bool value) { values.push_back(value); });
    }
};

class TlmInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TlmInitiator, DEFAULT_TLM_BUSWIDTH> socket;

    explicit TlmInitiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }
};

uint32_t access32(zena_reset_ctrl& dut, bool pik, uint64_t offset,
                  tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (pik) {
        dut.pik_b_transport(trans, delay);
    } else {
        dut.rgm_b_transport(trans, delay);
    }
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint32_t watchdog32(zena_watchdog& dut, uint64_t offset,
                    tlm::tlm_command command, uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.control_b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

}

TEST(ZenaResetCtrlTest, RgmAndPikPreserveResetOwnership)
{
    zena_reset_ctrl dut("reset_ctrl");
    TlmInitiator rgm("rgm_initiator");
    TlmInitiator pik("pik_initiator");
    SignalSource ap_ns_watchdog("ap_ns_watchdog_source");
    SignalSource ap_s_watchdog("ap_s_watchdog_source");
    SignalSource si_watchdog("si_watchdog_source");
    SignalSource rse_watchdog("rse_watchdog_source");
    SignalSource ap_power("ap_power_source");
    SignalSource power_request("power_request_source");
    SignalSource reset_request("reset_request_source");
    SignalSink ap_reset("ap_reset_sink");
    SignalSink si_reset("si_reset_sink");
    SignalSink rse_reset("rse_reset_sink");
    SignalSink power_ack("power_ack_sink");
    SignalSink reset_ack("reset_ack_sink");
    SignalSource safety_fault("safety_fault_source");

    auto broker = cci::cci_get_global_broker(cci::cci_originator("reset_test"));
    broker.set_preset_cci_value("expiry_watchdog.clock_frequency",
                                cci::cci_value(1000ull));
    zena_watchdog watchdog("expiry_watchdog");
    zena_reset_ctrl expiry_ctrl("expiry_ctrl");
    reset_fanout fanout("expiry_fanout");
    TlmInitiator wd_control("wd_control"), wd_refresh("wd_refresh");
    TlmInitiator expiry_rgm("expiry_rgm"), expiry_pik("expiry_pik");
    SignalSink expiry_sink("expiry_sink"), ws0_sink("ws0_sink");
    wd_control.socket.bind(watchdog.control);
    wd_refresh.socket.bind(watchdog.refresh);
    expiry_rgm.socket.bind(expiry_ctrl.rgm);
    expiry_pik.socket.bind(expiry_ctrl.pik);
    watchdog.ws0.bind(ws0_sink.signal);
    watchdog.ws1.bind(expiry_ctrl.ap_ns_watchdog_reset);
    expiry_ctrl.ap_reset.bind(fanout.reset_in);
    // Match Lua order: watchdog acknowledgment precedes downstream reset.
    fanout.reset_out.bind(watchdog.reset);
    fanout.reset_out.bind(expiry_sink.signal);

    rgm.socket.bind(dut.rgm);
    pik.socket.bind(dut.pik);
    ap_ns_watchdog.signal.bind(dut.ap_ns_watchdog_reset);
    ap_s_watchdog.signal.bind(dut.ap_s_watchdog_reset);
    si_watchdog.signal.bind(dut.si_watchdog_reset);
    rse_watchdog.signal.bind(dut.rse_watchdog_reset);
    ap_power.signal.bind(dut.ap_power_reset);
    safety_fault.signal.bind(dut.safety_fault_reset);
    power_request.signal.bind(dut.power_request);
    reset_request.signal.bind(dut.reset_request);
    dut.ap_reset.bind(ap_reset.signal);
    dut.si_reset.bind(si_reset.signal);
    dut.rse_reset.bind(rse_reset.signal);
    dut.power_ack.bind(power_ack.signal);
    dut.reset_ack.bind(reset_ack.signal);

    EXPECT_EQ(access32(dut, true, 0x800, tlm::TLM_READ_COMMAND), 0x20000101u);
    EXPECT_EQ(access32(dut, true, 0x874, tlm::TLM_READ_COMMAND), 0x20183101u);
    EXPECT_EQ(access32(dut, true, 0x8a0, tlm::TLM_READ_COMMAND), 0x20000202u);
    (void)access32(dut, true, 0xa04, tlm::TLM_WRITE_COMMAND, 0x5u);
    EXPECT_EQ(access32(dut, true, 0xa00, tlm::TLM_READ_COMMAND), 0x5u);
    (void)access32(dut, true, 0xa08, tlm::TLM_WRITE_COMMAND, 0x1u);
    EXPECT_EQ(access32(dut, true, 0xa00, tlm::TLM_READ_COMMAND), 0x4u);

    (void)access32(dut, false, 0x030, tlm::TLM_WRITE_COMMAND, 1u << 24);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    ap_ns_watchdog.signal->write(true);
    si_watchdog.signal->write(true);
    rse_watchdog.signal->write(true);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(access32(dut, false, 0x020, tlm::TLM_READ_COMMAND),
              (1u << 24) | (1u << 8) | (1u << 5));
    EXPECT_TRUE(ap_reset.values.back());
    EXPECT_TRUE(si_reset.values.back());
    EXPECT_TRUE(rse_reset.values.back());

    power_request.signal->write(true);
    reset_request.signal->write(true);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(access32(dut, true, 0xc00, tlm::TLM_READ_COMMAND), 1u);
    EXPECT_EQ(access32(dut, true, 0xc08, tlm::TLM_READ_COMMAND), 1u);
    (void)access32(dut, true, 0xc04, tlm::TLM_WRITE_COMMAND, 1u);
    (void)access32(dut, true, 0xc0c, tlm::TLM_WRITE_COMMAND, 1u);
    EXPECT_TRUE(power_ack.values.back());
    EXPECT_TRUE(reset_ack.values.back());

    // The secure and non-secure sources are independent wired-OR inputs.
    ap_s_watchdog.signal->write(true);
    ap_ns_watchdog.signal->write(false);
    EXPECT_TRUE(ap_reset.values.back());
    ap_power.signal->write(true);
    ap_s_watchdog.signal->write(false);
    EXPECT_TRUE(ap_reset.values.back());
    ap_power.signal->write(false);
    EXPECT_FALSE(ap_reset.values.back());

    // Mask changes must re-evaluate an already asserted SI watchdog input.
    safety_fault.signal->write(true);
    EXPECT_TRUE(ap_reset.values.back());
    (void)access32(dut, false, 0x030, tlm::TLM_WRITE_COMMAND, 0u);
    EXPECT_FALSE(si_reset.values.back());
    EXPECT_FALSE(ap_reset.values.back());
    (void)access32(dut, false, 0x030, tlm::TLM_WRITE_COMMAND, 1u << 24);
    EXPECT_TRUE(si_reset.values.back());
    EXPECT_TRUE(ap_reset.values.back());
    safety_fault.signal->write(false);
    si_watchdog.signal->write(false);
    EXPECT_FALSE(ap_reset.values.back());

    // Real WS1 -> cause latch -> asynchronous reset -> watchdog clears WS1
    // -> release. Repeat to reject historical-edge replay in the fanout.
    for (unsigned int run = 0; run < 3; ++run) {
        (void)watchdog32(watchdog, 0x008, tlm::TLM_WRITE_COMMAND, 2u);
        (void)watchdog32(watchdog, 0x000, tlm::TLM_WRITE_COMMAND, 1u);
        sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_MS));
        EXPECT_EQ(watchdog32(watchdog, 0, tlm::TLM_READ_COMMAND), 3u);
        EXPECT_EQ(expiry_sink.values.size(), 2u * run);
        sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_MS));
        EXPECT_EQ(access32(expiry_ctrl, false, 0x020, tlm::TLM_READ_COMMAND),
                  1u << 5);
        EXPECT_EQ(watchdog32(watchdog, 0, tlm::TLM_READ_COMMAND), 0u);
        ASSERT_EQ(expiry_sink.values.size(), 2u * (run + 1));
        EXPECT_TRUE(expiry_sink.values[2 * run]);
        EXPECT_FALSE(expiry_sink.values[2 * run + 1]);
        EXPECT_FALSE(ws0_sink.values.back());
        sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_MS));
        EXPECT_EQ(expiry_sink.values.size(), 2u * (run + 1));
    }
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
