/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <cortex-a720ae.h>
#include <cortex-r82.h>
#include <host_gtimer.h>
#include <module_factory_registery.h>
#include <qemu_arm_arch_timer_mmio.h>
#include <qemu_sse_timer.h>
#include <systemc>

class apollo_timer_snapshot : public sc_core::sc_module
{
    host_gtimer& m_smd;
    cpu_arm_cortexA720AE& m_ap_cpu;
    qemu_arm_arch_timer_mmio& m_ap_mmio;
    cpu_arm_cortexR82& m_si0_cpu;
    host_gtimer& m_si0_cntbase;
    cpu_arm_cortexR82& m_si1_cpu;
    qemu_sse_timer& m_rse_timer0;
    qemu_sse_timer& m_rse_timer1;
    qemu_sse_timer& m_rse_timer2;
    qemu_sse_timer& m_rse_timer3;
    bool m_enabled = false;
    bool m_written = false;
    int64_t m_start_ns = 0;
    int64_t m_interval_ns = 0;
    std::string m_path;
    std::string m_run_id;
    std::string m_failure_detail;
    std::vector<std::string> m_samples;

    static std::string escape(const std::string& value);
    static std::string captured_at();
    static int64_t current_time_ns();
    static std::string deadline_fields(uint64_t cval, uint32_t ctl,
                                       uint32_t irq);
    static std::string observation_fields(uint64_t observed_counter,
                                          int64_t observation_time_ns,
                                          const char* counter_basis);
    void wait_until(int64_t absolute_ns);
    std::string capture(const char* name, int64_t expected_ns);
    void write_snapshot(const char* status, const char* reason);
    void run();

public:
    SC_HAS_PROCESS(apollo_timer_snapshot);

    apollo_timer_snapshot(
        sc_core::sc_module_name name, sc_core::sc_object* smd,
        sc_core::sc_object* ap_cpu, sc_core::sc_object* ap_mmio,
        sc_core::sc_object* si0_cpu, sc_core::sc_object* si0_cntbase,
        sc_core::sc_object* si1_cpu, sc_core::sc_object* rse_timer0,
        sc_core::sc_object* rse_timer1, sc_core::sc_object* rse_timer2,
        sc_core::sc_object* rse_timer3);

    void end_of_simulation() override;
};

extern "C" void module_register();
