/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <apollo_timer_snapshot.h>

#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

template <typename T>
T& require_object(sc_core::sc_object* object, const char* type)
{
    T* typed = dynamic_cast<T*>(object);
    if (typed == nullptr) {
        throw std::invalid_argument(std::string("expected ") + type);
    }
    return *typed;
}

std::string require_env(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        throw std::invalid_argument(std::string("missing ") + name);
    }
    return value;
}

int64_t parse_non_negative(const char* name)
{
    const std::string value = require_env(name);
    size_t parsed = 0;
    const long long result = std::stoll(value, &parsed, 10);
    if (parsed != value.size() || result < 0) {
        throw std::invalid_argument(std::string("invalid ") + name);
    }
    return result;
}

}

apollo_timer_snapshot::apollo_timer_snapshot(
    sc_core::sc_module_name name, sc_core::sc_object* smd,
    sc_core::sc_object* ap_cpu, sc_core::sc_object* ap_mmio,
    sc_core::sc_object* si0_cpu, sc_core::sc_object* si0_cntbase,
    sc_core::sc_object* si1_cpu, sc_core::sc_object* rse_timer0,
    sc_core::sc_object* rse_timer1, sc_core::sc_object* rse_timer2,
    sc_core::sc_object* rse_timer3)
    : sc_core::sc_module(name)
    , m_smd(require_object<host_gtimer>(smd, "host_gtimer"))
    , m_ap_cpu(require_object<cpu_arm_cortexA720AE>(
          ap_cpu, "cpu_arm_cortexA720AE"))
    , m_ap_mmio(require_object<qemu_arm_arch_timer_mmio>(
          ap_mmio, "qemu_arm_arch_timer_mmio"))
    , m_si0_cpu(require_object<cpu_arm_cortexR82>(
          si0_cpu, "cpu_arm_cortexR82"))
    , m_si0_cntbase(require_object<host_gtimer>(si0_cntbase, "host_gtimer"))
    , m_si1_cpu(require_object<cpu_arm_cortexR82>(
          si1_cpu, "cpu_arm_cortexR82"))
    , m_rse_timer0(require_object<qemu_sse_timer>(
          rse_timer0, "qemu_sse_timer"))
    , m_rse_timer1(require_object<qemu_sse_timer>(
          rse_timer1, "qemu_sse_timer"))
    , m_rse_timer2(require_object<qemu_sse_timer>(
          rse_timer2, "qemu_sse_timer"))
    , m_rse_timer3(require_object<qemu_sse_timer>(
          rse_timer3, "qemu_sse_timer"))
{
    const char* enabled = std::getenv("QBOX_APOLLO_TIMER_SNAPSHOT");
    m_enabled = enabled != nullptr && std::string(enabled) == "1";
    if (m_enabled) {
        m_path = require_env("QBOX_APOLLO_TIMER_SNAPSHOT_PATH");
        m_run_id = require_env("QBOX_APOLLO_TIMER_SNAPSHOT_RUN_ID");
        m_start_ns = parse_non_negative("QBOX_APOLLO_TIMER_SNAPSHOT_TIME_NS");
        m_interval_ns = parse_non_negative(
            "QBOX_APOLLO_TIMER_SNAPSHOT_INTERVAL_NS");
        if (m_interval_ns == 0) {
            throw std::invalid_argument("snapshot interval must be positive");
        }
        if (m_start_ns >
            std::numeric_limits<int64_t>::max() - m_interval_ns) {
            throw std::invalid_argument("snapshot end time exceeds int64 range");
        }
    }
    SC_THREAD(run);
}

void apollo_timer_snapshot::run()
{
    if (!m_enabled) {
        return;
    }
    try {
        wait_until(m_start_ns);
        m_samples.push_back(capture("start", m_start_ns));
        wait_until(m_start_ns + m_interval_ns);
        m_samples.push_back(capture("end", m_start_ns + m_interval_ns));
        write_snapshot("pass", nullptr);
    } catch (const std::exception& error) {
        m_failure_detail = error.what();
        write_snapshot("fail", "snapshot_observation_failed");
    }
}

void apollo_timer_snapshot::end_of_simulation()
{
    if (!m_enabled || m_written) {
        return;
    }
    const char* reason = m_samples.empty() ?
        "snapshot_start_time_not_reached" : "snapshot_end_time_not_reached";
    write_snapshot("unavailable", reason);
}

void apollo_timer_snapshot::wait_until(int64_t absolute_ns)
{
    const int64_t now = current_time_ns();
    if (now > absolute_ns) {
        throw std::runtime_error("snapshot time already passed");
    }
    if (now < absolute_ns) {
        wait(sc_core::sc_time(absolute_ns - now, sc_core::SC_NS));
    }
}

void module_register()
{
    GSC_MODULE_REGISTER_C(
        apollo_timer_snapshot,
        sc_core::sc_object*, sc_core::sc_object*, sc_core::sc_object*,
        sc_core::sc_object*, sc_core::sc_object*, sc_core::sc_object*,
        sc_core::sc_object*, sc_core::sc_object*, sc_core::sc_object*,
        sc_core::sc_object*);
}
