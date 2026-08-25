/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <arm_system_counter.h>
#include <arm_gicv3.h>
#include <gicx00_multiview.h>
#include <qemu_pl061.h>
#include <runtime-action-service.h>
#include <zena_ssu.h>

#include <module_factory_registery.h>
#include <ports/target-signal-socket.h>
#include <systemc>

class apollo_runtime_injection : public sc_core::sc_module,
                                 public gs::RuntimeActionService
{
    static constexpr std::size_t MAX_REQUEST_HISTORY = 128;

    enum class ActionKind {
        GIC_PULSE,
        SSU_FAULT,
        COUNTER_CONTROL,
        COUNTER_CLEAR,
        GPIO_READ,
        GPIO_DRIVE,
        GPIO_PULSE,
        GPIO_RELEASE,
        GPIO_SET_DIRECTION,
        GPIO_WRITE_OUTPUT,
    };

    struct RequestRecord {
        gs::RuntimeActionRequest request;
        gs::RuntimeActionStatus status;
        sc_core::sc_time due = sc_core::SC_ZERO_TIME;
        ActionKind kind = ActionKind::GPIO_READ;
        bool bool_value = false;
        bool secondary_bool_value = false;
        uint64_t view = 0;
        uint64_t intid = 0;
        uint64_t duration_ns = 0;
    };

    struct GpioTarget {
        std::string id;
        std::string controller_name;
        qemu_pl061* controller = nullptr;
        std::size_t pin = 0;
    };

    gicx00_multiview& m_gic;
    std::array<arm_gicv3*, 2> m_gic_backends;
    zena_ssu& m_ssu;
    gs::arm_system_counter& m_counter;
    std::array<qemu_pl061*, 3> m_gpio_controllers;
    std::vector<GpioTarget> m_gpio_targets;
    std::map<uint64_t, RequestRecord> m_requests;
    std::map<std::string, uint64_t> m_active_targets;
    sc_core::sc_vector<TargetSignalSocket<bool>> m_gpio_output_observers;
    gs::arm_system_counter::StateSnapshot m_counter_baseline;
    bool m_counter_override = false;
    uint64_t m_next_id = 1;
    uint64_t m_generation = 0;
    bool m_reset_asserted = false;
    sc_core::sc_event m_schedule_changed;

    static gicx00_multiview& require_gic(sc_core::sc_object* object);
    static arm_gicv3* optional_gic_backend(sc_core::sc_object* object);
    static zena_ssu& require_ssu(sc_core::sc_object* object);
    static gs::arm_system_counter& require_counter(
        sc_core::sc_object* object);
    static qemu_pl061& require_gpio(sc_core::sc_object* object);

    static uint64_t simulation_time_ns(const sc_core::sc_time& time);
    static bool is_terminal(gs::RuntimeActionState state);
    static bool reserves_target(ActionKind kind);
    static gs::RuntimeActionStatusReply error_reply(
        int status, const std::string& code, const std::string& message);

    const GpioTarget* find_gpio(const std::string& target) const;
    RequestRecord* find_active(const std::string& target);
    void release_target(RequestRecord& record);
    void cancel_scheduled(RequestRecord& record, const std::string& result);
    void prune_history();
    gs::RuntimeActionStatusReply validate_request(
        const gs::RuntimeActionRequest& request, RequestRecord& record) const;
    gs::RuntimeActionStatusReply apply(RequestRecord& record);
    bool drive_gic(const RequestRecord& record, bool value,
                   bool require_owner);
    void clear(RequestRecord& record, const std::string& result,
               gs::RuntimeActionState state);
    void schedule_thread();
    void reset_changed(bool asserted);

public:
    SC_HAS_PROCESS(apollo_runtime_injection);

    TargetSignalSocket<bool> reset;

    apollo_runtime_injection(sc_core::sc_module_name name,
                             sc_core::sc_object* gic,
                             sc_core::sc_object* gic_view1_backend,
                             sc_core::sc_object* gic_view2_backend,
                             sc_core::sc_object* ssu,
                             sc_core::sc_object* counter,
                             sc_core::sc_object* host_smd_gpio,
                             sc_core::sc_object* rse_gpio_0,
                             sc_core::sc_object* rse_gpio_1);

    apollo_runtime_injection(sc_core::sc_module_name name,
                             sc_core::sc_object* gic,
                             sc_core::sc_object* ssu,
                             sc_core::sc_object* counter,
                             sc_core::sc_object* host_smd_gpio,
                             sc_core::sc_object* rse_gpio_0,
                             sc_core::sc_object* rse_gpio_1);

    std::vector<gs::RuntimeTargetCapability> capabilities() const override;
    gs::RuntimeTargetSnapshotReply target_snapshot(
        const std::string& target) const override;
    gs::RuntimeActionStatusReply submit(
        const gs::RuntimeActionRequest& request) override;
    std::vector<gs::RuntimeActionStatus> list() const override;
    gs::RuntimeActionStatusReply status(uint64_t id) const override;
    gs::RuntimeActionStatusReply cancel(uint64_t id) override;
};

extern "C" void module_register();
