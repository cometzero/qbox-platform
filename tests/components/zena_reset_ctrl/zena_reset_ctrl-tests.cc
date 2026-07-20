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

    rgm.socket.bind(dut.rgm);
    pik.socket.bind(dut.pik);
    ap_ns_watchdog.signal.bind(dut.ap_ns_watchdog_reset);
    ap_s_watchdog.signal.bind(dut.ap_s_watchdog_reset);
    si_watchdog.signal.bind(dut.si_watchdog_reset);
    rse_watchdog.signal.bind(dut.rse_watchdog_reset);
    ap_power.signal.bind(dut.ap_power_reset);
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
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
