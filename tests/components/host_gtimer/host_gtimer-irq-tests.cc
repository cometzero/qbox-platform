/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdint>
#include <utility>
#include <vector>

#include <arm_system_counter.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <host_gtimer.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

constexpr uint64_t P_CVALL = 0x020;
constexpr uint64_t P_CVALH = 0x024;
constexpr uint64_t P_CTL = 0x02c;
constexpr uint32_t P_CTL_ENABLE = 1u << 0;
constexpr uint32_t P_CTL_IMASK = 1u << 1;
constexpr uint32_t P_CTL_ISTATUS = 1u << 2;

std::pair<tlm::tlm_response_status, uint32_t> access32(
    host_gtimer& dut, tlm::tlm_command command, uint64_t offset,
    uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    trans.set_command(command);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    dut.b_transport(trans, delay);
    return {trans.get_response_status(), value};
}

uint32_t read32(host_gtimer& dut, uint64_t offset)
{
    auto result = access32(dut, tlm::TLM_READ_COMMAND, offset);
    EXPECT_EQ(result.first, tlm::TLM_OK_RESPONSE);
    return result.second;
}

void write32(host_gtimer& dut, uint64_t offset, uint32_t value)
{
    auto result = access32(dut, tlm::TLM_WRITE_COMMAND, offset, value);
    EXPECT_EQ(result.first, tlm::TLM_OK_RESPONSE);
}

class SignalSink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> signal;
    tlm_utils::simple_initiator_socket<SignalSink> initiator;
    std::vector<bool> values;

    explicit SignalSink(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , signal("signal")
        , initiator("initiator")
    {
        signal.register_value_changed_cb(
            [this](bool value) { values.push_back(value); });
    }
};

}

TEST(HostGtimerIrqTest, RaisesAndMasksCompareInterrupt)
{
    gs::arm_system_counter counter("timer_counter");
    host_gtimer dut("host_gtimer_compare", counter);
    SignalSink irq("host_gtimer_irq");
    dut.p_counter_base = true;
    dut.irq.bind(irq.signal);
    irq.initiator.bind(dut.target_socket);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    write32(dut, P_CVALL, 10u);
    write32(dut, P_CVALH, 0u);
    write32(dut, P_CTL, P_CTL_ENABLE);
    sc_core::sc_start(sc_core::sc_time(79, sc_core::SC_NS));

    EXPECT_EQ(read32(dut, P_CTL), P_CTL_ENABLE);
    EXPECT_TRUE(irq.values.empty() || !irq.values.back());

    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    EXPECT_EQ(read32(dut, P_CTL), P_CTL_ENABLE | P_CTL_ISTATUS);
    ASSERT_FALSE(irq.values.empty());
    EXPECT_TRUE(irq.values.back());

    write32(dut, P_CTL, P_CTL_ENABLE | P_CTL_IMASK);
    sc_core::sc_start(sc_core::sc_get_time_resolution());

    EXPECT_EQ(read32(dut, P_CTL),
              P_CTL_ENABLE | P_CTL_IMASK | P_CTL_ISTATUS);
    EXPECT_FALSE(irq.values.back());

    write32(dut, P_CVALL, 1000u);
    write32(dut, P_CVALH, 0u);
    write32(dut, P_CTL, P_CTL_ENABLE);
    const sc_core::sc_time anchor_time =
        sc_core::sc_time_stamp() + sc_core::sc_time(1, sc_core::SC_NS);
    ASSERT_TRUE(counter.set_enabled_at(false, anchor_time));

    EXPECT_NO_THROW(sc_core::sc_start(sc_core::SC_ZERO_TIME));
    EXPECT_NO_THROW(sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS)));
    EXPECT_FALSE(read32(dut, P_CTL) & P_CTL_ISTATUS);
    EXPECT_TRUE(irq.values.empty() || !irq.values.back());
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
