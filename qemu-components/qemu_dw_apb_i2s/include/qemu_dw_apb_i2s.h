/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

#include <stdexcept>
#include <string>
#include <cci_configuration>
#include <device.h>
#include <qemu_dma350.h>
#include <module_factory_registery.h>
#include <ports/qemu-initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <ports/target.h>

class qemu_dw_apb_i2s : public QemuDevice
{
    QemuDevice* m_dma = nullptr;

    QemuDevice& resolve(const std::string& path, const char* type)
    {
        auto* dev = dynamic_cast<QemuDevice*>(sc_core::sc_find_object(path.c_str()));
        if (!dev || &dev->get_qemu_inst() != &m_inst || std::string(dev->get_qom_type()) != type) {
            throw std::invalid_argument(std::string(name()) + ": invalid same-instance " + type + " reference: " + path);
        }
        dev->instantiate();
        return *dev;
    }

public:
    cci::cci_param<std::string> p_peer;
    cci::cci_param<std::string> p_dma_controller;
    cci::cci_param<unsigned int> p_tx_trigger;
    cci::cci_param<unsigned int> p_rx_trigger;
    cci::cci_param<bool> p_master_mode;
    cci::cci_param<bool> p_transmitter_enabled;
    cci::cci_param<bool> p_receiver_enabled;
    cci::cci_param<bool> p_functional_pacing;
    cci::cci_param<uint64_t> p_frame_period_ns;
    QemuTargetSocket<> target_socket;
    QemuInitiatorSignalSocket irq;
    TargetSignalSocket<bool> reset;

    qemu_dw_apb_i2s(sc_core::sc_module_name name, sc_core::sc_object* inst)
        : QemuDevice(name, *dynamic_cast<QemuInstance*>(inst), "dw-apb-i2s")
        , p_peer("peer", "", "Absolute SystemC path of native I2S peer")
        , p_dma_controller("dma_controller", "", "Absolute SystemC path of native DMA350")
        , p_tx_trigger("tx_trigger", 0, "DMA TX request index")
        , p_rx_trigger("rx_trigger", 1, "DMA RX request index")
        , p_master_mode("master_mode", false, "I2S clock master")
        , p_transmitter_enabled("transmitter_enabled", true, "Enable TX capability")
        , p_receiver_enabled("receiver_enabled", true, "Enable RX capability")
        , p_functional_pacing("functional_pacing", true, "Retry peer receive FIFO backpressure")
        , p_frame_period_ns("frame_period_ns", 20833, "Frame period in QEMU virtual nanoseconds")
        , target_socket("target_socket", m_inst)
        , irq("irq")
        , reset("reset")
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

    void before_end_of_elaboration() override
    {
        QemuDevice::before_end_of_elaboration();
        m_dev.set_prop_bool("master-mode", p_master_mode.get_value());
        m_dev.set_prop_bool("transmitter-enabled", p_transmitter_enabled.get_value());
        m_dev.set_prop_bool("receiver-enabled", p_receiver_enabled.get_value());
        m_dev.set_prop_bool("functional-pacing", p_functional_pacing.get_value());
        m_dev.set_prop_uint("frame-period-ns", p_frame_period_ns.get_value());
        if (!p_peer.get_value().empty()) {
            m_dev.set_prop_link("peer", resolve(p_peer.get_value(), "dw-apb-i2s").get_qemu_dev());
        }
        if (!p_dma_controller.get_value().empty()) {
            m_dma = &resolve(p_dma_controller.get_value(), "arm-dma350");
            auto* controller = dynamic_cast<qemu_dma350*>(m_dma);
            const auto count = controller ? controller->p_trigger_count.get_value() : 0;
            if (p_tx_trigger.get_value() >= count || p_rx_trigger.get_value() >= count ||
                p_tx_trigger.get_value() == p_rx_trigger.get_value()) {
                throw std::invalid_argument("I2S requires distinct DMA trigger indices within trigger_count");
            }
        }
    }

    void end_of_elaboration() override
    {
        set_sysbus_as_parent_bus();
        QemuDevice::end_of_elaboration();
        qemu::SysBusDevice sbd(m_dev);
        target_socket.init(sbd, 0);
        irq.init_sbd(sbd, 0);
    }

    void start_of_simulation() override
    {
        if (!m_dma) { return; }
        auto dma = m_dma->get_qemu_dev();
        // The native handshake is multi-bit. Boolean SystemC GPIO proxies
        // would discard request type and ACTIVE, so keep both directions native.
        m_dev.connect_gpio_out_named("dma-tx-req", 0,
                                    dma.get_gpio_in_named("dma-req", p_tx_trigger.get_value()));
        m_dev.connect_gpio_out_named("dma-rx-req", 0,
                                    dma.get_gpio_in_named("dma-req", p_rx_trigger.get_value()));
        dma.connect_gpio_out_named("dma-ack", p_tx_trigger.get_value(),
                                   m_dev.get_gpio_in_named("dma-tx-ack", 0));
        dma.connect_gpio_out_named("dma-ack", p_rx_trigger.get_value(),
                                   m_dev.get_gpio_in_named("dma-rx-ack", 0));
    }
};

extern "C" void module_register();
