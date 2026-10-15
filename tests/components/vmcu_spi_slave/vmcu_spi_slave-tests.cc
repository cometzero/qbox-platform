/* SPDX-License-Identifier: BSD-3-Clause */
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <vmcu_spi_slave.h>
#include <tlm_utils/simple_initiator_socket.h>
class Peer : public sc_core::sc_module
{
public:
    gs::biflow_socket<Peer> socket;
    tlm_utils::simple_initiator_socket<Peer, DEFAULT_TLM_BUSWIDTH> mmio;
    TargetSignalSocket<bool> irq;
    InitiatorSignalSocket<bool> reset;
    unsigned replies = 0;
    std::array<uint8_t, 64> reply{};
    bool level = false;
    Peer(sc_core::sc_module_name n): sc_module(n), socket("socket"), mmio("mmio"), irq("irq"), reset("reset")
    {
        socket.register_b_transport(this, &Peer::receive);
        irq.register_value_changed_cb([this](bool v) { level = v; });
    }
    void end_of_elaboration() override { socket.can_receive_any(); }
    void receive(tlm::tlm_generic_payload& t, sc_core::sc_time&)
    {
        ++replies;
        ASSERT_EQ(t.get_data_length(), 64u);
        std::copy_n(t.get_data_ptr(), 64, reply.begin());
    }
    uint32_t reg(unsigned a, bool write = false, uint32_t v = 0)
    {
        tlm::tlm_generic_payload t;
        sc_core::sc_time d = sc_core::SC_ZERO_TIME;
        t.set_address(a);
        t.set_data_ptr(reinterpret_cast<uint8_t*>(&v));
        t.set_data_length(4);
        t.set_streaming_width(4);
        t.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        mmio->b_transport(t, d);
        EXPECT_EQ(t.get_response_status(), tlm::TLM_OK_RESPONSE);
        return v;
    }
    void send(std::array<uint8_t, 64> r, bool split = false, bool advance = true, bool corrupt_crc = false)
    {
        uint32_t c = vmcu_spi_slave::crc(r.data(), 60);
        if (corrupt_crc) c ^= 1;
        for (unsigned i = 0; i < 4; i++) r[60 + i] = c >> (8 * i);
        tlm::tlm_generic_payload t;
        t.set_data_ptr(r.data());
        t.set_data_length(split ? 13 : 64);
        socket.force_send(t);
        if (split) {
            t.set_data_ptr(r.data() + 13);
            t.set_data_length(51);
            socket.force_send(t);
        }
        if (advance) sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    }
};
TEST(SpiSlave, FullDuplexCsBoundsResetAndStale)
{
    EXPECT_EQ(vmcu_spi_slave::crc(reinterpret_cast<const uint8_t*>("123456789"), 9), 0xcbf43926u);
    vmcu_spi_slave dut("dut");
    Peer p("peer");
    p.socket.bind(dut.backend_socket);
    p.mmio.bind(dut.target_socket);
    dut.irq.bind(p.irq);
    p.reset.bind(dut.reset);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(p.reg(0), 0x53504931u);
    p.reg(4, true, 3);
    p.reg(0x1c, true, 9);
    std::array<uint8_t, 64> r{};
    std::memcpy(r.data(), "SPI1", 4);
    r[4] = 1;
    r[8] = 1;
    p.send(r, true);
    EXPECT_EQ(p.reply[4], 2);
    p.reg(0x14, true, 0x55);
    p.reg(0x14, true, 0xaa);
    p.reg(0x24, true, 2);
    r[4] = 3;
    r[6] = 1;
    r[7] = 1;
    r[16] = 1;
    r[20] = 0x40;
    r[21] = 0x42;
    r[22] = 0xf;
    r[26] = 8;
    r[28] = 0x12;
    p.send(r);
    EXPECT_EQ(p.reply[5], 0);
    EXPECT_EQ(p.reply[28], 0x55);
    EXPECT_TRUE(p.reg(8) & 4);
    EXPECT_TRUE(p.level);
    EXPECT_EQ(p.reg(0x14), 0x12u);
    r[6] = 2;
    r[16] = 2;
    r[28] = 0x34;
    p.send(r);
    EXPECT_EQ(p.reply[28], 0xaa);
    EXPECT_FALSE(p.reg(8) & 4);
    EXPECT_TRUE(p.reg(0x18) & 8);
    p.send(r);
    EXPECT_EQ(p.reply[5], 1);
    EXPECT_EQ(p.reg(0x10), 1u); // Duplicate cannot push twice.
    p.reg(4, true, 3);
    EXPECT_EQ(p.reg(0x10), 0u);
    EXPECT_EQ(p.reg(0x20), 0u);
    r[6] = 3;
    r[16] = 3;
    p.send(r);
    EXPECT_EQ(p.reply[5], 3);
    EXPECT_EQ(p.reply[28], 0xff);
    EXPECT_EQ(p.reg(0x14), 0x34u);
    EXPECT_EQ(p.reg(0x20), 1u);
    p.reg(4, true, 3);
    for (unsigned i = 0; i < 65; i++) p.reg(0x14, true, i);
    EXPECT_EQ(p.reg(0xc), 64u);
    EXPECT_EQ(p.reg(0x20), 2u);
    r[16] = 4;
    r[25] = 1;
    p.send(r);
    EXPECT_EQ(p.reply[5], 1);
    EXPECT_EQ(p.reg(0xc), 64u);
    r[25] = 0;
    r[8] = 2;
    r[16] = 5;
    p.send(r);
    EXPECT_EQ(p.reply[5], 1);
    EXPECT_EQ(p.reg(0xc), 64u);
    // Queue saturation must not restore an error after hardware reset.
    r[8] = 1;
    for (unsigned i = 0; i < 5; ++i) p.send(r, false, false);
    p.reset->write(true);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(p.reply[4], 5);
    EXPECT_EQ(p.reply[5], 1);
    EXPECT_EQ(p.reply[8], 1);
    EXPECT_EQ(p.reg(0x20), 0u);
    EXPECT_EQ(p.reg(0xc), 0u);
    EXPECT_EQ(p.reg(0x10), 0u);
    const auto replies = p.replies;
    p.send(r);
    EXPECT_EQ(p.replies, replies); // No old-generation completion after CANCEL.
    p.reset->write(false);
    r.fill(0);
    std::memcpy(r.data(), "SPI1", 4);
    r[4] = 1;
    r[8] = 2;
    p.send(r);
    EXPECT_EQ(p.reply[4], 2);
    EXPECT_EQ(p.reply[8], 2);
    // BEGIN during staging cannot consume a partially prepared publication.
    p.reg(4, true, 3);
    p.reg(0x14, true, 0x10);
    r[4] = 3;
    r[6] = 1;
    r[7] = 1;
    r[16] = 1;
    r[20] = 1;
    r[26] = 8;
    p.send(r);
    EXPECT_EQ(p.reply[28], 0xff);
    EXPECT_EQ(p.reg(0xc), 1u);
    p.reg(0x14, true, 0x20);
    p.reg(0x14, true, 0x30);
    p.reg(0x24, true, 2); // Invalid partial publication must not expose bytes.
    EXPECT_EQ(p.reg(0x24), 0u);
    EXPECT_TRUE(p.reg(0x20) & 4);
    p.reg(0x24, true, 3);
    r[6] = 2;
    r[7] = 2;
    r[16] = 2;
    p.send(r);
    EXPECT_EQ(p.reply[28], 0xff);
    EXPECT_EQ(p.reply[29], 0xff);
    EXPECT_EQ(p.reg(0xc), 3u);
    EXPECT_EQ(p.reg(0x24), 3u);
    r[6] = 3;
    r[7] = 3;
    r[16] = 3;
    p.send(r);
    EXPECT_EQ(p.reply[5], 0);
    EXPECT_EQ(p.reply[28], 0x10);
    EXPECT_EQ(p.reply[29], 0x20);
    EXPECT_EQ(p.reply[30], 0x30);
    EXPECT_EQ(p.reg(0xc), 0u);
    EXPECT_EQ(p.reg(0x24), 0u);
    p.reg(0x14, true, 0x40);
    p.reg(0x14, true, 0x50);
    p.reg(0x24, true, 2);
    p.reg(0x24, true, 2); // Republish cannot change an outstanding publication.
    EXPECT_EQ(p.reg(0x24), 2u);
    EXPECT_TRUE(p.reg(0x20) & 4);
    r[7] = 1;
    r[16] = 4;
    p.send(r);
    EXPECT_EQ(p.reply[28], 0x40);
    EXPECT_EQ(p.reg(0x24), 1u); // Early END retains unclocked published bytes.
    r[16] = 5;
    p.send(r);
    EXPECT_EQ(p.reply[28], 0x50);
    EXPECT_EQ(p.reg(0x24), 0u);
    p.reg(0x14, true, 0x60);
    p.reg(0x24, true, 1);
    p.reg(4, true, 3);
    EXPECT_EQ(p.reg(0x24), 0u);
    // A malformed record cannot clock either FIFO; the next valid one resynchronizes.
    r[16] = 6;
    auto before = p.replies;
    p.send(r, false, true, true);
    EXPECT_EQ(p.replies, before);
    EXPECT_EQ(p.reg(0x10), 0u);
    p.send(r);
    EXPECT_EQ(p.replies, before + 1);
    EXPECT_EQ(p.reg(0x10), 1u);
    p.reg(4, true, 3);
    r[4] = 99;
    before = p.replies;
    p.send(r);
    EXPECT_EQ(p.replies, before);
    EXPECT_EQ(p.reg(0x20), 4u);
    EXPECT_EQ(p.reg(0x10), 0u);
    r[4] = 3;
    r[16] = 7;
    r[7] = 33;
    p.send(r);
    EXPECT_EQ(p.reply[5], 1);
    EXPECT_EQ(p.reg(0x10), 0u);
    p.reg(4, true, 3);

    // RX overflow is fatal result 4, distinct from empty-TX filler result 3.
    r[7] = 32;
    p.send(r);
    EXPECT_EQ(p.reply[5], 3);
    r[16] = 8;
    p.send(r);
    EXPECT_EQ(p.reg(0x10), 64u);
    r[16] = 9;
    r[7] = 1;
    p.send(r);
    EXPECT_EQ(p.reply[5], 4);
    EXPECT_EQ(p.reg(0x10), 64u);
    EXPECT_EQ(p.reg(0x20), 3u);
    p.reg(0x20, true, 3);
    EXPECT_EQ(p.reg(0x20), 0u);
    p.reg(4, true, 3);

    // Published and staged bytes share the same physical capacity.
    for (unsigned i = 0; i < 64; ++i) p.reg(0x14, true, i);
    p.reg(0x24, true, 64);
    r[16] = 10;
    r[7] = 32;
    p.send(r);
    EXPECT_EQ(p.reg(0x24), 32u);
    for (unsigned i = 0; i < 33; ++i) p.reg(0x14, true, i);
    EXPECT_EQ(p.reg(0xc), 64u);
    EXPECT_EQ(p.reg(0x24), 32u);
    EXPECT_TRUE(p.reg(0x20) & 2);
    p.reg(4, true, 3);

    // The ingress cap is four whole records, without additional FIFO capacity.
    before = p.replies;
    r[7] = 1;
    for (unsigned i = 0; i < 5; ++i) {
        r[16] = 11 + i;
        p.send(r, false, false);
    }
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    EXPECT_EQ(p.replies, before + 4);
    EXPECT_EQ(p.reg(0x10), 4u);
    EXPECT_TRUE(p.reg(0x20) & 4);
    sc_core::sc_stop();
}
int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker b("global_broker");
    cci::cci_register_broker(b);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
