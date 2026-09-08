/* SPDX-License-Identifier: BSD-3-Clause */

#include <array>
#include <cstdint>

#include <hsoc_pinctrl.h>
#include <test/test.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

class HsocPinctrlTest : public TestBench
{
    using CpuSocket = tlm_utils::simple_initiator_socket<
        HsocPinctrlTest, DEFAULT_TLM_BUSWIDTH>;

    hsoc_pinctrl m_dut;
    hsoc_pinctrl m_peri1;
    CpuSocket m_cpu;
    CpuSocket m_peri1_cpu;
    sc_core::sc_signal<bool> m_bank1_irq;
    sc_core::sc_signal<bool> m_bank7_irq;
    sc_core::sc_signal<bool> m_gpio12_oe;
    sc_core::sc_signal<bool> m_gpio48_oe;
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_peripheral_signals;
    sc_core::sc_vector<sc_core::sc_signal<bool>> m_peri1_peripheral_signals;
    sc_core::sc_signal<bool> m_peri1_bank1_irq;

    static uint32_t bank_reg(unsigned int bank, uint32_t reg)
    {
        return hsoc_pinctrl::BANK_BASE + bank * hsoc_pinctrl::BANK_STRIDE + reg;
    }

    static uint32_t pin_config(unsigned int bank, unsigned int pin)
    {
        return bank_reg(bank, hsoc_pinctrl::PIN_CONFIG_BASE + pin * 4);
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

    void test_identity_and_validation()
    {
        TEST_ASSERT(read32(0x00) == hsoc_pinctrl::ID);
        TEST_ASSERT(read32(0x04) == hsoc_pinctrl::VERSION);
        TEST_ASSERT(read32(0x08) == 14);
        TEST_ASSERT(read32(0x0c) == 56);
        TEST_ASSERT(read32(bank_reg(0, hsoc_pinctrl::BANK_NUM_PINS)) == 8);
        TEST_ASSERT(read32(bank_reg(6, hsoc_pinctrl::BANK_NUM_PINS)) == 1);
        TEST_ASSERT(read32(bank_reg(13, hsoc_pinctrl::BANK_NUM_PINS)) == 1);

        uint32_t value = 0;
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0x10, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, bank_reg(6, 0x44), value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0xf000, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0x01, value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0x00, value, 1) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        unsigned char byte_enable[4] = { 0xff, 0xff, 0xff, 0xff };
        TEST_ASSERT(access(tlm::TLM_READ_COMMAND, 0x00, value, 4,
                           byte_enable) ==
                    tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        value = (3u << 8) | 1u;
        TEST_ASSERT(access(tlm::TLM_WRITE_COMMAND, pin_config(0, 0), value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
        value = (4u << 8) | 5u;
        TEST_ASSERT(access(tlm::TLM_WRITE_COMMAND, pin_config(0, 0), value) ==
                    tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    void test_mux_and_gpio()
    {
        const uint32_t drive4 = 4u << 8;
        write32(pin_config(0, 1), drive4 | 2u);
        for (uint32_t function = 0; function <= 4; ++function) {
            write32(pin_config(0, 0), drive4 | function);
            settle();
            TEST_ASSERT(m_peripheral_signals[0].read() == (function == 2));
        }

        struct Group {
            unsigned int bank;
            unsigned int first;
            unsigned int count;
        };
        const std::array<Group, 14> groups{{
            { 0, 0, 2 }, { 0, 2, 2 }, { 0, 4, 2 }, { 0, 6, 2 },
            { 1, 0, 2 }, { 1, 2, 2 },
            { 2, 0, 4 }, { 2, 4, 4 }, { 3, 0, 4 }, { 3, 4, 4 },
            { 4, 0, 2 }, { 4, 2, 2 }, { 4, 4, 2 }, { 4, 6, 2 },
        }};
        for (unsigned int index = 0; index < groups.size(); ++index) {
            const Group& group = groups[index];
            for (unsigned int pin = group.first;
                 pin < group.first + group.count; ++pin)
                write32(pin_config(group.bank, pin), drive4 | 2u);
            settle();
            TEST_ASSERT(m_peripheral_signals[index].read());
            write32(pin_config(group.bank, group.first), drive4 | 3u);
            settle();
            TEST_ASSERT(!m_peripheral_signals[index].read());
            write32(pin_config(group.bank, group.first), drive4 | 2u);
        }

        write32(pin_config(1, 4), drive4 | 1u);
        write32(pin_config(1, 5), drive4 | 0u);
        write32(bank_reg(1, hsoc_pinctrl::GPIO_OUTPUT_SET), 1u << 4);
        settle();
        TEST_ASSERT(m_gpio12_oe.read());
        TEST_ASSERT(read32(bank_reg(1, hsoc_pinctrl::GPIO_OUTPUT)) &
                    (1u << 4));
        TEST_ASSERT(read32(bank_reg(1, hsoc_pinctrl::GPIO_INPUT)) & (1u << 5));
        write32(bank_reg(1, hsoc_pinctrl::GPIO_OUTPUT_CLEAR), 1u << 4);
        settle();
        TEST_ASSERT((read32(bank_reg(1, hsoc_pinctrl::GPIO_INPUT)) &
                     (1u << 5)) == 0);

        write32(pin_config(6, 0), drive4 | 1u);
        write32(pin_config(7, 0), drive4 | 0u);
        write32(bank_reg(6, hsoc_pinctrl::GPIO_OUTPUT), 1u);
        settle();
        TEST_ASSERT(m_gpio48_oe.read());
        TEST_ASSERT(read32(bank_reg(7, hsoc_pinctrl::GPIO_INPUT)) == 1u);

        const std::array<uint32_t, 5> drive_strengths{{ 2, 4, 8, 12, 16 }};
        for (const uint32_t drive : drive_strengths) {
            const uint32_t config = (drive << 8) | (1u << 4) | 3u;
            write32(pin_config(5, 3), config);
            TEST_ASSERT(read32(pin_config(5, 3)) == config);
        }
    }

    void test_interrupts()
    {
        const uint32_t output_bit = 1u << 4;
        const uint32_t input_bit = 1u << 5;
        const uint32_t base = bank_reg(1, 0);

        write32(base + hsoc_pinctrl::IRQ_ENABLE, input_bit);
        write32(base + hsoc_pinctrl::IRQ_RISING, input_bit);
        write32(base + hsoc_pinctrl::IRQ_FALLING, input_bit);
        write32(base + hsoc_pinctrl::GPIO_OUTPUT_SET, output_bit);
        settle();
        TEST_ASSERT(m_bank1_irq.read());
        TEST_ASSERT(read32(base + hsoc_pinctrl::IRQ_PENDING) == input_bit);
        write32(base + hsoc_pinctrl::IRQ_PENDING, input_bit);
        settle();
        TEST_ASSERT(!m_bank1_irq.read());

        write32(base + hsoc_pinctrl::GPIO_OUTPUT_CLEAR, output_bit);
        settle();
        TEST_ASSERT(m_bank1_irq.read());
        write32(base + hsoc_pinctrl::IRQ_PENDING, input_bit);
        write32(base + hsoc_pinctrl::IRQ_RISING, 0);
        write32(base + hsoc_pinctrl::IRQ_FALLING, 0);
        write32(base + hsoc_pinctrl::IRQ_LEVEL_HIGH, input_bit);
        settle();
        TEST_ASSERT(!m_bank1_irq.read());

        write32(base + hsoc_pinctrl::GPIO_OUTPUT_SET, output_bit);
        settle();
        TEST_ASSERT(m_bank1_irq.read());
        write32(base + hsoc_pinctrl::IRQ_PENDING, input_bit);
        settle();
        TEST_ASSERT(m_bank1_irq.read());
        write32(base + hsoc_pinctrl::GPIO_OUTPUT_CLEAR, output_bit);
        settle();
        TEST_ASSERT(!m_bank1_irq.read());

        write32(base + hsoc_pinctrl::IRQ_LEVEL_HIGH, 0);
        write32(base + hsoc_pinctrl::IRQ_LEVEL_LOW, input_bit);
        settle();
        TEST_ASSERT(m_bank1_irq.read());
        write32(base + hsoc_pinctrl::IRQ_ENABLE, 0);
        settle();
        TEST_ASSERT(!m_bank1_irq.read());
        write32(base + hsoc_pinctrl::IRQ_ENABLE, input_bit);
        settle();
        TEST_ASSERT(m_bank1_irq.read());

        write32(bank_reg(7, hsoc_pinctrl::IRQ_LEVEL_HIGH), 1u);
        write32(bank_reg(7, hsoc_pinctrl::IRQ_ENABLE), 1u);
        settle();
        TEST_ASSERT(m_bank7_irq.read());
        write32(bank_reg(7, hsoc_pinctrl::IRQ_PENDING), 1u);
        settle();
        TEST_ASSERT(m_bank7_irq.read());
        write32(bank_reg(6, hsoc_pinctrl::GPIO_OUTPUT_CLEAR), 1u);
        settle();
        TEST_ASSERT(!m_bank7_irq.read());
    }

    void test_reset()
    {
        m_dut.reset->write(true);
        settle();
        write32(pin_config(0, 0), (16u << 8) | 2u);
        write32(bank_reg(1, hsoc_pinctrl::GPIO_OUTPUT), 0xffu);
        TEST_ASSERT(read32(pin_config(0, 0)) == 0x400u);
        TEST_ASSERT(read32(bank_reg(1, hsoc_pinctrl::GPIO_OUTPUT)) == 0);
        TEST_ASSERT(read32(bank_reg(1, hsoc_pinctrl::IRQ_ENABLE)) == 0);
        TEST_ASSERT(read32(bank_reg(1, hsoc_pinctrl::IRQ_PENDING)) == 0);
        TEST_ASSERT(!m_bank1_irq.read());
        TEST_ASSERT(!m_bank7_irq.read());
        TEST_ASSERT(!m_gpio12_oe.read());
        for (unsigned int index = 0;
             index < hsoc_pinctrl::NUM_PERIPHERALS; ++index)
            TEST_ASSERT(!m_peripheral_signals[index].read());
        m_dut.reset->write(false);
    }

    void test_peri1_routes_are_instance_local()
    {
        TEST_ASSERT(peri1_read32(0x08) == 8);
        TEST_ASSERT(peri1_read32(0x0c) == 36);
        for (unsigned int id = 0; id < hsoc_pinctrl::NUM_PERIPHERALS; ++id)
            TEST_ASSERT(!m_peri1_peripheral_signals[id].read());

        struct Group {
            unsigned int id;
            unsigned int bank;
            unsigned int first;
            unsigned int count;
        };
        const std::array<Group, 4> groups{{
            { 8, 0, 0, 4 }, { 9, 0, 4, 4 },
            { 12, 1, 0, 2 }, { 13, 1, 2, 2 },
        }};
        for (const Group& group : groups) {
            for (unsigned int pin = group.first;
                 pin < group.first + group.count; ++pin)
                peri1_write32(pin_config(group.bank, pin), 0x402u);
        }
        settle();
        for (unsigned int id = 0; id < hsoc_pinctrl::NUM_PERIPHERALS; ++id) {
            const bool assigned = id == 8 || id == 9 || id == 12 || id == 13;
            TEST_ASSERT(m_peri1_peripheral_signals[id].read() == assigned);
            TEST_ASSERT(!m_peripheral_signals[id].read());
        }

        peri1_write32(pin_config(1, 3), 0x403u);
        settle();
        TEST_ASSERT(m_peri1_peripheral_signals[12].read());
        TEST_ASSERT(!m_peri1_peripheral_signals[13].read());

        const uint32_t bit = 1u << 4;
        peri1_write32(bank_reg(1, hsoc_pinctrl::IRQ_RISING), bit);
        peri1_write32(bank_reg(1, hsoc_pinctrl::IRQ_ENABLE), bit);
        m_peri1.gpio_in[12]->write(true);
        settle();
        TEST_ASSERT(m_peri1_bank1_irq.read());
        TEST_ASSERT(!m_bank1_irq.read());
        TEST_ASSERT(read32(bank_reg(1, hsoc_pinctrl::IRQ_PENDING)) == 0);
        peri1_write32(bank_reg(1, hsoc_pinctrl::IRQ_PENDING), bit);
        settle();
        TEST_ASSERT(!m_peri1_bank1_irq.read());
    }

    void run_test()
    {
        settle();
        test_identity_and_validation();
        test_mux_and_gpio();
        test_interrupts();
        test_reset();
        test_peri1_routes_are_instance_local();
        sc_core::sc_stop();
    }

public:
    explicit HsocPinctrlTest(sc_core::sc_module_name name)
        : TestBench(name)
        , m_dut("pinctrl")
        , m_peri1("peri1")
        , m_cpu("cpu")
        , m_peri1_cpu("peri1_cpu")
        , m_bank1_irq("bank1_irq")
        , m_bank7_irq("bank7_irq")
        , m_gpio12_oe("gpio12_oe")
        , m_gpio48_oe("gpio48_oe")
        , m_peripheral_signals("peripheral_signal",
                               hsoc_pinctrl::NUM_PERIPHERALS)
        , m_peri1_peripheral_signals("peri1_peripheral_signal",
                                     hsoc_pinctrl::NUM_PERIPHERALS)
        , m_peri1_bank1_irq("peri1_bank1_irq")
    {
        m_cpu.bind(m_dut.target_socket);
        m_peri1_cpu.bind(m_peri1.target_socket);
        m_dut.gpio_out[12].bind(m_dut.gpio_in[13]);
        m_dut.gpio_out[48].bind(m_dut.gpio_in[56]);
        m_dut.gpio_oe[12].bind(m_gpio12_oe);
        m_dut.gpio_oe[48].bind(m_gpio48_oe);
        m_dut.irq[1].bind(m_bank1_irq);
        m_dut.irq[7].bind(m_bank7_irq);
        for (unsigned int index = 0;
             index < hsoc_pinctrl::NUM_PERIPHERALS; ++index)
            m_dut.peripheral_enable[index].bind(m_peripheral_signals[index]);
        for (unsigned int index = 0;
             index < hsoc_pinctrl::NUM_PERIPHERALS; ++index)
            m_peri1.peripheral_enable[index].bind(
                m_peri1_peripheral_signals[index]);
        m_peri1.irq[1].bind(m_peri1_bank1_irq);
        SC_THREAD(run_test);
    }
};

} // namespace

int sc_main(int argc, char* argv[])
{
    gs::ConfigurableBroker broker{};
    const cci::cci_originator originator("hsoc_pinctrl_test");
    auto broker_handle = broker.create_broker_handle(originator);
    broker_handle.set_preset_cci_value(
        "test-bench.peri1.bank_sizes",
        cci::cci_value(std::string("8,8,8,8,1,1,1,1")));
    broker_handle.set_preset_cci_value(
        "test-bench.peri1.peripheral_routes",
        cci::cci_value(
            std::string("8:0:0:4;9:0:4:4;12:1:0:2;13:1:2:2")));

    HsocPinctrlTest test_bench("test-bench");
    test_bench.run();
    return test_bench.get_rc();
}
