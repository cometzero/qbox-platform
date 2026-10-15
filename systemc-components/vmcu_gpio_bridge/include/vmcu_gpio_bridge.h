/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <async_event.h>
#include <cci_configuration>
#include <module_factory_registery.h>
#include <ports/biflow-socket.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <systemc>

/* QVP board wires over an independent host transport, not a TC397 UART.
 * GP, version 1, direction I/O, LE16 levels, LE16 drive/valid mask.
 * I: SoC pins 0..2; O: MCU pins 3..4. RESET_N has a board pull-up.
 * Repeated snapshots restore state after reconnect. A host-time lease releases
 * MCU drive after transport loss; it is not a physical disconnect/timing model.
 */
class vmcu_gpio_bridge : public sc_core::sc_module
{
    using Clock = std::chrono::steady_clock;
    static constexpr uint16_t MCU_MASK = 0x18;
    static constexpr uint16_t RESET_N = 0x08;
    std::array<uint8_t, 8> m_frame{};
    size_t m_used = 0;
    uint16_t m_soc_levels = 0x04; // IST_DONE_N inactive until SCP configures it.
    uint16_t m_peer_levels = 0;
    uint16_t m_peer_drive = 0;
    Clock::time_point m_last_rx{};
    Clock::time_point m_last_byte{};
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_thread;
    bool m_stop = false;
    gs::async_event m_update{false};

    void stop_thread()
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_stop = true;
        }
        m_condition.notify_all();
        if (m_thread.joinable()) m_thread.join();
    }

    void host_ticks()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_condition.wait_for(lock,
                   std::chrono::milliseconds(p_publish_interval_ms.get_value()),
                   [this] { return m_stop; })) {
            lock.unlock();
            m_update.notify();
            lock.lock();
        }
    }

    void update()
    {
        uint16_t soc, peer;
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            if (Clock::now() - m_last_rx >=
                std::chrono::milliseconds(p_peer_timeout_ms.get_value())) {
                m_peer_drive = 0;
            }
            // Do not carry a truncated, idle connection frame into reconnect.
            if (Clock::now() - m_last_byte >=
                std::chrono::milliseconds(p_peer_timeout_ms.get_value())) m_used = 0;
            peer = (m_peer_levels & m_peer_drive) | (RESET_N & ~m_peer_drive);
            soc = m_soc_levels;
        }
        for (unsigned pin = 3; pin < 5; ++pin) {
            if (gpio_out[pin].size()) gpio_out[pin]->write((peer & (1u << pin)) != 0);
        }
        if (reset_request.size()) reset_request->write((peer & RESET_N) == 0);
        std::array<uint8_t, 8> bytes = {{'G', 'P', 1, 'I',
            static_cast<uint8_t>(soc), 0, static_cast<uint8_t>(p_soc_mask.get_value()), 0}};
        tlm::tlm_generic_payload trans;
        trans.set_data_ptr(bytes.data());
        trans.set_data_length(bytes.size());
        trans.set_streaming_width(bytes.size());
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        backend_socket.force_send(trans);
    }

public:
    SC_HAS_PROCESS(vmcu_gpio_bridge);
    cci::cci_param<unsigned> p_soc_mask;
    cci::cci_param<unsigned> p_publish_interval_ms;
    cci::cci_param<unsigned> p_peer_timeout_ms;
    gs::biflow_socket<vmcu_gpio_bridge> backend_socket;
    sc_core::sc_vector<TargetSignalSocket<bool>> gpio_in;
    sc_core::sc_vector<InitiatorSignalSocket<bool>> gpio_out;
    InitiatorSignalSocket<bool> reset_request;

    explicit vmcu_gpio_bridge(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , p_soc_mask("soc_mask", 0x07)
        , p_publish_interval_ms("publish_interval_ms", 100)
        , p_peer_timeout_ms("peer_timeout_ms", 1000)
        , backend_socket("backend_socket")
        , gpio_in("gpio_in", 6, [](const char* n, size_t) { return new TargetSignalSocket<bool>(n); })
        , gpio_out("gpio_out", 5)
        , reset_request("reset_request")
    {
        if (!p_publish_interval_ms.get_value() ||
            p_peer_timeout_ms.get_value() <= p_publish_interval_ms.get_value()) {
            throw std::invalid_argument("GPIO peer timeout must exceed nonzero publish interval");
        }
        backend_socket.register_b_transport(this, &vmcu_gpio_bridge::receive);
        for (unsigned pin = 0; pin < gpio_in.size(); ++pin) {
            gpio_in[pin].register_value_changed_cb([this, pin](bool value) {
                {
                    std::lock_guard<std::mutex> guard(m_mutex);
                    if (value) m_soc_levels |= 1u << pin;
                    else m_soc_levels &= ~(1u << pin);
                }
                m_update.notify();
            });
        }
        SC_METHOD(update);
        sensitive << m_update;
    }

    ~vmcu_gpio_bridge() override { stop_thread(); }
    void end_of_elaboration() override { backend_socket.can_receive_any(); }
    void start_of_simulation() override { m_thread = std::thread(&vmcu_gpio_bridge::host_ticks, this); }
    void end_of_simulation() override { stop_thread(); }

    void receive(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        const uint8_t* bytes = trans.get_data_ptr();
        bool accepted = false;
        std::lock_guard<std::mutex> guard(m_mutex);
        if (trans.get_data_length()) m_last_byte = Clock::now();
        for (unsigned i = 0; i < trans.get_data_length(); ++i) {
            m_frame[m_used++] = bytes[i];
            if (m_used != m_frame.size()) continue;
            const uint16_t drive = m_frame[6] | (uint16_t(m_frame[7]) << 8);
            if (m_frame[0] == 'G' && m_frame[1] == 'P' && m_frame[2] == 1 &&
                m_frame[3] == 'O' && !(drive & ~MCU_MASK)) {
                m_peer_levels = m_frame[4] | (uint16_t(m_frame[5]) << 8);
                m_peer_drive = drive;
                m_last_rx = Clock::now();
                m_used = 0;
                accepted = true;
            } else {
                for (size_t j = 1; j < m_frame.size(); ++j) m_frame[j - 1] = m_frame[j];
                --m_used;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        if (accepted) m_update.notify();
    }
};

extern "C" void module_register();
