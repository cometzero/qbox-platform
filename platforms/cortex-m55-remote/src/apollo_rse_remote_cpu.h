#ifndef QBOX_PLATFORM_APOLLO_RSE_REMOTE_CPU_H
#define QBOX_PLATFORM_APOLLO_RSE_REMOTE_CPU_H

#include <limits>
#include <string>

#include <cci_configuration>
#include <libgsutils.h>
#include <systemc>
#include <tlm>

#include <cpu_arm/cpu_arm_cortex_m55/include/cortex-m55.h>
#include <module_factory_registery.h>
#include <qemu-instance.h>
#include <remote.h>

#include "pass/include/pass.h"
#include "router/include/router.h"
#include "rse_cpu_accel.h"

class ApolloRseRemoteCPU : public sc_core::sc_module
{
    SCP_LOGGER();

public:
    ApolloRseRemoteCPU(const sc_core::sc_module_name& n, sc_core::sc_object* obj)
        : ApolloRseRemoteCPU(n, *(dynamic_cast<QemuInstance*>(obj)))
    {
    }

    ApolloRseRemoteCPU(const sc_core::sc_module_name& n, QemuInstance& qemu_inst)
        : sc_core::sc_module(n)
        , m_broker(cci::cci_get_broker())
        , m_gdb_port("gdb_port", 0, "GDB port")
        , m_qemu_inst(qemu_inst)
        , m_router("router")
        , m_cpu("cpu", m_qemu_inst)
        , m_rse_accel(m_cpu, std::string(m_cpu.name()) + ".")
    {
        unsigned int m_irq_num = m_broker.get_param_handle(std::string(this->name()) + ".cpu.nvic.num_irq")
                                     .get_cci_value()
                                     .get_uint();

        if (!m_gdb_port.is_default_value()) m_cpu.p_gdb_port = m_gdb_port;

        SCP_INFO(()) << "number of irqs  = " << m_irq_num;

        m_cpu.register_pc_entry_observer(m_rse_accel);
        m_router.initiator_socket.bind(m_cpu.m_nvic.socket);
        m_cpu.socket.bind(m_router.target_socket);
    }

    ~ApolloRseRemoteCPU() override
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

GSC_MODULE_REGISTER(ApolloRseRemoteCPU, sc_core::sc_object*);

#endif
