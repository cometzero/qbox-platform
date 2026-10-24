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

class ApolloGicAeContainmentTest : public CpuTestBenchBase
{
    static constexpr uint64_t DIST = 0x20800000;
    static constexpr uint64_t REDIST = 0x20880000;
    static constexpr const char* FIRMWARE = R"(
        _start:
            msr daifset, #0xf
            ldr x10, =0x80000000
            ldr x11, =0x20800000
            ldr x12, =0x20880000
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
            ldr x13, =0x2008002c
            mov w0, #1
            str w0, [x13]
            ldr w0, [x12, #0x14]
            bic w0, w0, #2
            str w0, [x12, #0x14]
            mov x4, #0xffff
        wake:
            ldr w0, [x12, #0x14]
            tst w0, #4
            b.eq awake
            subs x4, x4, #1
            b.ne wake
            b failed
        awake:
            mov w0, #0x11
            ldr x13, =0x20000000
            str w0, [x13]
            str w0, [x11]
            ldr x13, =0x2000f618
            ldr w0, =0x40000
            str w0, [x13]
            mov w5, #0x200
            str wzr, [x11, #0x8c]
            str wzr, [x11, #0xd0c]
            mov w0, #0x80
            strb w0, [x11, #0x469]
            ldr w0, =0x80000
            str w0, [x11, #0xc18]
            str xzr, [x11, #0x6348]
            str w5, [x11, #0x10c]
            str w5, [x11, #0x20c]
            dsb sy
            isb
            mov x4, #0xffff
        first_irq:
            mrs x0, icc_iar0_el1
            cmp x0, #105
            b.eq first_seen
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne first_irq
            b failed
        first_seen:
            str x0, [x10]
            msr icc_eoir0_el1, x0
            dsb sy
            isb

            ldr x13, =0x2000e20c
            str w5, [x13]
            ldr x13, =0x2000e10c
            ldr w0, [x13]
            cmp w0, w5
            b.ne failed
            str w5, [x11, #0x10c]
            dsb sy
            isb
            mov x4, #1024
        contained:
            mrs x0, icc_iar0_el1
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne contained
            str x0, [x10, #8]

            ldr x13, =0x2000f20c
            str w5, [x13]
            ldr x13, =0x2000f618
            ldr w0, =0x40000
            str w0, [x13]
            ldr x13, =0x2000e10c
            str w5, [x13]
            ldr w0, [x13]
            cbnz w0, failed
            ldr w0, [x11, #0x10c]
            tst w0, w5
            b.ne failed
            str wzr, [x11, #0x8c]
            str wzr, [x11, #0xd0c]
            mov w0, #0x80
            strb w0, [x11, #0x469]
            ldr w0, =0x80000
            str w0, [x11, #0xc18]
            str xzr, [x11, #0x6348]
            str w5, [x11, #0x10c]
            str w5, [x11, #0x20c]
            dsb sy
            isb
            mov x4, #0xffff
        recovered_irq:
            mrs x0, icc_iar0_el1
            cmp x0, #105
            b.eq recovered
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne recovered_irq
            b failed
        recovered:
            str x0, [x10, #16]
            msr icc_eoir0_el1, x0
            ldr x13, =0x2000f618
            ldr w0, =0x50000
            str w0, [x13]
            mov w0, #0x80
            strb w0, [x11, #0x468]
            mov w0, #0x100
            str w0, [x11, #0x10c]
            ldr x13, =0x2002e800
            mov w0, #104
            str w0, [x13]
            ldr x13, =0x20020088
            mov w0, #8
            str w0, [x13]
            ldr x13, =0x2000ea08
            ldr x0, =0x8000000080018000
            str x0, [x13]
            dsb sy
            mov x4, #0xffff
        ras_irq:
            mrs x0, icc_iar0_el1
            cmp x0, #104
            b.eq ras_seen
            cmp x0, #1023
            b.ne failed
            subs x4, x4, #1
            b.ne ras_irq
            b failed
        ras_seen:
            str x0, [x10, #40]
            ldr x13, =0x20020090
            mov w1, #-1
            str w1, [x13]
            dsb sy
            msr icc_eoir0_el1, x0
            isb
            mrs x0, icc_iar0_el1
            str x0, [x10, #48]
            cmp x0, #1023
            b.ne failed
            mov x0, #1
            str x0, [x10, #24]
        done:
            wfi
            b done
        failed:
            str x0, [x10, #32]
            mov x0, #2
            str x0, [x10, #24]
            b done
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
    )";

    QemuInstanceManager m_manager;
    QemuInstance m_inst;
    cpu_arm_cortexA720AE m_cpu;
    arm_gicv3 m_gic;
    gicx00_multiview m_front;
    gs::router<> m_backend;
    CpuTesterMmio m_tester;
    global_peripheral_initiator m_gpi;
    sc_core::sc_out<bool> m_reset;
    gs::async_event m_keepalive;
    gs::async_event m_progress;
    std::array<std::atomic<uint64_t>, 7> m_results {};

    void observe()
    {
        sc_core::sc_unsuspendable();
        while (m_results[3].load() == 0) wait(m_progress);
        const bool pass = m_results[0].load() == 105 &&
            m_results[1].load() == 1023 && m_results[2].load() == 105 &&
            m_results[3].load() == 1 && m_results[5].load() == 104 && m_results[6].load() == 1023;
        std::printf("GIC_AE_CPU baseline=%" PRIu64 " contained=%" PRIu64
                    " recovered=%" PRIu64 " detail=%" PRIu64 " ras_irq=%" PRIu64 " ras_clear=%" PRIu64 " result=%s\n",
                    m_results[0].load(), m_results[1].load(),
                    m_results[2].load(), m_results[4].load(), m_results[5].load(), m_results[6].load(), pass ? "PASS" : "FAIL");
        std::fflush(stdout);
        m_cpu.halt_cb(true);
        m_keepalive.async_detach_suspending();
        sc_core::sc_stop();
        TEST_ASSERT(pass);
        sc_core::sc_suspendable();
    }

public:
    SC_HAS_PROCESS(ApolloGicAeContainmentTest);

    explicit ApolloGicAeContainmentTest(sc_core::sc_module_name name)
        : CpuTestBenchBase(name, qemu::Target::AARCH64)
        , m_manager("inst_manager")
        , m_inst("inst", &m_manager, qemu::Target::AARCH64)
        , m_cpu("cpu", m_inst)
        , m_gic("gic", m_inst, 1)
        , m_front("front")
        , m_backend("backend")
        , m_tester("tester", *this)
        , m_gpi("gpi", m_inst, m_cpu)
        , m_reset("reset")
        , m_keepalive("keepalive")
        , m_progress("progress")
    {
        m_cpu.p_has_el3 = true;
        m_cpu.p_has_el2 = true;
        m_cpu.p_start_in_reset = true;
        m_cpu.p_start_powered_off = false;
        m_cpu.p_reset_power_on = true;
        m_cpu.p_psci_conduit = "disabled";
        m_front.p_backend_redist_count = 1;
        m_reset.bind(m_cpu.reset);
        m_front.internal_spi.bind(m_gic.internal_spi);
        m_router.add_initiator(m_cpu.socket);
        m_router.add_initiator(m_gpi.m_initiator);
        m_router.add_target(m_front.view0_dist, 0x20000000, 0x40000);
        m_router.add_target(m_front.view0_redist_0, 0x20080000, 0x40000);
        m_router.add_target(m_front.view1_dist, DIST, 0x10000);
        m_router.add_target(m_front.view1_redists, REDIST, 0x40000);
        m_backend.add_initiator(m_front.backend_socket);
        m_backend.add_target(m_gic.dist_iface, DIST, 0x10000);
        m_backend.add_target(m_gic.redist_iface[0], REDIST, 0x40000);
        m_gic.irq_out[0].bind(m_cpu.irq_in);
        m_gic.fiq_out[0].bind(m_cpu.fiq_in);
        m_gic.virq_out[0].bind(m_cpu.virq_in);
        m_gic.vfiq_out[0].bind(m_cpu.vfiq_in);
        m_keepalive.async_attach_suspending();
        set_firmware(FIRMWARE);
        SC_THREAD(observe);
        m_cpu.reset_cb(false);
    }

    void mmio_write(int, uint64_t address, uint64_t data, size_t size) override
    {
        if (size != 8 || address >= 56 || address % 8) {
            m_results[3].store(2);
        } else m_results[address / 8].store(data);
        m_progress.async_notify();
    }

    void map_irqs_to_cpus(sc_core::sc_vector<InitiatorSignalSocket<bool>>&) override {}
};

constexpr const char* ApolloGicAeContainmentTest::FIRMWARE;

int sc_main(int argc, char* argv[])
{
    return run_testbench<ApolloGicAeContainmentTest>(argc, argv);
}
