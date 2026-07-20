/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <zena_watchdog.h>

namespace {

constexpr uint64_t WCS = 0x000;
constexpr uint64_t WRR = 0x000;
constexpr uint64_t WOR = 0x008;
constexpr uint64_t W_IIDR = 0xfcc;
constexpr uint32_t WCS_EN = 1u << 0;
constexpr uint32_t WCS_WS0 = 1u << 1;
constexpr uint32_t WCS_WS1 = 1u << 2;

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

uint32_t access32(zena_watchdog& dut, bool refresh_frame,
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
    if (refresh_frame) {
        dut.refresh_b_transport(trans, delay);
    } else {
        dut.control_b_transport(trans, delay);
    }
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint32_t read_control(zena_watchdog& dut, uint64_t offset)
{
    return access32(dut, false, offset, tlm::TLM_READ_COMMAND);
}

void write_control(zena_watchdog& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, false, offset, tlm::TLM_WRITE_COMMAND, value);
}

uint32_t read_refresh(zena_watchdog& dut, uint64_t offset)
{
    return access32(dut, true, offset, tlm::TLM_READ_COMMAND);
}

void write_refresh(zena_watchdog& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, true, offset, tlm::TLM_WRITE_COMMAND, value);
}

}

TEST(ZenaWatchdogTest, ExpiresInTwoStagesAndRefreshRearms)
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("zena_watchdog_test"));
    broker.set_preset_cci_value("watchdog_expiry.clock_frequency",
                                cci::cci_value(1000ull));

    zena_watchdog dut("watchdog_expiry");
    TlmInitiator control("watchdog_control_initiator");
    TlmInitiator refresh("watchdog_refresh_initiator");
    SignalSink ws0("watchdog_ws0");
    SignalSink ws1("watchdog_ws1");
    control.socket.bind(dut.control);
    refresh.socket.bind(dut.refresh);
    dut.ws0.bind(ws0.signal);
    dut.ws1.bind(ws1.signal);

    ASSERT_EQ(dut.p_clock_frequency.get_value(), 1000u);
    EXPECT_EQ(read_control(dut, W_IIDR), 0x0001043bu);
    EXPECT_EQ(read_refresh(dut, W_IIDR), 0x0001043bu);
    write_control(dut, WOR, 0x1234u);
    EXPECT_EQ(read_control(dut, WOR), 0x1234u);
    write_refresh(dut, WOR, 0x5678u);
    EXPECT_EQ(read_refresh(dut, WOR), 0u);
    EXPECT_EQ(read_control(dut, WOR), 0x1234u);

    write_control(dut, WOR, 2u);
    write_control(dut, WCS, WCS_EN);
    sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_MS));

    EXPECT_EQ(read_control(dut, WCS), WCS_EN | WCS_WS0);
    ASSERT_FALSE(ws0.values.empty());
    EXPECT_TRUE(ws0.values.back());
    EXPECT_TRUE(ws1.values.empty() || !ws1.values.back());

    write_refresh(dut, WRR, 0u);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(read_control(dut, WCS), WCS_EN);
    EXPECT_FALSE(ws0.values.back());

    sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_MS));
    EXPECT_EQ(read_control(dut, WCS), WCS_EN | WCS_WS0);
    sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_MS));
    EXPECT_EQ(read_control(dut, WCS), WCS_EN | WCS_WS0 | WCS_WS1);
    ASSERT_FALSE(ws1.values.empty());
    EXPECT_TRUE(ws1.values.back());

    write_control(dut, WCS, 0u);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(read_control(dut, WCS), 0u);
    EXPECT_FALSE(ws0.values.back());
    EXPECT_FALSE(ws1.values.back());
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
