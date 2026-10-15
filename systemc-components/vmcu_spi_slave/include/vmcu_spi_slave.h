/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include <array>
#include <deque>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <async_event.h>
#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/biflow-socket.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <tlm_utils/simple_target_socket.h>

// QVP functional peripheral: this is not an RD-Aspen silicon register map.
class vmcu_spi_slave : public sc_core::sc_module
{
    using Record = std::array<uint8_t, 64>;
    std::deque<uint8_t> tx, rx;
    std::deque<Record> ingress;
    Record partial{};
    size_t used = 0;
    unsigned committed = 0, cs_budget = 0;
    std::mutex mutex;
    gs::async_event incoming{ false };
    bool ingress_overflow = false, enabled = false, selected = false, ready = false;
    uint64_t generation = 0;
    uint32_t last_id = 0, errors = 0, mask = 0, sticky = 0;
    static uint32_t get32(const uint8_t* p)
    {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }
    static void put32(uint8_t* p, uint32_t v)
    {
        for (unsigned i = 0; i < 4; i++) p[i] = v >> (8 * i);
    }
    static uint64_t get64(const uint8_t* p) { return get32(p) | uint64_t(get32(p + 4)) << 32; }
    uint32_t irq_status() const { return (!rx.empty() ? 1 : 0) | (tx.empty() ? 2 : 0) | (errors ? 4 : 0) | sticky; }
    void update_irq()
    {
        if (irq.size()) irq->write((irq_status() & mask) != 0);
    }
    void clear_fifos()
    {
        tx.clear();
        rx.clear();
        committed = cs_budget = 0;
        selected = false;
        errors = sticky = 0;
    }
    void process()
    {
        std::deque<Record> work;
        {
            std::lock_guard<std::mutex> lock(mutex);
            work.swap(ingress);
            if (ingress_overflow) {
                errors |= 4;
                ingress_overflow = false;
            }
        }
        for (auto r : work) {
            const uint8_t type = r[4];
            const uint64_t gen = get64(&r[8]);
            const uint32_t id = get32(&r[16]);
            uint8_t result = 0;
            if (type == 1 && gen && !r[5] && !r[6] && !r[7]) {
                if (!ready || generation != gen) {
                    generation = gen;
                    last_id = 0;
                    selected = false;
                }
                ready = true;
                r[4] = 2;
            } else if (type == 5 && ready && gen == generation) {
                selected = false;
                ready = false;
                continue;
            } else if (type == 3) {
                if (!ready || gen != generation) continue;
                r[4] = 4;
                const unsigned n = r[7], flags = r[6];
                if (!ready || gen != generation || !id || id <= last_id || r[5] || !n || n > 32 || flags & ~3u ||
                    r[24] || r[25] || r[26] != 8 || r[27] || !get32(&r[20]) || ((flags & 1) ? selected : !selected)) {
                    result = 1;
                    errors |= 4;
                } else if (!enabled) {
                    result = 2;
                    last_id = id;
                } else {
                    if (flags & 1) cs_budget = committed;
                    selected = true;
                    last_id = id;
                    for (unsigned i = 0; i < n; i++) {
                        uint8_t mosi = r[28 + i];
                        r[28 + i] = 0xff;
                        if (!cs_budget) {
                            errors |= 1;
                            if (result != 4) result = 3;
                        } else {
                            r[28 + i] = tx.front();
                            tx.pop_front();
                            --cs_budget;
                            --committed;
                        }
                        if (rx.size() == 64) {
                            errors |= 2;
                            result = 4;
                        } else
                            rx.push_back(mosi);
                    }
                    if (flags & 2) {
                        selected = false;
                        sticky |= 8;
                    }
                }
            } else {
                errors |= 4;
                continue;
            }
            r[5] = result;
            put32(&r[60], crc(r.data(), 60));
            tlm::tlm_generic_payload t;
            t.set_data_ptr(r.data());
            t.set_data_length(64);
            t.set_streaming_width(64);
            t.set_command(tlm::TLM_WRITE_COMMAND);
            backend_socket.force_send(t);
        }
        update_irq();
    }

public:
    SC_HAS_PROCESS(vmcu_spi_slave);
    cci::cci_param<bool> connected;
    tlm_utils::simple_target_socket<vmcu_spi_slave, DEFAULT_TLM_BUSWIDTH> target_socket;
    gs::biflow_socket<vmcu_spi_slave> backend_socket;
    InitiatorSignalSocket<bool> irq;
    TargetSignalSocket<bool> reset;
    explicit vmcu_spi_slave(sc_core::sc_module_name n)
        : sc_module(n)
        , connected("connected", false)
        , target_socket("target_socket")
        , backend_socket("backend_socket")
        , irq("irq")
        , reset("reset")
    {
        target_socket.register_b_transport(this, &vmcu_spi_slave::b_transport);
        backend_socket.register_b_transport(this, &vmcu_spi_slave::receive);
        reset.register_value_changed_cb([this](bool value) {
            if (value) {
                if (ready) {
                    Record cancel{};
                    std::memcpy(cancel.data(), "SPI1", 4);
                    cancel[4] = 5;
                    cancel[5] = 1;
                    put32(&cancel[8], generation);
                    put32(&cancel[12], generation >> 32);
                    put32(&cancel[16], last_id);
                    put32(&cancel[60], crc(cancel.data(), 60));
                    tlm::tlm_generic_payload t;
                    t.set_data_ptr(cancel.data());
                    t.set_data_length(cancel.size());
                    t.set_streaming_width(cancel.size());
                    t.set_command(tlm::TLM_WRITE_COMMAND);
                    // Preserve a partly sent COMPLETE before the ordered reset fence.
                    backend_socket.force_send(t);
                }
                clear_fifos();
                enabled = false;
                mask = 0;
                ready = false;
                update_irq();
                std::lock_guard<std::mutex> lock(mutex);
                ingress.clear();
                ingress_overflow = false;
                used = 0;
            }
        });
        SC_METHOD(process);
        sensitive << incoming;
        dont_initialize();
    }
    void end_of_elaboration() override { backend_socket.can_receive_any(); }
    static uint32_t crc(const uint8_t* p, size_t n)
    {
        uint32_t v = ~0u;
        while (n--) {
            v ^= *p++;
            for (unsigned i = 0; i < 8; i++) v = (v >> 1) ^ (0xedb88320u & (0u - (v & 1)));
        }
        return ~v;
    }
    void receive(tlm::tlm_generic_payload& t, sc_core::sc_time&)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (unsigned i = 0; i < t.get_data_length(); i++) {
            partial[used++] = t.get_data_ptr()[i];
            if (used != 64) continue;
            if (!std::memcmp(partial.data(), "SPI1", 4) && get32(&partial[60]) == crc(partial.data(), 60)) {
                if (ingress.size() < 4)
                    ingress.push_back(partial);
                else
                    ingress_overflow = true;
                used = 0;
                incoming.notify();
            } else {
                std::move(partial.begin() + 1, partial.end(), partial.begin());
                used = 63;
            }
        }
        t.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time&)
    {
        if (t.get_data_length() != 4 || t.get_streaming_width() < 4 || t.get_byte_enable_ptr() || !t.get_data_ptr() ||
            t.get_address() % 4 || t.get_address() > 0x28) {
            t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        uint32_t v = get32(t.get_data_ptr());
        const unsigned a = t.get_address();
        if (t.is_write()) {
            switch (a) {
            case 4:
                if (v & 2) clear_fifos();
                enabled = v & 1;
                break;
            case 0x14:
                if (tx.size() == 64)
                    errors |= 2;
                else
                    tx.push_back(v);
                break;
            case 0x18:
                sticky &= ~v;
                break;
            case 0x1c:
                mask = v & 15;
                break;
            case 0x20:
                errors &= ~v;
                break;
            case 0x24:
                if (!v || v != tx.size() || committed) errors |= 4;
                else committed = v;
                break;
            default:
                break;
            }
        } else if (t.is_read()) {
            switch (a) {
            case 0:
                v = 0x53504931;
                break;
            case 4:
                v = enabled;
                break;
            case 8:
                v = (tx.size() < 64 ? 1 : 0) | (!rx.empty() ? 2 : 0) | (selected ? 4 : 0);
                break;
            case 0xc:
                v = tx.size();
                break;
            case 0x10:
                v = rx.size();
                break;
            case 0x14:
                v = rx.empty() ? 0 : rx.front();
                if (!rx.empty()) rx.pop_front();
                break;
            case 0x18:
                v = irq_status();
                break;
            case 0x1c:
                v = mask;
                break;
            case 0x20:
                v = errors;
                break;
            case 0x24:
                v = committed;
                break;
            case 0x28:
                v = connected.get_value();
                break;
            default:
                v = 0;
            }
            put32(t.get_data_ptr(), v);
        } else {
            t.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        update_irq();
        t.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};
extern "C" void module_register();
