/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <vector>
#include <libgssync.h>
#include <cci_configuration>
#include <device.h>
#include <module_factory_registery.h>
#include <ports/qemu-initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <ports/target.h>

// DMA transactions use the instance's global_peripheral_initiator bridge.
class qemu_dma350 : public QemuDevice
{
    gs::runonsysc m_on_sysc;
    std::vector<qemu::Gpio> m_ack;

public:
    cci::cci_param<unsigned int> p_channel_count;
    cci::cci_param<unsigned int> p_trigger_count;
    QemuTargetSocket<> target_socket;
    sc_core::sc_vector<QemuInitiatorSignalSocket> irq;
    QemuInitiatorSignalSocket irq_comb_nonsec;
    TargetSignalSocket<bool> reset;
    sc_core::sc_vector<TargetSignalSocket<uint32_t>> trig_in;
    sc_core::sc_vector<InitiatorSignalSocket<uint32_t>> trig_ack;

    qemu_dma350(sc_core::sc_module_name name, sc_core::sc_object* inst)
        : QemuDevice(name, *dynamic_cast<QemuInstance*>(inst), "arm-dma350")
        , p_channel_count("channel_count", 8, "Number of DMA channels")
        , p_trigger_count("trigger_count", 8, "Number of peripheral triggers")
        , target_socket("target_socket", m_inst)
        , irq("irq", 8, [](const char* n, size_t) { return new QemuInitiatorSignalSocket(n); })
        , irq_comb_nonsec("irq_comb_nonsec")
        , reset("reset")
        , trig_in("trig_in", p_trigger_count.get_value())
        , trig_ack("trig_ack", p_trigger_count.get_value())
    {
        reset.register_value_changed_cb([this](bool asserted) {
            if (!asserted || !m_realized) { return; }
            auto& qemu = m_inst.get();
            const bool locked = qemu.iothread_locked();
            if (!locked) { qemu.lock_iothread(); }
            try { cold_reset(); } catch (...) {
                if (!locked) { qemu.unlock_iothread(); }
                throw;
            }
            if (!locked) { qemu.unlock_iothread(); }
        });
    }

    ~qemu_dma350() override
    {
        for (auto& ack : m_ack) { ack.set_level_event_callback(nullptr); }
        for (auto& input : trig_in) { input.register_value_changed_cb(nullptr); }
    }

    void before_end_of_elaboration() override
    {
        QemuDevice::before_end_of_elaboration();
        m_dev.set_prop_uint("channel-count", p_channel_count.get_value());
        m_dev.set_prop_uint("trigger-count", p_trigger_count.get_value());
    }

    void end_of_simulation() override
    {
        for (auto& ack : m_ack) { ack.set_level_event_callback(nullptr); }
    }

    void end_of_elaboration() override
    {
        set_sysbus_as_parent_bus();
        QemuDevice::end_of_elaboration();
        qemu::SysBusDevice sbd(m_dev);
        target_socket.init(sbd, 0);
        for (unsigned i = 0; i < irq.size(); ++i) { irq[i].init_sbd(sbd, i); }
        irq_comb_nonsec.init_sbd(sbd, 8);
        for (unsigned i = 0; i < trig_in.size(); ++i) {
            auto gpio = m_dev.get_gpio_in_named("dma-req", i);
            trig_in[i].register_value_changed_cb([gpio](uint32_t value) mutable {
                auto& qemu = gpio.get_inst();
                const bool locked = qemu.iothread_locked();
                if (!locked) { qemu.lock_iothread(); }
                gpio.set_level(static_cast<int>(value));
                if (!locked) { qemu.unlock_iothread(); }
            });
            // Native peripherals wire their ACKs directly; only export bound
            // SystemC consumers (SPI/UART) through the asynchronous proxy.
            if (!trig_ack[i].get_interface()) { continue; }
            auto ack = m_inst.get().gpio_new();
            ack.set_level_event_callback([this, i](int value) {
                auto& qemu = m_inst.get();
                const bool locked = qemu.iothread_locked();
                if (locked) { qemu.unlock_iothread(); }
                m_on_sysc.run_on_sysc([this, i, value] {
                    trig_ack[i]->write(static_cast<uint32_t>(value));
                }, false);
                if (locked) { qemu.lock_iothread(); }
            });
            m_dev.connect_gpio_out_named("dma-ack", i, ack);
            m_ack.push_back(ack);
        }
    }
};

extern "C" void module_register();
