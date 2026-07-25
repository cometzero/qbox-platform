/* SPDX-License-Identifier: BSD-3-Clause */

#include <apollo_timer_snapshot.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <limits>
#include <sstream>
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

bool rse_mirror_enabled()
{
    const char* raw = std::getenv("QBOX_APOLLO_RSE_SMD_COUNTER_MIRROR");
    if (raw == nullptr || *raw == '\0') {
        return true;
    }
    const std::string value(raw);
    if (value == "1" || value == "true" || value == "yes" ||
        value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" ||
        value == "off") {
        return false;
    }
    throw std::invalid_argument(
        "invalid QBOX_APOLLO_RSE_SMD_COUNTER_MIRROR");
}

}

apollo_timer_snapshot::apollo_timer_snapshot(
    sc_core::sc_module_name name, sc_core::sc_object* smd,
    sc_core::sc_object* ap_cpu, sc_core::sc_object* ap_mmio,
    sc_core::sc_object* si0_cpu, sc_core::sc_object* si0_cntbase,
    sc_core::sc_object* si1_cpu, sc_core::sc_object* rse_counter,
    sc_core::sc_object* rse_timer0, sc_core::sc_object* rse_timer1,
    sc_core::sc_object* rse_timer2, sc_core::sc_object* rse_timer3)
    : sc_core::sc_module(name)
    , m_smd(require_object<host_gtimer>(smd, "host_gtimer"))
    , m_ap_cpu(require_object<qemu_arm_counter_mirror>(
          ap_cpu, "qemu_arm_counter_mirror"))
    , m_ap_mmio(require_object<qemu_arm_mmio_counter_mirror>(
          ap_mmio, "qemu_arm_mmio_counter_mirror"))
    , m_si0_cpu(require_object<qemu_arm_counter_mirror>(
          si0_cpu, "qemu_arm_counter_mirror"))
    , m_si0_cntbase(require_object<host_gtimer>(
          si0_cntbase, "host_gtimer"))
    , m_si1_cpu(require_object<qemu_arm_counter_mirror>(
          si1_cpu, "qemu_arm_counter_mirror"))
    , m_rse_counter(require_object<qemu_sse_counter>(
          rse_counter, "qemu_sse_counter"))
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
    m_rse_mirror = rse_mirror_enabled();
    if (m_enabled) {
        m_path = require_env("QBOX_APOLLO_TIMER_SNAPSHOT_PATH");
        m_run_id = require_env("QBOX_APOLLO_TIMER_SNAPSHOT_RUN_ID");
        m_start_ns = parse_non_negative("QBOX_APOLLO_TIMER_SNAPSHOT_TIME_NS");
        m_interval_ns = parse_non_negative(
            "QBOX_APOLLO_TIMER_SNAPSHOT_INTERVAL_NS");
        if (m_interval_ns == 0 ||
            m_start_ns >
                std::numeric_limits<int64_t>::max() - m_interval_ns) {
            throw std::invalid_argument("invalid timer snapshot interval");
        }
    }
    SC_THREAD(run);
}

int64_t apollo_timer_snapshot::current_time_ns()
{
    using Wide = unsigned __int128;
    const Wide numerator =
        Wide(sc_core::sc_time_stamp().value()) * 1000000000ULL;
    const sc_dt::uint64 ticks_per_second =
        sc_core::sc_time(1, sc_core::SC_SEC).value();
    return static_cast<int64_t>(numerator / ticks_per_second);
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

std::string apollo_timer_snapshot::observation_fields(
    uint64_t observed_counter, int64_t observation_time_ns,
    const char* counter_basis)
{
    std::ostringstream out;
    out << "\"observed_counter\":" << observed_counter
        << ",\"observation_time_ns\":" << observation_time_ns
        << ",\"counter_basis\":\"" << counter_basis << "\"";
    return out.str();
}

std::string apollo_timer_snapshot::timer_fields(
    const qbox_platform::qemu_timer::ArmMMIOTimerSnapshot& snapshot,
    uint32_t irq)
{
    std::ostringstream out;
    out << "\"cval\":" << snapshot.cval
        << ",\"enabled\":" << ((snapshot.ctl & 1u) ? "true" : "false")
        << ",\"masked\":" << ((snapshot.ctl & 2u) ? "true" : "false")
        << ",\"istatus\":" << ((snapshot.ctl & 4u) ? "true" : "false")
        << ",\"irq\":" << irq;
    return out.str();
}

std::string apollo_timer_snapshot::timer_fields(
    const qbox_platform::qemu_timer::ArmSSETimerSnapshot& snapshot,
    uint32_t irq)
{
    std::ostringstream out;
    out << "\"cval\":" << snapshot.cval
        << ",\"enabled\":" << ((snapshot.ctl & 1u) ? "true" : "false")
        << ",\"masked\":" << ((snapshot.ctl & 2u) ? "true" : "false")
        << ",\"istatus\":" << ((snapshot.ctl & 4u) ? "true" : "false")
        << ",\"irq\":" << irq;
    return out.str();
}

std::string apollo_timer_snapshot::capture(
    const char* name, int64_t expected_ns)
{
    if (current_time_ns() != expected_ns) {
        throw std::runtime_error("SystemC snapshot timestamp mismatch");
    }
    const host_gtimer::FrontendSnapshot smd =
        m_smd.snapshot_at(sc_core::sc_time_stamp());
    const host_gtimer::FrontendSnapshot si0 =
        m_si0_cntbase.snapshot_at(sc_core::sc_time_stamp());
    const uint64_t ap_cpu = m_ap_cpu.synchronize();
    const uint64_t ap_mmio = m_ap_mmio.synchronize();
    const uint64_t si0_cpu = m_si0_cpu.synchronize();
    const uint64_t si1_cpu = m_si1_cpu.synchronize();
    qemu_sse_counter_mirror* rse_counter_mirror =
        dynamic_cast<qemu_sse_counter_mirror*>(&m_rse_counter);
    if (rse_counter_mirror == nullptr) {
        if (m_rse_mirror) {
            throw std::runtime_error(
                "RSE mirror mode requires qemu_sse_counter_mirror");
        }
    }
    const uint64_t rse_counter =
        rse_counter_mirror != nullptr
            ? rse_counter_mirror->synchronize()
            : m_rse_counter.local_count();
    const auto ap_ns = m_ap_mmio.snapshot(0);
    const auto ap_s = m_ap_mmio.snapshot(1);
    const auto rse0 = m_rse_timer0.snapshot();
    const auto rse1 = m_rse_timer1.snapshot();
    const auto rse2 = m_rse_timer2.snapshot();
    const auto rse3 = m_rse_timer3.snapshot();

    if (!smd.observed || !si0.observed || smd.counter != si0.counter) {
        throw std::runtime_error("shared CSS SystemC views differ");
    }

    const uint64_t css = smd.counter;
    const uint64_t rse_visible = m_rse_mirror ? css : rse_counter;
    const char* rse_basis = m_rse_mirror ? "css_mirror" : "rse_local";
    const char* rse_reset =
        m_rse_mirror ? "css_authority" : "rse_local_aon";

    std::ostringstream out;
    out << "{\"name\":\"" << name << "\",\"marker\":\"" << name
        << "\",\"sim_time_ns\":" << expected_ns << ",\"views\":{"
        << "\"smd\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"input_frequency_hz\":" << smd.input_frequency_hz
        << ",\"increment\":"
        << (smd.increment_8_24 >>
            gs::arm_system_counter::fractional_bits)
        << ",\"reported_frequency_hz\":" << smd.reported_frequency_hz
        << "," << observation_fields(css, expected_ns,
                                      "common_sample_time")
        << ",\"observed\":true},"
        << "\"ap_cpu0\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"reported_frequency_hz\":125000000,"
        << observation_fields(ap_cpu, expected_ns, "qemu_local_mirror")
        << ",\"observed\":true},"
        << "\"ap_refclk_ns\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"reported_frequency_hz\":" << ap_ns.cntfrq << ","
        << timer_fields(ap_ns, 49) << ","
        << observation_fields(ap_mmio, expected_ns,
                              "qemu_local_mirror")
        << ",\"observed\":true},"
        << "\"ap_refclk_s\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"reported_frequency_hz\":" << ap_s.cntfrq << ","
        << timer_fields(ap_s, 48) << ","
        << observation_fields(ap_mmio, expected_ns,
                              "qemu_local_mirror")
        << ",\"access_control_state\":\"enabled\",\"observed\":true},"
        << "\"si0_cpu0\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"reported_frequency_hz\":125000000,"
        << observation_fields(si0_cpu, expected_ns, "qemu_local_mirror")
        << ",\"observed\":true},"
        << "\"si0_cntbase\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"reported_frequency_hz\":" << si0.reported_frequency_hz
        << "," << observation_fields(si0.counter, expected_ns,
                                      "common_sample_time")
        << ",\"observed\":true},"
        << "\"si1_cpu0\":{\"domain\":\"css\",\"counter\":" << css
        << ",\"reported_frequency_hz\":100000000,"
        << observation_fields(si1_cpu, expected_ns, "qemu_local_mirror")
        << ",\"observed\":true},";
    const qbox_platform::qemu_timer::ArmSSETimerSnapshot rse[] = {
        rse0, rse1, rse2, rse3
    };
    const uint32_t irq[] = {3, 4, 5, 27};
    for (unsigned int i = 0; i < 4; ++i) {
        if (i != 0) {
            out << ',';
        }
        out << "\"rse_timer" << i
            << "\":{\"domain\":\"rse\",\"counter\":" << rse_visible
            << ",\"input_frequency_hz\":"
            << rse[i].counter_frequency_hz
            << ",\"reported_frequency_hz\":" << rse[i].cntfrq
            << "," << timer_fields(rse[i], irq[i])
            << "," << observation_fields(
                   rse_counter, expected_ns, rse_basis)
            << ",\"reset_domain\":\"" << rse_reset
            << "\",\"observed\":true}";
    }
    out << "}}";
    return out.str();
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
    write_snapshot(
        "unavailable",
        m_samples.empty() ? "snapshot_start_time_not_reached"
                          : "snapshot_end_time_not_reached");
}

std::string apollo_timer_snapshot::escape(const std::string& value)
{
    std::ostringstream out;
    for (char character : value) {
        switch (character) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << character; break;
        }
    }
    return out.str();
}

std::string apollo_timer_snapshot::captured_at()
{
    const std::time_t now = std::time(nullptr);
    std::tm utc {};
    gmtime_r(&now, &utc);
    char value[32] = {};
    std::strftime(value, sizeof(value), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return value;
}

void apollo_timer_snapshot::write_snapshot(
    const char* status, const char* reason)
{
    if (m_written) {
        return;
    }
    std::ostringstream json;
    json << "{\n  \"schema_version\": 1,\n"
         << "  \"producer\": \"qbox\",\n"
         << "  \"status\": \"" << status << "\",\n"
         << "  \"captured_at\": \"" << captured_at() << "\",\n"
         << "  \"source\": {\"machine\": \"apollo-qvp\", "
         << "\"revision\": \"workspace\", \"run_id\": \""
         << escape(m_run_id)
         << "\", \"rse_smd_counter_mirror\": "
         << (m_rse_mirror ? "true" : "false") << "},\n";
    if (reason != nullptr) {
        json << "  \"reason\": \"" << reason << "\",\n";
    }
    if (!m_failure_detail.empty()) {
        json << "  \"detail\": \"" << escape(m_failure_detail) << "\",\n";
    }
    json << "  \"samples\": [";
    for (size_t i = 0; i < m_samples.size(); ++i) {
        json << (i == 0 ? "\n    " : ",\n    ") << m_samples[i];
    }
    json << (m_samples.empty() ? "" : "\n  ") << "]\n}\n";

    const std::string temporary = m_path + ".tmp." + m_run_id;
    {
        std::ofstream output(temporary, std::ios::out | std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "cannot open timer snapshot temporary file");
        }
        output << json.str();
        output.flush();
        if (!output) {
            throw std::runtime_error("cannot write timer snapshot");
        }
    }
    if (std::rename(temporary.c_str(), m_path.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot publish timer snapshot");
    }
    m_written = true;
}

void module_register()
{
    GSC_MODULE_REGISTER_C(
        apollo_timer_snapshot,
        sc_core::sc_object*, sc_core::sc_object*, sc_core::sc_object*,
        sc_core::sc_object*, sc_core::sc_object*, sc_core::sc_object*,
        sc_core::sc_object*, sc_core::sc_object*, sc_core::sc_object*,
        sc_core::sc_object*, sc_core::sc_object*);
}
