/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <async_event.h>
#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class hsoc_pinctrl : public sc_core::sc_module
{
public:
    static constexpr unsigned int MAX_BANKS = 14;
    static constexpr unsigned int PINS_PER_BANK = 8;
    static constexpr unsigned int NUM_PIN_SLOTS = MAX_BANKS * PINS_PER_BANK;
    static constexpr unsigned int NUM_PERIPHERALS = 14;

    static constexpr uint32_t ID = 0x48535043;
    static constexpr uint32_t VERSION = 0x00010000;
    static constexpr uint32_t BANK_BASE = 0x1000;
    static constexpr uint32_t BANK_STRIDE = 0x1000;

    enum BankRegister : uint32_t {
        BANK_NUM_PINS = 0x00,
        GPIO_INPUT = 0x04,
        GPIO_OUTPUT = 0x08,
        GPIO_OUTPUT_SET = 0x0c,
        GPIO_OUTPUT_CLEAR = 0x10,
        IRQ_ENABLE = 0x14,
        IRQ_PENDING = 0x18,
        IRQ_RISING = 0x1c,
        IRQ_FALLING = 0x20,
        IRQ_LEVEL_HIGH = 0x24,
        IRQ_LEVEL_LOW = 0x28,
        PIN_CONFIG_BASE = 0x40,
    };

    cci::cci_param<std::string> p_bank_sizes;
    cci::cci_param<std::string> p_peripheral_routes;
    cci::cci_param<sc_core::sc_time> p_access_latency;

    tlm_utils::simple_target_socket<hsoc_pinctrl, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    TargetSignalSocket<bool> reset;
    sc_core::sc_vector<TargetSignalSocket<bool>> gpio_in;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> gpio_out;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> gpio_oe;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> irq;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> peripheral_enable;

    SC_HAS_PROCESS(hsoc_pinctrl);

    explicit hsoc_pinctrl(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_bank_sizes("bank_sizes", "8,8,8,8,8,8,1,1,1,1,1,1,1,1")
        , p_peripheral_routes("peripheral_routes", "")
        , p_access_latency("access_latency",
                           sc_core::sc_time(10, sc_core::SC_NS))
        , target_socket("target_socket")
        , reset("reset")
        , gpio_in("gpio_in", NUM_PIN_SLOTS,
                  [](const char* socket_name, std::size_t) {
                      return new TargetSignalSocket<bool>(socket_name);
                  })
        , gpio_out("gpio_out", NUM_PIN_SLOTS)
        , gpio_oe("gpio_oe", NUM_PIN_SLOTS)
        , irq("irq", MAX_BANKS)
        , peripheral_enable("peripheral_enable", NUM_PERIPHERALS)
        , m_update_event(false)
    {
        parse_bank_sizes();
        parse_peripheral_routes();
        target_socket.register_b_transport(this, &hsoc_pinctrl::b_transport);
        reset.register_value_changed_cb(
            [this](bool asserted) { set_reset(asserted); });
        for (unsigned int pin = 0; pin < NUM_PIN_SLOTS; ++pin) {
            gpio_in[pin].register_value_changed_cb(
                [this, pin](bool level) { set_gpio_input(pin, level); });
        }

        SC_METHOD(drive_outputs);
        sensitive << m_update_event;
    }

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        trans.set_dmi_allowed(false);
        if (!valid_payload(trans)) return;

        const uint32_t offset = static_cast<uint32_t>(trans.get_address());
        uint32_t value = 0;
        bool ok = false;
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            ok = read_register(offset, value);
            if (ok) store_le32(trans.get_data_ptr(), value);
        } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            value = load_le32(trans.get_data_ptr());
            ok = write_register(offset, value);
        } else {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        if (!ok) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        delay += p_access_latency.get_value();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

private:
    struct Bank {
        uint32_t output = 0;
        uint32_t irq_enable = 0;
        uint32_t edge_pending = 0;
        uint32_t rising = 0;
        uint32_t falling = 0;
        uint32_t level_high = 0;
        uint32_t level_low = 0;
    };

    struct PeripheralGroup {
        unsigned int bank;
        unsigned int first_pin;
        unsigned int pin_count;
    };

    static constexpr std::array<PeripheralGroup, NUM_PERIPHERALS>
        DEFAULT_PERIPHERAL_GROUPS = {{
            { 0, 0, 2 }, { 0, 2, 2 }, { 0, 4, 2 }, { 0, 6, 2 },
            { 1, 0, 2 }, { 1, 2, 2 },
            { 2, 0, 4 }, { 2, 4, 4 }, { 3, 0, 4 }, { 3, 4, 4 },
            { 4, 0, 2 }, { 4, 2, 2 }, { 4, 4, 2 }, { 4, 6, 2 },
        }};

    std::array<uint8_t, MAX_BANKS> m_bank_sizes{};
    std::array<PeripheralGroup, NUM_PERIPHERALS> m_peripheral_groups{};
    unsigned int m_num_banks = 0;
    unsigned int m_num_pins = 0;
    std::array<Bank, MAX_BANKS> m_banks{};
    std::array<uint32_t, NUM_PIN_SLOTS> m_pin_config{};
    std::array<bool, NUM_PIN_SLOTS> m_input_levels{};
    bool m_reset_asserted = false;
    gs::async_event m_update_event;

    static uint32_t load_le32(const unsigned char* data)
    {
        return static_cast<uint32_t>(data[0]) |
               (static_cast<uint32_t>(data[1]) << 8) |
               (static_cast<uint32_t>(data[2]) << 16) |
               (static_cast<uint32_t>(data[3]) << 24);
    }

    static void store_le32(unsigned char* data, uint32_t value)
    {
        data[0] = value;
        data[1] = value >> 8;
        data[2] = value >> 16;
        data[3] = value >> 24;
    }

    void parse_bank_sizes()
    {
        const std::string specification = p_bank_sizes.get_value();
        if (specification.empty() || specification.front() == ',' ||
            specification.back() == ',')
            throw std::invalid_argument("invalid hsoc_pinctrl bank_sizes");
        std::stringstream list(specification);
        std::string entry;
        while (std::getline(list, entry, ',')) {
            if (entry.empty() || m_num_banks == MAX_BANKS)
                throw std::invalid_argument("invalid hsoc_pinctrl bank_sizes");
            std::size_t consumed = 0;
            unsigned long size = 0;
            try {
                size = std::stoul(entry, &consumed, 10);
            } catch (const std::exception&) {
                throw std::invalid_argument("invalid hsoc_pinctrl bank_sizes");
            }
            if (consumed != entry.size() || size == 0 || size > PINS_PER_BANK)
                throw std::invalid_argument("invalid hsoc_pinctrl bank_sizes");
            m_bank_sizes[m_num_banks++] = static_cast<uint8_t>(size);
            m_num_pins += static_cast<unsigned int>(size);
        }
        if (m_num_banks == 0)
            throw std::invalid_argument("invalid hsoc_pinctrl bank_sizes");
        reset_registers();
    }

    static unsigned long parse_route_number(const std::string& text)
    {
        if (text.empty())
            throw std::invalid_argument("invalid hsoc_pinctrl peripheral_routes");
        std::size_t consumed = 0;
        unsigned long value = 0;
        try {
            value = std::stoul(text, &consumed, 10);
        } catch (const std::exception&) {
            throw std::invalid_argument("invalid hsoc_pinctrl peripheral_routes");
        }
        if (consumed != text.size())
            throw std::invalid_argument("invalid hsoc_pinctrl peripheral_routes");
        return value;
    }

    void parse_peripheral_routes()
    {
        const std::string specification = p_peripheral_routes.get_value();
        if (specification.empty()) {
            m_peripheral_groups = DEFAULT_PERIPHERAL_GROUPS;
            return;
        }
        if (specification.front() == ';' || specification.back() == ';')
            throw std::invalid_argument("invalid hsoc_pinctrl peripheral_routes");

        std::array<bool, NUM_PERIPHERALS> assigned{};
        std::stringstream routes(specification);
        std::string route;
        while (std::getline(routes, route, ';')) {
            if (route.empty() || route.front() == ':' || route.back() == ':')
                throw std::invalid_argument(
                    "invalid hsoc_pinctrl peripheral_routes");
            std::array<unsigned long, 4> fields{};
            std::stringstream values(route);
            std::string field;
            unsigned int count = 0;
            while (std::getline(values, field, ':')) {
                if (count == fields.size())
                    throw std::invalid_argument(
                        "invalid hsoc_pinctrl peripheral_routes");
                fields[count++] = parse_route_number(field);
            }
            const unsigned long id = fields[0];
            const unsigned long bank = fields[1];
            const unsigned long first = fields[2];
            const unsigned long pins = fields[3];
            if (count != fields.size() || id >= NUM_PERIPHERALS ||
                assigned[id] || bank >= m_num_banks || pins == 0 ||
                pins > PINS_PER_BANK || first >= m_bank_sizes[bank] ||
                pins > m_bank_sizes[bank] - first)
                throw std::invalid_argument(
                    "invalid hsoc_pinctrl peripheral_routes");
            assigned[id] = true;
            m_peripheral_groups[id] = {
                static_cast<unsigned int>(bank),
                static_cast<unsigned int>(first),
                static_cast<unsigned int>(pins),
            };
        }
    }

    bool valid_payload(tlm::tlm_generic_payload& trans)
    {
        if (!trans.get_data_ptr()) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return false;
        }
        if (trans.get_byte_enable_ptr()) {
            trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return false;
        }
        if (trans.get_data_length() != 4 || trans.get_streaming_width() != 4 ||
            (trans.get_address() & 3) != 0 || trans.get_address() >= 0x10000) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return false;
        }
        return true;
    }

    uint32_t valid_mask(unsigned int bank) const
    {
        return (1u << m_bank_sizes[bank]) - 1u;
    }

    bool valid_pin(unsigned int slot) const
    {
        const unsigned int bank = slot / PINS_PER_BANK;
        return bank < m_num_banks &&
               (slot % PINS_PER_BANK) < m_bank_sizes[bank];
    }

    unsigned int mux(unsigned int slot) const
    {
        return m_pin_config[slot] & 0x7u;
    }

    uint32_t input_value(unsigned int bank) const
    {
        uint32_t value = 0;
        for (unsigned int pin = 0; pin < m_bank_sizes[bank]; ++pin) {
            const unsigned int slot = bank * PINS_PER_BANK + pin;
            const bool level = mux(slot) == 1
                ? (m_banks[bank].output & (1u << pin)) != 0
                : m_input_levels[slot];
            if (level) value |= 1u << pin;
        }
        return value;
    }

    uint32_t level_pending(unsigned int bank) const
    {
        uint32_t result = 0;
        const uint32_t inputs = input_value(bank);
        for (unsigned int pin = 0; pin < m_bank_sizes[bank]; ++pin) {
            const uint32_t bit = 1u << pin;
            const unsigned int slot = bank * PINS_PER_BANK + pin;
            if (mux(slot) != 0) continue;
            if (((m_banks[bank].level_high & bit) && (inputs & bit)) ||
                ((m_banks[bank].level_low & bit) && !(inputs & bit)))
                result |= bit;
        }
        return result;
    }

    uint32_t pending(unsigned int bank) const
    {
        return (m_banks[bank].edge_pending | level_pending(bank)) &
               valid_mask(bank);
    }

    bool decode_bank(uint32_t offset, unsigned int& bank,
                     uint32_t& bank_offset) const
    {
        if (offset < BANK_BASE) return false;
        bank = (offset - BANK_BASE) / BANK_STRIDE;
        bank_offset = (offset - BANK_BASE) % BANK_STRIDE;
        return bank < m_num_banks;
    }

    bool read_register(uint32_t offset, uint32_t& value) const
    {
        switch (offset) {
        case 0x00: value = ID; return true;
        case 0x04: value = VERSION; return true;
        case 0x08: value = m_num_banks; return true;
        case 0x0c: value = m_num_pins; return true;
        default: break;
        }

        unsigned int bank = 0;
        uint32_t reg = 0;
        if (!decode_bank(offset, bank, reg)) return false;
        const Bank& state = m_banks[bank];
        switch (reg) {
        case BANK_NUM_PINS: value = m_bank_sizes[bank]; return true;
        case GPIO_INPUT: value = input_value(bank); return true;
        case GPIO_OUTPUT: value = state.output; return true;
        case GPIO_OUTPUT_SET:
        case GPIO_OUTPUT_CLEAR: value = 0; return true;
        case IRQ_ENABLE: value = state.irq_enable; return true;
        case IRQ_PENDING: value = pending(bank); return true;
        case IRQ_RISING: value = state.rising; return true;
        case IRQ_FALLING: value = state.falling; return true;
        case IRQ_LEVEL_HIGH: value = state.level_high; return true;
        case IRQ_LEVEL_LOW: value = state.level_low; return true;
        default: break;
        }
        if (reg >= PIN_CONFIG_BASE && ((reg - PIN_CONFIG_BASE) % 4) == 0) {
            const unsigned int pin = (reg - PIN_CONFIG_BASE) / 4;
            if (pin < m_bank_sizes[bank]) {
                value = m_pin_config[bank * PINS_PER_BANK + pin];
                return true;
            }
        }
        return false;
    }

    bool write_register(uint32_t offset, uint32_t value)
    {
        unsigned int bank = 0;
        uint32_t reg = 0;
        if (!decode_bank(offset, bank, reg)) return false;
        if (m_reset_asserted) return true;
        Bank& state = m_banks[bank];
        const uint32_t mask = valid_mask(bank);
        switch (reg) {
        case GPIO_OUTPUT: state.output = value & mask; break;
        case GPIO_OUTPUT_SET: state.output |= value & mask; break;
        case GPIO_OUTPUT_CLEAR: state.output &= ~(value & mask); break;
        case IRQ_ENABLE: state.irq_enable = value & mask; break;
        case IRQ_PENDING: state.edge_pending &= ~(value & mask); break;
        case IRQ_RISING: state.rising = value & mask; break;
        case IRQ_FALLING: state.falling = value & mask; break;
        case IRQ_LEVEL_HIGH: state.level_high = value & mask; break;
        case IRQ_LEVEL_LOW: state.level_low = value & mask; break;
        default:
            if (reg < PIN_CONFIG_BASE || ((reg - PIN_CONFIG_BASE) % 4) != 0)
                return false;
            {
                const unsigned int pin = (reg - PIN_CONFIG_BASE) / 4;
                if (pin >= m_bank_sizes[bank] || !valid_config(value))
                    return false;
                m_pin_config[bank * PINS_PER_BANK + pin] = value;
            }
            break;
        }
        m_update_event.notify(sc_core::SC_ZERO_TIME);
        return true;
    }

    static bool valid_config(uint32_t value)
    {
        const uint32_t mux_value = value & 0x7u;
        const uint32_t drive = (value >> 8) & 0xffu;
        const uint32_t reserved = value & 0xffff00e8u;
        return mux_value <= 4 && reserved == 0 &&
               (drive == 2 || drive == 4 || drive == 8 || drive == 12 ||
                drive == 16);
    }

    void set_gpio_input(unsigned int slot, bool level)
    {
        const bool old = m_input_levels[slot];
        m_input_levels[slot] = level;
        if (old == level || !valid_pin(slot)) return;

        const unsigned int bank = slot / PINS_PER_BANK;
        const unsigned int pin = slot % PINS_PER_BANK;
        const uint32_t bit = 1u << pin;
        if (mux(slot) == 0 &&
            ((!old && level && (m_banks[bank].rising & bit)) ||
             (old && !level && (m_banks[bank].falling & bit))))
            m_banks[bank].edge_pending |= bit;
        m_update_event.notify(sc_core::SC_ZERO_TIME);
    }

    void set_reset(bool asserted)
    {
        if (asserted && !m_reset_asserted) reset_registers();
        m_reset_asserted = asserted;
    }

    void reset_registers()
    {
        m_banks = {};
        m_pin_config.fill(4u << 8);
        m_update_event.notify(sc_core::SC_ZERO_TIME);
    }

    bool peripheral_enabled(unsigned int index) const
    {
        const PeripheralGroup& group = m_peripheral_groups[index];
        if (group.pin_count == 0 || group.bank >= m_num_banks ||
            group.first_pin + group.pin_count > m_bank_sizes[group.bank])
            return false;
        for (unsigned int pin = group.first_pin;
             pin < group.first_pin + group.pin_count; ++pin) {
            if (mux(group.bank * PINS_PER_BANK + pin) != 2) return false;
        }
        return true;
    }

    void write_if_bound(InitiatorSignalSocket<bool>& socket, bool value)
    {
        if (socket.get_interface()) socket->write(value);
    }

    void drive_outputs()
    {
        for (unsigned int slot = 0; slot < NUM_PIN_SLOTS; ++slot) {
            bool value = false;
            bool output_enable = false;
            if (valid_pin(slot)) {
                const unsigned int bank = slot / PINS_PER_BANK;
                const unsigned int pin = slot % PINS_PER_BANK;
                output_enable = mux(slot) == 1;
                value = output_enable
                    ? (m_banks[bank].output & (1u << pin)) != 0
                    : m_input_levels[slot];
            }
            write_if_bound(gpio_out[slot], value);
            write_if_bound(gpio_oe[slot], output_enable);
        }
        for (unsigned int bank = 0; bank < MAX_BANKS; ++bank) {
            const bool asserted = bank < m_num_banks &&
                (pending(bank) & m_banks[bank].irq_enable) != 0;
            write_if_bound(irq[bank], asserted);
        }
        for (unsigned int index = 0; index < NUM_PERIPHERALS; ++index)
            write_if_bound(peripheral_enable[index],
                           peripheral_enabled(index));
    }
};

constexpr std::array<hsoc_pinctrl::PeripheralGroup,
                     hsoc_pinctrl::NUM_PERIPHERALS>
    hsoc_pinctrl::DEFAULT_PERIPHERAL_GROUPS;

extern "C" void module_register();
