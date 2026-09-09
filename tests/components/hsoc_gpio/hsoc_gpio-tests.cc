/* SPDX-License-Identifier: BSD-3-Clause */

#include <array>
#include <cstdint>

#include <hsoc_gpio.h>
#include <test/test.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

class HsocGpioTest : public TestBench
{
    using CpuSocket = tlm_utils::simple_initiator_socket<
        HsocGpioTest, DEFAULT_TLM_BUSWIDTH>;

    hsoc_gpio m_dut;
    hsoc_gpio m_peri1;
    CpuSocket m_cpu;
    CpuSocket m_peri1_cpu;
    sc_core::sc_signal<bool> m_bank1_irq;
    sc_core::sc_signal<bool> m_bank0_irq;
    sc_core::sc_signal<bool> m_gpio12_oe;
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_peripheral_signals;
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_peri1_peripheral_signals;

    static uint32_t bank_reg(unsigned int bank, uint32_t reg)
    {
        return bank * hsoc_gpio::BANK_STRIDE + reg;
    }

    tlm::tlm_response_status access(CpuSocket& cpu,
                                    tlm::tlm_command command,
                                    uint32_t offset, uint32_t& value,
                                    unsigned int length = 4,
                                    unsigned char* byte_enable = nullptr)
    {
        std::array<unsigned char, 4> data{{
            static_cast<unsigned char>(value),
            static_cast<unsigned char>(value >> 8),
            static_cast<unsigned char>(value >> 16),
            static_cast<unsigned char>(value >> 24),
        }};
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(offset);
        trans.set_data_ptr(data.data());
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(byte_enable);
        trans.set_byte_enable_length(byte_enable ? length : 0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        cpu->b_transport(trans, delay);
        if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
            TEST_ASSERT(delay == sc_core::sc_time(10, sc_core::SC_NS));
            wait(delay);
            value = static_cast<uint32_t>(data[0]) |
                    (static_cast<uint32_t>(data[1]) << 8) |
                    (static_cast<uint32_t>(data[2]) << 16) |
                    (static_cast<uint32_t>(data[3]) << 24);
        }
        return trans.get_response_status();
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    uint32_t offset, uint32_t& value,
                                    unsigned int length = 4,
                                    unsigned char* byte_enable = nullptr)
    {
        return access(m_cpu, command, offset, value, length, byte_enable);
    }

    uint32_t read32(uint32_t offset)
    {
        uint32_t value = 0;
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, offset, value) ==
                    tlm::TLM_OK_RESPONSE);
        return value;
    }

    void write32(uint32_t offset, uint32_t value)
    {
        TEST_ASSERT(access(tlm::TLM_WRITE_COMMAND, offset, value) ==
                    tlm::TLM_OK_RESPONSE);
    }

    uint32_t peri1_read32(uint32_t offset)
    {
        uint32_t value = 0;
        TEST_ASSERT(access(m_peri1_cpu, tlm::TLM_READ_COMMAND, offset,
                           value) == tlm::TLM_OK_RESPONSE);
        return value;
    }

    void peri1_write32(uint32_t offset, uint32_t value)
    {
        TEST_ASSERT(access(m_peri1_cpu, tlm::TLM_WRITE_COMMAND, offset,
                           value) == tlm::TLM_OK_RESPONSE);
    }

    void settle()
    {
        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
    }

    void test_register_map_and_masks()
    {
        TEST_ASSERT(read32(hsoc_gpio::PROT) == 0);
        write32(hsoc_gpio::PROT, 0xffffffffu);
        TEST_ASSERT(read32(hsoc_gpio::PROT) == 0);

        const uint32_t bank6 = bank_reg(6, 0);
        write32(bank6 + hsoc_gpio::SEL, 0xffffffffu);
        write32(bank6 + hsoc_gpio::DAT, 0xffffffffu);
        write32(bank6 + hsoc_gpio::PS, 0xffffffffu);
        write32(bank6 + hsoc_gpio::PE, 0xffffffffu);
        write32(bank6 + hsoc_gpio::DS, 0xffffffffu);
        write32(bank6 + hsoc_gpio::IS, 0xffffffffu);
        write32(bank6 + hsoc_gpio::IE, 0xffffffffu);
        write32(bank6 + hsoc_gpio::INTR_CON, 4);
        write32(bank6 + hsoc_gpio::INTR_MASK, 0xffffffffu);
        write32(bank6 + hsoc_gpio::INTR_FLT_TYP, 0xffffffffu);
        write32(bank6 + hsoc_gpio::INTR_FLT_DEPTH, 0xffffffffu);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::SEL) == 0xf);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::DAT) == 0);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::PS) == 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::PE) == 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::DS) == 3);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::IS) == 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::IE) == 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::INTR_CON) == 4);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::INTR_MASK) == 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::INTR_FLT_TYP) == 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::INTR_FLT_DEPTH) == 0xf);
        write32(bank6 + hsoc_gpio::SEL, 1);
        TEST_ASSERT(read32(bank6 + hsoc_gpio::DAT) == 1);

        uint32_t value = 0;
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0x004, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0xf000, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 1, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0, value, 1) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        unsigned char byte_enable[4] = { 0xff, 0xff, 0xff, 0xff };
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0, value, 4,
                           byte_enable) ==
                    tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        value = 5;
        TEST_ASSERT(access(tlm::TLM_WRITE_COMMAND,
                           hsoc_gpio::INTR_CON, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        value = 0;
        TEST_ASSERT(access(tlm::TLM_WRITE_COMMAND,
                           hsoc_gpio::INTR_MIRR_PEND, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    void test_mux_gpio_and_pulls()
    {
        write32(hsoc_gpio::SEL, 0x22u);
        settle();
        TEST_ASSERT(m_peripheral_signals[0].read());
        write32(hsoc_gpio::SEL, 0x32u);
        settle();
        TEST_ASSERT(!m_peripheral_signals[0].read());

        const uint32_t bank1 = bank_reg(1, 0);
        write32(bank1 + hsoc_gpio::SEL, 1u << (4 * 4));
        write32(bank1 + hsoc_gpio::IE, 1u << 5);
        write32(bank1 + hsoc_gpio::DAT, 1u << 4);
        settle();
        TEST_ASSERT(m_gpio12_oe.read());
        TEST_ASSERT((read32(bank1 + hsoc_gpio::DAT) & 0x30u) == 0x30u);
        write32(bank1 + hsoc_gpio::DAT, 0);
        settle();
        TEST_ASSERT((read32(bank1 + hsoc_gpio::DAT) & 0x30u) == 0);

        const uint32_t bank5 = bank_reg(5, 0);
        write32(bank5 + hsoc_gpio::IE, 1);
        write32(bank5 + hsoc_gpio::PS, 1);
        write32(bank5 + hsoc_gpio::PE, 0);
        TEST_ASSERT((read32(bank5 + hsoc_gpio::DAT) & 1u) == 1u);
        write32(bank5 + hsoc_gpio::PS, 0);
        settle();
        TEST_ASSERT((read32(bank5 + hsoc_gpio::DAT) & 1u) == 0);
        write32(bank5 + hsoc_gpio::IE, 0);
        write32(bank5 + hsoc_gpio::PS, 1);
        settle();
        TEST_ASSERT((read32(bank5 + hsoc_gpio::DAT) & 1u) == 0);
    }

    void clear_pending(uint32_t base, uint32_t bit)
    {
        write32(base + hsoc_gpio::INTR_PEND, bit);
        TEST_ASSERT((read32(base + hsoc_gpio::INTR_PEND) & bit) == 0);
    }

    void drive_loopback(uint32_t base, bool level)
    {
        write32(base + hsoc_gpio::DAT, level ? 1u << 4 : 0);
        settle();
    }

    void test_interrupt_modes()
    {
        const uint32_t base = bank_reg(1, 0);
        const uint32_t bit = 1u << 5;
        write32(base + hsoc_gpio::SEL, 1u << (4 * 4));
        write32(base + hsoc_gpio::IE, bit);
        write32(base + hsoc_gpio::INTR_MASK, 0);

        drive_loopback(base, false);
        write32(base + hsoc_gpio::INTR_CON, 3u << (5 * 4));
        clear_pending(base, bit);
        drive_loopback(base, true);
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_PEND) == bit);
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_MIRR_PEND) == bit);
        TEST_ASSERT(m_bank1_irq.read());

        clear_pending(base, bit);
        write32(base + hsoc_gpio::INTR_CON, 2u << (5 * 4));
        drive_loopback(base, false);
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_PEND) == bit);

        clear_pending(base, bit);
        write32(base + hsoc_gpio::INTR_CON, 4u << (5 * 4));
        drive_loopback(base, true);
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_PEND) == bit);
        clear_pending(base, bit);
        drive_loopback(base, false);
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_PEND) == bit);

        clear_pending(base, bit);
        write32(base + hsoc_gpio::INTR_CON, 0);
        drive_loopback(base, true);
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_PEND) == bit);
        write32(base + hsoc_gpio::INTR_MASK, bit);
        settle();
        TEST_ASSERT(!m_bank1_irq.read());
        write32(base + hsoc_gpio::INTR_MASK, 0);
        settle();
        TEST_ASSERT(m_bank1_irq.read());

        drive_loopback(base, false);
        clear_pending(base, bit);
        write32(base + hsoc_gpio::INTR_CON, 1u << (5 * 4));
        settle();
        TEST_ASSERT(read32(base + hsoc_gpio::INTR_PEND) == bit);
    }

    void test_filters()
    {
        const uint32_t base = bank_reg(0, 0);
        const uint32_t bit = 1;
        write32(base + hsoc_gpio::SEL, 0);
        write32(base + hsoc_gpio::IE, bit);
        write32(base + hsoc_gpio::INTR_CON, 3);
        write32(base + hsoc_gpio::INTR_MASK, 0);
        write32(base + hsoc_gpio::INTR_FLT_DEPTH, 2);
        write32(base + hsoc_gpio::INTR_FLT_TYP, 0);
        const sc_core::sc_time period(10, sc_core::SC_NS);
        const uint64_t remainder = sc_core::sc_time_stamp().value() %
            period.value();
        if (remainder != 0)
            wait(sc_core::sc_time::from_value(period.value() - remainder));
        wait(5, sc_core::SC_NS);
        m_dut.gpio_in[0]->write(true);
        wait(14, sc_core::SC_NS);
        settle();
        TEST_ASSERT(!m_bank0_irq.read());
        wait(1, sc_core::SC_NS);
        settle();
        TEST_ASSERT(m_bank0_irq.read());

        clear_pending(base, bit);
        write32(base + hsoc_gpio::INTR_FLT_DEPTH, 0);
        m_dut.gpio_in[0]->write(false);
        settle();
        write32(base + hsoc_gpio::INTR_FLT_DEPTH, 3);
        m_dut.gpio_in[0]->write(true);
        wait(5, sc_core::SC_NS);
        write32(base + hsoc_gpio::INTR_FLT_DEPTH, 0);
        settle();
        TEST_ASSERT((read32(base + hsoc_gpio::INTR_PEND) & bit) != 0);

        clear_pending(base, bit);
        write32(base + hsoc_gpio::INTR_FLT_DEPTH, 2);
        write32(base + hsoc_gpio::INTR_FLT_TYP, bit);
        m_dut.gpio_in[0]->write(false);
        wait(20, sc_core::SC_NS);
        settle();
        const uint64_t time_remainder = sc_core::sc_time_stamp().value() %
            period.value();
        if (time_remainder != 0)
            wait(sc_core::sc_time::from_value(
                period.value() - time_remainder));
        wait(5, sc_core::SC_NS);
        m_dut.gpio_in[0]->write(true);
        wait(19, sc_core::SC_NS);
        settle();
        TEST_ASSERT(!m_bank0_irq.read());
        wait(1, sc_core::SC_NS);
        settle();
        TEST_ASSERT(m_bank0_irq.read());

        clear_pending(base, bit);
        m_dut.gpio_in[0]->write(false);
        wait(20, sc_core::SC_NS);
        m_dut.gpio_in[0]->write(true);
        wait(5, sc_core::SC_NS);
        m_dut.gpio_in[0]->write(false);
        wait(20, sc_core::SC_NS);
        settle();
        TEST_ASSERT((read32(base + hsoc_gpio::INTR_PEND) & bit) == 0);
    }

    void test_peri1_and_reset()
    {
        for (unsigned int id = 0; id < hsoc_gpio::NUM_PERIPHERALS; ++id)
            TEST_ASSERT(!m_peri1_peripheral_signals[id].read());
        peri1_write32(hsoc_gpio::SEL, 0x22222222u);
        settle();
        TEST_ASSERT(m_peri1_peripheral_signals[8].read());
        TEST_ASSERT(m_peri1_peripheral_signals[9].read());
        TEST_ASSERT(!m_peripheral_signals[8].read());

        m_dut.reset->write(true);
        settle();
        write32(hsoc_gpio::SEL, 0xffffffffu);
        TEST_ASSERT(read32(hsoc_gpio::SEL) == 0);
        TEST_ASSERT(read32(hsoc_gpio::PE) == 0xffu);
        TEST_ASSERT(read32(hsoc_gpio::INTR_MASK) == 0xffu);
        TEST_ASSERT(read32(hsoc_gpio::INTR_PEND) == 0);
        TEST_ASSERT(!m_bank1_irq.read());
        TEST_ASSERT(!m_gpio12_oe.read());
        for (unsigned int id = 0; id < hsoc_gpio::NUM_PERIPHERALS; ++id)
            TEST_ASSERT(!m_peripheral_signals[id].read());
        m_dut.reset->write(false);
    }

    void run_test()
    {
        settle();
        test_register_map_and_masks();
        test_mux_gpio_and_pulls();
        test_interrupt_modes();
        test_filters();
        test_peri1_and_reset();
        sc_core::sc_stop();
    }

public:
    explicit HsocGpioTest(sc_core::sc_module_name name)
        : TestBench(name)
        , m_dut("gpio")
        , m_peri1("peri1")
        , m_cpu("cpu")
        , m_peri1_cpu("peri1_cpu")
        , m_bank1_irq("bank1_irq")
        , m_bank0_irq("bank0_irq")
        , m_gpio12_oe("gpio12_oe")
        , m_peripheral_signals("peripheral_signal",
                               hsoc_gpio::NUM_PERIPHERALS)
        , m_peri1_peripheral_signals("peri1_peripheral_signal",
                                     hsoc_gpio::NUM_PERIPHERALS)
    {
        m_cpu.bind(m_dut.target_socket);
        m_peri1_cpu.bind(m_peri1.target_socket);
        m_dut.gpio_out[12].bind(m_dut.gpio_in[13]);
        m_dut.gpio_oe[12].bind(m_gpio12_oe);
        m_dut.irq[1].bind(m_bank1_irq);
        m_dut.irq[0].bind(m_bank0_irq);
        for (unsigned int id = 0; id < hsoc_gpio::NUM_PERIPHERALS; ++id) {
            m_dut.peripheral_enable[id].bind(m_peripheral_signals[id]);
            m_peri1.peripheral_enable[id].bind(
                m_peri1_peripheral_signals[id]);
        }
        SC_THREAD(run_test);
    }
};

} // namespace

int sc_main(int argc, char* argv[])
{
    gs::ConfigurableBroker broker{};
    const cci::cci_originator originator("hsoc_gpio_test");
    auto broker_handle = broker.create_broker_handle(originator);
    broker_handle.set_preset_cci_value(
        "test-bench.peri1.bank_sizes",
        cci::cci_value(std::string("8,8,8,8,1,1,1,1")));
    broker_handle.set_preset_cci_value(
        "test-bench.peri1.peripheral_routes",
        cci::cci_value(
            std::string("8:0:0:4;9:0:4:4;12:1:0:2;13:1:2:2")));

    HsocGpioTest test_bench("test-bench");
    test_bench.run();
    return test_bench.get_rc();
}
