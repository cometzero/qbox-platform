/* SPDX-License-Identifier: BSD-3-Clause */

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdio>

#include "test/cpu.h"
#include "test/tester/mmio.h"
#include "arm_gicv3.h"
#include "cortex-a720ae.h"
#include "gicx00_multiview.h"

class ApolloGicAeViewIsolationTest : public CpuTestBenchBase
{
    static constexpr uint64_t DIST = 0x20800000;
    static constexpr uint64_t REDIST = 0x20880000;
    static constexpr const char* FIRMWARE = R"(
        _start:
            msr daifset, #0xf
            ldr x10, =0x80000000
            adr x0, vectors
            msr vbar_el3, x0
            mov x0, #0xf
            msr icc_sre_el3, x0
            isb
            mov x0, #1
            msr icc_sre_el1, x0
            isb
            mov x0, #0xff
            msr icc_pmr_el1, x0
            mov x0, #1
            msr icc_igrpen0_el1, x0
            mrs x0, mpidr_el1
            and x0, x0, #0xff
            cbnz x0, secondary
            ldr x11, =0x20800000
            ldr x12, =0x21000000
            ldr x13, =0x2008002c
            mov w0, #1
            str w0, [x13]
            ldr x13, =0x200c002c
            mov w0, #2
            str w0, [x13]
            ldr x13, =0x20880014
            str wzr, [x13]
            ldr x13, =0x210c0014
            str wzr, [x13]
            mov w0, #0x11
            ldr x13, =0x20000000
            str w0, [x13]
            str w0, [x11]
            str wzr, [x12]
            ldr x13, =0x2000f618
            ldr w0, =0x240000
            str w0, [x13]
            mov w0, #0x80
            strb w0, [x11, #0x469]
            strb w0, [x12, #0x46a]
            ldr w0, =0x80000
            str w0, [x11, #0xc18]
            ldr w0, =0x200000
            str w0, [x12, #0xc18]
            mov x0, #1
            str x0, [x11, #0x6348]
            ldr x0, =0x80000000
            str x0, [x12, #0x6350]
            mov w0, #0x200
            str w0, [x11, #0x10c]
            str w0, [x11, #0x20c]
            mov w0, #0x400
            str w0, [x12, #0x10c]
            str w0, [x12, #0x20c]
            ldr x13, =0x20890400
            mov w0, #0x80
            strb w0, [x13, #3]
            ldr x13, =0x210d0400
            strb w0, [x13, #3]
            ldr x13, =0x20890100
            mov w0, #8
            str w0, [x13]
            ldr x13, =0x210d0100
            str w0, [x13]
            dsb sy
            mov x20, #1
            bl stage
            mrs x0, icc_iar0_el1
            cmp x0, #1023
            b.ne failed
            str xzr, [x11, #0x6348]
            dsb sy
            mov x4, #0xffff
        own_spi:
            mrs x0, icc_iar0_el1
            cmp x0, #105
            b.eq own_seen
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne own_spi
            b failed
        own_seen:
            msr icc_eoir0_el1, x0
            mov w0, #0x11
            str w0, [x12]
            mov w0, #0x400
            str w0, [x12, #0x20c]
            dsb sy
            mov x20, #2
            bl stage
            mov x0, #106
            msr icc_dir_el1, x0
            dsb sy
            ldr w0, [x12, #0x30c]
            tst w0, #0x400
            b.eq failed
            mov x20, #3
            bl stage
            ldr x0, =0x03000002
            msr icc_sgi0r_el1, x0
            dsb sy
            mov x20, #4
            bl stage
            ldr x0, =0x10003000000
            msr icc_sgi0r_el1, x0
            dsb sy
            mov x20, #5
            bl stage
            ldr x0, =0x03000001
            msr icc_sgi0r_el1, x0
            dsb sy
            mov x4, #0xffff
        own_sgi:
            mrs x0, icc_iar0_el1
            cmp x0, #3
            b.eq success
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne own_sgi
            b failed
        success:
            msr icc_eoir0_el1, x0
            mov x0, #1
            str x0, [x10, #24]
        stop:
            wfi
            b stop
        stage:
            str x20, [x10]
            mov x4, #0xffff
        wait_ack:
            ldr x0, [x10, #8]
            cmp x0, x20
            b.eq acked
            subs x4, x4, #1
            b.ne wait_ack
            b failed
        acked:
            ret
        secondary:
            mov x20, #0
        secondary_wait:
            ldr x0, [x10]
            cmp x0, x20
            b.eq secondary_wait
            mov x20, x0
            cmp x20, #3
            b.eq deactivate
            cmp x20, #2
            b.eq get_spi
            mov x4, #1024
        foreign_absent:
            mrs x0, icc_iar0_el1
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne foreign_absent
            b secondary_ack
        get_spi:
            mov x4, #0xffff
        irq_poll:
            mrs x0, icc_iar0_el1
            cmp x0, #106
            b.eq secondary_ack
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne irq_poll
            b failed
        deactivate:
            mov x0, #106
            msr icc_eoir0_el1, x0
        secondary_ack:
            str x0, [x10, #16]
            str x20, [x10, #8]
            cmp x20, #5
            b.eq stop
            b secondary_wait
        failed:
            str x0, [x10, #32]
            mov x0, #2
            str x0, [x10, #24]
            b stop
        .balign 2048
        vectors:
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
            b failed
        .balign 128
    )";

    QemuInstanceManager m_manager;
    QemuInstance m_inst;
    cpu_arm_cortexA720AE m_cpu;
    cpu_arm_cortexA720AE m_cpu1;
    arm_gicv3 m_gic;
    gicx00_multiview m_front;
    gs::router<> m_backend;
    CpuTesterMmio m_tester;
    global_peripheral_initiator m_gpi;
    sc_core::sc_out<bool> m_reset;
    gs::async_event m_keepalive;
    gs::async_event m_progress;
    std::array<std::atomic<uint64_t>, 5> m_results {};

    void observe()
    {
        sc_core::sc_unsuspendable();
        while (m_results[3].load() == 0) wait(m_progress);
        const bool pass = m_results[3].load() == 1 && m_results[1].load() == 5;
        std::printf("GIC_AE_2PE stages=%" PRIu64 " detail=%" PRIu64 " result=%s\n",
                    m_results[1].load(), m_results[4].load(), pass ? "PASS" : "FAIL");
        std::fflush(stdout);
        m_cpu.halt_cb(true);
        m_cpu1.halt_cb(true);
        m_keepalive.async_detach_suspending();
        sc_core::sc_stop();
        TEST_ASSERT(pass);
        sc_core::sc_suspendable();
    }

public:
    SC_HAS_PROCESS(ApolloGicAeViewIsolationTest);

    explicit ApolloGicAeViewIsolationTest(sc_core::sc_module_name name)
        : CpuTestBenchBase(name, qemu::Target::AARCH64)
        , m_manager("inst_manager")
        , m_inst("inst", &m_manager, qemu::Target::AARCH64)
        , m_cpu("cpu", m_inst)
        , m_cpu1("cpu1", m_inst)
        , m_gic("gic", m_inst, 2)
        , m_front("front")
        , m_backend("backend")
        , m_tester("tester", *this)
        , m_gpi("gpi", m_inst, m_cpu)
        , m_reset("reset")
        , m_keepalive("keepalive")
        , m_progress("progress")
    {
        for (auto* cpu : {&m_cpu, &m_cpu1}) {
            cpu->p_has_el3 = true;
            cpu->p_has_el2 = true;
            cpu->p_start_in_reset = true;
            cpu->p_start_powered_off = false;
            cpu->p_reset_power_on = true;
            cpu->p_psci_conduit = "disabled";
        }
        m_cpu.p_mp_affinity = 0;
        m_cpu1.p_mp_affinity = 1;
        m_front.p_backend_redist_count = 2;
        m_front.p_view_redist_stride = 0x40000;
        m_front.p_backend_multi_view = true;
        m_front.p_backend_dist_base = 0x60000000;
        m_front.p_backend_redist_base = 0x61000000;
        m_reset.bind(m_cpu.reset);
        m_router.add_initiator(m_cpu.socket);
        m_router.add_initiator(m_cpu1.socket);
        m_router.add_initiator(m_gpi.m_initiator);
        m_router.add_target(m_front.view0_dist, 0x20000000, 0x10000);
        m_router.add_target(m_front.view0_redist_0, 0x20080000, 0x40000);
        m_router.add_target(m_front.view0_redist_1, 0x200c0000, 0x40000);
        m_router.add_target(m_front.view1_dist, DIST, 0x10000);
        m_router.add_target(m_front.view1_redists, REDIST, 0x80000);
        m_router.add_target(m_front.view2_dist, 0x21000000, 0x10000);
        m_router.add_target(m_front.view2_redists, 0x21080000, 0x80000);
        m_backend.add_initiator(m_front.backend_socket);
        m_backend.add_target(m_gic.dist_iface, 0x60000000, 0x200000);
        m_backend.add_target(m_gic.redist_iface[0], 0x61000000, 0x40000);
        m_backend.add_target(m_gic.redist_iface[1], 0x61040000, 0x40000);
        m_gic.irq_out[0].bind(m_cpu.irq_in);
        m_gic.fiq_out[0].bind(m_cpu.fiq_in);
        m_gic.virq_out[0].bind(m_cpu.virq_in);
        m_gic.vfiq_out[0].bind(m_cpu.vfiq_in);
        m_gic.irq_out[1].bind(m_cpu1.irq_in);
        m_gic.fiq_out[1].bind(m_cpu1.fiq_in);
        m_gic.virq_out[1].bind(m_cpu1.virq_in);
        m_gic.vfiq_out[1].bind(m_cpu1.vfiq_in);
        m_keepalive.async_attach_suspending();
        set_firmware(FIRMWARE);
        SC_THREAD(observe);
        m_cpu.reset_cb(false);
        m_cpu1.reset_cb(false);
    }

    uint64_t mmio_read(int, uint64_t address, size_t size) override
    {
        if (size != 8 || address >= 40 || address % 8) return 0;
        return m_results[address / 8].load();
    }

    void mmio_write(int, uint64_t address, uint64_t data, size_t size) override
    {
        if (size != 8 || address >= 40 || address % 8) {
            m_results[3].store(2);
        } else m_results[address / 8].store(data);
        m_progress.async_notify();
    }

    void map_irqs_to_cpus(sc_core::sc_vector<InitiatorSignalSocket<bool>>&) override {}
};

constexpr const char* ApolloGicAeViewIsolationTest::FIRMWARE;

int sc_main(int argc, char* argv[])
{
    return run_testbench<ApolloGicAeViewIsolationTest>(argc, argv);
}
