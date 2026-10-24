/* SPDX-License-Identifier: BSD-3-Clause */
#include <zena_ni710ae_fmu.h>
#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <map>

namespace {
constexpr uint32_t V = 1u << 30, UE = 1u << 29, OF = 1u << 27;
constexpr uint32_t MV = 1u << 26, CE = 2u << 24, CI = 1u << 19;
class Bus : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<Bus, DEFAULT_TLM_BUSWIDTH> socket;
    explicit Bus(sc_core::sc_module_name name) : sc_module(name), socket("socket") {}
    tlm::tlm_response_status access(uint64_t address, uint64_t& value, bool write,
        unsigned length = 4, bool ns = false, unsigned char* enables = nullptr,
        unsigned enable_length = 0, unsigned width = 0)
    {
        tlm::tlm_generic_payload tx;
        RequestContextTlmExtension context;
        context.get_context().secure_valid = true;
        context.get_context().secure = !ns;
        tx.set_extension(&context);
        tx.set_address(address);
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        tx.set_data_length(length);
        tx.set_streaming_width(width ? width : length);
        tx.set_byte_enable_ptr(enables);
        tx.set_byte_enable_length(enable_length);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(tx, delay);
        tx.clear_extension(&context);
        EXPECT_FALSE(tx.is_dmi_allowed());
        EXPECT_EQ(delay, sc_core::SC_ZERO_TIME);
        return tx.get_response_status();
    }
    uint64_t read(uint64_t address, unsigned length = 4)
    {
        uint64_t value = 0;
        EXPECT_EQ(access(address, value, false, length), tlm::TLM_OK_RESPONSE);
        return value;
    }
    void raw(uint64_t address, uint64_t value, unsigned length = 4)
    { EXPECT_EQ(access(address, value, true, length), tlm::TLM_OK_RESPONSE); }
    void write(uint64_t address, uint64_t value, unsigned length = 4)
    { raw(0xe200, 0xbe); raw(address, value, length); }
};
class Parent : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Parent, DEFAULT_TLM_BUSWIDTH> socket;
    std::map<uint64_t, uint32_t> levels;
    unsigned calls = 0;
    explicit Parent(sc_core::sc_module_name name) : sc_module(name), socket("socket")
    { socket.register_b_transport(this, &Parent::transport); }
    void transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&)
    {
        EXPECT_TRUE(tx.is_write());
        EXPECT_EQ(tx.get_data_length(), 4u);
        uint32_t value;
        std::memcpy(&value, tx.get_data_ptr(), 4);
        EXPECT_EQ(value & ~3u, 0u);
        levels[tx.get_address()] = value;
        ++calls;
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

TEST(Ni710Fmu, RegistersSecurityClassesOverflowResetAndDetectorTraffic)
{
    auto broker = cci::cci_get_global_broker(cci::cci_originator("ni_test"));
    // Test synthesis policy, not an assertion about Apollo hardware wiring.
    broker.set_preset_cci_value("leaf.critical_vector", cci::cci_value(1u << 2));
    broker.set_preset_cci_value("leaf.node_info", cci::cci_value(std::vector<uint64_t>{4, 0x61}));
    broker.set_preset_cci_value("wide.node_index", cci::cci_value(67u));
    // This is the indexed-preset shape produced by a Lua array. More than ten
    // records catches lexical sorting (0,1,10,...,2) of the hardware record map.
    for (unsigned i = 0; i < 68; ++i) {
        const uint64_t info = i == 67 ? 0x61 : i == 10 ? 0xa0004 : 0;
        broker.set_preset_cci_value("wide.node_info." + std::to_string(i), cci::cci_value(info));
    }
    zena_ni710ae_fmu leaf("leaf"), wide("wide");
    Bus bus("bus"), detector("detector"), wide_bus("wide_bus");
    Parent parent("parent");
    bus.socket.bind(leaf.target_socket);
    detector.socket.bind(leaf.error_socket);
    wide_bus.socket.bind(wide.target_socket);
    leaf.fault_socket.bind(parent.socket);
    sc_core::sc_signal<bool> eri("eri"), fhi("fhi"), cri("cri");
    leaf.non_critical_irq.bind(eri);
    leaf.fault_irq.bind(fhi);
    leaf.critical_irq.bind(cri);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    auto settle = [] { sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS)); };
    auto select = [&](unsigned node) { bus.write(0xe210, node ? 0x61 : 4, 8); };
    auto ack = [&](unsigned node) { const auto address = node * 0x40 + 0x10; bus.write(address, bus.read(address)); settle(); };
    auto inject = [&](unsigned node, unsigned mechanism) { select(node); bus.write(0xe208, mechanism); settle(); };
    EXPECT_EQ(bus.read(0), 0x8008a2u);
    EXPECT_EQ(bus.read(0x40), 0u);
    EXPECT_EQ(bus.read(8), 1u);
    EXPECT_EQ(bus.read(0x48), 0u);
    EXPECT_EQ(bus.read(0xffc8), 2u);
    EXPECT_EQ(bus.read(0xffbc), 0x47710a00u);
    EXPECT_EQ(bus.read(0xe100), 0x43d1043bu);
    EXPECT_EQ(bus.read(0x20, 8), 4u);
    EXPECT_EQ(bus.read(0x60, 8), 0x61u);

    // Locked writes, repeated full strobes, NS trust boundary, split64 pairing.
    bus.raw(8, 0x210d);
    EXPECT_EQ(bus.read(8), 1u);
    uint64_t value = 0x123456be;
    unsigned char full = 0xff;
    EXPECT_EQ(bus.access(0xe200, value, true, 4, false, &full, 1), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(bus.read(0xe200), 0xbeu);
    value = 0;
    EXPECT_EQ(bus.access(0xe204, value, true, 4, true), tlm::TLM_COMMAND_ERROR_RESPONSE);
    EXPECT_EQ(bus.read(0xe200), 0xbeu);
    bus.raw(0xc, 0); // upper first
    EXPECT_EQ(bus.read(0xe200), 0u);
    bus.raw(8, 0x210d);
    EXPECT_EQ(bus.read(8), 0x210du);
    bus.raw(8, 0);
    EXPECT_EQ(bus.read(8), 0x210du);
    bus.raw(0xe200, 0xbe);
    bus.raw(0xe210, 0x61); // lower first
    bus.raw(0xe214, 0);
    EXPECT_EQ(bus.read(0xe210, 8), 0x61u);
    bus.raw(0xe214, 3); // third write is locked
    EXPECT_EQ(bus.read(0xe214), 0u);
    bus.raw(0xe200, 0xbe);
    unsigned char partial[4] = {0xff, 0, 0xff, 0xff};
    EXPECT_EQ(bus.access(8, value, true, 4, false, partial, 4), tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    EXPECT_EQ(bus.read(0xe200), 0u);
    bus.raw(0xe200, 0xbe);
    EXPECT_EQ(bus.access(0xe200, value, true, 8), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(bus.read(0xe200), 0u);
    EXPECT_EQ(bus.access(0x11, value, false), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_EQ(bus.access(8, value, false, 8, false, nullptr, 0, 4), tlm::TLM_BURST_ERROR_RESPONSE);

    // Node selection affects SMEN and injection; unknown identity is inert.
    bus.write(0xe204, 0x3ffff);
    select(0);
    EXPECT_EQ(bus.read(0xe204), 0u);
    inject(0, 2);
    EXPECT_EQ(bus.read(0xe000, 8), 0u);
    select(0);
    bus.write(0xe204, 0x3ffff);
    bus.write(0xe210, 0xdead, 8);
    bus.write(0xe204, 0x3ffff);
    bus.write(0xe208, 2);
    EXPECT_EQ(bus.read(0xe000), 0u);
    inject(1, 63);
    EXPECT_EQ(bus.read(0xe000), 0u);

    // All three output levels are independent. Critical UE can assert all three.
    inject(1, 2);
    EXPECT_TRUE(eri.read()); EXPECT_TRUE(fhi.read()); EXPECT_TRUE(cri.read());
    EXPECT_EQ(bus.read(0x50) & (V | UE | CI), V | UE | CI);
    EXPECT_EQ(parent.levels[0x10000], 1u);
    EXPECT_EQ(parent.levels[0x10040], 1u);
    EXPECT_EQ(parent.levels[0x10080], 3u);
    ack(1);
    EXPECT_FALSE(eri.read()); EXPECT_FALSE(fhi.read()); EXPECT_FALSE(cri.read());
    EXPECT_EQ(parent.levels[0x10000], 0u);
    EXPECT_EQ(parent.levels[0x10040], 0u);
    EXPECT_EQ(parent.levels[0x10080], 2u);
    EXPECT_EQ(bus.read(0x50) & 1, 0u);
    EXPECT_EQ((bus.read(0x50) >> 8) & 0xff, 2u);

    inject(1, 14);
    EXPECT_FALSE(eri.read()); EXPECT_TRUE(fhi.read()); EXPECT_FALSE(cri.read());
    EXPECT_EQ(bus.read(0x50) & (V | UE | CE | CI), V | CE);
    // UE overwrites CE syndrome, with OF; CE flag survives for independent FHI.
    inject(1, 13);
    EXPECT_EQ((bus.read(0x50) >> 8) & 0xff, 13u);
    EXPECT_EQ(bus.read(0x50) & (V | UE | CE | OF), V | UE | CE | OF);
    bus.write(0x50, V | UE); // stale snapshot lacks OF and cannot clear UE/V
    EXPECT_EQ(bus.read(0x50) & (V | UE), V | UE);
    bus.write(0x50, V | UE | OF | MV | (13u << 8));
    settle();
    EXPECT_EQ(bus.read(0x50) & (V | UE | CE), V | CE);
    EXPECT_FALSE(eri.read()); EXPECT_TRUE(fhi.read());
    EXPECT_EQ((bus.read(0x50) >> 8) & 0xff, 13u); // No fabricated replay of CE syndrome.
    ack(1);
    EXPECT_FALSE(fhi.read());

    // Same-class overflow retains first syndrome. Independent detector packet
    // uses the same logging policy but never calls the software inject register.
    inject(0, 3);
    detector.raw(0, 5);
    settle();
    EXPECT_EQ((bus.read(0x10) >> 8) & 0xff, 3u);
    EXPECT_NE(bus.read(0x10) & OF, 0u);
    EXPECT_EQ(bus.read(0xe000), 1u);
    value = 1u << 31;
    EXPECT_EQ(detector.access(0, value, true), tlm::TLM_ADDRESS_ERROR_RESPONSE);
    EXPECT_FALSE(leaf.runtime_report_error(2, 0));
    ack(0);
    // Higher UET severity wins; equal severity retains the first syndrome.
    EXPECT_TRUE(leaf.runtime_report_error(0, 3, 3));
    EXPECT_TRUE(leaf.runtime_report_error(0, 5, 1));
    EXPECT_EQ((bus.read(0x10) >> 8) & 0xff, 5u);
    ack(0);
    bus.write(8, 0);
    EXPECT_TRUE(leaf.runtime_report_error(0, 3));
    EXPECT_EQ(bus.read(0xe000), 0u);
    bus.write(8, 0x210d);

    // Warm reset preserves diagnostics, cold and record resets clear them.
    inject(1, 2);
    leaf.functional_reset(); settle();
    EXPECT_NE(bus.read(0x50) & V, 0u);
    EXPECT_FALSE(cri.read());
    EXPECT_EQ(bus.read(0xe204), 0u);
    bus.write(8, 0x210d); settle();
    EXPECT_TRUE(cri.read());
    leaf.record_reset_in->write(true); settle();
    EXPECT_EQ(bus.read(0xe000), 0u);
    EXPECT_FALSE(leaf.runtime_report_error(1, 2));
    EXPECT_EQ(bus.read(8), 0x210du);
    leaf.record_reset_in->write(false);
    select(1); bus.write(0xe204, 0x3ffff); inject(1, 2);
    leaf.reset_in->write(true); settle();
    EXPECT_EQ(bus.read(0xe000), 0u);
    EXPECT_FALSE(leaf.runtime_report_error(1, 2));
    bus.raw(0xe200, 0xbe);
    EXPECT_EQ(bus.read(0xe200), 0u);
    leaf.reset_in->write(false);

    // Sparse discovery preserves a platform self-node index and GSR > 64.
    EXPECT_EQ(wide_bus.read(0xffc8), 68u);
    EXPECT_EQ(wide_bus.read(0x20), 0u);
    EXPECT_EQ(wide_bus.read(10 * 0x40 + 0x20), 0xa0004u);
    EXPECT_EQ(wide_bus.read(67 * 0x40 + 0x20), 0x61u);
    EXPECT_FALSE(wide.runtime_report_error(0, 2));
    wide_bus.write(0xe210, 0x61, 8);
    wide_bus.write(0xe204, 1u << 2);
    wide_bus.write(0xe208, 2);
    EXPECT_EQ(wide_bus.read(0xe000, 8), 0u);
    EXPECT_EQ(wide_bus.read(0xe008, 8), 8u);
    EXPECT_EQ(wide_bus.read(67 * 0x40 + 0x10) & CI, 0u); // default policy is non-critical.
    wide.reset();
    sc_core::sc_stop();
}
}
int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
