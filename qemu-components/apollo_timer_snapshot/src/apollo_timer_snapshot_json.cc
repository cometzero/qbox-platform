/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <apollo_timer_snapshot.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>

std::string apollo_timer_snapshot::escape(const std::string& value)
{
    std::ostringstream out;
    for (char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << c; break;
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

void apollo_timer_snapshot::write_snapshot(const char* status,
                                           const char* reason)
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
         << escape(m_run_id) << "\", \"configured_start_time_ns\": \""
         << m_start_ns << "\", \"configured_end_time_ns\": \""
         << (m_start_ns + m_interval_ns) << "\"},\n";
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
            throw std::runtime_error("cannot open timer snapshot temporary file");
        }
        output << json.str();
        output.flush();
        if (!output) {
            throw std::runtime_error("cannot write timer snapshot temporary file");
        }
    }
    if (std::rename(temporary.c_str(), m_path.c_str()) != 0) {
        std::remove(temporary.c_str());
        throw std::runtime_error("cannot atomically publish timer snapshot");
    }
    m_written = true;
}
