/* SPDX-License-Identifier: BSD-3-Clause */

#include "apollo_runtime_injection.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

using gs::RuntimeActionReply;
using gs::RuntimeActionSchema;
using gs::RuntimeActionState;
using gs::RuntimeActionValue;
using gs::RuntimeResourceType;
using gs::RuntimeTargetCapability;

const char* state_name(RuntimeActionState state)
{
    switch (state) {
    case RuntimeActionState::ACCEPTED:
        return "accepted";
    case RuntimeActionState::SCHEDULED:
        return "scheduled";
    case RuntimeActionState::ACTIVE:
        return "active";
    case RuntimeActionState::COMPLETED:
        return "completed";
    case RuntimeActionState::CANCELLED:
        return "cancelled";
    case RuntimeActionState::FAILED:
        return "failed";
    }
    return "failed";
}

std::string json_string(const std::string& value)
{
    std::ostringstream result;
    for (const char c : value) {
        switch (c) {
        case '\\':
            result << "\\\\";
            break;
        case '"':
            result << "\\\"";
            break;
        case '\n':
            result << "\\n";
            break;
        case '\r':
            result << "\\r";
            break;
        case '\t':
            result << "\\t";
            break;
        default:
            result << c;
            break;
        }
    }
    return result.str();
}

const RuntimeActionValue* parameter(
    const gs::RuntimeActionRequest& request, const std::string& name)
{
    const auto it = request.parameters.find(name);
    return it == request.parameters.end() ? nullptr : &it->second;
}

bool boolean_parameter(const gs::RuntimeActionRequest& request,
                       const std::string& name, bool& value)
{
    const auto* item = parameter(request, name);
    if (item == nullptr || item->type() != RuntimeActionValue::Type::BOOLEAN) {
        return false;
    }
    value = item->boolean();
    return true;
}

bool unsigned_parameter(const gs::RuntimeActionRequest& request,
                        const std::string& name, uint64_t& value)
{
    const auto* item = parameter(request, name);
    if (item == nullptr ||
        item->type() != RuntimeActionValue::Type::UNSIGNED_INTEGER) {
        return false;
    }
    value = item->unsigned_integer();
    return true;
}

bool string_parameter(const gs::RuntimeActionRequest& request,
                      const std::string& name, std::string& value)
{
    const auto* item = parameter(request, name);
    if (item == nullptr || item->type() != RuntimeActionValue::Type::STRING) {
        return false;
    }
    value = item->string();
    return true;
}

RuntimeActionSchema schema(
    const std::string& name,
    std::initializer_list<const char*> required = {})
{
    RuntimeActionSchema result;
    result.name = name;
    for (const auto* item : required) {
        result.required_parameters.emplace_back(item);
    }
    return result;
}

}

gicx00_multiview& apollo_runtime_injection::require_gic(
    sc_core::sc_object* object)
{
    auto* result = dynamic_cast<gicx00_multiview*>(object);
    if (result == nullptr) {
        throw std::invalid_argument(
            "apollo_runtime_injection requires gicx00_multiview");
    }
    return *result;
}

arm_gicv3* apollo_runtime_injection::optional_gic_backend(
    sc_core::sc_object* object)
{
    if (object == nullptr) {
        return nullptr;
    }
    auto* result = dynamic_cast<arm_gicv3*>(object);
    if (result == nullptr) {
        throw std::invalid_argument(
            "apollo_runtime_injection requires arm_gicv3 backends");
    }
    return result;
}

zena_ssu& apollo_runtime_injection::require_ssu(sc_core::sc_object* object)
{
    auto* result = dynamic_cast<zena_ssu*>(object);
    if (result == nullptr) {
        throw std::invalid_argument(
            "apollo_runtime_injection requires zena_ssu");
    }
    return *result;
}

gs::arm_system_counter& apollo_runtime_injection::require_counter(
    sc_core::sc_object* object)
{
    auto* result = dynamic_cast<gs::arm_system_counter*>(object);
    if (result == nullptr) {
        throw std::invalid_argument(
            "apollo_runtime_injection requires arm_system_counter");
    }
    return *result;
}

qemu_pl061& apollo_runtime_injection::require_gpio(sc_core::sc_object* object)
{
    auto* result = dynamic_cast<qemu_pl061*>(object);
    if (result == nullptr) {
        throw std::invalid_argument(
            "apollo_runtime_injection requires qemu_pl061");
    }
    return *result;
}

uint64_t apollo_runtime_injection::simulation_time_ns(
    const sc_core::sc_time& time)
{
    const auto ticks_per_ns = sc_core::sc_time(1, sc_core::SC_NS).value();
    return ticks_per_ns == 0 ? 0 : time.value() / ticks_per_ns;
}

bool apollo_runtime_injection::simulation_time_from_ns(
    uint64_t value, sc_core::sc_time& time)
{
    const auto ticks_per_ns = sc_core::sc_time(1, sc_core::SC_NS).value();
    if (ticks_per_ns == 0 ||
        value > std::numeric_limits<uint64_t>::max() / ticks_per_ns) {
        return false;
    }
    time = sc_core::sc_time::from_value(value * ticks_per_ns);
    return true;
}

bool apollo_runtime_injection::add_simulation_time_ns(
    const sc_core::sc_time& base, uint64_t value, sc_core::sc_time& time)
{
    sc_core::sc_time delta;
    if (!simulation_time_from_ns(value, delta) ||
        base.value() > std::numeric_limits<uint64_t>::max() - delta.value()) {
        return false;
    }
    time = sc_core::sc_time::from_value(base.value() + delta.value());
    return true;
}

bool apollo_runtime_injection::is_terminal(RuntimeActionState state)
{
    return state == RuntimeActionState::COMPLETED ||
           state == RuntimeActionState::CANCELLED ||
           state == RuntimeActionState::FAILED;
}

bool apollo_runtime_injection::reserves_target(ActionKind kind)
{
    return kind == ActionKind::GIC_PULSE ||
           kind == ActionKind::COUNTER_CONTROL ||
           kind == ActionKind::GPIO_DRIVE ||
           kind == ActionKind::GPIO_PULSE ||
           kind == ActionKind::MHU_DROP_DOORBELL ||
           kind == ActionKind::SIGNAL_FAULT ||
           kind == ActionKind::SYSTEM_RESET_PULSE;
}

gs::RuntimeActionStatusReply apollo_runtime_injection::error_reply(
    int status, const std::string& code, const std::string& message)
{
    gs::RuntimeActionStatusReply reply;
    reply.http_status = status;
    reply.error_code = code;
    reply.error_message = message;
    return reply;
}

apollo_runtime_injection::apollo_runtime_injection(
    sc_core::sc_module_name name, sc_core::sc_object* gic,
    sc_core::sc_object* gic_view1_backend,
    sc_core::sc_object* gic_view2_backend,
    sc_core::sc_object* ssu, sc_core::sc_object* counter,
    sc_core::sc_object* host_smd_gpio, sc_core::sc_object* rse_gpio_0,
    sc_core::sc_object* rse_gpio_1)
    : sc_core::sc_module(name)
    , m_gic(require_gic(gic))
    , m_gic_backends{ { optional_gic_backend(gic_view1_backend),
                        optional_gic_backend(gic_view2_backend) } }
    , m_ssu(require_ssu(ssu))
    , m_counter(require_counter(counter))
    , m_gpio_controllers{ { &require_gpio(host_smd_gpio),
                           &require_gpio(rse_gpio_0),
                           &require_gpio(rse_gpio_1) } }
    , m_counter_baseline(m_counter.snapshot())
    , m_gpio_output_observers(
          "gpio_output_observer", 24,
          [](const char* socket_name, std::size_t) {
              return new TargetSignalSocket<bool>(socket_name);
          })
    , p_mhu_target("mhu_target", std::string(""))
    , p_signal_target("signal_target", std::string(""))
    , reset("reset")
    , system_reset("system_reset")
{
    const std::array<std::string, 3> ids = {
        { "host_smd", "rse0", "rse1" }
    };
    const std::array<std::string, 3> names = {
        { "platform.host_smd_gpio", "platform.rse_gpio_0",
          "platform.rse_gpio_1" }
    };
    for (std::size_t controller = 0;
         controller < m_gpio_controllers.size(); ++controller) {
        for (std::size_t pin = 0; pin < 8; ++pin) {
            GpioTarget target;
            target.id = "apollo.gpio." + ids[controller] + ".pin" +
                        std::to_string(pin);
            target.controller_name = names[controller];
            target.controller = m_gpio_controllers[controller];
            target.pin = pin;
            m_gpio_targets.push_back(target);
        }
    }

    reset.register_value_changed_cb(
        [this](bool asserted) { reset_changed(asserted); });
    SC_THREAD(schedule_thread);
    SC_THREAD(reset_release_thread);
}

apollo_runtime_injection::apollo_runtime_injection(
    sc_core::sc_module_name name, sc_core::sc_object* gic,
    sc_core::sc_object* ssu, sc_core::sc_object* counter,
    sc_core::sc_object* host_smd_gpio, sc_core::sc_object* rse_gpio_0,
    sc_core::sc_object* rse_gpio_1)
    : apollo_runtime_injection(
          name, gic, nullptr, nullptr, ssu, counter, host_smd_gpio,
          rse_gpio_0, rse_gpio_1)
{
}

void apollo_runtime_injection::before_end_of_elaboration()
{
    std::size_t observer = 0;
    for (auto* controller : m_gpio_controllers) {
        for (std::size_t pin = 0; pin < 8; ++pin, ++observer) {
            if (controller->gpio_out[pin].bind_count() == 0) {
                controller->gpio_out[pin].bind(
                    m_gpio_output_observers[observer]);
            }
        }
    }
}

void apollo_runtime_injection::end_of_elaboration()
{
    if (!p_mhu_target.get_value().empty()) {
        m_mhu = dynamic_cast<mhu320ae*>(
            sc_core::sc_find_object(p_mhu_target.get_value().c_str()));
        if (m_mhu == nullptr) {
            SC_REPORT_FATAL(name(), "configured MHU runtime target is invalid");
        }
    }
    if (!p_signal_target.get_value().empty()) {
        m_signal_fault = dynamic_cast<gs::signal_fault_injector*>(
            sc_core::sc_find_object(p_signal_target.get_value().c_str()));
        if (m_signal_fault == nullptr) {
            SC_REPORT_FATAL(name(), "configured signal fault target is invalid");
        }
    }
}

const apollo_runtime_injection::GpioTarget*
apollo_runtime_injection::find_gpio(const std::string& target) const
{
    const auto found = std::find_if(
        m_gpio_targets.begin(), m_gpio_targets.end(),
        [&target](const GpioTarget& gpio) { return gpio.id == target; });
    return found == m_gpio_targets.end() ? nullptr : &*found;
}

apollo_runtime_injection::RequestRecord*
apollo_runtime_injection::find_active(const std::string& target)
{
    const auto active = m_active_targets.find(target);
    if (active == m_active_targets.end()) {
        return nullptr;
    }
    const auto request = m_requests.find(active->second);
    return request == m_requests.end() ? nullptr : &request->second;
}

gs::RuntimeActionStatus apollo_runtime_injection::effective_status(
    const RequestRecord& record) const
{
    auto status = record.status;
    if (status.state != RuntimeActionState::ACTIVE) {
        return status;
    }
    if (record.kind == ActionKind::MHU_DROP_DOORBELL && m_mhu != nullptr) {
        const auto state = m_mhu->runtime_doorbell_fault_snapshot();
        if (!state.armed && state.match_count > record.match_count) {
            status.state = RuntimeActionState::COMPLETED;
            status.result = "consumed";
            status.has_cleared_sim_time_ns = true;
            status.cleared_sim_time_ns =
                simulation_time_ns(sc_core::sc_time_stamp());
        }
    } else if (record.kind == ActionKind::SIGNAL_FAULT &&
               m_signal_fault != nullptr) {
        const auto state = m_signal_fault->snapshot();
        const bool consumed =
            record.request.action == "drop-next-assert" &&
            !state.drop_pending && state.match_count > record.match_count;
        const bool expired = record.request.action == "pulse" &&
                             !state.pulse_active;
        if (consumed || expired) {
            status.state = RuntimeActionState::COMPLETED;
            status.result = consumed ? "consumed" : "ok";
            status.has_cleared_sim_time_ns = true;
            status.cleared_sim_time_ns =
                simulation_time_ns(sc_core::sc_time_stamp());
        }
    }
    return status;
}

void apollo_runtime_injection::refresh_external_requests()
{
    for (auto& item : m_requests) {
        auto& record = item.second;
        const auto refreshed = effective_status(record);
        if (record.status.state == RuntimeActionState::ACTIVE &&
            refreshed.state == RuntimeActionState::COMPLETED) {
            record.status = refreshed;
            release_target(record);
            audit(record, "injection_cleared");
            audit(record, "injection_completed");
        }
    }
}

void apollo_runtime_injection::release_target(RequestRecord& record)
{
    const auto active = m_active_targets.find(record.request.target);
    if (active != m_active_targets.end() &&
        active->second == record.status.id) {
        m_active_targets.erase(active);
    }
}

void apollo_runtime_injection::cancel_scheduled(
    RequestRecord& record, const std::string& result)
{
    release_target(record);
    record.status.state = RuntimeActionState::CANCELLED;
    record.status.result = result;
    audit(record, "injection_cancelled");
}

bool apollo_runtime_injection::prune_history()
{
    while (m_requests.size() >= MAX_REQUEST_HISTORY) {
        const auto terminal = std::find_if(
            m_requests.begin(), m_requests.end(),
            [](const std::pair<const uint64_t, RequestRecord>& request) {
                return is_terminal(request.second.status.state);
            });
        if (terminal == m_requests.end()) {
            return false;
        }
        m_requests.erase(terminal);
    }
    return true;
}

void apollo_runtime_injection::audit(
    const RequestRecord& record, const char* event) const
{
    std::ostringstream message;
    message << "{\"schema\":\"qbox-injection-trace/v1\""
            << ",\"event\":\"" << event << "\""
            << ",\"id\":" << record.status.id
            << ",\"target\":\"" << json_string(record.status.target) << "\""
            << ",\"action\":\"" << json_string(record.status.action) << "\""
            << ",\"state\":\"" << state_name(record.status.state) << "\""
            << ",\"generation\":" << record.status.generation;
    if (record.status.has_requested_sim_time_ns) {
        message << ",\"requested_sim_time_ns\":"
                << record.status.requested_sim_time_ns;
    }
    if (record.status.has_applied_sim_time_ns) {
        message << ",\"applied_sim_time_ns\":"
                << record.status.applied_sim_time_ns;
    }
    if (record.status.has_cleared_sim_time_ns) {
        message << ",\"cleared_sim_time_ns\":"
                << record.status.cleared_sim_time_ns;
    }
    message << ",\"result\":\"" << json_string(record.status.result)
            << "\"}";
    SC_REPORT_INFO(name(), message.str().c_str());
}

void apollo_runtime_injection::audit_reset() const
{
    std::ostringstream message;
    message << "{\"schema\":\"qbox-injection-trace/v1\""
            << ",\"event\":\"reset_generation_changed\""
            << ",\"generation\":" << m_generation
            << ",\"sim_time_ns\":"
            << simulation_time_ns(sc_core::sc_time_stamp()) << "}";
    SC_REPORT_INFO(name(), message.str().c_str());
}

std::vector<RuntimeTargetCapability>
apollo_runtime_injection::capabilities() const
{
    std::vector<RuntimeTargetCapability> result;

    RuntimeTargetCapability gic;
    gic.target = "platform.si_gic_multiview";
    gic.resource_type = RuntimeResourceType::INTERRUPT;
    gic.actions = { schema("pulse-spi", { "view", "intid",
                                           "duration_ns" }) };
    result.push_back(gic);

    RuntimeTargetCapability ssu;
    ssu.target = "apollo.event.si-cl0-ssu-fault";
    ssu.resource_type = RuntimeResourceType::EVENT;
    ssu.actions = { schema("trigger-fault", { "critical" }) };
    result.push_back(ssu);

    RuntimeTargetCapability counter;
    counter.target = "apollo.control.css-system-counter";
    counter.resource_type = RuntimeResourceType::CONTROL;
    counter.actions = {
        schema("set-control", { "enabled", "halt_on_debug" }),
        schema("clear")
    };
    result.push_back(counter);

    RuntimeTargetCapability system_reset_capability;
    system_reset_capability.target = "apollo.control.system-reset";
    system_reset_capability.resource_type = RuntimeResourceType::CONTROL;
    system_reset_capability.actions = {
        schema("pulse", { "duration_ns" })
    };
    system_reset_capability.attributes.emplace(
        "reset_domain", RuntimeActionValue(std::string("system")));
    result.push_back(system_reset_capability);

    if (m_mhu != nullptr) {
        RuntimeTargetCapability mhu;
        mhu.target = p_mhu_target.get_value();
        mhu.resource_type = RuntimeResourceType::EVENT;
        mhu.actions = {
            schema("drop-next-doorbell", { "channel" })
        };
        mhu.attributes.emplace(
            "reset_domain", RuntimeActionValue(std::string("ap")));
        result.push_back(std::move(mhu));
    }

    if (m_signal_fault != nullptr) {
        RuntimeTargetCapability signal;
        signal.target = "apollo.irq.i2c5";
        signal.resource_type = RuntimeResourceType::INTERRUPT;
        signal.actions = {
            schema("pass"), schema("drop-next-assert"),
            schema("force-high"), schema("force-low"),
            schema("pulse", { "duration_ns" })
        };
        signal.attributes.emplace(
            "reset_domain", RuntimeActionValue(std::string("ap")));
        result.push_back(std::move(signal));
    }

    for (const auto& gpio : m_gpio_targets) {
        RuntimeTargetCapability pin;
        pin.target = gpio.id;
        pin.resource_type = RuntimeResourceType::GPIO_PIN;
        pin.actions = {
            schema("read"), schema("observe"), schema("drive-high"),
            schema("drive-low"),
            schema("pulse", { "active_level", "duration_ns" }),
            schema("release"), schema("set-direction", { "direction" }),
            schema("write-output-high"), schema("write-output-low")
        };
        pin.attributes.emplace(
            "controller", RuntimeActionValue(gpio.controller_name));
        pin.attributes.emplace(
            "pin", RuntimeActionValue(static_cast<uint64_t>(gpio.pin)));
        pin.attributes.emplace("active_low", RuntimeActionValue(false));
        pin.attributes.emplace("fault_override", RuntimeActionValue(false));
        result.push_back(std::move(pin));
    }
    return result;
}

gs::RuntimeTargetSnapshotReply apollo_runtime_injection::target_snapshot(
    const std::string& target) const
{
    gs::RuntimeTargetSnapshotReply reply;
    reply.value.target = target;

    if (target == "platform.si_gic_multiview") {
        reply.value.resource_type = RuntimeResourceType::INTERRUPT;
        reply.value.values.emplace(
            "generation", RuntimeActionValue(m_generation));
        reply.value.values.emplace(
            "reset_asserted", RuntimeActionValue(m_reset_asserted));
        return reply;
    }
    if (target == "apollo.event.si-cl0-ssu-fault") {
        reply.value.resource_type = RuntimeResourceType::EVENT;
        reply.value.values.emplace(
            "fault_active",
            RuntimeActionValue(m_ssu.runtime_fault_active()));
        reply.value.values.emplace(
            "system_status",
            RuntimeActionValue(
                static_cast<uint64_t>(
                    m_ssu.runtime_system_status())));
        reply.value.values.emplace(
            "generation", RuntimeActionValue(m_generation));
        return reply;
    }
    if (target == "apollo.control.css-system-counter") {
        const auto state = m_counter.snapshot_at(sc_core::sc_time_stamp());
        reply.value.resource_type = RuntimeResourceType::CONTROL;
        reply.value.values.emplace("enabled", RuntimeActionValue(state.enabled));
        reply.value.values.emplace(
            "halt_on_debug", RuntimeActionValue(state.halt_on_debug));
        reply.value.values.emplace("running", RuntimeActionValue(state.running()));
        reply.value.values.emplace(
            "generation", RuntimeActionValue(state.generation));
        return reply;
    }
    if (target == "apollo.control.system-reset") {
        reply.value.resource_type = RuntimeResourceType::CONTROL;
        reply.value.values.emplace(
            "asserted", RuntimeActionValue(m_reset_pulse_active));
        reply.value.values.emplace(
            "generation", RuntimeActionValue(m_generation));
        return reply;
    }
    if (m_mhu != nullptr && target == p_mhu_target.get_value()) {
        const auto state = m_mhu->runtime_doorbell_fault_snapshot();
        reply.value.resource_type = RuntimeResourceType::EVENT;
        reply.value.values.emplace("armed", RuntimeActionValue(state.armed));
        reply.value.values.emplace(
            "channel", RuntimeActionValue(static_cast<uint64_t>(state.channel)));
        reply.value.values.emplace(
            "match_count", RuntimeActionValue(state.match_count));
        reply.value.values.emplace(
            "generation", RuntimeActionValue(m_generation));
        return reply;
    }
    if (m_signal_fault != nullptr && target == "apollo.irq.i2c5") {
        const auto state = m_signal_fault->snapshot();
        reply.value.resource_type = RuntimeResourceType::INTERRUPT;
        reply.value.values.emplace(
            "action", RuntimeActionValue(state.action));
        reply.value.values.emplace(
            "source_level", RuntimeActionValue(state.source_level));
        reply.value.values.emplace(
            "output_level", RuntimeActionValue(state.output_level));
        reply.value.values.emplace(
            "drop_pending", RuntimeActionValue(state.drop_pending));
        reply.value.values.emplace(
            "pulse_active", RuntimeActionValue(state.pulse_active));
        reply.value.values.emplace(
            "match_count", RuntimeActionValue(state.match_count));
        reply.value.values.emplace(
            "generation", RuntimeActionValue(m_generation));
        return reply;
    }

    const auto* gpio = find_gpio(target);
    if (gpio == nullptr) {
        gs::RuntimeTargetSnapshotReply missing;
        missing.http_status = 404;
        missing.error_code = "target-not-found";
        missing.error_message = "runtime target is not allow-listed";
        return missing;
    }

    qemu_pl061::RuntimePinSnapshot state;
    if (!gpio->controller->runtime_pin_snapshot(gpio->pin, state)) {
        gs::RuntimeTargetSnapshotReply unavailable;
        unavailable.http_status = 503;
        unavailable.error_code = "target-unavailable";
        unavailable.error_message = "PL061 runtime state is not ready";
        return unavailable;
    }
    reply.value.resource_type = RuntimeResourceType::GPIO_PIN;
    reply.value.values.emplace(
        "controller", RuntimeActionValue(gpio->controller_name));
    reply.value.values.emplace(
        "pin", RuntimeActionValue(static_cast<uint64_t>(gpio->pin)));
    reply.value.values.emplace(
        "direction", RuntimeActionValue(
            state.direction_output ? std::string("output") :
                                     std::string("input")));
    reply.value.values.emplace(
        "level", RuntimeActionValue(
            state.direction_output ? state.data_level : state.input_level));
    reply.value.values.emplace(
        "input_level", RuntimeActionValue(state.input_level));
    reply.value.values.emplace(
        "output_level", RuntimeActionValue(state.data_level));
    reply.value.values.emplace(
        "observed_output_level",
        RuntimeActionValue(
            gpio->controller->gpio_out[gpio->pin]->read()));
    reply.value.values.emplace(
        "reset_default", RuntimeActionValue(state.initial_input_level));
    reply.value.values.emplace(
        "override_active",
        RuntimeActionValue(m_active_targets.count(target) != 0));
    reply.value.values.emplace(
        "generation", RuntimeActionValue(m_generation));
    return reply;
}

gs::RuntimeActionStatusReply apollo_runtime_injection::validate_request(
    const gs::RuntimeActionRequest& request, RequestRecord& record) const
{
    if (request.schema_version != 1) {
        return error_reply(400, "invalid-request",
                           "schema_version must be 1");
    }
    if (!request.clear_on_reset) {
        return error_reply(400, "unsupported-reset-policy",
                           "clear_on_reset must be true");
    }
    if (request.has_expected_generation &&
        request.expected_generation != m_generation) {
        return error_reply(409, "stale-generation",
                           "expected_generation does not match reset state");
    }

    if (request.target == "platform.si_gic_multiview") {
        if (request.action != "pulse-spi" ||
            !unsigned_parameter(request, "view", record.view) ||
            !unsigned_parameter(request, "intid", record.intid) ||
            !unsigned_parameter(
                request, "duration_ns", record.duration_ns) ||
            record.duration_ns == 0) {
            return error_reply(400, "invalid-request",
                               "pulse-spi requires view, intid and duration_ns");
        }
        record.kind = ActionKind::GIC_PULSE;
        return {};
    }

    if (request.target == "apollo.event.si-cl0-ssu-fault") {
        if (request.action != "trigger-fault" ||
            !boolean_parameter(request, "critical", record.bool_value)) {
            return error_reply(400, "invalid-request",
                               "trigger-fault requires critical");
        }
        record.kind = ActionKind::SSU_FAULT;
        return {};
    }

    if (request.target == "apollo.control.css-system-counter") {
        if (request.action == "clear") {
            record.kind = ActionKind::COUNTER_CLEAR;
            return {};
        }
        if (request.action != "set-control" ||
            !boolean_parameter(request, "enabled", record.bool_value) ||
            !boolean_parameter(request, "halt_on_debug",
                               record.secondary_bool_value)) {
            return error_reply(
                400, "invalid-request",
                "set-control requires enabled and halt_on_debug");
        }
        record.kind = ActionKind::COUNTER_CONTROL;
        return {};
    }

    if (request.target == "apollo.control.system-reset") {
        if ((!request.reset_domain.empty() &&
             request.reset_domain != "system") ||
            request.action != "pulse" ||
            !unsigned_parameter(
                request, "duration_ns", record.duration_ns) ||
            record.duration_ns == 0) {
            return error_reply(
                400, "invalid-request",
                "system reset pulse requires reset_domain system and duration_ns");
        }
        record.kind = ActionKind::SYSTEM_RESET_PULSE;
        return {};
    }

    if (m_mhu != nullptr && request.target == p_mhu_target.get_value()) {
        if ((!request.reset_domain.empty() && request.reset_domain != "ap") ||
            request.action != "drop-next-doorbell" ||
            !unsigned_parameter(request, "channel", record.channel) ||
            record.channel > std::numeric_limits<unsigned int>::max()) {
            return error_reply(
                400, "invalid-request",
                "drop-next-doorbell requires reset_domain ap and channel");
        }
        record.kind = ActionKind::MHU_DROP_DOORBELL;
        return {};
    }

    if (m_signal_fault != nullptr && request.target == "apollo.irq.i2c5") {
        if (!request.reset_domain.empty() && request.reset_domain != "ap") {
            return error_reply(400, "invalid-request",
                               "I2C5 IRQ target uses reset_domain ap");
        }
        if (request.action == "pass") {
            record.kind = ActionKind::SIGNAL_CLEAR;
            return {};
        }
        if (request.action == "pulse" &&
            (!unsigned_parameter(
                 request, "duration_ns", record.duration_ns) ||
             record.duration_ns == 0)) {
            return error_reply(400, "invalid-request",
                               "signal pulse requires duration_ns");
        }
        if (request.action != "drop-next-assert" &&
            request.action != "force-high" &&
            request.action != "force-low" &&
            request.action != "pulse") {
            return error_reply(400, "unsupported-action",
                               "I2C5 IRQ action is not supported");
        }
        record.kind = ActionKind::SIGNAL_FAULT;
        return {};
    }

    if (find_gpio(request.target) == nullptr) {
        return error_reply(404, "target-not-found",
                           "runtime target is not allow-listed");
    }
    if (request.action == "read" || request.action == "observe") {
        record.kind = ActionKind::GPIO_READ;
    } else if (request.action == "drive-high" ||
               request.action == "drive-low") {
        record.kind = ActionKind::GPIO_DRIVE;
        record.bool_value = request.action == "drive-high";
    } else if (request.action == "pulse") {
        std::string level;
        if (!string_parameter(request, "active_level", level) ||
            (level != "high" && level != "low") ||
            !unsigned_parameter(
                request, "duration_ns", record.duration_ns) ||
            record.duration_ns == 0) {
            return error_reply(
                400, "invalid-request",
                "pulse requires high/low active_level and duration_ns");
        }
        record.kind = ActionKind::GPIO_PULSE;
        record.bool_value = level == "high";
    } else if (request.action == "release") {
        record.kind = ActionKind::GPIO_RELEASE;
    } else if (request.action == "set-direction") {
        std::string direction;
        if (!string_parameter(request, "direction", direction) ||
            (direction != "input" && direction != "output")) {
            return error_reply(
                400, "invalid-request",
                "set-direction requires input/output direction");
        }
        record.kind = ActionKind::GPIO_SET_DIRECTION;
        record.bool_value = direction == "output";
    } else if (request.action == "write-output-high" ||
               request.action == "write-output-low") {
        record.kind = ActionKind::GPIO_WRITE_OUTPUT;
        record.bool_value = request.action == "write-output-high";
    } else {
        return error_reply(400, "unsupported-action",
                           "action is not supported by this GPIO target");
    }
    return {};
}

bool apollo_runtime_injection::drive_gic(
    const RequestRecord& record, bool value, bool require_owner)
{
    const auto view = static_cast<unsigned int>(record.view);
    const auto intid = static_cast<uint32_t>(record.intid);
    if (m_gic_backends[0] == nullptr && m_gic_backends[1] == nullptr) {
        return m_gic.inject_spi(view, intid, value);
    }
    if (view < 1 || view > m_gic_backends.size() || intid < 32) {
        return false;
    }
    auto* backend = m_gic_backends[view - 1];
    if (backend == nullptr ||
        (require_owner && m_gic.runtime_spi_owner(intid) != view)) {
        return false;
    }
    return backend->runtime_set_spi(intid - 32, value);
}

gs::RuntimeActionStatusReply apollo_runtime_injection::apply(
    RequestRecord& record)
{
    if (m_reset_asserted) {
        record.status.state = RuntimeActionState::FAILED;
        record.status.result = "target-in-reset";
        release_target(record);
        audit(record, "injection_failed");
        auto reply = error_reply(409, "target-in-reset",
                                 "runtime target is in reset");
        reply.value = record.status;
        return reply;
    }

    const auto now = sc_core::sc_time_stamp();
    record.status.has_applied_sim_time_ns = true;
    record.status.applied_sim_time_ns = simulation_time_ns(now);

    if (record.kind == ActionKind::GIC_PULSE) {
        if (!add_simulation_time_ns(now, record.duration_ns, record.due)) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "simulation-time-overflow";
            release_target(record);
            audit(record, "injection_failed");
            auto reply = error_reply(
                400, "simulation-time-overflow",
                "SPI pulse exceeds simulation time range");
            reply.value = record.status;
            return reply;
        }
        if (!drive_gic(record, true, true)) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "target-not-owner";
            release_target(record);
            audit(record, "injection_failed");
            auto reply = error_reply(
                409, "target-not-owner",
                "SPI view is not the active owner or is not connected");
            reply.value = record.status;
            return reply;
        }
        record.status.state = RuntimeActionState::ACTIVE;
        record.status.result = "active";
        m_active_targets[record.request.target] = record.status.id;
        m_schedule_changed.notify(sc_core::SC_ZERO_TIME);
    } else if (record.kind == ActionKind::SSU_FAULT) {
        if (!m_ssu.inject_fault(record.bool_value)) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "fault-disabled";
            audit(record, "injection_failed");
            auto reply = error_reply(
                409, "fault-disabled",
                "SSU error detection or requested fault class is disabled");
            reply.value = record.status;
            return reply;
        }
        record.status.state = RuntimeActionState::COMPLETED;
        record.status.result = "ok";
    } else if (record.kind == ActionKind::COUNTER_CONTROL) {
        m_counter_baseline = m_counter.save_state_at(now);
        m_counter_override = true;
        m_counter.set_control_at(record.bool_value,
                                 record.secondary_bool_value, now);
        record.status.state = RuntimeActionState::ACTIVE;
        record.status.result = "active";
        m_active_targets[record.request.target] = record.status.id;
    } else if (record.kind == ActionKind::COUNTER_CLEAR) {
        auto* active = find_active(record.request.target);
        if (active == nullptr || !m_counter_override) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "no-active-action";
            audit(record, "injection_failed");
            auto reply = error_reply(409, "no-active-action",
                                     "counter control has no active override");
            reply.value = record.status;
            return reply;
        }
        clear(*active, "cleared", RuntimeActionState::COMPLETED);
        record.status.state = RuntimeActionState::COMPLETED;
        record.status.result = "ok";
    } else if (record.kind == ActionKind::MHU_DROP_DOORBELL) {
        const auto state = m_mhu->runtime_doorbell_fault_snapshot();
        record.match_count = state.match_count;
        if (!m_mhu->runtime_drop_next_doorbell(
                static_cast<unsigned int>(record.channel))) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "target-unavailable";
            release_target(record);
            audit(record, "injection_failed");
            auto reply = error_reply(
                409, "target-unavailable",
                "MHU doorbell fault cannot be armed in the current state");
            reply.value = record.status;
            return reply;
        }
        record.status.state = RuntimeActionState::ACTIVE;
        record.status.result = "active";
        m_active_targets[record.request.target] = record.status.id;
    } else if (record.kind == ActionKind::SIGNAL_FAULT) {
        const auto state = m_signal_fault->snapshot();
        record.match_count = state.match_count;
        if (!m_signal_fault->arm(record.request.action,
                                 record.duration_ns)) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "apply-failed";
            release_target(record);
            audit(record, "injection_failed");
            auto reply = error_reply(409, "apply-failed",
                                     "signal fault could not be armed");
            reply.value = record.status;
            return reply;
        }
        record.status.state = RuntimeActionState::ACTIVE;
        record.status.result = "active";
        m_active_targets[record.request.target] = record.status.id;
    } else if (record.kind == ActionKind::SIGNAL_CLEAR) {
        auto* active = find_active(record.request.target);
        if (active != nullptr) {
            clear(*active, "cleared", RuntimeActionState::COMPLETED);
        } else {
            m_signal_fault->clear();
        }
        record.status.state = RuntimeActionState::COMPLETED;
        record.status.result = "ok";
    } else if (record.kind == ActionKind::SYSTEM_RESET_PULSE) {
        if (m_reset_pulse_active ||
            !add_simulation_time_ns(
                now, record.duration_ns, m_reset_release_due)) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = m_reset_pulse_active ?
                                       "target-busy" :
                                       "simulation-time-overflow";
            release_target(record);
            audit(record, "injection_failed");
            auto reply = error_reply(
                m_reset_pulse_active ? 409 : 400,
                record.status.result,
                m_reset_pulse_active ?
                    "system reset pulse is already active" :
                    "reset pulse exceeds simulation time range");
            reply.value = record.status;
            return reply;
        }
        record.status.state = RuntimeActionState::COMPLETED;
        record.status.result = "ok";
        release_target(record);
        m_reset_pulse_active = true;
        m_reset_release_event.notify(sc_core::SC_ZERO_TIME);
        system_reset->write(true);
    } else {
        const auto* gpio = find_gpio(record.request.target);
        qemu_pl061::RuntimePinSnapshot state;
        if (gpio == nullptr ||
            !gpio->controller->runtime_pin_snapshot(gpio->pin, state)) {
            record.status.state = RuntimeActionState::FAILED;
            record.status.result = "target-unavailable";
            release_target(record);
            audit(record, "injection_failed");
            auto reply = error_reply(503, "target-unavailable",
                                     "PL061 runtime state is not ready");
            reply.value = record.status;
            return reply;
        }

        if (record.kind == ActionKind::GPIO_READ) {
            record.status.state = RuntimeActionState::COMPLETED;
            record.status.result = "ok";
        } else if (record.kind == ActionKind::GPIO_DRIVE ||
                   record.kind == ActionKind::GPIO_PULSE) {
            if (record.kind == ActionKind::GPIO_PULSE &&
                !add_simulation_time_ns(
                    now, record.duration_ns, record.due)) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "simulation-time-overflow";
                release_target(record);
                audit(record, "injection_failed");
                auto reply = error_reply(
                    400, "simulation-time-overflow",
                    "GPIO pulse exceeds simulation time range");
                reply.value = record.status;
                return reply;
            }
            if (state.direction_output) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "direction-mismatch";
                release_target(record);
                audit(record, "injection_failed");
                auto reply = error_reply(
                    409, "direction-mismatch",
                    "input drive requires PL061 input direction");
                reply.value = record.status;
                return reply;
            }
            if (!gpio->controller->runtime_drive_input(
                    gpio->pin, record.bool_value)) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "apply-failed";
                release_target(record);
                audit(record, "injection_failed");
                auto reply = error_reply(500, "apply-failed",
                                         "PL061 input drive failed");
                reply.value = record.status;
                return reply;
            }
            record.status.state = RuntimeActionState::ACTIVE;
            record.status.result = "active";
            m_active_targets[record.request.target] = record.status.id;
            if (record.kind == ActionKind::GPIO_PULSE) {
                m_schedule_changed.notify(sc_core::SC_ZERO_TIME);
            }
        } else if (record.kind == ActionKind::GPIO_RELEASE) {
            auto* active = find_active(record.request.target);
            if (active == nullptr) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "no-active-action";
                audit(record, "injection_failed");
                auto reply = error_reply(409, "no-active-action",
                                         "GPIO pin has no active drive");
                reply.value = record.status;
                return reply;
            }
            clear(*active, "released", RuntimeActionState::COMPLETED);
            record.status.state = RuntimeActionState::COMPLETED;
            record.status.result = "ok";
        } else if (record.kind == ActionKind::GPIO_SET_DIRECTION) {
            if (!gpio->controller->runtime_set_direction(
                    gpio->pin, record.bool_value)) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "apply-failed";
                audit(record, "injection_failed");
                auto reply = error_reply(500, "apply-failed",
                                         "PL061 direction update failed");
                reply.value = record.status;
                return reply;
            }
            record.status.state = RuntimeActionState::COMPLETED;
            record.status.result = "ok";
        } else if (record.kind == ActionKind::GPIO_WRITE_OUTPUT) {
            if (!state.direction_output) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "direction-mismatch";
                audit(record, "injection_failed");
                auto reply = error_reply(
                    409, "direction-mismatch",
                    "output write requires PL061 output direction");
                reply.value = record.status;
                return reply;
            }
            if (!gpio->controller->runtime_write_output(
                    gpio->pin, record.bool_value)) {
                record.status.state = RuntimeActionState::FAILED;
                record.status.result = "apply-failed";
                audit(record, "injection_failed");
                auto reply = error_reply(500, "apply-failed",
                                         "PL061 output update failed");
                reply.value = record.status;
                return reply;
            }
            record.status.state = RuntimeActionState::COMPLETED;
            record.status.result = "ok";
        }
    }

    gs::RuntimeActionStatusReply reply;
    reply.http_status = 202;
    reply.value = record.status;
    audit(record, "injection_applied");
    if (record.status.state == RuntimeActionState::COMPLETED) {
        audit(record, "injection_completed");
    }
    return reply;
}

void apollo_runtime_injection::clear(RequestRecord& record,
                                     const std::string& result,
                                     RuntimeActionState state)
{
    const auto now = sc_core::sc_time_stamp();
    if (record.kind == ActionKind::GIC_PULSE) {
        drive_gic(record, false, false);
    } else if (record.kind == ActionKind::COUNTER_CONTROL) {
        if (m_counter_override) {
            m_counter.restore_state_at(m_counter_baseline, now);
            m_counter_override = false;
        }
    } else if (record.kind == ActionKind::MHU_DROP_DOORBELL) {
        if (m_mhu != nullptr) {
            m_mhu->runtime_clear_doorbell_fault();
        }
    } else if (record.kind == ActionKind::SIGNAL_FAULT) {
        if (m_signal_fault != nullptr) {
            m_signal_fault->clear();
        }
    } else if (record.kind == ActionKind::GPIO_DRIVE ||
               record.kind == ActionKind::GPIO_PULSE) {
        const auto* gpio = find_gpio(record.request.target);
        if (gpio != nullptr) {
            gpio->controller->runtime_release_input(gpio->pin);
        }
    }
    release_target(record);
    record.status.has_cleared_sim_time_ns = true;
    record.status.cleared_sim_time_ns = simulation_time_ns(now);
    record.status.state = state;
    record.status.result = result;
    audit(record, "injection_cleared");
    audit(record, state == RuntimeActionState::CANCELLED ?
                      "injection_cancelled" :
                      "injection_completed");
}

gs::RuntimeActionStatusReply apollo_runtime_injection::submit(
    const gs::RuntimeActionRequest& request)
{
    if (m_stopping) {
        return error_reply(503, "simulation-unavailable",
                           "runtime injection is shutting down");
    }

    refresh_external_requests();

    RequestRecord record;
    record.request = request;
    auto validation = validate_request(request, record);
    if (!validation.ok()) {
        return validation;
    }

    if (reserves_target(record.kind) &&
        (m_active_targets.count(request.target) != 0 ||
         (record.kind == ActionKind::SYSTEM_RESET_PULSE &&
          m_reset_pulse_active))) {
        return error_reply(409, "target-busy",
                           "runtime target already has an active action");
    }

    const auto now = sc_core::sc_time_stamp();
    record.status.state = RuntimeActionState::ACCEPTED;
    record.status.target = request.target;
    record.status.action = request.action;
    record.status.generation = m_generation;
    record.status.has_requested_sim_time_ns = true;
    record.due = now;
    switch (request.trigger.type) {
    case gs::RuntimeActionTrigger::Type::IMMEDIATE:
        break;
    case gs::RuntimeActionTrigger::Type::ABSOLUTE_SIMULATION_TIME:
        if (!simulation_time_from_ns(request.trigger.time_ns, record.due)) {
            return error_reply(400, "simulation-time-overflow",
                               "absolute simulation time is not representable");
        }
        if (record.due < now) {
            return error_reply(400, "trigger-in-past",
                               "absolute simulation time is in the past");
        }
        record.status.state = RuntimeActionState::SCHEDULED;
        break;
    case gs::RuntimeActionTrigger::Type::RELATIVE_SIMULATION_TIME:
        if (!add_simulation_time_ns(
                now, request.trigger.delay_ns, record.due)) {
            return error_reply(400, "simulation-time-overflow",
                               "relative simulation time is not representable");
        }
        record.status.state = RuntimeActionState::SCHEDULED;
        break;
    default:
        return error_reply(400, "invalid-request",
                           "unsupported runtime action trigger");
    }
    record.status.requested_sim_time_ns = simulation_time_ns(record.due);

    if (record.duration_ns != 0) {
        sc_core::sc_time clear_due;
        if (!add_simulation_time_ns(record.due, record.duration_ns,
                                    clear_due)) {
            return error_reply(400, "simulation-time-overflow",
                               "action duration exceeds simulation time range");
        }
    }
    if (!prune_history()) {
        return error_reply(503, "request-history-full",
                           "runtime request history has no terminal entry");
    }

    record.status.id = m_next_id++;
    const uint64_t id = record.status.id;
    m_requests.emplace(id, std::move(record));
    auto& stored = m_requests.at(id);
    audit(stored, "injection_received");
    if (stored.status.state == RuntimeActionState::SCHEDULED) {
        if (reserves_target(stored.kind)) {
            m_active_targets[stored.request.target] = id;
        }
        m_schedule_changed.notify(sc_core::SC_ZERO_TIME);
        audit(stored, "injection_scheduled");
        gs::RuntimeActionStatusReply reply;
        reply.http_status = 202;
        reply.value = stored.status;
        return reply;
    }
    return apply(stored);
}

std::vector<gs::RuntimeActionStatus> apollo_runtime_injection::list() const
{
    std::vector<gs::RuntimeActionStatus> result;
    result.reserve(m_requests.size());
    for (const auto& request : m_requests) {
        result.push_back(effective_status(request.second));
    }
    return result;
}

gs::RuntimeActionStatusReply apollo_runtime_injection::status(
    uint64_t id) const
{
    const auto found = m_requests.find(id);
    if (found == m_requests.end()) {
        return error_reply(404, "request-not-found",
                           "runtime action request does not exist");
    }
    gs::RuntimeActionStatusReply reply;
    reply.value = effective_status(found->second);
    return reply;
}

gs::RuntimeActionStatusReply apollo_runtime_injection::cancel(uint64_t id)
{
    refresh_external_requests();
    const auto found = m_requests.find(id);
    if (found == m_requests.end()) {
        return error_reply(404, "request-not-found",
                           "runtime action request does not exist");
    }
    auto& record = found->second;
    if (is_terminal(record.status.state)) {
        auto reply = error_reply(409, "request-not-active",
                                 "runtime action is already terminal");
        reply.value = record.status;
        return reply;
    }
    if (record.status.state == RuntimeActionState::SCHEDULED) {
        cancel_scheduled(record, "cancelled");
    } else {
        clear(record, "cancelled", RuntimeActionState::CANCELLED);
    }
    gs::RuntimeActionStatusReply reply;
    reply.value = record.status;
    return reply;
}

void apollo_runtime_injection::schedule_thread()
{
    while (!m_stopping) {
        RequestRecord* next = nullptr;
        for (auto& request : m_requests) {
            auto& record = request.second;
            const bool scheduled =
                record.status.state == RuntimeActionState::SCHEDULED;
            const bool timed_active =
                record.status.state == RuntimeActionState::ACTIVE &&
                (record.kind == ActionKind::GIC_PULSE ||
                 record.kind == ActionKind::GPIO_PULSE);
            if ((scheduled || timed_active) &&
                (next == nullptr || record.due < next->due)) {
                next = &record;
            }
        }
        if (next == nullptr) {
            sc_core::wait(m_schedule_changed);
            continue;
        }
        const auto now = sc_core::sc_time_stamp();
        if (next->due > now) {
            sc_core::wait(next->due - now, m_schedule_changed);
            continue;
        }
        if (next->status.generation != m_generation) {
            if (next->status.state == RuntimeActionState::SCHEDULED) {
                cancel_scheduled(*next, "stale-generation");
            } else {
                clear(*next, "stale-generation",
                      RuntimeActionState::CANCELLED);
            }
        } else if (next->status.state == RuntimeActionState::SCHEDULED) {
            apply(*next);
        } else {
            clear(*next, "ok", RuntimeActionState::COMPLETED);
        }
    }
}

void apollo_runtime_injection::reset_release_thread()
{
    while (!m_stopping) {
        sc_core::wait(m_reset_release_event);
        if (m_stopping) {
            break;
        }
        const auto now = sc_core::sc_time_stamp();
        if (m_reset_release_due > now) {
            sc_core::wait(m_reset_release_due - now);
        }
        if (m_reset_pulse_active) {
            system_reset->write(false);
            m_reset_pulse_active = false;
        }
    }
}

void apollo_runtime_injection::reset_changed(bool asserted)
{
    if (asserted == m_reset_asserted) {
        return;
    }
    m_reset_asserted = asserted;
    if (!asserted) {
        return;
    }
    ++m_generation;
    audit_reset();
    for (auto& request : m_requests) {
        auto& record = request.second;
        if (!is_terminal(record.status.state) &&
            record.request.clear_on_reset) {
            if (record.status.state == RuntimeActionState::SCHEDULED) {
                cancel_scheduled(record, "cancelled-by-reset");
            } else {
                clear(record, "cancelled-by-reset",
                      RuntimeActionState::CANCELLED);
            }
        }
    }
    m_schedule_changed.notify(sc_core::SC_ZERO_TIME);
}

void apollo_runtime_injection::end_of_simulation()
{
    m_stopping = true;
}

extern "C" void module_register()
{
    GSC_MODULE_REGISTER_C(apollo_runtime_injection,
                          sc_core::sc_object*, sc_core::sc_object*,
                          sc_core::sc_object*, sc_core::sc_object*,
                          sc_core::sc_object*, sc_core::sc_object*,
                          sc_core::sc_object*, sc_core::sc_object*);
}
