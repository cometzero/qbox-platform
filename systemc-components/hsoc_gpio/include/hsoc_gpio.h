/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

#include <async_event.h>
#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>
#include <tlm>
#include <tlm_sockets_buswidth.h>
#include <tlm_utils/simple_target_socket.h>

class hsoc_gpio : public sc_core::sc_module
{
public:
    static constexpr unsigned int MAX_BANKS = 14;
    static constexpr unsigned int PINS_PER_BANK = 8;
    static constexpr unsigned int NUM_PIN_SLOTS = MAX_BANKS * PINS_PER_BANK;
    static constexpr unsigned int NUM_PERIPHERALS = 16;
    static constexpr uint32_t BANK_STRIDE = 0x1000;

    enum BankRegister : uint32_t {
        PROT = 0x000,
        SEL = 0x100,
        DAT = 0x104,
        PS = 0x108,
        PE = 0x10c,
        DS = 0x110,
        IS = 0x114,
        IE = 0x118,
        INTR_CON = 0x200,
        INTR_PEND = 0x204,
        INTR_MIRR_PEND = 0x208,
        INTR_MASK = 0x20c,
        INTR_FLT_TYP = 0x210,
        INTR_FLT_DEPTH = 0x214,
    };

    cci::cci_param<std::string> p_bank_sizes;
    cci::cci_param<std::string> p_peripheral_routes;
    cci::cci_param<sc_core::sc_time> p_access_latency;
    cci::cci_param<sc_core::sc_time> p_filter_clock_period;

    tlm_utils::simple_target_socket<hsoc_gpio, DEFAULT_TLM_BUSWIDTH>
        target_socket;
    TargetSignalSocket<bool> reset;
    sc_core::sc_vector<TargetSignalSocket<bool>> gpio_in;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> gpio_out;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> gpio_oe;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> irq;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> peripheral_enable;

    SC_HAS_PROCESS(hsoc_gpio);

    explicit hsoc_gpio(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_bank_sizes("bank_sizes", "8,8,8,8,8,8,1,1,1,1,1,1,1,1")
        , p_peripheral_routes("peripheral_routes", "")
        , p_access_latency("access_latency",
                           sc_core::sc_time(10, sc_core::SC_NS))
        , p_filter_clock_period("filter_clock_period",
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
        , m_filter_event(false)
        , m_level_recheck_event(false)
    {
        parse_bank_sizes();
        parse_peripheral_routes();
        if (p_filter_clock_period.get_value() <= sc_core::SC_ZERO_TIME)
            throw std::invalid_argument(
                "hsoc_gpio filter_clock_period must be positive");
        target_socket.register_b_transport(this, &hsoc_gpio::b_transport);
        reset.register_value_changed_cb(
            [this](bool asserted) { set_reset(asserted); });
        for (unsigned int slot = 0; slot < NUM_PIN_SLOTS; ++slot) {
            gpio_in[slot].register_value_changed_cb(
                [this, slot](bool level) { set_gpio_input(slot, level); });
        }

        SC_METHOD(drive_outputs);
        sensitive << m_update_event;
        SC_METHOD(recheck_level_interrupts);
        sensitive << m_level_recheck_event;
        dont_initialize();
        SC_THREAD(filter_inputs);
        m_filter_event.notify(sc_core::SC_ZERO_TIME);
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
    enum InterruptMode : unsigned int {
        HIGH_LEVEL = 0,
        LOW_LEVEL = 1,
        FALLING_EDGE = 2,
        RISING_EDGE = 3,
        BOTH_EDGES = 4,
    };

    struct Bank {
        uint32_t sel = 0;
        uint32_t data = 0;
        uint32_t pull_select = 0;
        uint32_t pull_enable = 0;
        uint32_t drive_strength = 0;
        uint32_t schmitt_enable = 0;
        uint32_t input_enable = 0;
        uint32_t interrupt_control = 0;
        uint32_t interrupt_pending = 0;
        uint32_t interrupt_mask = 0;
        uint32_t filter_type = 0;
        uint32_t filter_depth = 0;
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
    std::array<Bank, MAX_BANKS> m_banks{};
    std::array<bool, NUM_PIN_SLOTS> m_raw_input_levels{};
    std::array<bool, NUM_PIN_SLOTS> m_external_input_written{};
    std::array<bool, NUM_PIN_SLOTS> m_observed_input_levels{};
    std::array<bool, NUM_PIN_SLOTS> m_filtered_input_levels{};
    std::array<bool, NUM_PIN_SLOTS> m_filter_candidates{};
    std::array<uint8_t, NUM_PIN_SLOTS> m_filter_samples{};
    std::array<bool, NUM_PIN_SLOTS> m_filter_active{};
    std::array<sc_core::sc_time, NUM_PIN_SLOTS> m_filter_due{};
    mutable std::mutex m_input_mutex;
    bool m_reset_asserted = false;
    gs::async_event m_update_event;
    gs::async_event m_filter_event;
    gs::async_event m_level_recheck_event;

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

    [[noreturn]] static void invalid_bank_sizes()
    {
        throw std::invalid_argument("invalid hsoc_gpio bank_sizes");
    }

    void parse_bank_sizes()
    {
        const std::string specification = p_bank_sizes.get_value();
        if (specification.empty() || specification.front() == ',' ||
            specification.back() == ',')
            invalid_bank_sizes();
        std::stringstream list(specification);
        std::string entry;
        while (std::getline(list, entry, ',')) {
            if (entry.empty() || m_num_banks == MAX_BANKS)
                invalid_bank_sizes();
            std::size_t consumed = 0;
            unsigned long size = 0;
            try {
                size = std::stoul(entry, &consumed, 10);
            } catch (const std::exception&) {
                invalid_bank_sizes();
            }
            if (consumed != entry.size() || size == 0 ||
                size > PINS_PER_BANK)
                invalid_bank_sizes();
            m_bank_sizes[m_num_banks++] = static_cast<uint8_t>(size);
        }
        if (m_num_banks == 0) invalid_bank_sizes();
        reset_registers();
    }

    [[noreturn]] static void invalid_peripheral_routes()
    {
        throw std::invalid_argument("invalid hsoc_gpio peripheral_routes");
    }

    static unsigned long parse_route_number(const std::string& text)
    {
        if (text.empty()) invalid_peripheral_routes();
        std::size_t consumed = 0;
        unsigned long value = 0;
        try {
            value = std::stoul(text, &consumed, 10);
        } catch (const std::exception&) {
            invalid_peripheral_routes();
        }
        if (consumed != text.size()) invalid_peripheral_routes();
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
            invalid_peripheral_routes();

        std::array<bool, NUM_PERIPHERALS> assigned{};
        std::stringstream routes(specification);
        std::string route;
        while (std::getline(routes, route, ';')) {
            if (route.empty() || route.front() == ':' || route.back() == ':')
                invalid_peripheral_routes();
            std::array<unsigned long, 4> fields{};
            std::stringstream values(route);
            std::string field;
            unsigned int count = 0;
            while (std::getline(values, field, ':')) {
                if (count == fields.size()) invalid_peripheral_routes();
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
                invalid_peripheral_routes();
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

    uint32_t packed_mask(unsigned int bank, unsigned int width) const
    {
        const unsigned int bits = m_bank_sizes[bank] * width;
        return bits == 32 ? 0xffffffffu : (1u << bits) - 1u;
    }

    bool valid_pin(unsigned int slot) const
    {
        const unsigned int bank = slot / PINS_PER_BANK;
        return bank < m_num_banks &&
               slot % PINS_PER_BANK < m_bank_sizes[bank];
    }

    unsigned int mux(unsigned int slot) const
    {
        const unsigned int bank = slot / PINS_PER_BANK;
        const unsigned int pin = slot % PINS_PER_BANK;
        return (m_banks[bank].sel >> (pin * 4)) & 0xfu;
    }

    unsigned int interrupt_mode(unsigned int bank, unsigned int pin) const
    {
        return (m_banks[bank].interrupt_control >> (pin * 4)) & 0xfu;
    }

    unsigned int filter_depth(unsigned int bank, unsigned int pin) const
    {
        return (m_banks[bank].filter_depth >> (pin * 4)) & 0xfu;
    }

    bool input_enabled(unsigned int bank, unsigned int pin) const
    {
        return (m_banks[bank].input_enable & (1u << pin)) != 0;
    }

    bool effective_input_level(unsigned int slot) const
    {
        const unsigned int bank = slot / PINS_PER_BANK;
        const unsigned int pin = slot % PINS_PER_BANK;
        bool level = false;
        bool externally_driven = false;
        {
            std::lock_guard<std::mutex> lock(m_input_mutex);
            level = m_raw_input_levels[slot];
            externally_driven = m_external_input_written[slot];
        }
        if (!externally_driven &&
            (m_banks[bank].pull_enable & (1u << pin)) == 0)
            level = (m_banks[bank].pull_select & (1u << pin)) != 0;
        return level;
    }

    uint32_t data_value(unsigned int bank) const
    {
        uint32_t value = 0;
        for (unsigned int pin = 0; pin < m_bank_sizes[bank]; ++pin) {
            const uint32_t bit = 1u << pin;
            const unsigned int slot = bank * PINS_PER_BANK + pin;
            const bool level = mux(slot) == 1
                ? (m_banks[bank].data & bit) != 0
                : input_enabled(bank, pin) && effective_input_level(slot);
            if (level) value |= bit;
        }
        return value;
    }

    bool decode_bank(uint32_t offset, unsigned int& bank,
                     uint32_t& bank_offset) const
    {
        bank = offset / BANK_STRIDE;
        bank_offset = offset % BANK_STRIDE;
        return bank < m_num_banks;
    }

    bool read_register(uint32_t offset, uint32_t& value) const
    {
        unsigned int bank = 0;
        uint32_t reg = 0;
        if (!decode_bank(offset, bank, reg)) return false;
        const Bank& state = m_banks[bank];
        switch (reg) {
        case PROT: value = 0; return true;
        case SEL: value = state.sel; return true;
        case DAT: value = data_value(bank); return true;
        case PS: value = state.pull_select; return true;
        case PE: value = state.pull_enable; return true;
        case DS: value = state.drive_strength; return true;
        case IS: value = state.schmitt_enable; return true;
        case IE: value = state.input_enable; return true;
        case INTR_CON: value = state.interrupt_control; return true;
        case INTR_PEND: value = state.interrupt_pending; return true;
        case INTR_MIRR_PEND: value = state.interrupt_pending; return true;
        case INTR_MASK: value = state.interrupt_mask; return true;
        case INTR_FLT_TYP: value = state.filter_type; return true;
        case INTR_FLT_DEPTH: value = state.filter_depth; return true;
        default: return false;
        }
    }

    static bool valid_interrupt_control(uint32_t value, unsigned int pins)
    {
        for (unsigned int pin = 0; pin < pins; ++pin) {
            if (((value >> (pin * 4)) & 0xfu) > BOTH_EDGES) return false;
        }
        return true;
    }

    bool write_register(uint32_t offset, uint32_t value)
    {
        unsigned int bank = 0;
        uint32_t reg = 0;
        if (!decode_bank(offset, bank, reg)) return false;
        if (m_reset_asserted) return true;
        Bank& state = m_banks[bank];
        const uint32_t mask = valid_mask(bank);
        bool restart_filters = false;
        bool recheck_levels = false;
        switch (reg) {
        case PROT: break;
        case SEL:
            state.sel = value & packed_mask(bank, 4);
            restart_filters = true;
            recheck_levels = true;
            break;
        case DAT:
            state.data = value & mask;
            break;
        case PS:
            state.pull_select = value & mask;
            restart_filters = true;
            break;
        case PE:
            state.pull_enable = value & mask;
            restart_filters = true;
            break;
        case DS:
            state.drive_strength = value & packed_mask(bank, 2);
            break;
        case IS:
            state.schmitt_enable = value & mask;
            break;
        case IE:
            state.input_enable = value & mask;
            restart_filters = true;
            recheck_levels = true;
            break;
        case INTR_CON:
            if (!valid_interrupt_control(value, m_bank_sizes[bank]))
                return false;
            state.interrupt_control = value & packed_mask(bank, 4);
            recheck_levels = true;
            break;
        case INTR_PEND:
            state.interrupt_pending &= ~(value & mask);
            m_level_recheck_event.notify(sc_core::SC_ZERO_TIME);
            break;
        case INTR_MIRR_PEND:
            return false;
        case INTR_MASK:
            state.interrupt_mask = value & mask;
            break;
        case INTR_FLT_TYP:
            state.filter_type = value & mask;
            restart_filters = true;
            break;
        case INTR_FLT_DEPTH:
            state.filter_depth = value & packed_mask(bank, 4);
            restart_filters = true;
            break;
        default: return false;
        }
        if (restart_filters) restart_bank_filters(bank);
        if (recheck_levels)
            m_level_recheck_event.notify(sc_core::SC_ZERO_TIME);
        m_update_event.notify(sc_core::SC_ZERO_TIME);
        return true;
    }

    void set_gpio_input(unsigned int slot, bool level)
    {
        if (!valid_pin(slot)) return;
        {
            std::lock_guard<std::mutex> lock(m_input_mutex);
            m_raw_input_levels[slot] = level;
            m_external_input_written[slot] = true;
        }
        m_filter_event.notify(sc_core::SC_ZERO_TIME);
    }

    void set_reset(bool asserted)
    {
        if (asserted && !m_reset_asserted) reset_registers();
        m_reset_asserted = asserted;
        if (!asserted) restart_all_filters();
    }

    void reset_registers()
    {
        m_banks = {};
        for (unsigned int bank = 0; bank < m_num_banks; ++bank) {
            m_banks[bank].pull_enable = valid_mask(bank);
            m_banks[bank].interrupt_mask = valid_mask(bank);
        }
        m_observed_input_levels.fill(false);
        m_filtered_input_levels.fill(false);
        m_filter_candidates.fill(false);
        m_filter_samples.fill(0);
        m_filter_active.fill(false);
        m_update_event.notify(sc_core::SC_ZERO_TIME);
        m_filter_event.notify(sc_core::SC_ZERO_TIME);
    }

    void restart_bank_filters(unsigned int bank)
    {
        for (unsigned int pin = 0; pin < m_bank_sizes[bank]; ++pin) {
            const unsigned int slot = bank * PINS_PER_BANK + pin;
            m_filter_active[slot] = false;
            m_observed_input_levels[slot] = m_filtered_input_levels[slot];
        }
        m_filter_event.notify(sc_core::SC_ZERO_TIME);
    }

    void restart_all_filters()
    {
        m_filter_active.fill(false);
        m_filter_event.notify(sc_core::SC_ZERO_TIME);
    }

    void start_filter(unsigned int slot, bool candidate,
                      const sc_core::sc_time& now)
    {
        const unsigned int bank = slot / PINS_PER_BANK;
        const unsigned int pin = slot % PINS_PER_BANK;
        const unsigned int depth = filter_depth(bank, pin);
        m_filter_candidates[slot] = candidate;
        m_filter_samples[slot] = 0;
        if (depth == 0) {
            m_filter_active[slot] = false;
            accept_input(slot, candidate);
            return;
        }
        m_filter_active[slot] = true;
        const sc_core::sc_time period = p_filter_clock_period.get_value();
        const bool time_filter = (m_banks[bank].filter_type & (1u << pin)) != 0;
        if (time_filter) {
            m_filter_candidates[slot] = candidate;
            m_filter_due[slot] = now + period * depth;
        } else {
            const uint64_t period_ticks = period.value();
            const uint64_t now_ticks = now.value();
            m_filter_due[slot] = sc_core::sc_time::from_value(
                (now_ticks / period_ticks + 1) * period_ticks);
        }
    }

    void accept_input(unsigned int slot, bool level)
    {
        const bool old = m_filtered_input_levels[slot];
        m_filtered_input_levels[slot] = level;
        if (old == level) return;

        const unsigned int bank = slot / PINS_PER_BANK;
        const unsigned int pin = slot % PINS_PER_BANK;
        const uint32_t bit = 1u << pin;
        if (mux(slot) == 0 && input_enabled(bank, pin)) {
            const unsigned int mode = interrupt_mode(bank, pin);
            if ((!old && level &&
                 (mode == RISING_EDGE || mode == BOTH_EDGES)) ||
                (old && !level &&
                 (mode == FALLING_EDGE || mode == BOTH_EDGES)))
                m_banks[bank].interrupt_pending |= bit;
        }
        latch_level_interrupts(bank);
        m_update_event.notify(sc_core::SC_ZERO_TIME);
    }

    void filter_inputs()
    {
        while (true) {
            wait(m_filter_event);
            while (true) {
                const sc_core::sc_time now = sc_core::sc_time_stamp();
                for (unsigned int slot = 0; slot < NUM_PIN_SLOTS; ++slot) {
                    if (!valid_pin(slot)) continue;
                    const bool level = effective_input_level(slot);
                    if (!m_filter_active[slot] &&
                        m_observed_input_levels[slot] == level)
                        continue;
                    if (m_observed_input_levels[slot] != level) {
                        m_observed_input_levels[slot] = level;
                        const unsigned int bank = slot / PINS_PER_BANK;
                        const unsigned int pin = slot % PINS_PER_BANK;
                        const bool logic_filter =
                            filter_depth(bank, pin) != 0 &&
                            (m_banks[bank].filter_type & (1u << pin)) == 0;
                        if (!logic_filter || !m_filter_active[slot])
                            start_filter(slot, level, now);
                    }
                }

                bool have_due = false;
                sc_core::sc_time earliest = sc_core::SC_ZERO_TIME;
                for (unsigned int slot = 0; slot < NUM_PIN_SLOTS; ++slot) {
                    if (!m_filter_active[slot]) continue;
                    if (m_filter_due[slot] <= now) {
                        const unsigned int bank = slot / PINS_PER_BANK;
                        const unsigned int pin = slot % PINS_PER_BANK;
                        const unsigned int depth = filter_depth(bank, pin);
                        const bool level = effective_input_level(slot);
                        const bool time_filter =
                            (m_banks[bank].filter_type & (1u << pin)) != 0;
                        if (time_filter) {
                            if (level != m_filter_candidates[slot]) {
                                start_filter(slot, level, now);
                            } else {
                                m_filter_active[slot] = false;
                                accept_input(slot, level);
                            }
                        } else {
                            if (m_filter_samples[slot] == 0 ||
                                level != m_filter_candidates[slot]) {
                                m_filter_candidates[slot] = level;
                                m_filter_samples[slot] = 1;
                            } else {
                                ++m_filter_samples[slot];
                            }
                            if (m_filter_samples[slot] >= depth) {
                                m_filter_active[slot] = false;
                                accept_input(slot, level);
                            } else {
                                const uint64_t period_ticks =
                                    p_filter_clock_period.get_value().value();
                                m_filter_due[slot] =
                                    sc_core::sc_time::from_value(
                                        (now.value() / period_ticks + 1) *
                                        period_ticks);
                            }
                        }
                    }
                    if (m_filter_active[slot] &&
                        (!have_due || m_filter_due[slot] < earliest)) {
                        earliest = m_filter_due[slot];
                        have_due = true;
                    }
                }
                if (!have_due) break;
                wait(earliest - sc_core::sc_time_stamp(), m_filter_event);
            }
        }
    }

    void latch_level_interrupts(unsigned int bank)
    {
        for (unsigned int pin = 0; pin < m_bank_sizes[bank]; ++pin) {
            if (!input_enabled(bank, pin) ||
                mux(bank * PINS_PER_BANK + pin) != 0)
                continue;
            const bool level = m_filtered_input_levels[
                bank * PINS_PER_BANK + pin];
            const unsigned int mode = interrupt_mode(bank, pin);
            if ((mode == HIGH_LEVEL && level) ||
                (mode == LOW_LEVEL && !level))
                m_banks[bank].interrupt_pending |= 1u << pin;
        }
    }

    void recheck_level_interrupts()
    {
        for (unsigned int bank = 0; bank < m_num_banks; ++bank)
            latch_level_interrupts(bank);
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
                output_enable = !m_reset_asserted && mux(slot) == 1;
                value = output_enable
                    ? (m_banks[bank].data & (1u << pin)) != 0
                    : input_enabled(bank, pin) &&
                      effective_input_level(slot);
            }
            write_if_bound(gpio_out[slot], value);
            write_if_bound(gpio_oe[slot], output_enable);
        }
        for (unsigned int bank = 0; bank < MAX_BANKS; ++bank) {
            const bool asserted = !m_reset_asserted && bank < m_num_banks &&
                (m_banks[bank].interrupt_pending &
                 ~m_banks[bank].interrupt_mask & valid_mask(bank)) != 0;
            write_if_bound(irq[bank], asserted);
        }
        for (unsigned int index = 0; index < NUM_PERIPHERALS; ++index)
            write_if_bound(peripheral_enable[index],
                           !m_reset_asserted && peripheral_enabled(index));
    }
};

constexpr std::array<hsoc_gpio::PeripheralGroup, hsoc_gpio::NUM_PERIPHERALS>
    hsoc_gpio::DEFAULT_PERIPHERAL_GROUPS;

extern "C" void module_register();
