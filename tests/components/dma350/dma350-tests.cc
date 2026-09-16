/* SPDX-License-Identifier: BSD-3-Clause */

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
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
constexpr uint64_t CH_LINKADDR = 0x78;
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
    sc_core::sc_time latency{ 100, sc_core::SC_PS };

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
        delay += latency;
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
    EXPECT_EQ(read32(dut, CH0 + CH_BUILDCFG1), 0x1b3u);

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

    // A temporally decoupled CPU may issue ENABLE ahead of kernel time.
    // Neither the transfer nor a status read may observe that future early.
    memory.fail_read_address = std::numeric_limits<uint64_t>::max();
    const uint64_t timed_dest = dest + 0x7000;
    configure_copy(dut, source, timed_dest, 1);
    write32(dut, CH0 + CH_INTREN, INTR_DONE);
    const auto start = sc_core::sc_time_stamp();
    bool enable_returned = false;
    bool status_returned = false;
    sc_core::sc_spawn([&]() {
        uint32_t value = 1;
        tlm::tlm_generic_payload trans;
        trans.set_address(CH0 + CH_CMD);
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        trans.set_dmi_allowed(true);
        sc_core::sc_time delay(10, sc_core::SC_NS);
        mmio.socket->b_transport(trans, delay);
        EXPECT_EQ(sc_core::sc_time_stamp(), start + sc_core::sc_time(10, sc_core::SC_NS));
        EXPECT_EQ(delay, sc_core::SC_ZERO_TIME);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        EXPECT_FALSE(trans.is_dmi_allowed());
        enable_returned = true;
    });
    sc_core::sc_spawn([&]() {
        uint32_t value = 0;
        tlm::tlm_generic_payload trans;
        trans.set_address(CH0 + CH_STATUS);
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        sc_core::sc_time delay(12, sc_core::SC_NS);
        mmio.socket->b_transport(trans, delay);
        EXPECT_EQ(sc_core::sc_time_stamp(), start + sc_core::sc_time(12, sc_core::SC_NS));
        EXPECT_EQ(delay, sc_core::SC_ZERO_TIME);
        EXPECT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        EXPECT_NE(value & STAT_DONE, 0u);
        status_returned = true;
    });
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_FALSE(enable_returned);
    EXPECT_FALSE(status_returned);
    EXPECT_EQ(memory.bytes.count(timed_dest), 0u);
    EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
    EXPECT_EQ(read32(dut, CH0 + CH_STATUS), 0u);
    EXPECT_FALSE(irq.read());
    sc_core::sc_start(sc_core::sc_time(10, sc_core::SC_NS));
    EXPECT_TRUE(enable_returned);
    EXPECT_TRUE(status_returned);
    EXPECT_EQ(memory.bytes[timed_dest], memory.bytes[source]);
    EXPECT_TRUE(irq.read());

    // Debug transport must remain callable without a SystemC process and
    // must not execute command side effects.
    uint32_t clear_command = 1u << 1;
    tlm::tlm_generic_payload debug;
    debug.set_address(CH0 + CH_CMD);
    debug.set_command(tlm::TLM_WRITE_COMMAND);
    debug.set_data_ptr(reinterpret_cast<unsigned char*>(&clear_command));
    debug.set_data_length(sizeof(clear_command));
    debug.set_streaming_width(sizeof(clear_command));
    const auto debug_start = sc_core::sc_time_stamp();
    EXPECT_EQ(mmio.socket->transport_dbg(debug), sizeof(clear_command));
    EXPECT_EQ(debug.get_response_status(), tlm::TLM_OK_RESPONSE);
    EXPECT_EQ(sc_core::sc_time_stamp(), debug_start);
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_DONE, 0u);

    // Hardware descriptors are fetched through the memory initiator, including
    // high address bits. No CPU command programming occurs between commands.
    const uint64_t descriptor = 0x200000100ULL;
    auto put_words = [&](uint64_t address, std::initializer_list<uint32_t> words) {
        for (uint32_t word : words) {
            for (unsigned int byte = 0; byte < 4; ++byte)
                memory.bytes[address++] = (word >> (8 * byte)) & 0xffu;
        }
    };
    write32(dut, CH0 + CH_CMD, 2);
    configure_copy(dut, source, dest, 0); // Empty bootstrap command.
    write64_address(dut, CH0 + CH_LINKADDR, descriptor | 1u);
    write32(dut, CH0 + CH_INTREN, INTR_DONE | INTR_ERR);
    // REGCLEAR + INTREN/CTRL/SRC{HI}/DES{HI}/XSIZE/XADDRINC.
    // Clearing LINKADDR terminates this command even though it is omitted.
    put_words(descriptor, { 0x11fdu, 0u, 0x200u,
        uint32_t(source), uint32_t(source >> 32), uint32_t(dest + 0x9000),
        uint32_t(dest >> 32), 0x00040004u, 0x00010001u });
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
    EXPECT_EQ(read32(dut, CH0 + CH_LINKADDR), 0u);
    EXPECT_EQ(read32(dut, CH0 + CH_STATUS) & (STAT_DONE | INTR_DONE),
              STAT_DONE | INTR_DONE); // Initial IRQ survives REGCLEAR/INTREN=0.
    EXPECT_TRUE(irq.read());
    for (unsigned int i = 0; i < 4; ++i)
        EXPECT_EQ(memory.bytes[dest + 0x9000 + i], memory.bytes[source + i]);

    // Two sparse cyclic descriptors service a peripheral indefinitely with a
    // sticky/coalescing DONE interrupt. Each requests one byte, then waits for
    // the next hardware handshake; the CPU only observes and acknowledges IRQ.
    write32(dut, CH0 + CH_CMD, 2);
    configure_copy(dut, source, memory.fifo_address, 0, 0, 1, 0);
    write32(dut, CH0 + CH_DESTRIGINCFG, (2u << 8) | (2u << 10));
    write32(dut, CH0 + CH_INTREN, INTR_DONE | INTR_ERR);
    write64_address(dut, CH0 + CH_LINKADDR, descriptor | 1u);
    put_words(descriptor, { 0x40000118u, 0x200u | (1u << 21) | (1u << 26),
                            uint32_t(source), 0x10001u,
                            uint32_t(descriptor + 0x40) | 1u });
    put_words(descriptor + 0x40, { 0xc0000110u, uint32_t(source + 1),
        0x10001u, uint32_t(descriptor) | 1u, uint32_t(descriptor >> 32) });
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    const size_t previous_writes = memory.fifo_writes.size();
    for (unsigned int i = 0; i < 8; ++i) {
        drive(dut.trig_in[0], TRIGGER_ACTIVE);
        sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_NS));
        EXPECT_EQ(memory.fifo_writes.size(), previous_writes + i + 1);
        EXPECT_EQ(memory.fifo_writes.back()[0], memory.bytes[source + (i & 1)]);
        drive(dut.trig_in[0], 0u);
        sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_NS));
        EXPECT_EQ(read32(dut, CH0 + CH_CMD), 1u);
        EXPECT_TRUE(irq.read());
    }
    write32(dut, CH0 + CH_STATUS, STAT_DONE);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
    EXPECT_FALSE(irq.read());
    write32(dut, CH0 + CH_CMD, 1u << 4);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    drive(dut.trig_in[0], TRIGGER_ACTIVE);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_EQ(memory.fifo_writes.size(), previous_writes + 8);
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_PAUSED, 0u);
    write32(dut, CH0 + CH_CMD, 1u << 5);
    sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_NS));
    EXPECT_EQ(memory.fifo_writes.size(), previous_writes + 9);
    // DISABLE drains the current handshake but never fetches the next link.
    write32(dut, CH0 + CH_CMD, 1u << 2);
    drive(dut.trig_in[0], 0u);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
    EXPECT_NE(read32(dut, CH0 + CH_STATUS) & (1u << 18), 0u);
    EXPECT_TRUE(irq.read());

    // DONEPAUSE stops at the command boundary, before loading the link.
    write32(dut, CH0 + CH_CMD, 2);
    configure_copy(dut, source, dest + 0xb000, 0, 0, 1, 1, 1u << 24);
    put_words(descriptor, { 0x40000100u, 0x10001u, 0u });
    write64_address(dut, CH0 + CH_LINKADDR, descriptor | 1u);
    write32(dut, CH0 + CH_CMD, 1);
    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, CH0 + CH_STATUS) & (STAT_PAUSED | STAT_RESUMEWAIT),
              STAT_PAUSED | STAT_RESUMEWAIT);
    EXPECT_EQ(memory.bytes.count(dest + 0xb000), 0u);
    write32(dut, CH0 + CH_CMD, 1u << 5);
    sc_core::sc_start(sc_core::sc_time(10, sc_core::SC_NS));
    EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
    EXPECT_EQ(memory.bytes[dest + 0xb000], memory.bytes[source]);

    // Exact d350_build_slave_cmd()/prep_dma_cyclic() layout: fourteen words
    // in a 64-byte allocation, including XSIZEHI and one directional trigger.
    // Use poison padding so accidental dense-register decoding is observable.
    for (bool playback : { true, false }) {
        for (bool done_pause : { false, true }) {
            for (uint32_t count : { 2u, 65537u }) {
                write32(dut, CH0 + CH_CMD, 2);
                const uint64_t buffer = source + 0x10000;
                const uint32_t trigger_bit = playback ? 20 : 19;
                const uint32_t header = 0xc0001ff8u | (1u << trigger_bit);
                const uint32_t ctrl = 2u | (1u << 9) | (1u << 21) | (done_pause ? (1u << 24) : 0u) |
                                      (1u << (playback ? 26 : 25));
                for (unsigned int node = 0; node < 2; ++node) {
                    const uint64_t mem = buffer + node * 8;
                    const uint64_t src = playback ? mem : memory.fifo_address;
                    const uint64_t dst = playback ? memory.fifo_address : mem;
                    const uint64_t next = descriptor + (node ? 0 : 64);
                    put_words(descriptor + node * 64,
                              { header, ctrl, uint32_t(src), uint32_t(src >> 32), uint32_t(dst), uint32_t(dst >> 32),
                                (count & 0xffffu) * 0x10001u, (count >> 16) * 0x10001u, playback ? 0xf0244u : 0xf0200u,
                                playback ? 0xf0200u : 0xf0244u, playback ? 1u : 0x10000u, (2u << 8) | (2u << 10),
                                uint32_t(next) | 1u, uint32_t(next >> 32), 0xdeadbeefu, 0xfeedfaceu });
                }
                for (unsigned int i = 0; i < 16; ++i) memory.bytes[buffer + i] = 0x40 + i;
                configure_copy(dut, source, dest, 0);
                write32(dut, CH0 + CH_INTREN, INTR_DONE | INTR_ERR);
                write64_address(dut, CH0 + CH_LINKADDR, descriptor | 1u);
                write32(dut, CH0 + CH_CMD, 1);
                sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
                EXPECT_EQ(read32(dut, CH0 + CH_CTRL), ctrl);
                EXPECT_EQ(read32(dut, CH0 + 0x28), playback ? 0xf0244u : 0xf0200u);
                EXPECT_EQ(read32(dut, CH0 + 0x2c), playback ? 0xf0200u : 0xf0244u);
                EXPECT_EQ(read32(dut, CH0 + trigger_bit * 4), 0xa00u);
                if (done_pause) write32(dut, CH0 + CH_STATUS, STAT_DONE);
                const size_t fifo_start = memory.fifo_writes.size();
                // Six beats cross both links and revisit node zero; the large
                // count case instead checks the 16-bit counter borrow boundary.
                const unsigned int beats = count == 2 ? 6 : 1;
                for (unsigned int beat = 0; beat < beats; ++beat) {
                    for (unsigned int byte = 0; byte < 4; ++byte)
                        memory.bytes[memory.fifo_address + byte] = 0x80 + beat * 4 + byte;
                    drive(dut.trig_in[0], TRIGGER_ACTIVE);
                    sc_core::sc_start(sc_core::sc_time(3, sc_core::SC_NS));
                    const unsigned int offset = (beat % 4) * 4;
                    if (playback) {
                        ASSERT_EQ(memory.fifo_writes.size(), fifo_start + beat + 1);
                        ASSERT_EQ(memory.fifo_writes.back().size(), 4u);
                        for (unsigned int byte = 0; byte < 4; ++byte)
                            EXPECT_EQ(memory.fifo_writes.back()[byte], 0x40 + offset + byte);
                    } else {
                        for (unsigned int byte = 0; byte < 4; ++byte)
                            EXPECT_EQ(memory.bytes[buffer + offset + byte], 0x80 + beat * 4 + byte);
                    }
                    drive(dut.trig_in[0], 0u);
                    sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
                    if (done_pause && count == 2 && beat % 2 == 1) {
                        EXPECT_EQ(read32(dut, CH0 + CH_STATUS) & (STAT_PAUSED | STAT_RESUMEWAIT),
                                  STAT_PAUSED | STAT_RESUMEWAIT);
                        EXPECT_TRUE(irq.read());
                        const uint32_t position = read32(dut, CH0 + (playback ? CH_SRCADDR : CH_DESADDR));
                        // Acknowledging DONE is not RESUME: callback latency must
                        // not advance the ring, even with a new peripheral request.
                        write32(dut, CH0 + CH_STATUS, STAT_DONE);
                        drive(dut.trig_in[0], TRIGGER_ACTIVE);
                        sc_core::sc_start(sc_core::sc_time(50, sc_core::SC_NS));
                        EXPECT_FALSE(irq.read());
                        EXPECT_EQ(read32(dut, CH0 + (playback ? CH_SRCADDR : CH_DESADDR)), position);
                        if (playback) EXPECT_EQ(memory.fifo_writes.size(), fifo_start + beat + 1);
                        drive(dut.trig_in[0], 0u);
                        write32(dut, CH0 + CH_CMD, 1u << 5);
                        sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
                    }
                    EXPECT_EQ(read32(dut, CH0 + CH_CMD), 1u);
                    EXPECT_EQ(read32(dut, CH0 + CH_STATUS) & STAT_ERR, 0u);
                    EXPECT_EQ(irq.read(), !done_pause);
                }
                if (count == 65537) {
                    EXPECT_EQ(read32(dut, CH0 + CH_XSIZE), 0u);
                    EXPECT_EQ(read32(dut, CH0 + CH_XSIZEHI), 0x10001u);
                    EXPECT_EQ(read32(dut, CH0 + (playback ? CH_SRCADDR : CH_DESADDR)), uint32_t(buffer + 4));
                }
                write32(dut, CH0 + CH_CMD, 1u << 3);
                sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
                EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
            }
        }
    }

    // Descriptor read failures and an empty header have distinct TRM errors.
    for (bool bus_error : { false, true }) {
        write32(dut, CH0 + CH_CMD, 2);
        configure_copy(dut, source, dest, 0);
        put_words(descriptor, { 0 });
        write64_address(dut, CH0 + CH_LINKADDR, descriptor | 1u);
        write32(dut, CH0 + CH_INTREN, INTR_ERR);
        if (bus_error) memory.fail_read_address = descriptor;
        write32(dut, CH0 + CH_CMD, 1);
        sc_core::sc_start(sc_core::sc_time(5, sc_core::SC_NS));
        EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
        EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_ERR, 0u);
        EXPECT_EQ(read32(dut, CH0 + CH_ERRINFO),
                  bus_error ? (1u | (1u << 16)) : (2u | (1u << 24)));
        EXPECT_TRUE(irq.read());
    }

    // A delayed descriptor fetch must not re-enable the channel after STOP
    // or reset. The outstanding read completes but no payload is transferred.
    memory.fail_read_address = std::numeric_limits<uint64_t>::max();
    memory.latency = sc_core::sc_time(10, sc_core::SC_NS);
    for (bool reset_fetch : { false, true }) {
        write32(dut, CH0 + CH_CMD, 2);
        configure_copy(dut, source, dest + 0xa000, 0);
        put_words(descriptor, { 0x40000100u, 0x10001u, 0u });
        write64_address(dut, CH0 + CH_LINKADDR, descriptor | 1u);
        write32(dut, CH0 + CH_CMD, 1);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        if (reset_fetch)
            drive(dut.reset, true);
        else
            write32(dut, CH0 + CH_CMD, 1u << 3);
        sc_core::sc_start(sc_core::sc_time(50, sc_core::SC_NS));
        EXPECT_EQ(read32(dut, CH0 + CH_CMD), 0u);
        EXPECT_EQ(memory.bytes.count(dest + 0xa000), 0u);
        if (reset_fetch) {
            EXPECT_EQ(read32(dut, CH0 + CH_STATUS), 0u);
            drive(dut.reset, false);
            sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_PS));
        } else {
            EXPECT_NE(read32(dut, CH0 + CH_STATUS) & STAT_STOPPED, 0u);
        }
    }
}

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    if (argc == 2 && std::string(argv[1]) == "--reject-nine-channels") {
        cci::cci_originator originator("dma350-test");
        broker.set_preset_cci_value("invalid_dma.channel_count",
                                    cci::cci_value(9u), originator);
        sc_core::sc_report_handler::set_actions(sc_core::SC_FATAL,
                                                sc_core::SC_THROW);
        try {
            dma350 invalid_dma("invalid_dma");
        } catch (const sc_core::sc_report& report) {
            return std::string(report.what()).find(
                       "channel_count must be between 1 and 8") !=
                           std::string::npos
                       ? 0
                       : 1;
        }
        return 1;
    }
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
