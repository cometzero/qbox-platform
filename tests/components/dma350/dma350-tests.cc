/* SPDX-License-Identifier: BSD-3-Clause */

#include <cstdint>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#include <cci/utils/broker.h>
#include <dma350.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace {
constexpr uint64_t DMA_BUILDCFG0 = 0xfb0;
constexpr uint64_t DMA_BUILDCFG1 = 0xfb4;
constexpr uint64_t DMAINFO_IIDR = 0xfc8;
constexpr uint64_t NSEC_CHINTRSTATUS0 = 0x200;
constexpr uint64_t NSEC_STATUS = 0x208;
constexpr uint64_t NSEC_CTRL = 0x20c;
constexpr uint64_t CH0 = 0x1000;
constexpr uint64_t CH1 = 0x1100;
constexpr uint64_t CH_CMD = 0x00;
constexpr uint64_t CH_STATUS = 0x04;
constexpr uint64_t CH_INTREN = 0x08;
constexpr uint64_t CH_CTRL = 0x0c;
constexpr uint64_t CH_SRCADDR = 0x10;
constexpr uint64_t CH_SRCADDRHI = 0x14;
constexpr uint64_t CH_DESADDR = 0x18;
constexpr uint64_t CH_DESADDRHI = 0x1c;
constexpr uint64_t CH_XSIZE = 0x20;
constexpr uint64_t CH_XSIZEHI = 0x24;
constexpr uint64_t CH_XADDRINC = 0x30;
constexpr uint64_t CH_FILLVAL = 0x38;
constexpr uint64_t CH_DESTRIGINCFG = 0x50;
constexpr uint64_t CH_ERRINFO = 0x90;
constexpr uint64_t CH_BUILDCFG1 = 0xfc;
constexpr uint32_t STAT_DONE = 1u << 16;
constexpr uint32_t STAT_ERR = 1u << 17;
constexpr uint32_t STAT_STOPPED = 1u << 19;
constexpr uint32_t STAT_PAUSED = 1u << 20;
constexpr uint32_t STAT_RESUMEWAIT = 1u << 21;
constexpr uint32_t INTR_DONE = 1u << 0;
constexpr uint32_t INTR_ERR = 1u << 1;
constexpr uint32_t TRIGGER_ACTIVE = 1u << 2;
constexpr uint32_t ACK_LAST_OKAY = 2u;

class TestMemory : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<TestMemory, DEFAULT_TLM_BUSWIDTH> target_socket;
    std::unordered_map<uint64_t, uint8_t> bytes;
    std::vector<std::vector<uint8_t>> fifo_writes;
    uint64_t fifo_address = std::numeric_limits<uint64_t>::max();
    uint64_t fail_read_address = std::numeric_limits<uint64_t>::max();
    uint64_t fail_write_address = std::numeric_limits<uint64_t>::max();

    explicit TestMemory(sc_core::sc_module_name name)
        : sc_core::sc_module(name), target_socket("target_socket")
    {
        target_socket.register_b_transport(this, &TestMemory::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const uint64_t address = trans.get_address();
        const unsigned int len = trans.get_data_length();
        uint8_t* data = trans.get_data_ptr();
        if (!data) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            if (address == fail_read_address) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            for (unsigned int i = 0; i < len; ++i) data[i] = bytes[address + i];
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            if (address == fail_write_address) {
                trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            if (address == fifo_address)
                fifo_writes.emplace_back(data, data + len);
            else
                for (unsigned int i = 0; i < len; ++i) bytes[address + i] = data[i];
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        delay += sc_core::sc_time(100, sc_core::SC_PS);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class MmioInitiator : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<MmioInitiator, DEFAULT_TLM_BUSWIDTH> socket;

    explicit MmioInitiator(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket")
    {
    }
};

uint32_t access32(dma350& dut, uint64_t offset, tlm::tlm_command command,
                  uint32_t value = 0)
{
    tlm::tlm_generic_payload trans;
    trans.set_address(offset);
    trans.set_command(command);
    trans.set_data_length(sizeof(value));
    trans.set_streaming_width(sizeof(value));
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    dut.b_transport(trans, delay);
    EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
    return value;
}

uint32_t read32(dma350& dut, uint64_t offset)
{
    return access32(dut, offset, tlm::TLM_READ_COMMAND);
}

void write32(dma350& dut, uint64_t offset, uint32_t value)
{
    (void)access32(dut, offset, tlm::TLM_WRITE_COMMAND, value);
}

void write64_address(dma350& dut, uint64_t low_register, uint64_t address)
{
    write32(dut, low_register, static_cast<uint32_t>(address));
    write32(dut, low_register + 4, static_cast<uint32_t>(address >> 32));
}

void configure_copy(dma350& dut, uint64_t source, uint64_t dest,
                    uint32_t count, unsigned int transfer_log2 = 0,
                    int16_t source_inc = 1, int16_t dest_inc = 1,
                    uint32_t extra_ctrl = 0, uint64_t channel = CH0)
{
    write64_address(dut, channel + CH_SRCADDR, source);
    write64_address(dut, channel + CH_DESADDR, dest);
    write32(dut, channel + CH_XSIZE,
            (count & 0xffffu) | ((count & 0xffffu) << 16));
    write32(dut, channel + CH_XSIZEHI,
            (count >> 16) | ((count >> 16) << 16));
    write32(dut, channel + CH_XADDRINC,
            static_cast<uint16_t>(source_inc) |
                (static_cast<uint32_t>(static_cast<uint16_t>(dest_inc)) << 16));
    write32(dut, channel + CH_CTRL,
            transfer_log2 | (1u << 9) | (1u << 21) | extra_ctrl);
}

template <typename T>
void drive(TargetSignalSocket<T>& socket, const T& value)
{
    auto* signal = dynamic_cast<sc_core::sc_signal_inout_if<T>*>(
        socket.get_interface());
    ASSERT_NE(signal, nullptr);
    signal->write(value);
}
} // namespace

TEST(Dma350Test, ExecutesMemoryPeripheralAndLifecycleOperations)
{
    dma350 dut("dma350");
    dut.p_trace = true;
    dut.p_trace_limit = 256;
    TestMemory memory("memory");
    MmioInitiator mmio("mmio");
    sc_core::sc_signal<uint32_t> trigger_ack("trigger_ack");
    sc_core::sc_signal<bool> irq("irq");
    sc_core::sc_signal<bool> irq1("irq1");
    sc_core::sc_signal<bool> irq_comb_nonsec("irq_comb_nonsec");
    dut.initiator_socket.bind(memory.target_socket);
    mmio.socket.bind(dut.target_socket);
    dut.trig_ack[0].bind(trigger_ack);
    dut.irq[0].bind(irq);
    dut.irq[1].bind(irq1);
    dut.irq_comb_nonsec.bind(irq_comb_nonsec);

    EXPECT_EQ((read32(dut, DMA_BUILDCFG0) >> 4) & 0x3fu, 3u);
    EXPECT_EQ(read32(dut, DMA_BUILDCFG1) & 0x1ffu, 28u);
    EXPECT_EQ(read32(dut, DMAINFO_IIDR), 0x3a00043bu);
    EXPECT_EQ(read32(dut, CH0 + CH_BUILDCFG1), 0xb3u);

    sc_core::sc_start(sc_core::SC_ZERO_TIME);

    const uint64_t source = 0x100000020ULL;
    const uint64_t dest = 0x100001000ULL;
    for (uint32_t i = 0; i < 32; ++i) memory.bytes[source + i] = i ^ 0x5a;
    configure_copy(dut, source, dest, 8, 2);
    write32(dut, CH0 + CH_INTREN, INTR_DONE | INTR_ERR);
    write32(dut, CH0 + CH_CMD, 1);
    EXPECT_EQ(read32(dut, CH0 + CH_CMD), 1u);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_DONE, 0u);
    EXPECT_TRUE(irq.read());
    for (uint32_t i = 0; i < 32; ++i)
        EXPECT_EQ(memory.bytes[dest + i], static_cast<uint8_t>(i ^ 0x5a));
    EXPECT_EQ(read32(dut, CH0 + CH_SRCADDR), 0x40u);
    EXPECT_EQ(read32(dut, CH0 + CH_SRCADDRHI), 1u);
    EXPECT_EQ(read32(dut, CH0 + CH_XSIZE), 0u);
    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(irq.read());

    write32(dut, CH0 + CH_FILLVAL, 0xa5c33c5au);
    write64_address(dut, CH0 + CH_DESADDR, dest + 0x100);
    write32(dut, CH0 + CH_XSIZE, 4u << 16);
    write32(dut, CH0 + CH_XSIZEHI, 0);
    write32(dut, CH0 + CH_XADDRINC, 1u << 16);
    write32(dut, CH0 + CH_CTRL, 2u | (3u << 9) | (1u << 21));
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    const uint8_t pattern[] = { 0x5a, 0x3c, 0xc3, 0xa5 };
    for (uint32_t i = 0; i < 16; ++i)
        EXPECT_EQ(memory.bytes[dest + 0x100 + i], pattern[i & 3]);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    const uint64_t wrap_source = source + 0x300;
    const uint64_t wrap_dest = dest + 0x200;
    for (uint32_t i = 0; i < 4; ++i) memory.bytes[wrap_source + i] = 0x30 + i;
    write64_address(dut, CH0 + CH_SRCADDR, wrap_source);
    write64_address(dut, CH0 + CH_DESADDR, wrap_dest);
    write32(dut, CH0 + CH_XSIZE, 4u | (10u << 16));
    write32(dut, CH0 + CH_XSIZEHI, 0);
    write32(dut, CH0 + CH_XADDRINC, 0x00010001u);
    write32(dut, CH0 + CH_CTRL, (2u << 9) | (1u << 21));
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_DONE, 0u);
    for (uint32_t i = 0; i < 10; ++i)
        EXPECT_EQ(memory.bytes[wrap_dest + i],
                  static_cast<uint8_t>(0x30 + (i & 3)));

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    memory.fifo_address = 0x8000;
    for (uint32_t i = 0; i < 4; ++i) memory.bytes[source + 0x100 + i] = 0x70 + i;
    configure_copy(dut, source + 0x100, memory.fifo_address, 4, 0, 1, 0,
                   1u << 26);
    write32(dut, CH0 + CH_DESTRIGINCFG,
            (2u << 8) | (2u << 10));
    write32(dut, CH0 + CH_CMD, 1);
    for (uint32_t i = 0; i < 4; ++i) {
        drive(dut.trig_in[0], TRIGGER_ACTIVE);
        sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
        EXPECT_NE(trigger_ack.read() & TRIGGER_ACTIVE, 0u);
        if (i == 3) EXPECT_EQ(trigger_ack.read() & 3u, ACK_LAST_OKAY);
        EXPECT_EQ(memory.fifo_writes.size(), i + 1);
        EXPECT_EQ(memory.fifo_writes.back().size(), 1u);
        EXPECT_EQ(memory.fifo_writes.back()[0], 0x70 + i);
        drive(dut.trig_in[0], 0u);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
        EXPECT_EQ(trigger_ack.read(), 0u);
    }
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_DONE, 0u);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    configure_copy(dut, source, dest + 0x400, 1024);
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(1100, sc_core::SC_PS));
    write32(dut, CH0 + CH_CMD, 1u << 4);
    sc_core::sc_start(sc_core::sc_time(500, sc_core::SC_PS));
    uint32_t status = read32(dut, CH0 + CH_STATUS);
    EXPECT_EQ(status & (STAT_PAUSED | STAT_RESUMEWAIT),
              STAT_PAUSED | STAT_RESUMEWAIT);
    const uint32_t paused_residue = read32(dut, CH0 + CH_XSIZE) >> 16;
    EXPECT_GT(paused_residue, 0u);
    EXPECT_LT(paused_residue, 1024u);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, CH0 + CH_XSIZE) >> 16, paused_residue);
    write32(dut, CH0 + CH_CMD, 1u << 5);
    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_NS));
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_DONE, 0u);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    configure_copy(dut, source, dest + 0x1000, 1024);
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(1100, sc_core::SC_PS));
    write32(dut, CH0 + CH_CMD, 1u << 3);
    sc_core::sc_start(sc_core::sc_time(500, sc_core::SC_PS));
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_STOPPED, 0u);
    EXPECT_GT(read32(dut, CH0 + CH_XSIZE) >> 16, 0u);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    configure_copy(dut, source, dest, 1, 0, 1, 1, 1u << 26);
    write32(dut, CH0 + CH_DESTRIGINCFG,
            99u | (2u << 8) | (2u << 10));
    write32(dut, CH0 + CH_CMD, 1);
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_ERR, 0u);
    EXPECT_NE(read32(dut, CH0 + CH_ERRINFO) & (1u << 1), 0u);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    memory.fail_read_address = source + 0x200;
    configure_copy(dut, memory.fail_read_address, dest, 1);
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_ERR, 0u);
    EXPECT_NE(read32(dut, CH0 + CH_ERRINFO) & (1u << 16), 0u);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    memory.fail_read_address = std::numeric_limits<uint64_t>::max();
    for (uint32_t i = 0; i < 8; ++i) {
        memory.bytes[source + 0x500 + i] = 0xa0 + i;
        memory.bytes[source + 0x600 + i] = 0xb0 + i;
    }
    configure_copy(dut, source + 0x500, dest + 0x3000, 8);
    configure_copy(dut, source + 0x600, dest + 0x4000, 8, 0, 1, 1, 0,
                   CH1);
    write32(dut, CH0 + CH_INTREN, INTR_DONE);
    write32(dut, CH1 + CH_INTREN, INTR_DONE);
    write32(dut, CH0 + CH_CMD, 1);
    write32(dut, CH1 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 3u);
    EXPECT_EQ(read32(dut, NSEC_STATUS), 0u);
    EXPECT_FALSE(irq_comb_nonsec.read());

    write32(dut, NSEC_CTRL, ~0u);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_EQ(read32(dut, NSEC_CTRL), 1u);
    EXPECT_EQ(read32(dut, NSEC_STATUS), 1u);
    EXPECT_TRUE(irq_comb_nonsec.read());
    write32(dut, NSEC_CHINTRSTATUS0, 0);
    write32(dut, NSEC_STATUS, 0);
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 3u);
    EXPECT_EQ(read32(dut, NSEC_STATUS), 1u);

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 2u);
    EXPECT_TRUE(irq_comb_nonsec.read());
    write32(dut, CH1 + CH_STATUS, read32(dut, CH1 + CH_STATUS));
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 0u);
    EXPECT_EQ(read32(dut, NSEC_STATUS), 0u);
    EXPECT_FALSE(irq_comb_nonsec.read());

    configure_copy(dut, source + 0x500, dest + 0x5000, 8);
    write32(dut, CH0 + CH_INTREN, 0);
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_DONE, 0u);
    EXPECT_EQ(read32(dut, CH0 + CH_STATUS) & INTR_DONE, 0u);
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 0u);
    EXPECT_FALSE(irq.read());
    EXPECT_FALSE(irq_comb_nonsec.read());

    write32(dut, CH0 + CH_STATUS, read32(dut, CH0 + CH_STATUS));
    memory.fail_read_address = source + 0x700;
    configure_copy(dut, memory.fail_read_address, dest + 0x6000, 1);
    write32(dut, CH0 + CH_INTREN, INTR_ERR);
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 1u);
    EXPECT_EQ(read32(dut, NSEC_STATUS), 1u);
    EXPECT_TRUE(irq_comb_nonsec.read());

    drive(dut.reset, true);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    drive(dut.reset, false);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_EQ(read32(dut, CH0 + CH_STATUS), 0u);
    EXPECT_EQ(read32(dut, NSEC_CHINTRSTATUS0), 0u);
    EXPECT_EQ(read32(dut, NSEC_STATUS), 0u);
    EXPECT_FALSE(irq.read());
    EXPECT_FALSE(irq_comb_nonsec.read());
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
