/* SPDX-License-Identifier: BSD-3-Clause */

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/multi_passthrough_target_socket.h>

#include <dma350.h>
#include <dw-apb-i2c.h>
#include <dw-apb-ssi.h>
#include <dw-apb-uart.h>
#include <ports/target-signal-socket.h>
#include <tests/test-bench.h>

namespace {

constexpr uint64_t RAM_BASE = 0x80000000;
constexpr uint64_t DMA_BASE = 0x10000000;
constexpr uint64_t I2C_BASE = 0x10010000;
constexpr uint64_t SPI_BASE = 0x10020000;
constexpr uint64_t UART0_BASE = 0x10030000;
constexpr uint64_t UART1_BASE = 0x10040000;

constexpr uint32_t CH_BASE = 0x1000;
constexpr uint32_t CH_STRIDE = 0x100;
constexpr uint32_t CH_CMD = 0x00;
constexpr uint32_t CH_STATUS = 0x04;
constexpr uint32_t CH_INTREN = 0x08;
constexpr uint32_t CH_CTRL = 0x0c;
constexpr uint32_t CH_SRCADDR = 0x10;
constexpr uint32_t CH_DESADDR = 0x18;
constexpr uint32_t CH_XSIZE = 0x20;
constexpr uint32_t CH_XADDRINC = 0x30;
constexpr uint32_t CH_SRCTRIGINCFG = 0x4c;
constexpr uint32_t CH_DESTRIGINCFG = 0x50;

constexpr uint32_t DMA_CTRL_CONTINUE = 1u << 9;
constexpr uint32_t DMA_CTRL_DONE = 1u << 21;
constexpr uint32_t DMA_CTRL_SOURCE_TRIGGER = 1u << 25;
constexpr uint32_t DMA_CTRL_DEST_TRIGGER = 1u << 26;
constexpr uint32_t DMA_STATUS_DONE = 1u << 16;

constexpr uint32_t DMA_TRIGGER_HARDWARE = 2u << 8;
constexpr uint32_t DMA_TRIGGER_DMA_FLOW = 2u << 10;

class Memory : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<Memory, DEFAULT_TLM_BUSWIDTH> target_socket;
    std::vector<uint8_t> bytes;

    Memory(sc_core::sc_module_name name, size_t size)
        : sc_core::sc_module(name), target_socket("target_socket"), bytes(size, 0)
    {
        target_socket.register_b_transport(this, &Memory::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        const uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        auto* data = trans.get_data_ptr();

        if (!data || address + length > bytes.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(data, bytes.data() + address, length);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(bytes.data() + address, data, length);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class AddressSpace : public sc_core::sc_module
{
public:
    tlm_utils::multi_passthrough_target_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> target_socket;
    tlm_utils::simple_initiator_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> memory_socket;
    tlm_utils::simple_initiator_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> dma_socket;
    tlm_utils::simple_initiator_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> i2c_socket;
    tlm_utils::simple_initiator_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> spi_socket;
    tlm_utils::simple_initiator_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> uart0_socket;
    tlm_utils::simple_initiator_socket<AddressSpace, DEFAULT_TLM_BUSWIDTH> uart1_socket;

    unsigned int i2c_accesses = 0;
    unsigned int spi_accesses = 0;
    unsigned int uart0_accesses = 0;
    unsigned int uart1_accesses = 0;

    explicit AddressSpace(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , target_socket("target_socket")
        , memory_socket("memory_socket")
        , dma_socket("dma_socket")
        , i2c_socket("i2c_socket")
        , spi_socket("spi_socket")
        , uart0_socket("uart0_socket")
        , uart1_socket("uart1_socket")
    {
        target_socket.register_b_transport(this, &AddressSpace::b_transport);
    }

    void reset_access_counts()
    {
        i2c_accesses = 0;
        spi_accesses = 0;
        uart0_accesses = 0;
        uart1_accesses = 0;
    }

private:
    template <typename Socket>
    static void forward(Socket& socket, tlm::tlm_generic_payload& trans,
                        sc_core::sc_time& delay, uint64_t base)
    {
        const uint64_t address = trans.get_address();
        trans.set_address(address - base);
        socket->b_transport(trans, delay);
        trans.set_address(address);
    }

    void b_transport(int, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const uint64_t address = trans.get_address();
        if (address >= RAM_BASE && address < RAM_BASE + 0x10000) {
            forward(memory_socket, trans, delay, RAM_BASE);
        } else if (address >= DMA_BASE && address < DMA_BASE + 0x2000) {
            forward(dma_socket, trans, delay, DMA_BASE);
        } else if (address >= I2C_BASE && address < I2C_BASE + 0x1000) {
            ++i2c_accesses;
            forward(i2c_socket, trans, delay, I2C_BASE);
        } else if (address >= SPI_BASE && address < SPI_BASE + 0x1000) {
            ++spi_accesses;
            forward(spi_socket, trans, delay, SPI_BASE);
        } else if (address >= UART0_BASE && address < UART0_BASE + 0x1000) {
            ++uart0_accesses;
            forward(uart0_socket, trans, delay, UART0_BASE);
        } else if (address >= UART1_BASE && address < UART1_BASE + 0x1000) {
            ++uart1_accesses;
            forward(uart1_socket, trans, delay, UART1_BASE);
        } else {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
    }
};

class PeripheralDmaBench : public TestBench
{
protected:
    Memory memory;
    AddressSpace address_space;
    dma350 dma;
    dw_apb_i2c i2c;
    dw_i2c_eeprom eeprom;
    dw_apb_ssi spi;
    dw_apb_uart uart0;
    dw_apb_uart uart1;
    tlm_utils::simple_initiator_socket<PeripheralDmaBench, DEFAULT_TLM_BUSWIDTH> cpu_socket;

    TargetSignalSocket<bool> i2c_irq_sink;
    TargetSignalSocket<bool> spi_irq_sink;
    TargetSignalSocket<bool> uart0_irq_sink;
    TargetSignalSocket<bool> uart1_irq_sink;
    sc_core::sc_signal<bool> irq0;
    sc_core::sc_signal<bool> irq1;

    PeripheralDmaBench(const sc_core::sc_module_name& name)
        : TestBench(name)
        , memory("memory", 0x10000)
        , address_space("address_space")
        , dma("dma")
        , i2c("i2c")
        , eeprom("eeprom")
        , spi("spi")
        , uart0("uart0")
        , uart1("uart1")
        , cpu_socket("cpu_socket")
        , i2c_irq_sink("i2c_irq_sink")
        , spi_irq_sink("spi_irq_sink")
        , uart0_irq_sink("uart0_irq_sink")
        , uart1_irq_sink("uart1_irq_sink")
        , irq0("irq0")
        , irq1("irq1")
    {
        cpu_socket.bind(address_space.target_socket);
        dma.initiator_socket.bind(address_space.target_socket);
        address_space.memory_socket.bind(memory.target_socket);
        address_space.dma_socket.bind(dma.target_socket);
        address_space.i2c_socket.bind(i2c.target_socket);
        address_space.spi_socket.bind(spi.target_socket);
        address_space.uart0_socket.bind(uart0.target_socket);
        address_space.uart1_socket.bind(uart1.target_socket);
        i2c.i2c_socket.bind(eeprom.i2c_socket);
        uart0.backend_socket.bind(uart1.backend_socket);
        eeprom.p_page_size = 32;
        i2c.irq.bind(i2c_irq_sink);
        spi.irq.bind(spi_irq_sink);
        uart0.irq.bind(uart0_irq_sink);
        uart1.irq.bind(uart1_irq_sink);

        i2c.dma_tx_req.bind(dma.trig_in[0]);
        i2c.dma_rx_req.bind(dma.trig_in[1]);
        spi.dma_tx_req.bind(dma.trig_in[2]);
        spi.dma_rx_req.bind(dma.trig_in[3]);
        uart0.dma_tx_req.bind(dma.trig_in[4]);
        uart1.dma_rx_req.bind(dma.trig_in[5]);
        dma.trig_ack[0].bind(i2c.dma_tx_ack);
        dma.trig_ack[1].bind(i2c.dma_rx_ack);
        dma.trig_ack[2].bind(spi.dma_tx_ack);
        dma.trig_ack[3].bind(spi.dma_rx_ack);
        dma.trig_ack[4].bind(uart0.dma_tx_ack);
        dma.trig_ack[5].bind(uart1.dma_rx_ack);
        dma.irq[0].bind(irq0);
        dma.irq[1].bind(irq1);
    }

    template <typename T>
    void cpu_access(tlm::tlm_command command, uint64_t address, T& value)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(sizeof(value));
        trans.set_streaming_width(sizeof(value));
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        cpu_socket->b_transport(trans, delay);
        ASSERT_EQ(trans.get_response_status(), tlm::TLM_OK_RESPONSE);
        wait(delay);
    }

    void write32(uint64_t address, uint32_t value)
    {
        cpu_access(tlm::TLM_WRITE_COMMAND, address, value);
    }

    uint32_t read32(uint64_t address)
    {
        uint32_t value = 0;
        cpu_access(tlm::TLM_READ_COMMAND, address, value);
        return value;
    }

    void configure_channel(unsigned int channel, uint64_t source, uint64_t dest,
                           uint32_t transfers, unsigned int width_shift,
                           int16_t source_increment, int16_t dest_increment,
                           bool source_trigger, unsigned int trigger)
    {
        const uint64_t base = DMA_BASE + CH_BASE + channel * CH_STRIDE;
        const uint32_t trigger_config = trigger | DMA_TRIGGER_HARDWARE |
                                        DMA_TRIGGER_DMA_FLOW;
        const uint32_t increments = static_cast<uint16_t>(source_increment) |
                                    (static_cast<uint32_t>(static_cast<uint16_t>(dest_increment)) << 16);
        const uint32_t ctrl = DMA_CTRL_CONTINUE | DMA_CTRL_DONE | width_shift |
                              (source_trigger ? DMA_CTRL_SOURCE_TRIGGER : DMA_CTRL_DEST_TRIGGER);

        write32(base + CH_INTREN, 1);
        write32(base + CH_SRCADDR, static_cast<uint32_t>(source));
        write32(base + CH_SRCADDR + 4, static_cast<uint32_t>(source >> 32));
        write32(base + CH_DESADDR, static_cast<uint32_t>(dest));
        write32(base + CH_DESADDR + 4, static_cast<uint32_t>(dest >> 32));
        write32(base + CH_XSIZE, transfers | (transfers << 16));
        write32(base + CH_XADDRINC, increments);
        write32(base + CH_CTRL, ctrl);
        write32(base + (source_trigger ? CH_SRCTRIGINCFG : CH_DESTRIGINCFG), trigger_config);
    }

    void start_channel(unsigned int channel)
    {
        write32(DMA_BASE + CH_BASE + channel * CH_STRIDE + CH_CMD, 1);
    }

    void wait_for_done(unsigned int channel, sc_core::sc_signal<bool>& irq)
    {
        if (!irq.read()) {
            wait(sc_core::sc_time(5, sc_core::SC_MS), irq.posedge_event());
        }
        ASSERT_TRUE(irq.read()) << "channel=" << channel;
        EXPECT_NE(read32(DMA_BASE + CH_BASE + channel * CH_STRIDE + CH_STATUS) & DMA_STATUS_DONE, 0u);
        write32(DMA_BASE + CH_BASE + channel * CH_STRIDE + CH_STATUS, 0xffffffffu);
        wait(sc_core::SC_ZERO_TIME);
        EXPECT_FALSE(irq.read());
    }

    void configure_i2c()
    {
        write32(I2C_BASE + dw_apb_i2c::IC_ENABLE, 0);
        write32(I2C_BASE + dw_apb_i2c::IC_SS_SCL_HCNT, 400);
        write32(I2C_BASE + dw_apb_i2c::IC_SS_SCL_LCNT, 470);
        write32(I2C_BASE + dw_apb_i2c::IC_RX_TL, 0);
        write32(I2C_BASE + dw_apb_i2c::IC_TX_TL, 0);
        write32(I2C_BASE + dw_apb_i2c::IC_CON, 1 | (2 << 1) | (1 << 5) | (1 << 6) | (1 << 8));
        write32(I2C_BASE + dw_apb_i2c::IC_TAR, 0x50);
        write32(I2C_BASE + dw_apb_i2c::IC_ENABLE, 1);
    }
};

TEST_BENCH(PeripheralDmaBench, I2cEepromTxRxUsesDreqAndIrq)
{
    constexpr uint32_t count = 20;
    constexpr uint32_t read_count = count - 1;
    constexpr uint64_t write_commands = RAM_BASE + 0x100;
    constexpr uint64_t read_commands = RAM_BASE + 0x200;
    constexpr uint64_t read_data = RAM_BASE + 0x300;
    std::array<uint16_t, count> write_data{};
    std::array<uint16_t, count> read_data_commands{};

    write_data[0] = 0x40;
    for (uint32_t i = 1; i < count; ++i) {
        write_data[i] = 0xa0 + i;
    }
    write_data.back() |= dw_apb_i2c::DATA_CMD_STOP;
    std::memcpy(memory.bytes.data() + 0x100, write_data.data(), sizeof(write_data));

    configure_i2c();
    configure_channel(0, write_commands, I2C_BASE + dw_apb_i2c::IC_DATA_CMD,
                      count, 1, 1, 0, false, 0);
    address_space.reset_access_counts();
    start_channel(0);
    wait(sc_core::sc_time(50, sc_core::SC_US));
    EXPECT_EQ(address_space.i2c_accesses, 0u) << "DMA must wait for I2C DREQ";

    write32(I2C_BASE + dw_apb_i2c::IC_DMA_CR, dw_apb_i2c::DMA_TDMAE);
    wait_for_done(0, irq0);
    EXPECT_GT(address_space.i2c_accesses, count);

    read_data_commands[0] = 0x40;
    for (uint32_t i = 1; i < count; ++i) {
        read_data_commands[i] = dw_apb_i2c::DATA_CMD_READ;
    }
    read_data_commands[1] |= dw_apb_i2c::DATA_CMD_RESTART;
    read_data_commands.back() |= dw_apb_i2c::DATA_CMD_STOP;
    std::memcpy(memory.bytes.data() + 0x200, read_data_commands.data(), sizeof(read_data_commands));

    configure_channel(0, read_commands, I2C_BASE + dw_apb_i2c::IC_DATA_CMD,
                      count, 1, 1, 0, false, 0);
    configure_channel(1, I2C_BASE + dw_apb_i2c::IC_DATA_CMD, read_data,
                      read_count, 0, 0, 1, true, 1);
    start_channel(1);
    start_channel(0);
    write32(I2C_BASE + dw_apb_i2c::IC_DMA_CR,
            dw_apb_i2c::DMA_TDMAE | dw_apb_i2c::DMA_RDMAE);
    wait_for_done(0, irq0);
    wait_for_done(1, irq1);

    for (uint32_t i = 0; i < read_count; ++i) {
        EXPECT_EQ(memory.bytes[0x300 + i], 0xa1 + i) << "byte=" << i;
    }
    sc_core::sc_stop();
}

TEST_BENCH(PeripheralDmaBench, SpiLoopbackUsesTwoDmaChannels)
{
    constexpr uint32_t count = 24;
    constexpr uint64_t tx_data = RAM_BASE + 0x1000;
    constexpr uint64_t rx_data = RAM_BASE + 0x2000;
    std::array<uint32_t, count> payload{};

    for (uint32_t i = 0; i < count; ++i) {
        payload[i] = (0x40 + i) & 0xff;
    }
    std::memcpy(memory.bytes.data() + 0x1000, payload.data(), sizeof(payload));

    write32(SPI_BASE + dw_apb_ssi::SSIENR, 0);
    write32(SPI_BASE + dw_apb_ssi::CTRLR0, 7 | (1u << 11));
    write32(SPI_BASE + dw_apb_ssi::BAUDR, 2);
    write32(SPI_BASE + dw_apb_ssi::DMATDLR, 0);
    write32(SPI_BASE + dw_apb_ssi::DMARDLR, 0);
    write32(SPI_BASE + dw_apb_ssi::SER, 1);
    write32(SPI_BASE + dw_apb_ssi::SSIENR, 1);

    configure_channel(0, tx_data, SPI_BASE + dw_apb_ssi::DR, count, 2, 1, 0, false, 2);
    configure_channel(1, SPI_BASE + dw_apb_ssi::DR, rx_data, count, 2, 0, 1, true, 3);
    address_space.reset_access_counts();
    start_channel(1);
    start_channel(0);
    write32(SPI_BASE + dw_apb_ssi::DMACR, 3);
    wait_for_done(0, irq0);
    wait_for_done(1, irq1);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t received = 0;
        std::memcpy(&received, memory.bytes.data() + 0x2000 + i * sizeof(received), sizeof(received));
        EXPECT_EQ(received, payload[i]) << "word=" << i;
    }
    EXPECT_GE(address_space.spi_accesses, count * 2);
    sc_core::sc_stop();
}

TEST_BENCH(PeripheralDmaBench, UartCrossconnectUsesTxAndRxDma)
{
    constexpr uint32_t count = 24;
    constexpr uint64_t tx_data = RAM_BASE + 0x3000;
    constexpr uint64_t rx_data = RAM_BASE + 0x4000;
    std::array<uint8_t, count> payload{};

    for (uint32_t i = 0; i < count; ++i) {
        payload[i] = static_cast<uint8_t>(0x60 + i);
    }
    std::memcpy(memory.bytes.data() + 0x3000, payload.data(), payload.size());

    write32(UART0_BASE + 0x08, 0x01);
    write32(UART1_BASE + 0x08, 0x01);
    configure_channel(0, tx_data, UART0_BASE, count, 0, 1, 0, false, 4);
    configure_channel(1, UART1_BASE, rx_data, count, 0, 0, 1, true, 5);
    address_space.reset_access_counts();
    start_channel(1);
    start_channel(0);
    wait_for_done(0, irq0);
    wait_for_done(1, irq1);

    EXPECT_EQ(std::memcmp(memory.bytes.data() + 0x4000, payload.data(), payload.size()), 0);
    EXPECT_GE(address_space.uart0_accesses, count);
    EXPECT_GE(address_space.uart1_accesses, count);
    sc_core::sc_stop();
}

} // namespace

int sc_main(int argc, char* argv[])
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
