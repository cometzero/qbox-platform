#ifndef QBOX_PLATFORM_APOLLO_RSE_CPU_H
#define QBOX_PLATFORM_APOLLO_RSE_CPU_H

#include <string>

#include <cci_configuration>
#include <libgsutils.h>
#include <ports/target-signal-socket.h>
#include <systemc>

#include <cpu_arm/cpu_arm_cortex_m55/include/cortex-m55.h>
#include <module_factory_registery.h>
#include <qemu-instance.h>

#include "router/include/router.h"
#include "rse_cpu_accel.h"

class ApolloRseCPU : public sc_core::sc_module
{
    SCP_LOGGER();

public:
    TargetSignalSocket<bool> accel_reset;

    ApolloRseCPU(const sc_core::sc_module_name& n, sc_core::sc_object* obj)
        : ApolloRseCPU(n, *(dynamic_cast<QemuInstance*>(obj)))
    {
    }

    ApolloRseCPU(const sc_core::sc_module_name& n, QemuInstance& qemu_inst)
        : sc_core::sc_module(n)
        , accel_reset("accel_reset")
        , m_broker(cci::cci_get_broker())
        , m_gdb_port("gdb_port", 0, "GDB port")
        , m_qemu_inst(qemu_inst)
        , m_router("router")
        , m_cpu("cpu", m_qemu_inst)
        , m_rse_accel(m_cpu, std::string(m_cpu.name()) + ".", m_cpu.name())
    {
        unsigned int irq_num =
            m_broker.get_param_handle(std::string(this->name()) + ".cpu.nvic.num_irq")
                .get_cci_value()
                .get_uint();

        if (!m_gdb_port.is_default_value()) m_cpu.p_gdb_port = m_gdb_port;

        SCP_INFO(()) << "number of irqs  = " << irq_num;

        m_cpu.register_pc_entry_observer(m_rse_accel);
        accel_reset.register_value_changed_cb([this](const bool& asserted) {
            if (asserted) {
                m_rse_accel.reset_runtime_state();
            }
        });
        m_router.initiator_socket.bind(m_cpu.m_nvic.socket);
        m_cpu.socket.bind(m_router.target_socket);
    }

    ~ApolloRseCPU() override
    {
        m_cpu.unregister_pc_entry_observer(m_rse_accel);
    }

private:
    cci::cci_broker_handle m_broker;
    cci::cci_param<int> m_gdb_port;
    QemuInstance& m_qemu_inst;
    gs::router<> m_router;
    cpu_arm_cortexM55 m_cpu;
    qbox::platform::rse_cpu_accel::RseCpuAccel m_rse_accel;
};

#endif
