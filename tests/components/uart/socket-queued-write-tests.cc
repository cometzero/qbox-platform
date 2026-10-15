/* SPDX-License-Identifier: BSD-3-Clause */
#include <char_backend_socket.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <future>

class SocketPeer : public sc_core::sc_module
{
public:
    gs::biflow_socket<SocketPeer> socket;
    SocketPeer(sc_core::sc_module_name n): sc_module(n), socket("socket")
    {
        socket.register_b_transport(this, &SocketPeer::receive);
    }
    void receive(tlm::tlm_generic_payload& t, sc_core::sc_time&) { t.set_response_status(tlm::TLM_OK_RESPONSE); }
    void end_of_elaboration() override { socket.can_receive_any(); }
};

TEST(SocketQueue, BackpressurePreservesEntireStream)
{
    asio::io_context io;
    tcp::acceptor listener(io, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    const auto address = "127.0.0.1:" + std::to_string(listener.local_endpoint().port());
    auto broker = cci::cci_get_global_broker(cci::cci_originator("test"));
    broker.set_preset_cci_value("dut.address", cci::cci_value(address));
    broker.set_preset_cci_value("dut.server", cci::cci_value(false));
    broker.set_preset_cci_value("dut.write_queue_size", cci::cci_value(16u * 1024 * 1024));
    broker.set_preset_cci_value("dut.poll_interval_ms", cci::cci_value(1u));
    tcp::acceptor unused(io, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    const auto absent_address = "127.0.0.1:" + std::to_string(unused.local_endpoint().port());
    unused.close();
    broker.set_preset_cci_value("absent.address", cci::cci_value(absent_address));
    broker.set_preset_cci_value("absent.server", cci::cci_value(false));
    broker.set_preset_cci_value("absent.write_queue_size", cci::cci_value(256u));
    char_backend_socket absent("absent");
    SocketPeer absent_peer("absent_peer");
    absent_peer.socket.bind(absent.m_biflow_socket);
    char_backend_socket dut("dut");
    SocketPeer peer("peer");
    peer.socket.bind(dut.m_biflow_socket);
    std::vector<uint8_t> expected(8u * 1024 * 1024), received(expected.size());
    for (size_t i = 0; i < expected.size(); i++) expected[i] = i * 17;
    std::promise<void> connected;
    auto connection = connected.get_future();
    std::promise<void> read_done, release_reader;
    auto read_finished = read_done.get_future();
    auto release = release_reader.get_future();
    auto reader = std::async(std::launch::async, [&] {
        tcp::socket socket(io);
        listener.accept(socket);
        connected.set_value();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        asio::read(socket, asio::buffer(received));
        read_done.set_value();
        release.wait();
    });
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    ASSERT_EQ(connection.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    tlm::tlm_generic_payload t;
    t.set_data_ptr(expected.data());
    t.set_data_length(expected.size());
    t.set_streaming_width(expected.size());
    peer.socket.force_send(t);
    EXPECT_EQ(t.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(read_finished.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    EXPECT_TRUE(received == expected);
    peer.socket.force_send(t);
    EXPECT_EQ(t.get_response_status(), tlm::TLM_OK_RESPONSE);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto start = std::chrono::steady_clock::now();
    dut.cleanup_receive_thread();
    absent.cleanup_receive_thread();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
    release_reader.set_value();
    reader.get();
    sc_core::sc_stop();
}
int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker b("global_broker");
    cci::cci_register_broker(b);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
