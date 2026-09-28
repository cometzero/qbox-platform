/* SPDX-License-Identifier: BSD-3-Clause */

#include <cstdint>
#include <cstdlib>
#include <functional>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <host_ppu.h>
#include <reset_fanout.h>
#include <signal_fanout.h>
#include <zena_reset_ctrl.h>
#include <zena_watchdog.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

class Initiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<Initiator, DEFAULT_TLM_BUSWIDTH> socket;
    explicit Initiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {}

    uint32_t access(uint64_t address, tlm::tlm_command command, uint32_t data = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_address(address);
        trans.set_command(command);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(sizeof(data));
        trans.set_streaming_width(sizeof(data));
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        return data;
    }
};

class Sink : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> signal;
    bool level = false;
    unsigned int rises = 0;
    unsigned int falls = 0;
    sc_core::sc_time last_fall = sc_core::SC_ZERO_TIME;
    std::function<void(bool)> observe;

    explicit Sink(sc_core::sc_module_name name)
        : sc_core::sc_module(name), signal("signal")
    {
        signal.register_value_changed_cb([this](bool value) {
            if (level && !value) {
                ++falls;
                last_fall = sc_core::sc_time_stamp();
            }
            if (!level && value)
                ++rises;
            level = value;
            if (observe)
                observe(value);
        });
    }
};

} // namespace

TEST(ApWatchdogPowerSequenceTest, ClearsWs1BeforeCpuReleaseAndPowerOnTwice)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("power_test"));
    broker.set_preset_cci_value("ppu.assert_power_on_load", cci::cci_value(true));
    broker.set_preset_cci_value("ppu.assert_power_on_reset", cci::cci_value(true));
    // Match CPU0 Lua: both phases must advance beyond the +1 ps reset fanout.
    // Explicit opt-in negative control reproduces the former delta-only Lua.
    const auto phase_ns = std::getenv("AP_WATCHDOG_TEST_ZERO_PHASE") ? 0ull : 1ull;
    SCOPED_TRACE(::testing::Message() << "power-on phase_ns=" << phase_ns);
    broker.set_preset_cci_value("ppu.power_on_load_pulse_width_ns", cci::cci_value(phase_ns));
    broker.set_preset_cci_value("ppu.power_on_load_to_reset_delay_ns", cci::cci_value(phase_ns));
    broker.set_preset_cci_value("watchdog.clock_frequency", cci::cci_value(1000ull));

    host_ppu ppu("ppu");
    zena_reset_ctrl controller("controller");
    reset_fanout cold_reset("cold_reset");
    zena_watchdog watchdog("watchdog");
    signal_fanout ws1_fanout("ws1_fanout");
    Initiator ppu_bus("ppu_bus"), rgm("rgm"), pik("pik");
    Initiator wd_control("wd_control"), wd_refresh("wd_refresh");
    Sink cpu_reset("cpu_reset"), ws0("ws0"), ap_irq("ap_irq"), si_irq("si_irq");
    Sink reset_state("reset_state");
    ppu_bus.socket.bind(ppu.target_socket);
    rgm.socket.bind(controller.rgm);
    pik.socket.bind(controller.pik);
    wd_control.socket.bind(watchdog.control);
    wd_refresh.socket.bind(watchdog.refresh);
    ppu.power_on_load.bind(controller.ap_power_reset);
    ppu.power_on_reset.bind(cpu_reset.signal);
    controller.ap_reset.bind(cold_reset.reset_in);
    cold_reset.reset_out.bind(watchdog.reset);
    cold_reset.reset_out.bind(reset_state.signal);
    watchdog.ws0.bind(ws0.signal);
    // Full-system NS WS1 notifies AP and SI; it does not directly reset AP.
    watchdog.ws1.bind(ws1_fanout.signal_in);
    ws1_fanout.signal_out.bind(ap_irq.signal);
    ws1_fanout.signal_out.bind(si_irq.signal);

    unsigned int releases = 0;
    cpu_reset.observe = [&](bool asserted) {
        if (asserted)
            return;
        ++releases;
        EXPECT_FALSE(si_irq.level);
        EXPECT_FALSE(ap_irq.level);
        EXPECT_FALSE(reset_state.level);
        EXPECT_EQ(wd_control.access(0, tlm::TLM_READ_COMMAND), 0u);
        EXPECT_LT(si_irq.last_fall, sc_core::sc_time_stamp());
        EXPECT_LT(reset_state.last_fall, sc_core::sc_time_stamp());
        // PWSR ON is published only after the CPU release callback returns.
        EXPECT_EQ(ppu_bus.access(8, tlm::TLM_READ_COMMAND) & 0xfu, 0u);
    };
    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    for (unsigned int run = 0; run < 2; ++run) {
        ASSERT_TRUE(cpu_reset.level);
        wd_control.access(8, tlm::TLM_WRITE_COMMAND, 2);
        wd_control.access(0, tlm::TLM_WRITE_COMMAND, 1);
        sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_MS));
        ASSERT_TRUE(si_irq.level);
        ASSERT_EQ(wd_control.access(0, tlm::TLM_READ_COMMAND), 7u);

        ppu_bus.access(0, tlm::TLM_WRITE_COMMAND, 8);
        sc_core::sc_start(sc_core::sc_time(500, sc_core::SC_PS));
        EXPECT_FALSE(si_irq.level);
        EXPECT_TRUE(cpu_reset.level);
        EXPECT_EQ(ppu_bus.access(8, tlm::TLM_READ_COMMAND) & 0xfu, 0u);
        sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_NS));
        EXPECT_FALSE(cpu_reset.level);
        EXPECT_EQ(ppu_bus.access(8, tlm::TLM_READ_COMMAND) & 0xfu, 8u);
        EXPECT_EQ(releases, run + 1);
        EXPECT_EQ(si_irq.rises, run + 1);
        EXPECT_EQ(si_irq.falls, run + 1);
        EXPECT_EQ(reset_state.rises, run + 1);
        EXPECT_EQ(reset_state.falls, run + 1);

        ppu_bus.access(0, tlm::TLM_WRITE_COMMAND, 0);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    }
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
