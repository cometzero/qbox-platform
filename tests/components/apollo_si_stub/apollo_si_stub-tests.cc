/* SPDX-License-Identifier: BSD-3-Clause */
#include <apollo_si_stub.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <ports/target-signal-socket.h>

namespace {
constexpr uint64_t RX = 0x120000, TX = 0x140000;
uint64_t get(const uint8_t* p, unsigned n)
{
    uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
    return v;
}
void put(uint8_t* p, uint64_t v, unsigned n)
{ for (unsigned i = 0; i < n; ++i) p[i] = v >> (8 * i); }
uint16_t checksum(const uint8_t* p, unsigned n)
{
    uint32_t sum = 0;
    while (n > 1) { sum += (unsigned(p[0]) << 8) | p[1]; p += 2; n -= 2; }
    if (n) sum += unsigned(p[0]) << 8;
    while (sum >> 16) sum = (sum & 65535) + (sum >> 16);
    return uint16_t(~sum);
}
void be(uint8_t* p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
struct bench : sc_core::sc_module {
    apollo_si_stub dut;
    tlm_utils::simple_target_socket<bench, DEFAULT_TLM_BUSWIDTH> memory;
    TargetSignalSocket<bool> pbx_irq, mbx_irq;
    bool pbx_level = false, mbx_level = false;
    std::vector<uint8_t> ram;
    unsigned transactions = 0;
    explicit bench(sc_core::sc_module_name name)
        : sc_module(name), dut("dut"), memory("memory"), pbx_irq("pbx_irq"),
          mbx_irq("mbx_irq"), ram(0x80000)
    {
        dut.initiator_socket.bind(memory);
        memory.register_b_transport(this, &bench::transport);
        dut.irq_pbx.bind(pbx_irq); dut.irq_mbx.bind(mbx_irq);
        pbx_irq.register_value_changed_cb([this](bool value) { pbx_level = value; });
        mbx_irq.register_value_changed_cb([this](bool value) { mbx_level = value; });
    }
    uint8_t* ptr(uint64_t a) { return ram.data() + a - 0x100000; }
    void transport(tlm::tlm_generic_payload& t, sc_core::sc_time& delay)
    {
        ++transactions;
        EXPECT_GE(t.get_address(), 0x100000u);
        EXPECT_LE(t.get_address() + t.get_data_length(), 0x180000u);
        if (t.get_address() < 0x100000 || t.get_address() + t.get_data_length() > 0x180000) {
            t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
        }
        if (t.is_write()) std::copy(t.get_data_ptr(), t.get_data_ptr() + t.get_data_length(), ptr(t.get_address()));
        else std::copy(ptr(t.get_address()), ptr(t.get_address()) + t.get_data_length(), t.get_data_ptr());
        delay += sc_core::sc_time(1, sc_core::SC_NS);
        t.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    uint32_t mmio(uint64_t address, uint32_t value = 0, bool write = true)
    {
        uint8_t data[4]; put(data, value, 4);
        tlm::tlm_generic_payload t;
        t.set_address(address); t.set_data_ptr(data); t.set_data_length(4); t.set_streaming_width(4);
        t.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        dut.b_transport(t, delay);
        EXPECT_EQ(t.get_response_status(), tlm::TLM_OK_RESPONSE);
        EXPECT_FALSE(t.is_dmi_allowed());
        return get(data, 4);
    }
    void run() { sc_core::sc_start(10, sc_core::SC_US); }
    void ack() { mmio(0x31008, 7); mmio(0x1014, 1); }
    void rings()
    {
        std::fill(ptr(RX), ptr(RX) + 0x20000, 0);
        std::fill(ptr(TX), ptr(TX) + 0x20000, 0);
        for (unsigned i = 0; i < 32; ++i) {
            put(ptr(RX + i * 16), 0x160000 + i * 512, 8);
            put(ptr(RX + i * 16 + 8), 512, 4);
            put(ptr(RX + i * 16 + 12), 2, 2);
            put(ptr(RX + 516 + i * 2), i, 2);
            put(ptr(TX + i * 16), 0x164000 + i * 512, 8);
        }
        put(ptr(RX + 514), 32, 2);
        // Linux publishes DRIVER_OK after setting up the shared virtqueues.
        put(ptr(0x10002c), 7, 1);
    }
    void kick() { mmio(0x100c, 3); run(); }
    std::vector<uint8_t> take()
    {
        uint16_t used = get(ptr(RX + 594), 2);
        EXPECT_NE(used, 0);
        uint64_t slot = RX + 596 + ((used - 1) % 32) * 8;
        unsigned id = get(ptr(slot), 4), length = get(ptr(slot + 4), 4);
        EXPECT_LT(id, 32u); EXPECT_LE(length, 512u);
        std::vector<uint8_t> out(ptr(0x160000 + id * 512), ptr(0x160000 + id * 512) + length);
        uint16_t avail = get(ptr(RX + 514), 2);
        put(ptr(RX + 516 + (avail % 32) * 2), id, 2);
        put(ptr(RX + 514), uint16_t(avail + 1), 2);
        ack(); return out;
    }
    void send(const std::vector<uint8_t>& payload, uint32_t dst = 1024)
    {
        uint16_t avail = get(ptr(TX + 514), 2);
        unsigned id = avail % 32;
        auto* data = ptr(0x164000 + id * 512);
        std::fill(data, data + 512, 0);
        put(data, 2048, 4); put(data + 4, dst, 4); put(data + 12, payload.size(), 2);
        std::copy(payload.begin(), payload.end(), data + 16);
        put(ptr(TX + id * 16 + 8), payload.size() + 16, 4);
        put(ptr(TX + 516 + id * 2), id, 2);
        put(ptr(TX + 514), uint16_t(avail + 1), 2);
        kick();
        EXPECT_EQ(get(ptr(TX + 594), 2), uint16_t(avail + 1));
    }
};
std::vector<uint8_t> packet(bool vlan, bool arp, unsigned payload = 56)
{
    unsigned offset = vlan ? 18 : 14;
    std::vector<uint8_t> out(offset + (arp ? 28 : 28 + payload));
    const uint8_t mac[] = {2, 3, 4, 5, 6, 7}, peer[] = {0, 1, 2, 3, 4, 6};
    std::copy(peer, peer + 6, out.begin()); std::copy(mac, mac + 6, out.begin() + 6);
    be(out.data() + 12, vlan ? 0x8100 : arp ? 0x806 : 0x800);
    if (vlan) { be(out.data() + 14, 200); be(out.data() + 16, arp ? 0x806 : 0x800); }
    auto* p = out.data() + offset;
    const uint8_t local_ip[] = {192, 168, 1, 2}, remote_ip[] = {192, 168, 1, 1};
    if (arp) {
        be(p, 1); be(p + 2, 0x800); p[4] = 6; p[5] = 4; be(p + 6, 1);
        std::copy(mac, mac + 6, p + 8); std::copy(local_ip, local_ip + 4, p + 14);
        std::copy(remote_ip, remote_ip + 4, p + 24);
    } else {
        p[0] = 0x45; be(p + 2, 28 + payload); p[8] = 64; p[9] = 1;
        std::copy(local_ip, local_ip + 4, p + 12); std::copy(remote_ip, remote_ip + 4, p + 16);
        be(p + 10, checksum(p, 20)); p[20] = 8; be(p + 24, 123); be(p + 26, 456);
        for (unsigned i = 0; i < payload; ++i) p[28 + i] = i;
        be(p + 22, checksum(p + 20, 8 + payload));
    }
    return out;
}
} // namespace

TEST(ApolloSiStub, AttachTrafficWrapMalformedAndReattach)
{
    bench b("bench");
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    EXPECT_EQ(b.mmio(0xfcc, 0, false), 0x20u);
    EXPECT_EQ(b.mmio(0x30000, 0, false), 1u);
    b.mmio(0x101c, 1); b.mmio(0x1018, 1);
    b.mmio(0x3101c, 1); b.mmio(0x31018, 7);
    b.mmio(0x100c, 8); b.run();
    EXPECT_EQ(b.dut.attach_count(), 1u);
    EXPECT_EQ(get(b.ptr(0x100000), 4), 1u);
    EXPECT_EQ(get(b.ptr(0x10001c), 4), 0u);
    EXPECT_EQ(b.mmio(0x1000, 0, false), 0u);
    EXPECT_TRUE(b.pbx_level); EXPECT_TRUE(b.mbx_level);
    b.ack(); EXPECT_FALSE(b.pbx_level); EXPECT_FALSE(b.mbx_level);
    b.rings(); b.kick(); auto ns = b.take();
    ASSERT_EQ(ns.size(), 56u); EXPECT_EQ(get(ns.data() + 4, 4), 53u);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(ns.data() + 16)), "ethsi1");
    b.send({0}); EXPECT_EQ(b.dut.rx_count(), 1u); b.ack();
    for (bool vlan : {false, true}) {
        b.send(packet(vlan, true)); auto reply = b.take();
        unsigned offset = 16 + (vlan ? 18 : 14);
        ASSERT_GT(reply.size(), offset + 27);
        EXPECT_EQ(reply[offset + 7], 2u);
        EXPECT_EQ(reply[offset + 17], 1u); EXPECT_EQ(reply[offset + 27], 2u);
        for (unsigned i = 0; i < 72; ++i) {
            unsigned payload = i % 2 ? 450 : 56;
            const auto request = packet(vlan, false, payload);
            b.send(request); reply = b.take();
            ASSERT_EQ(reply.size(), request.size() + 16);
            EXPECT_EQ(get(reply.data() + 4, 4), 2048u);
            EXPECT_EQ(reply[offset + 20], 0u);
            EXPECT_EQ(checksum(reply.data() + offset, 20), 0u);
            EXPECT_EQ(checksum(reply.data() + offset + 20, 8 + payload), 0u);
            EXPECT_TRUE(std::equal(request.begin() + (vlan ? 18 : 14) + 28,
                                  request.end(), reply.begin() + offset + 28));
            if (vlan) EXPECT_EQ(reply[16 + 15], 200u);
        }
    }
    EXPECT_EQ(b.dut.error_count(), 0u);
    // Bad checksum and unsupported protocol do not produce fabricated replies.
    auto bad = packet(false, false); bad.back() ^= 1;
    const auto before = b.dut.rx_count(); b.send(bad);
    EXPECT_EQ(b.dut.rx_count(), before); EXPECT_GT(b.dut.drop_count(), 0u); b.ack();
    // Out-of-carveout descriptor must never escape to the memory target.
    unsigned avail = get(b.ptr(TX + 514), 2), id = avail % 32;
    put(b.ptr(TX + id * 16), 0xdeadbeef, 8);
    b.send({0}); EXPECT_GT(b.dut.error_count(), 0u); b.ack();
    // Debug trigger is non-executing; malformed MMIO reports a bus error.
    uint32_t value = 8; tlm::tlm_generic_payload t;
    t.set_command(tlm::TLM_WRITE_COMMAND); t.set_address(0x100c);
    t.set_data_ptr(reinterpret_cast<uint8_t*>(&value)); t.set_data_length(4); t.set_streaming_width(4);
    EXPECT_EQ(b.dut.transport_dbg(t), 4u); b.run(); EXPECT_EQ(b.dut.attach_count(), 1u);
    t.set_address(0x60000); sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    b.dut.b_transport(t, delay); EXPECT_EQ(t.get_response_status(), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    // Linux detach/start does not repeat the initial attach mailbox handshake.
    // Coalesced kicks are from the old session: detach must dominate them.
    const auto before_detach = b.dut.rx_count();
    put(b.ptr(0x10002c), 0, 1);
    b.mmio(0x100c, 7); b.run(); sc_core::sc_start(200, sc_core::SC_US);
    EXPECT_EQ(b.dut.rx_count(), before_detach);
    // .attach() does not issue a fresh handshake; recover even if its only
    // vring kick was coalesced with the detach that just completed.
    b.ack(); b.rings(); sc_core::sc_start(200, sc_core::SC_US); ns = b.take();
    ASSERT_EQ(ns.size(), 56u); EXPECT_EQ(get(ns.data() + 4, 4), 53u);
    b.send(packet(true, false, 450)); auto reply = b.take(); ASSERT_EQ(reply.size(), 512u);
    EXPECT_EQ(b.dut.attach_count(), 1u);
    sc_core::sc_stop();
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker"); cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
