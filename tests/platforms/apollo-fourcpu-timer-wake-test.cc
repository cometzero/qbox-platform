/* SPDX-License-Identifier: BSD-3-Clause */

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cinttypes>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

#include <systemc>

#include "test/cpu.h"
#include "test/tester/mmio.h"

#include "arm_gicv3.h"
#include "cortex-a720ae.h"
#include "qemu-instance.h"
#include "qemu_arm_arch_timer_mmio.h"
#include "qemu_timer_api.h"

class ObservedA720AE : public cpu_arm_cortexA720AE
{
public:
    using cpu_arm_cortexA720AE::cpu_arm_cortexA720AE;

    uint64_t run_state()
    {
        return m_inst.get().plugin_api().cpu_get_run_state(
            m_cpu.get_qemu_obj());
    }

    int64_t virtual_deadline_estimate()
    {
        return m_inst.get().get_virtual_clock() +
               static_cast<int64_t>(m_quantum_ns);
    }

    std::string qk_state()
    {
        auto* qk = dynamic_cast<gs::tlm_quantumkeeper_multithread*>(m_qk.get());
        return qk ? qk->get_status_json() : "{\"state\":\"none\"}";
    }
};

class ApolloFourCpuTimerWakeTest : public CpuTestBenchBase
{
    static constexpr unsigned NUM_CPUS = 4;
    static constexpr uint64_t GICD_BASE = 0x20800000;
    static constexpr uint64_t GICD_SIZE = 0x00010000;
    static constexpr uint64_t GICR_BASE = 0x20880000;
    static constexpr uint64_t GICR_STRIDE = 0x00040000;
    static constexpr uint64_t TIMER_BASE = 0x1a810000;
    static constexpr uint64_t TIMER_SIZE = 0x00030000;
    static constexpr uint64_t TIMER_FRAME0 = TIMER_BASE + 0x00020000;
    static constexpr unsigned TIMER_SPI = 49;
    static constexpr unsigned TIMER_INTID = 32 + TIMER_SPI;
    static constexpr unsigned PHYS_TIMER_INTID = 30;
    static constexpr uint64_t TIMER_TICKS = 2500000;
    static constexpr uint64_t RECORD_STRIDE = 0x40;

    enum RecordOffset : unsigned {
        RECORD_STAGE = 0x00,
        RECORD_MPIDR = 0x08,
        RECORD_COUNT = 0x10,
        RECORD_CVAL = 0x18,
        RECORD_CTL = 0x20,
        RECORD_IAR = 0x28,
        RECORD_EOI = 0x30,
        RECORD_RESUME = 0x38,
    };

    enum Stage : unsigned {
        STAGE_NONE = 0,
        STAGE_BOOT = 1,
        STAGE_GIC_READY = 2,
        STAGE_TIMER_PROGRAMMED = 3,
        STAGE_WFI_ENTRY = 4,
        STAGE_ISR = 5,
        STAGE_EOI = 6,
        STAGE_RESUME = 7,
        STAGE_UNEXPECTED = 8,
    };

    enum class Outcome {
        pending,
        pass,
        unexpected,
        preentry_timeout,
        wake_timeout,
    };

    static constexpr const char* FIRMWARE = R"(
        _start:
            msr daifset, #0xf
            mrs x0, mpidr_el1
            ubfx x20, x0, #8, #8
            cmp x20, #3
            b.hi unexpected_early

            ldr x21, =0x%08)" PRIx64 R"(
            lsl x1, x20, #6
            add x21, x21, x1
            str x0, [x21, #%u]
            mov x0, #%u
            str x0, [x21, #%u]

            ldr x11, =0x%08)" PRIx64 R"(
            ldr x14, =0x%08)" PRIx64 R"(
            ldr x0, =0x%08)" PRIx64 R"(
            mul x0, x20, x0
            add x14, x14, x0
            add x12, x14, #0x10000
            ldr x13, =0x%08)" PRIx64 R"(

            ldr w0, [x14, #0x14]
            bic w0, w0, #2
            str w0, [x14, #0x14]
        wait_children_awake_el3:
            ldr w0, [x14, #0x14]
            tst w0, #4
            b.ne wait_children_awake_el3

            ldr x0, =0x40000000
            str w0, [x12, #0x80]
            str w0, [x12, #0x100]

            cbnz x20, distributor_done_el3
            mov x0, #0x32
            str w0, [x11]
            ldr x0, =0x00020000
            str w0, [x11, #0x88]
            str w0, [x11, #0x108]
        distributor_done_el3:
            mov x0, #1
            msr icc_sre_el3, x0
            isb

            ldr x0, =el1_entry
            msr elr_el3, x0
            ldr x0, =0x3c5
            msr spsr_el3, x0
            ldr x0, =0x501
            msr scr_el3, x0
            isb
            eret

        el1_entry:
            msr spsel, #1
            adr x0, vectors
            msr vbar_el1, x0
            isb

            mov x0, #1
            msr icc_sre_el1, x0
            isb
            mov x0, #0xff
            msr icc_pmr_el1, x0
            mov x0, #1
            msr icc_igrpen1_el1, x0
            isb

            mov x0, #%u
            str x0, [x21, #%u]

            ldr x15, =0x20000
            lsl x0, x20, #3
            add x0, x15, x0
            mov x1, #1
            str x1, [x0]
            dmb sy
            cbnz x20, wait_release
        wait_all_ready:
            ldr x0, [x15, #0]
            ldr x1, [x15, #8]
            ldr x2, [x15, #16]
            ldr x3, [x15, #24]
            and x0, x0, x1
            and x0, x0, x2
            and x0, x0, x3
            cbz x0, wait_all_ready
            mov x0, #1
            str x0, [x15, #0x100]
            dmb sy
            b released
        wait_release:
            ldr x0, [x15, #0x100]
            cbz x0, wait_release
        released:

            ldr x22, =%u
            cbnz x22, mmio_phase
            mrs x0, cntpct_el0
            ldr x1, =%llu
            add x1, x0, x1
            msr cntp_cval_el0, x1
            mov x2, #1
            msr cntp_ctl_el0, x2
            isb
            mrs x0, cntpct_el0
            str x0, [x21, #%u]
            mrs x0, cntp_cval_el0
            str x0, [x21, #%u]
            mrs x0, cntp_ctl_el0
            str x0, [x21, #%u]
            mov x0, #%u
            str x0, [x21, #%u]
        mmio_phase:
            mov x0, #%u
            str x0, [x21, #%u]
            msr daifclr, #2
            isb
        idle:
            wfi
            mov x0, #%u
            str x0, [x21, #%u]
            mov x0, #1
            str x0, [x21, #0x38]
            b idle

        .balign 2048
        vectors:
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b irq_handler
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected
        .balign 128
            b unexpected

        irq_handler:
            mrs x0, icc_iar1_el1
            str x0, [x21, #%u]
            mov x1, #%u
            str x1, [x21, #%u]
            cbnz x22, expect_spi
            cmp x0, #%u
            b.ne unexpected
            msr cntp_ctl_el0, xzr
            b complete_irq
        expect_spi:
            cmp x0, #%u
            b.ne unexpected
            str wzr, [x13, #0x2c]
        complete_irq:
            msr icc_eoir1_el1, x0
            mov x1, #%u
            str x1, [x21, #%u]
            mov x1, #1
            str x1, [x21, #%u]
            eret

        unexpected:
            mov x0, #%u
            str x0, [x21, #%u]
        unexpected_loop:
            wfi
            b unexpected_loop

        unexpected_early:
            wfi
            b unexpected_early
    )";

    cci::cci_param<std::string> p_phase;
    cci::cci_param<unsigned> p_target_cpu;
    QemuInstanceManager m_inst_manager;
    QemuInstance m_inst;
    sc_core::sc_vector<ObservedA720AE> m_cpus;
    arm_gicv3 m_gic;
    qemu_arm_arch_timer_mmio m_timer;
    CpuTesterMmio m_tester;
    global_peripheral_initiator m_gpi;
    sc_core::sc_vector<sc_core::sc_out<bool>> m_reset;
    gs::async_event m_keepalive;
    gs::async_event m_progress;
    gs::async_event m_watchdog_event;
    std::array<std::array<std::atomic<uint64_t>, 8>, NUM_CPUS> m_record{};
    std::atomic<Outcome> m_outcome{ Outcome::pending };
    std::atomic_bool m_all_wfi{ false };
    std::mutex m_watchdog_mutex;
    std::condition_variable m_watchdog_cancel;
    bool m_watchdog_cancelled = false;
    std::thread m_watchdog;

    bool is_mmio_phase() const { return p_phase.get_value() == "mmio-broadcast"; }

    static const char* outcome_name(Outcome outcome)
    {
        switch (outcome) {
        case Outcome::pending: return "pending";
        case Outcome::pass: return "pass";
        case Outcome::unexpected: return "unexpected";
        case Outcome::preentry_timeout: return "preentry-timeout";
        case Outcome::wake_timeout: return "wake-timeout";
        }
        return "invalid";
    }

    unsigned count_stage_at_least(Stage stage) const
    {
        unsigned count = 0;
        for (const auto& cpu : m_record) {
            if (cpu[0].load() >= static_cast<uint64_t>(stage)) {
                ++count;
            }
        }
        return count;
    }

    void start_watchdog()
    {
        m_watchdog = std::thread([this] {
            std::unique_lock<std::mutex> lock(m_watchdog_mutex);
            if (m_watchdog_cancel.wait_for(
                    lock, std::chrono::seconds(4),
                    [this] { return m_watchdog_cancelled; })) {
                return;
            }
            Outcome expected = Outcome::pending;
            const Outcome timeout = m_all_wfi.load()
                ? Outcome::wake_timeout : Outcome::preentry_timeout;
            if (m_outcome.compare_exchange_strong(expected, timeout)) {
                m_watchdog_event.async_notify();
            }
        });
    }

    void cancel_watchdog()
    {
        {
            std::lock_guard<std::mutex> lock(m_watchdog_mutex);
            m_watchdog_cancelled = true;
        }
        m_watchdog_cancel.notify_one();
        if (m_watchdog.joinable()) {
            m_watchdog.join();
        }
    }

    uint64_t qemu_read(uint64_t address, unsigned size)
    {
        uint64_t value = 0;
        qemu::MemoryRegionOps::MemTxAttrs attrs = {};
        m_gpi.m_initiator.qemu_io_read(address, &value, size, attrs);
        return value;
    }

    void qemu_write(uint64_t address, uint64_t value, unsigned size)
    {
        qemu::MemoryRegionOps::MemTxAttrs attrs = {};
        m_gpi.m_initiator.qemu_io_write(address, value, size, attrs);
    }

    void capture(const char* point)
    {
        qbox_platform::qemu_timer::ArmMMIOTimerSnapshot timer{};
        qemu::Device timer_device = m_timer.get_qemu_dev();
        const bool timer_valid =
            qbox_platform::qemu_timer::get_arm_mmio_timer_snapshot(
                timer_device, 0, timer);

        m_inst.get().lock_iothread();
        const int64_t virtual_clock = m_inst.get().get_virtual_clock();
        const uint64_t dist_enable = qemu_read(GICD_BASE + 0x108, 4);
        const uint64_t dist_pending = qemu_read(GICD_BASE + 0x208, 4);
        const uint64_t dist_active = qemu_read(GICD_BASE + 0x308, 4);
        const uint64_t irouter = qemu_read(GICD_BASE + 0x6288, 8);
        std::array<uint64_t, NUM_CPUS> run_state{};
        std::array<uint64_t, NUM_CPUS> ppi_group{};
        std::array<uint64_t, NUM_CPUS> ppi_enable{};
        std::array<uint64_t, NUM_CPUS> ppi_pending{};
        std::array<uint64_t, NUM_CPUS> ppi_active{};
        for (unsigned cpu = 0; cpu < NUM_CPUS; ++cpu) {
            run_state[cpu] = m_cpus[cpu].run_state();
            const uint64_t sgi = GICR_BASE + cpu * GICR_STRIDE + 0x10000;
            ppi_group[cpu] = qemu_read(sgi + 0x80, 4);
            ppi_enable[cpu] = qemu_read(sgi + 0x100, 4);
            ppi_pending[cpu] = qemu_read(sgi + 0x200, 4);
            ppi_active[cpu] = qemu_read(sgi + 0x300, 4);
        }
        m_inst.get().unlock_iothread();

        std::printf(
            "QBOX_FOURCPU_GIC point=%s phase=%s sc_time=%s vclock_ns=%" PRId64
            " dist_enable=0x%08" PRIx64 " dist_pending=0x%08" PRIx64
            " dist_active=0x%08" PRIx64 " irouter81=0x%016" PRIx64
            " timer_valid=%d timer_count=%" PRIu64 " timer_cval=%" PRIu64
            " timer_ctl=0x%x timer_gpio=%u\n",
            point, p_phase.get_value().c_str(),
            sc_core::sc_time_stamp().to_string().c_str(), virtual_clock,
            dist_enable, dist_pending, dist_active, irouter,
            timer_valid, timer.count, timer.cval, timer.ctl, timer.irq_level);
        for (unsigned cpu = 0; cpu < NUM_CPUS; ++cpu) {
            const bool can_run = (run_state[cpu] & ((1ULL << 0) | (1ULL << 4) |
                                                   (1ULL << 5))) != 0 ||
                                 (run_state[cpu] & ((1ULL << 1) | (1ULL << 3))) == 0;
            const bool has_work = (run_state[cpu] & (1ULL << 5)) != 0;
            std::printf(
                "QBOX_FOURCPU_CPU point=%s phase=%s cpu=%u affinity=0x%x"
                " stage=%" PRIu64 " mpidr=0x%" PRIx64
                " count=%" PRIu64 " cval=%" PRIu64 " ctl=0x%" PRIx64
                " iar=%" PRIu64 " eoi=%" PRIu64 " resume=%" PRIu64
                " ppi_group=0x%08" PRIx64 " ppi_enable=0x%08" PRIx64
                " ppi_pending=0x%08" PRIx64 " ppi_active=0x%08" PRIx64
                " run_state=0x%02" PRIx64 " can_run=%d has_work=%d"
                " parent_irq_work=%d qemu_deadline_estimate_ns=%" PRId64
                " qk=%s\n",
                point, p_phase.get_value().c_str(), cpu, cpu << 8,
                m_record[cpu][0].load(), m_record[cpu][1].load(),
                m_record[cpu][2].load(), m_record[cpu][3].load(),
                m_record[cpu][4].load(), m_record[cpu][5].load(),
                m_record[cpu][6].load(), m_record[cpu][7].load(),
                ppi_group[cpu], ppi_enable[cpu], ppi_pending[cpu],
                ppi_active[cpu], run_state[cpu], can_run, has_work, has_work,
                m_cpus[cpu].virtual_deadline_estimate(),
                m_cpus[cpu].qk_state().c_str());
        }
        std::fflush(stdout);
    }

    void program_mmio_timer()
    {
        const uint64_t route = static_cast<uint64_t>(p_target_cpu.get_value()) << 8;
        m_inst.get().lock_iothread();
        qemu_write(GICD_BASE + 0x6288, route, 8);
        qemu_write(TIMER_FRAME0 + 0x28, TIMER_TICKS, 4);
        qemu_write(TIMER_FRAME0 + 0x2c, 1, 4);
        m_inst.get().unlock_iothread();
        std::printf(
            "QBOX_FOURCPU_INJECT phase=mmio-broadcast target_cpu=%u"
            " affinity=0x%x spi=49 intid=81 ticks=%" PRIu64 "\n",
            p_target_cpu.get_value(), p_target_cpu.get_value() << 8,
            TIMER_TICKS);
        std::fflush(stdout);
    }

    bool phase_complete() const
    {
        if (!is_mmio_phase()) {
            for (const auto& cpu : m_record) {
                if (cpu[0].load() < STAGE_RESUME ||
                    cpu[5].load() != PHYS_TIMER_INTID ||
                    cpu[6].load() != 1 || cpu[7].load() != 1) {
                    return false;
                }
            }
            return true;
        }

        const unsigned target = p_target_cpu.get_value();
        if (m_record[target][0].load() < STAGE_RESUME ||
            m_record[target][5].load() != TIMER_INTID ||
            m_record[target][6].load() != 1 ||
            m_record[target][7].load() != 1) {
            return false;
        }
        for (unsigned cpu = 0; cpu < NUM_CPUS; ++cpu) {
            if (cpu != target && m_record[cpu][0].load() != STAGE_WFI_ENTRY) {
                return false;
            }
        }
        return true;
    }

    void stop_cpus()
    {
        for (auto& cpu : m_cpus) {
            cpu.halt_cb(true);
        }
        m_keepalive.async_detach_suspending();
        sc_core::sc_stop();
    }

    void observe()
    {
        sc_core::sc_unsuspendable();
        start_watchdog();
        while (m_outcome.load() == Outcome::pending &&
               count_stage_at_least(STAGE_WFI_ENTRY) != NUM_CPUS) {
            wait(m_progress | m_watchdog_event);
        }

        if (m_outcome.load() != Outcome::pending) {
            capture("terminal-preentry");
            cancel_watchdog();
            stop_cpus();
            TEST_FAIL("four-CPU timer seam failed before all CPUs entered WFI");
            sc_core::sc_suspendable();
            return;
        }

        m_all_wfi.store(true);
        std::printf("QBOX_FOURCPU_ALL_WFI phase=%s count=4 sc_time=%s\n",
                    p_phase.get_value().c_str(),
                    sc_core::sc_time_stamp().to_string().c_str());
        capture("pre-inject");
        if (is_mmio_phase()) {
            program_mmio_timer();
        }

        while (m_outcome.load() == Outcome::pending && !phase_complete()) {
            wait(m_progress | m_watchdog_event);
        }
        if (m_outcome.load() == Outcome::pending && phase_complete()) {
            m_outcome.store(Outcome::pass);
        }
        capture("terminal");
        cancel_watchdog();

        const Outcome result = m_outcome.load();
        std::printf(
            "QBOX_FOURCPU_TERMINAL phase=%s outcome=%s all_wfi=%d"
            " target_cpu=%u ppi_resumes=%u\n",
            p_phase.get_value().c_str(), outcome_name(result),
            m_all_wfi.load(), p_target_cpu.get_value(),
            count_stage_at_least(STAGE_RESUME));
        std::fflush(stdout);
        const bool passed = result == Outcome::pass && m_all_wfi.load() &&
                            phase_complete();
        stop_cpus();
        TEST_ASSERT(passed);
        sc_core::sc_suspendable();
    }

public:
    ApolloFourCpuTimerWakeTest(const sc_core::sc_module_name& name)
        : CpuTestBenchBase(name, qemu::Target::AARCH64)
        , p_phase("phase", "local-ppi", "local-ppi or mmio-broadcast")
        , p_target_cpu("target_cpu", 2, "MMIO SPI target CPU")
        , m_inst_manager("inst_manager")
        , m_inst("inst", &m_inst_manager, qemu::Target::AARCH64)
        , m_cpus("cpu", NUM_CPUS,
                 [this](const char* cpu_name, size_t) {
                     return new ObservedA720AE(cpu_name, m_inst);
                 })
        , m_gic("gic", m_inst, NUM_CPUS)
        , m_timer("timer", m_inst)
        , m_tester("tester", *this)
        , m_gpi("gpi", m_inst, m_cpus[0])
        , m_reset("reset", NUM_CPUS)
        , m_keepalive("keepalive")
        , m_progress("progress")
        , m_watchdog_event("watchdog_event")
    {
        if (p_phase.get_value() != "local-ppi" &&
            p_phase.get_value() != "mmio-broadcast") {
            TEST_FAIL("phase must be local-ppi or mmio-broadcast");
        }
        if (p_target_cpu.get_value() >= NUM_CPUS) {
            TEST_FAIL("target_cpu must be in range 0..3");
        }
        TEST_ASSERT(m_inst.manages_start_in_reset_release());
        TEST_ASSERT(m_gic.redist_iface.size() == NUM_CPUS);
        m_keepalive.async_attach_suspending();

        for (unsigned cpu = 0; cpu < NUM_CPUS; ++cpu) {
            m_cpus[cpu].p_mp_affinity = cpu << 8;
            m_cpus[cpu].p_has_el3 = true;
            m_cpus[cpu].p_has_el2 = true;
            m_cpus[cpu].p_start_powered_off = false;
            m_cpus[cpu].p_start_in_reset = true;
            m_cpus[cpu].p_reset_power_on = true;
            m_cpus[cpu].p_cntfrq_hz = 125000000;
            m_cpus[cpu].p_psci_conduit = "disabled";
            m_reset[cpu].bind(m_cpus[cpu].reset);
            m_router.add_initiator(m_cpus[cpu].socket);

            m_cpus[cpu].irq_timer_phys_out.bind(m_gic.ppi_in[cpu][30]);
            m_cpus[cpu].irq_timer_virt_out.bind(m_gic.ppi_in[cpu][27]);
            m_cpus[cpu].irq_timer_hyp_out.bind(m_gic.ppi_in[cpu][26]);
            m_cpus[cpu].irq_timer_sec_out.bind(m_gic.ppi_in[cpu][29]);
            m_gic.irq_out[cpu].bind(m_cpus[cpu].irq_in);
            m_gic.fiq_out[cpu].bind(m_cpus[cpu].fiq_in);
            m_gic.virq_out[cpu].bind(m_cpus[cpu].virq_in);
            m_gic.vfiq_out[cpu].bind(m_cpus[cpu].vfiq_in);
        }
        m_router.add_initiator(m_gpi.m_initiator);
        m_router.add_target(m_gic.dist_iface, GICD_BASE, GICD_SIZE);
        for (unsigned cpu = 0; cpu < NUM_CPUS; ++cpu) {
            m_router.add_target(m_gic.redist_iface[cpu],
                                GICR_BASE + cpu * GICR_STRIDE,
                                GICR_STRIDE);
        }
        m_router.add_target(m_timer.socket, TIMER_BASE, TIMER_SIZE);
        m_timer.irq[0].bind(m_gic.spi_in[TIMER_SPI]);

        char firmware[32768];
        const int length = std::snprintf(
            firmware, sizeof(firmware), FIRMWARE,
            CpuTesterMmio::MMIO_ADDR,
            RECORD_MPIDR, STAGE_BOOT, RECORD_STAGE,
            GICD_BASE, GICR_BASE, GICR_STRIDE, TIMER_FRAME0,
            STAGE_GIC_READY, RECORD_STAGE,
            is_mmio_phase() ? 1U : 0U,
            static_cast<unsigned long long>(TIMER_TICKS),
            RECORD_COUNT, RECORD_CVAL, RECORD_CTL,
            STAGE_TIMER_PROGRAMMED, RECORD_STAGE,
            STAGE_WFI_ENTRY, RECORD_STAGE,
            STAGE_RESUME, RECORD_STAGE,
            RECORD_IAR, STAGE_ISR, RECORD_STAGE,
            PHYS_TIMER_INTID, TIMER_INTID,
            STAGE_EOI, RECORD_STAGE, RECORD_EOI,
            STAGE_UNEXPECTED, RECORD_STAGE);
        TEST_ASSERT(length > 0 && static_cast<size_t>(length) < sizeof(firmware));
        set_firmware(firmware);

        SC_THREAD(observe);
        for (auto& cpu : m_cpus) {
            cpu.reset_cb(false);
        }
    }

    ~ApolloFourCpuTimerWakeTest() override { cancel_watchdog(); }

    void mmio_write(int, uint64_t address, uint64_t data, size_t) override
    {
        const unsigned cpu = address / RECORD_STRIDE;
        const uint64_t offset = address % RECORD_STRIDE;
        if (cpu >= NUM_CPUS || (offset % 8) != 0 || offset > RECORD_RESUME) {
            Outcome expected = Outcome::pending;
            m_outcome.compare_exchange_strong(expected, Outcome::unexpected);
            m_progress.async_notify();
            return;
        }
        const unsigned field = static_cast<unsigned>(offset / 8);
        m_record[cpu][field].store(data);
        if (offset == RECORD_STAGE && data == STAGE_UNEXPECTED) {
            Outcome expected = Outcome::pending;
            m_outcome.compare_exchange_strong(expected, Outcome::unexpected);
        }
        m_progress.async_notify();
    }

    uint64_t mmio_read(int, uint64_t, size_t) override
    {
        TEST_FAIL("unexpected CPU tester read");
    }

    bool dmi_request(int, uint64_t, size_t, tlm::tlm_dmi&) override
    {
        TEST_FAIL("unexpected CPU tester DMI request");
    }

    void map_irqs_to_cpus(
        sc_core::sc_vector<InitiatorSignalSocket<bool>>&) override
    {
    }
};

constexpr const char* ApolloFourCpuTimerWakeTest::FIRMWARE;

int sc_main(int argc, char* argv[])
{
    return run_testbench<ApolloFourCpuTimerWakeTest>(argc, argv);
}
