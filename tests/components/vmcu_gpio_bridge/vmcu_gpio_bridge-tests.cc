/* SPDX-License-Identifier: BSD-3-Clause */
#include <array>
#include <chrono>
#include <thread>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <vmcu_gpio_bridge.h>

namespace {
class Peer : public sc_core::sc_module
{
public:
    gs::biflow_socket<Peer> socket;
    InitiatorSignalSocket<bool> soc_error;
    InitiatorSignalSocket<bool> soc_power;
    InitiatorSignalSocket<bool> ist_done_n;
    TargetSignalSocket<bool> reset_n;
    TargetSignalSocket<bool> wake;
    TargetSignalSocket<bool> reset_request;
    bool reset_level = false, wake_level = false, reset_asserted = false;
    unsigned reset_assertions = 0;
    std::vector<std::array<uint8_t, 8>> snapshots;

    explicit Peer(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket"), soc_error("soc_error"),
          soc_power("soc_power"), ist_done_n("ist_done_n"), reset_n("reset_n"),
          wake("wake"), reset_request("reset_request")
    {
        socket.register_b_transport(this, &Peer::receive);
        reset_n.register_value_changed_cb([this](bool v) { reset_level = v; });
        wake.register_value_changed_cb([this](bool v) { wake_level = v; });
        reset_request.register_value_changed_cb([this](bool v) {
            reset_asserted = v;
            if (v) ++reset_assertions;
        });
    }
    void end_of_elaboration() override { socket.can_receive_any(); }
    void receive(tlm::tlm_generic_payload& t, sc_core::sc_time&)
    {
        ASSERT_EQ(t.get_data_length(), 8u);
        std::array<uint8_t, 8> frame;
        std::copy_n(t.get_data_ptr(), 8, frame.begin());
        snapshots.push_back(frame);
        t.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    void send(const std::vector<uint8_t>& bytes)
    {
        tlm::tlm_generic_payload t;
        t.set_data_ptr(const_cast<uint8_t*>(bytes.data()));
        t.set_data_length(bytes.size());
        socket.force_send(t);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    }
};
}

TEST(VmcuGpioBridgeTest, DefaultsFramingDriveReleaseAndTransportTimeout)
{
    vmcu_gpio_bridge dut("dut");
    Peer peer("peer");
    peer.socket.bind(dut.backend_socket);
    peer.soc_error.bind(dut.gpio_in[0]);
    peer.soc_power.bind(dut.gpio_in[1]);
    peer.ist_done_n.bind(dut.gpio_in[2]);
    dut.gpio_out[3].bind(peer.reset_n);
    dut.gpio_out[4].bind(peer.wake);
    dut.reset_request.bind(peer.reset_request);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

    ASSERT_FALSE(peer.snapshots.empty());
    EXPECT_EQ(peer.snapshots.back(), (std::array<uint8_t, 8>{{'G','P',1,'I',4,0,7,0}}));
    EXPECT_TRUE(peer.reset_level);
    EXPECT_FALSE(peer.reset_asserted);
    EXPECT_FALSE(peer.wake_level);
    EXPECT_EQ(peer.reset_assertions, 0u);

    // MCU reset defaults floating: OUT=0 with DIR=0 cannot assert RESET_N.
    peer.send({'G','P',1,'O',0,0,0,0});
    EXPECT_TRUE(peer.reset_level);
    EXPECT_EQ(peer.reset_assertions, 0u);
    // Stream fragmentation and noise resynchronization, active reset + wake.
    peer.send({0xff,'x','G','P',1});
    EXPECT_FALSE(peer.reset_asserted);
    peer.send({'O',0x10,0,0x18,0});
    EXPECT_FALSE(peer.reset_level);
    EXPECT_TRUE(peer.reset_asserted);
    EXPECT_TRUE(peer.wake_level);
    EXPECT_EQ(peer.reset_assertions, 1u);
    peer.send({'G','P',1,'O',8,0,0x18,0});
    EXPECT_TRUE(peer.reset_level);
    EXPECT_FALSE(peer.reset_asserted);
    EXPECT_FALSE(peer.wake_level);

    // Wrong direction or output ownership must not change the board wires.
    peer.send({'G','P',1,'I',0,0,0x18,0,'G','P',1,'O',0,0,0x1f,0});
    EXPECT_FALSE(peer.reset_asserted);
    peer.send({'G','P',1,'O',0,0,8,0});
    EXPECT_TRUE(peer.reset_asserted);
    // Bad input never renews the valid snapshot lease.
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
    peer.send({'G','P',9,'O',0,0,8,0});
    EXPECT_TRUE(peer.reset_level);
    EXPECT_FALSE(peer.reset_asserted);
    EXPECT_FALSE(peer.wake_level);

    // Reconnect restores a currently asserted output, then float releases it.
    peer.send({'G','P',1,'O',0,0,8,0});
    EXPECT_TRUE(peer.reset_asserted);
    peer.send({'G','P',1,'O',0,0,0,0});
    EXPECT_TRUE(peer.reset_level);
    EXPECT_FALSE(peer.reset_asserted);

    peer.soc_error->write(true);
    peer.soc_power->write(true);
    peer.ist_done_n->write(false);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(peer.snapshots.back()[4], 3u);
    EXPECT_EQ(peer.snapshots.back()[6], 7u);
    const auto sent = peer.snapshots.size();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_GT(peer.snapshots.size(), sent);
    sc_core::sc_stop();
}

int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    auto global = cci::cci_get_global_broker(cci::cci_originator("vmcu_gpio_test"));
    global.set_preset_cci_value("dut.publish_interval_ms", cci::cci_value(10u));
    global.set_preset_cci_value("dut.peer_timeout_ms", cci::cci_value(50u));
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
