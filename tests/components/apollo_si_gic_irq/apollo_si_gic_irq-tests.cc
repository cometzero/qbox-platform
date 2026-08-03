/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../apollo_si_single_instance/lua_test_support.h"
#include "../apollo_si_canonical_gic/canonical_gic_integration.h"

#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <async_event.h>
#include <cci/utils/broker.h>
#include <cortex-r82.h>
#include <gs_memory.h>
#include <gtest/gtest.h>
#include <router.h>
#include <sys/wait.h>
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>
#include <unistd.h>

#include <arm_gicv3.h>
#include <gicx00_multiview.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <qemu-instance.h>

namespace {

using apollo_si_test::Environment;
using apollo_si_test::LuaState;
using apollo_si_test::quote;
using apollo_si_test::run;

const std::map<std::string, std::string> kSingleMode = {
    { "QBOX_APOLLO_FULL_SI_SINGLE_GIC", "true" },
    { "QBOX_APOLLO_FULL_SI_ACCEL", "tcg" },
    { "QBOX_APOLLO_FULL_SI_TCG_MODE", "MULTI" },
    { "QBOX_APOLLO_FULL_SI_SYNC_POLICY", "multithread-quantum" },
    { "QBOX_APOLLO_FULL_SI_CL0_ACCEL", "tcg" },
    { "QBOX_APOLLO_FULL_SI_CL0_TCG_MODE", "MULTI" },
    { "QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY", "multithread-quantum" },
    { "QBOX_APOLLO_FULL_SI_CL1_ACCEL", "tcg" },
    { "QBOX_APOLLO_FULL_SI_CL1_TCG_MODE", "MULTI" },
    { "QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY", "multithread-quantum" },
};

struct ChildResult {
    int exit_code;
    std::string output;
};

ChildResult run_child(const char* scenario)
{
    int output_pipe[2];
    if (pipe(output_pipe) != 0) {
        return {127, "pipe failed"};
    }
    const pid_t pid = fork();
    if (pid == 0) {
        dup2(output_pipe[1], STDOUT_FILENO);
        dup2(output_pipe[1], STDERR_FILENO);
        close(output_pipe[0]);
        close(output_pipe[1]);
        execl("/proc/self/exe", "apollo_si_gic_irq-tests", scenario, nullptr);
        _exit(127);
    }
    close(output_pipe[1]);
    std::string output;
    char buffer[4096];
    ssize_t length;
    while ((length = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(length));
    }
    close(output_pipe[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return {
        WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status),
        output
    };
}

std::string graph_script()
{
    const std::string directory = APOLLO_PLATFORM_DIR;
    return "enable_ap_cpus=true\n"
           "dofile(" + quote(directory + "/apollo-qvp.lua") + ")\n";
}

std::string route_contract_script()
{
    const std::string directory = APOLLO_PLATFORM_DIR;
    return "local routes=dofile(" +
           quote(directory + "/hw-block/signal_routes.lua") + ")\n"
           "local cl0=dofile(" +
           quote(directory + "/hw-block/si_cl0.lua") + ")\n"
           "local active=routes.si_active_routes.routes\n";
}

int run_negative_route_validation(const std::string& selector)
{
    Environment environment(kSingleMode);
    LuaState lua;
    std::string mutation;
    if (selector == "--negative-duplicate") {
        mutation =
            "local route=cl0.irq_route(active,'si_cl0_system_timer')\n"
            "cl0.validate_irq_routes({route,route})\n";
    } else {
        mutation =
            "local route=cl0.irq_route(active,'si_cl0_system_timer')\n"
            "local invalid={}\n"
            "for key,value in pairs(route) do invalid[key]=value end\n"
            "invalid.socket_index=invalid.architectural_intid\n"
            "cl0.validate_irq_routes({invalid})\n";
    }
    try {
        run(lua.get(), route_contract_script() + mutation, 0);
    } catch (const std::exception& error) {
        std::cerr << "route validation rejected " << selector << ": "
                  << error.what() << "\n";
        return 64;
    }
    std::cerr << "route validation unexpectedly accepted "
              << selector << "\n";
    return 0;
}

constexpr uint64_t kGicdBase = 0x100000;
constexpr uint64_t kGicrBase = 0x200000;
constexpr uint64_t kGicrStride = 0x20000;
constexpr uint64_t kReadyBase = 0x300000;
constexpr uint64_t kSgiMarkerOffset = 5 * sizeof(uint64_t);
constexpr uint64_t kSgiReleaseOffset = 6 * sizeof(uint64_t);
constexpr uint64_t kSgiSendDoneOffset = 7 * sizeof(uint64_t);

const std::array<uint32_t, 40> kSpiFirmware = {
    0xd53800a0, 0x92401c00, 0xd282000a, 0x8b00154a,
    0xd5384249, 0xf9000149, 0xd2800029, 0xf9000549,
    0xd2a00401, 0xd2a00042, 0x9b020401, 0xb900143f,
    0x91404021, 0x52a00202, 0xb9008022, 0xb9010022,
    0x52801003, 0x39105023, 0xd2800049, 0xf9000549,
    0xb5000080, 0xd2a00201, 0x52800042, 0xb9000022,
    0xd28000e1, 0xd51cc9a1, 0xd518cca1, 0xd2801fe1,
    0xd5184601, 0xd2800021, 0xd518cce1, 0xd5033fdf,
    0xd2800069, 0xf9000549, 0xd2a00601, 0x8b000c21,
    0xd2800022, 0xf9000022, 0xd503207f, 0x17ffffff,
};

const std::array<uint32_t, 42> kSgiDirectedFirmware = {
    0xd53800a0, 0x92401c00, 0xd2a00401, 0x8b004421,
    0xb900143f, 0x91404021, 0x52800c02, 0xb9008022,
    0xb9010022, 0x52801003, 0x39105023, 0x39106023,
    0xb5000080, 0xd2a00201, 0x52800042, 0xb9000022,
    0xd28000e1, 0xd51cc9a1, 0xd518cca1, 0xd2801fe1,
    0xd5184601, 0xd2800021, 0xd518cce1, 0xd5033fdf,
    0xd2a00601, 0x8b000c21, 0xd2800022, 0xf9000022,
    0xb5000180, 0xf9001422, 0xf9401822, 0xb4ffffe2,
    0xd28000a3, 0xd3689c63, 0xd2800024,
    0xd37cec84, 0xaa040063, 0xd518cba3, 0xd5033fdf,
    0xf9001c22, 0xd503207f, 0x17ffffff
};

const std::array<uint32_t, 43> kSgiBroadcastFirmware = {
    0xd53800a0, 0x92401c00, 0xd2a00401, 0x8b004421,
    0xb900143f, 0x91404021, 0x52800c02, 0xb9008022,
    0xb9010022, 0x52801003, 0x39105023, 0x39106023,
    0xb5000080, 0xd2a00201, 0x52800042, 0xb9000022,
    0xd28000e1, 0xd51cc9a1, 0xd518cca1, 0xd2801fe1,
    0xd5184601, 0xd2800021, 0xd518cce1, 0xd5033fdf,
    0xd2a00601, 0x8b000c21, 0xd2800022, 0xf9000022,
    0xb5000180, 0xf9001422, 0xf9401822, 0xb4ffffe2,
    0xd2800023, 0xd3585c63, 0xd28000c4,
    0xd3689c84, 0xaa040063, 0xd518cba3, 0xd5033fdf,
    0xf9001c22, 0x14000001, 0xd503207f, 0x17ffffff
};

class SignalSource : public sc_core::sc_module
{
public:
    InitiatorSignalSocket<bool> signal;

    explicit SignalSource(sc_core::sc_module_name name)
        : sc_core::sc_module(name), signal("signal")
    {
    }

    void write(bool value) { signal->write(value); }
};

class SignalCounter : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> input;
    InitiatorSignalSocket<bool> output;
    unsigned rises = 0;
    bool value = false;

    SignalCounter(sc_core::sc_module_name name, gs::async_event& changed)
        : sc_core::sc_module(name), input("input"), output("output")
    {
        input.register_value_changed_cb([this, &changed](bool value) {
            this->value = value;
            if (value) {
                ++rises;
            }
            changed.async_notify();
            output->write(value);
        });
    }
};

class ControlTarget : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<ControlTarget> socket;
    std::atomic<unsigned> ready_mask;
    std::atomic<unsigned> sgi_marker;
    std::atomic<unsigned> sgi_release;
    std::atomic<unsigned> sgi_send_done;
    gs::async_event changed;
    bool accept_sgi_marker = true;
    bool accept_sgi_send_done = true;

    explicit ControlTarget(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , ready_mask(0)
        , sgi_marker(0)
        , sgi_release(0)
        , sgi_send_done(0)
        , changed("changed")
    {
        socket.register_b_transport(this, &ControlTarget::b_transport);
    }

    void b_transport(
        tlm::tlm_generic_payload& transaction, sc_core::sc_time&)
    {
        const uint64_t address = transaction.get_address();
        const unsigned int length = transaction.get_data_length();
        const bool word_access =
            address % sizeof(uint64_t) == 0 &&
            length == sizeof(uint64_t);
        if (transaction.get_command() == tlm::TLM_WRITE_COMMAND &&
            word_access) {
            uint64_t value = 0;
            std::memcpy(&value, transaction.get_data_ptr(), sizeof(value));
            if (address < kSgiMarkerOffset && value == 1) {
                ready_mask.fetch_or(
                    1u << (address / sizeof(uint64_t)));
                changed.async_notify();
                transaction.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }
            if (address == kSgiMarkerOffset && value == 1 &&
                accept_sgi_marker) {
                sgi_marker.store(1);
                changed.async_notify();
                transaction.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }
            if (address == kSgiSendDoneOffset && value == 1 &&
                accept_sgi_send_done &&
                sgi_marker.load() == 1 && sgi_release.load() == 1) {
                sgi_send_done.store(1);
                changed.async_notify();
                transaction.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }
        }
        if (transaction.get_command() == tlm::TLM_READ_COMMAND &&
            word_access && address == kSgiReleaseOffset) {
            const uint64_t value = sgi_release.load();
            std::memcpy(transaction.get_data_ptr(), &value, sizeof(value));
            transaction.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }
        transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    bool release_sgi()
    {
        unsigned expected = 0;
        return sgi_marker.load() == 1 &&
            sgi_release.compare_exchange_strong(expected, 1);
    }
};

class TlmWriter : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TlmWriter> socket;
    bool ok = true;

    explicit TlmWriter(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket")
    {
    }

    template <typename T>
    void write(uint64_t address, T value)
    {
        tlm::tlm_generic_payload transaction;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        transaction.set_command(tlm::TLM_WRITE_COMMAND);
        transaction.set_address(address);
        transaction.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        transaction.set_data_length(sizeof(value));
        transaction.set_streaming_width(sizeof(value));
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(transaction, delay);
        ok &= transaction.is_response_ok();
    }
};

class FivePeGic : public sc_core::sc_module
{
public:
    QemuInstanceManager manager;
    QemuInstance instance;
    sc_core::sc_vector<cpu_arm_cortexR82> cpus;
    gs::router<> router;
    gs::gs_memory<> memory;
    ControlTarget control;
    arm_gicv3 gic;
    gs::async_event counter_changed;
    sc_core::sc_vector<SignalCounter> counters;
    gs::async_event keepalive;

    FivePeGic(
        sc_core::sc_module_name name,
        const uint32_t* firmware,
        size_t firmware_size)
        : sc_core::sc_module(name)
        , manager("manager")
        , instance("instance", &manager, QemuInstance::Target::AARCH64)
        , cpus("cpu", 5, [this](const char* n, int) {
            return new cpu_arm_cortexR82(n, instance);
        })
        , router("router")
        , memory("memory", 0x10000)
        , control("control")
        , gic("gic", instance, 5)
        , counter_changed("counter_changed")
        , counters("counter", 5, [this](const char* n, int) {
            return new SignalCounter(n, counter_changed);
        })
        , keepalive("keepalive")
    {
        keepalive.async_attach_suspending();
        router.add_target(memory.socket, 0, 0x10000);
        router.add_target(gic.dist_iface, kGicdBase, 0x10000);
        router.add_target(gic.redist_iface[0], kGicrBase, kGicrStride);
        router.add_target(
            gic.redist_iface[1],
            kGicrBase + kGicrStride,
            4 * kGicrStride);
        router.add_target(
            control.socket, kReadyBase, 8 * sizeof(uint64_t));
        for (unsigned int cpu = 0; cpu < cpus.size(); ++cpu) {
            cpus[cpu].p_mp_affinity = cpu;
            cpus[cpu].p_start_in_reset = true;
            cpus[cpu].p_reset_power_on = true;
            router.add_initiator(cpus[cpu].socket);
            gic.irq_out[cpu].bind(counters[cpu].input);
            counters[cpu].output.bind(cpus[cpu].irq_in);
        }
        memory.load.ptr_load(
            reinterpret_cast<uint8_t*>(
                const_cast<uint32_t*>(firmware)),
            0, firmware_size);
    }

    void release_cpus()
    {
        for (auto& cpu : cpus) {
            cpu.reset_cb(false);
        }
    }

    void wait_ready()
    {
        while (control.ready_mask.load() != 0x1f) {
            wait(control.changed);
        }
    }

    std::array<unsigned, 5> snapshot() const
    {
        std::array<unsigned, 5> result {};
        for (unsigned int cpu = 0; cpu < counters.size(); ++cpu) {
            result[cpu] = counters[cpu].rises;
        }
        return result;
    }

    void finish()
    {
        keepalive.async_detach_suspending();
        sc_core::sc_stop();
    }
};

class SpiBench : public FivePeGic
{
public:
    SC_HAS_PROCESS(SpiBench);

    gicx00_multiview multiview;
    sc_core::sc_vector<SignalSource> sources;
    TlmWriter view0;
    TlmWriter view1;
    TlmWriter view2;
    unsigned intid;
    unsigned owner_view;
    unsigned target_cpu;
    std::array<unsigned, 5> wrong_owner {};
    std::array<unsigned, 5> delivered {};
    bool timed_out = false;

    SpiBench(
        sc_core::sc_module_name name,
        unsigned irq,
        unsigned owner,
        unsigned target)
        : FivePeGic(name, kSpiFirmware.data(), sizeof(kSpiFirmware))
        , multiview("multiview")
        , sources("source", 2)
        , view0("view0")
        , view1("view1")
        , view2("view2")
        , intid(irq)
        , owner_view(owner)
        , target_cpu(target)
    {
        const unsigned socket_index = intid - 32;
        multiview.backend_socket.bind(router.target_socket);
        multiview.spi_out[socket_index].bind(gic.spi_in[socket_index]);
        sources[0].signal.bind(multiview.view1_spi_in[socket_index]);
        sources[1].signal.bind(multiview.view2_spi_in[socket_index]);
        view0.socket.bind(multiview.view0_dist);
        view1.socket.bind(multiview.view1_dist);
        view2.socket.bind(multiview.view2_dist);
        SC_THREAD(run);
    }

    void configure()
    {
        const uint32_t register_index = intid / 16;
        const uint32_t owner =
            owner_view << ((intid % 16) * 2);
        view0.write(0xf600 + register_index * sizeof(uint32_t), owner);
        TlmWriter& view = owner_view == 1 ? view1 : view2;
        const uint32_t bit = 1u << (intid % 32);
        view.write(0x80 + (intid / 32) * sizeof(uint32_t), bit);
        view.write(0x100 + (intid / 32) * sizeof(uint32_t), bit);
        view.write<uint8_t>(0x400 + intid, 0x80);
        view.write<uint64_t>(0x6000 + intid * sizeof(uint64_t), target_cpu);
    }

    void run()
    {
        sc_core::sc_unsuspendable();
        release_cpus();
        wait_ready();
        configure();
        sources[2 - owner_view].write(true);
        wait(sc_core::SC_ZERO_TIME);
        wrong_owner = snapshot();
        sources[2 - owner_view].write(false);
        sources[owner_view - 1].write(true);
        sc_core::sc_event timeout;
        timeout.notify(sc_core::sc_time(10, sc_core::SC_MS));
        while (counters[target_cpu].rises == 0) {
            wait(counter_changed | timeout);
            if (timeout.triggered()) {
                timed_out = true;
                break;
            }
        }
        delivered = snapshot();
        sources[owner_view - 1].write(false);
        sc_core::sc_event deassert_timeout;
        deassert_timeout.notify(sc_core::sc_time(10, sc_core::SC_MS));
        while (counters[target_cpu].value) {
            wait(counter_changed | deassert_timeout);
            if (deassert_timeout.triggered()) {
                timed_out = true;
                break;
            }
        }
        finish();
        sc_core::sc_suspendable();
    }
};

class SgiBench : public FivePeGic
{
public:
    SC_HAS_PROCESS(SgiBench);

    uint32_t scenario;
    std::array<unsigned, 5> delivered {};
    std::array<uint64_t, 5> pc_at_ready {};
    bool timed_out = false;
    bool marker_seen = false;
    bool observer_armed = false;
    bool release_issued = false;
    bool send_done_seen = false;
    bool baseline_zero = false;
    bool issue_release;
    sc_core::sc_event observer_armed_event;

    SgiBench(
        sc_core::sc_module_name name,
        uint32_t selected_scenario,
        bool accept_marker = true,
        bool release_cpu = true,
        bool accept_send_done = true)
        : FivePeGic(
              name,
              selected_scenario == 1 ?
                  kSgiDirectedFirmware.data() :
                  kSgiBroadcastFirmware.data(),
              selected_scenario == 1 ?
                  sizeof(kSgiDirectedFirmware) :
                  sizeof(kSgiBroadcastFirmware))
        , scenario(selected_scenario)
        , issue_release(release_cpu)
    {
        control.accept_sgi_marker = accept_marker;
        control.accept_sgi_send_done = accept_send_done;
        SC_THREAD(run);
        SC_THREAD(release_sender);
    }

    void release_sender()
    {
        wait(observer_armed_event);
        if (issue_release) {
            release_issued = control.release_sgi();
        }
    }

    void run()
    {
        sc_core::sc_unsuspendable();
        release_cpus();
        wait_ready();
        while (control.sgi_marker.load() == 0) {
            wait(control.changed);
        }
        marker_seen = true;
        for (unsigned int cpu = 0; cpu < cpus.size(); ++cpu) {
            pc_at_ready[cpu] = cpus[cpu].get_cpu_aarch64().get_pc();
        }
        baseline_zero = true;
        for (unsigned count : snapshot()) {
            baseline_zero &= count == 0;
        }
        if (!baseline_zero) {
            finish();
            sc_core::sc_suspendable();
            return;
        }
        observer_armed = true;
        observer_armed_event.notify(sc_core::SC_ZERO_TIME);
        while (control.sgi_send_done.load() == 0) {
            wait(counter_changed | control.changed);
        }
        send_done_seen = true;
        delivered = snapshot();
        finish();
        sc_core::sc_suspendable();
    }
};

std::string counters(const std::array<unsigned, 5>& values)
{
    std::string result;
    for (unsigned value : values) {
        result += std::to_string(value);
    }
    return result;
}

void set_gic_presets(
    cci_utils::consuming_broker& broker,
    bool multiview,
    bool security_extensions)
{
    cci::cci_originator originator("apollo_si_gic_irq");
    broker.set_preset_cci_value(
        "bench.gic.num_spi", cci::cci_value(960u), originator);
    broker.set_preset_cci_value(
        "bench.gic.redist_region.0", cci::cci_value(1u), originator);
    broker.set_preset_cci_value(
        "bench.gic.redist_region.1", cci::cci_value(4u), originator);
    broker.set_preset_cci_value(
        "bench.gic.has_security_extensions",
        cci::cci_value(security_extensions),
        originator);
    broker.set_preset_cci_value(
        "bench.instance.tcg_mode", cci::cci_value("SINGLE"), originator);
    broker.set_preset_cci_value(
        "bench.instance.sync_policy",
        cci::cci_value("multithread-quantum"), originator);
    if (multiview) {
        broker.set_preset_cci_value(
            "bench.multiview.spi_count", cci::cci_value(960u), originator);
        broker.set_preset_cci_value(
            "bench.multiview.backend_dist_base",
            cci::cci_value(kGicdBase), originator);
        broker.set_preset_cci_value(
            "bench.multiview.backend_redist_base",
            cci::cci_value(kGicrBase), originator);
        broker.set_preset_cci_value(
            "bench.multiview.backend_redist_stride",
            cci::cci_value(kGicrStride), originator);
        broker.set_preset_cci_value(
            "bench.multiview.backend_redist_count",
            cci::cci_value(5u), originator);
    }
}

int run_irq_integration(
    const std::string& scenario,
    cci_utils::consuming_broker& broker)
{
    if (scenario == "--negative-sgi-wrong-order") {
        ControlTarget control("control");
        const bool rejected = !control.release_sgi();
        std::cerr << "sgi rendezvous rejected wrong-order"
                  << " marker_seen=" << control.sgi_marker.load()
                  << " release_issued=" << control.sgi_release.load()
                  << " rejected=" << rejected << "\n";
        return rejected ? 73 : 74;
    }
    const bool is_spi = scenario.find("--integration-spi-") == 0 ||
        scenario == "--integration-pfdi";
    set_gic_presets(broker, is_spi, false);
    tlm_utils::tlm_quantumkeeper::set_global_quantum(
        sc_core::sc_time(1, sc_core::SC_MS));
    if (scenario == "--integration-sgi-directed" ||
        scenario == "--integration-sgi-broadcast" ||
        scenario == "--negative-sgi-missing-marker" ||
        scenario == "--negative-sgi-missing-release" ||
        scenario == "--negative-sgi-missing-send-done") {
        const bool directed = scenario == "--integration-sgi-directed";
        const bool missing_marker =
            scenario == "--negative-sgi-missing-marker";
        const bool missing_release =
            scenario == "--negative-sgi-missing-release";
        const bool missing_send_done =
            scenario == "--negative-sgi-missing-send-done";
        SgiBench bench(
            "bench",
            directed || missing_marker || missing_release ||
                missing_send_done ? 1 : 2,
            !missing_marker,
            !missing_release,
            !missing_send_done);
        sc_core::sc_start();
        const std::string observed = counters(bench.delivered);
        if (missing_marker || missing_release || missing_send_done) {
            const bool rejected =
                bench.timed_out && observed == "00000" &&
                bench.marker_seen == !missing_marker &&
                bench.observer_armed == !missing_marker &&
                bench.release_issued == !missing_marker &&
                !bench.send_done_seen;
            std::cerr << "sgi rendezvous rejected "
                      << (missing_marker ? "missing-marker" :
                          missing_release ? "missing-release" :
                          "missing-send-done")
                      << " marker_seen=" << bench.marker_seen
                      << " observer_armed=" << bench.observer_armed
                      << " release_issued=" << bench.release_issued
                      << " send_done_seen=" << bench.send_done_seen
                      << " target_counters=" << observed
                      << " rejected=" << rejected << "\n";
            return rejected ? 73 : 74;
        }
        const std::string expected = directed ? "00001" : "01111";
        const bool pass =
            !bench.timed_out && bench.marker_seen &&
            bench.baseline_zero && bench.observer_armed &&
            bench.release_issued && bench.send_done_seen &&
            observed == expected;
        std::cout << "real_gic_sgi mode="
                  << (directed ? "directed" : "broadcast")
                  << " target_counters=" << observed
                  << " non_target_counters=0"
                  << " backend=arm-gicv3 pe_count=5 shared_state=1"
                  << " rendezvous_marker=" << bench.marker_seen
                  << " baseline_zero=" << bench.baseline_zero
                  << " observer_armed=" << bench.observer_armed
                  << " release_issued=" << bench.release_issued
                  << " send_done_seen=" << bench.send_done_seen
                  << " observed=" << pass;
        for (unsigned int cpu = 0; cpu < bench.pc_at_ready.size(); ++cpu) {
            std::cout << " pc" << cpu << "=0x" << std::hex
                      << bench.pc_at_ready[cpu]
                      << " final_pc" << cpu << "=0x"
                      << bench.cpus[cpu].get_cpu_aarch64().get_pc()
                      << std::dec;
        }
        std::cout << "\n";
        return pass ? 0 : 70;
    }

    unsigned intid = 34;
    unsigned view = 1;
    unsigned target = 0;
    if (scenario == "--integration-spi-last") {
        intid = 129;
    } else if (scenario == "--integration-pfdi") {
        intid = 82;
        view = 2;
        target = 1;
    } else if (scenario != "--integration-spi-first") {
        std::cerr << "unknown IRQ integration selector: " << scenario << "\n";
        return 64;
    }
    SpiBench bench("bench", intid, view, target);
    sc_core::sc_start();
    const std::string wrong = counters(bench.wrong_owner);
    const std::string delivered = counters(bench.delivered);
    const std::string expected = target == 0 ? "10000" : "01000";
    const bool pass =
        !bench.timed_out && bench.view0.ok && bench.view1.ok &&
        bench.view2.ok && wrong == "00000" && delivered == expected;
    std::cout << "real_gic_spi intid=" << intid
              << " socket_index=" << intid - 32
              << " owner_view=View" << view
              << " target_counters=" << delivered
              << " wrong_owner_counters=" << wrong
              << " non_target_counters=0"
              << " backend=arm-gicv3 multiview=gicx00_multiview"
              << " pe_count=5 shared_state=1 observed=" << pass << "\n";
    return pass ? 0 : 71;
}

TEST(ApolloSiGicIrq, SpiFirstAndLastUseProductionViewOneBindings)
{
    Environment environment(kSingleMode);
    LuaState lua;
    run(lua.get(),
        graph_script() + route_contract_script() +
        "local first=cl0.irq_route(active,'si_cl0_system_timer')\n"
        "local last=cl0.irq_route(active,'si_cl0_fmu_noncritical')\n"
        "return first.architectural_intid,first.socket_index,"
        "last.architectural_intid,last.socket_index,"
        "platform.si_cl0_timer_cntbase.irq.bind,"
        "platform.si_cl0_fmu.non_critical_irq.bind",
        6);

    EXPECT_EQ(lua_tointeger(lua.get(), -6), 34);
    EXPECT_EQ(lua_tointeger(lua.get(), -5), 2);
    EXPECT_EQ(lua_tointeger(lua.get(), -4), 129);
    EXPECT_EQ(lua_tointeger(lua.get(), -3), 97);
    EXPECT_STREQ(
        lua_tostring(lua.get(), -2),
        "&si_gic_multiview.view1_spi_in_2");
    EXPECT_STREQ(
        lua_tostring(lua.get(), -1),
        "&si_gic_multiview.view1_spi_in_97");

    for (const char* scenario :
         {"--integration-spi-first", "--integration-spi-last"}) {
        const ChildResult child = run_child(scenario);
        ASSERT_EQ(child.exit_code, 0) << child.output;
        EXPECT_NE(
            child.output.find("wrong_owner_counters=00000"),
            std::string::npos);
        EXPECT_NE(
            child.output.find("target_counters=10000"),
            std::string::npos);
        EXPECT_NE(child.output.find("observed=1"), std::string::npos);
    }
}

TEST(ApolloSiGicIrq, PpiPhysicalTimerTargetsEachCanonicalPe)
{
    Environment environment(kSingleMode);
    LuaState lua;
    run(lua.get(),
        graph_script() + route_contract_script() +
        "local route=cl0.irq_route(active,'si_physical_timer')\n"
        "local bindings={platform.si_cl0_cpu_0.irq_timer_phys_out.bind}\n"
        "for pe=0,3 do bindings[#bindings+1]="
        "platform['si_cl1_cpu_'..pe].irq_timer_phys_out.bind end\n"
        "return route.architectural_intid,route.target_semantics,"
        "#route.target_pes,table.concat(bindings,',')",
        4);

    EXPECT_EQ(lua_tointeger(lua.get(), -4), 20);
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "per_cpu");
    EXPECT_EQ(lua_tointeger(lua.get(), -2), 5);
    EXPECT_STREQ(
        lua_tostring(lua.get(), -1),
        "&si_cl0_gic.ppi_in_cpu_0_20,"
        "&si_cl0_gic.ppi_in_cpu_1_20,"
        "&si_cl0_gic.ppi_in_cpu_2_20,"
        "&si_cl0_gic.ppi_in_cpu_3_20,"
        "&si_cl0_gic.ppi_in_cpu_4_20");

    const ChildResult child = run_child("--integration-ppi");
    ASSERT_EQ(child.exit_code, 0) << child.output;
    EXPECT_NE(child.output.find("cpu0_assert=10000"), std::string::npos);
    EXPECT_NE(child.output.find("cpu4_assert=00001"), std::string::npos);
    EXPECT_NE(child.output.find("observed=1"), std::string::npos);
}

TEST(ApolloSiGicIrq, SgiDirectedAndBroadcastAreTargetAware)
{
    LuaState lua;
    run(lua.get(),
        route_contract_script() +
        "local directed=cl0.irq_route(active,'si_sgi_directed')\n"
        "local broadcast=cl0.irq_route(active,'si_sgi_broadcast')\n"
        "return directed.architectural_intid,directed.target_semantics,"
        "#directed.target_pes,broadcast.architectural_intid,"
        "broadcast.target_semantics,#broadcast.target_pes",
        6);

    EXPECT_EQ(lua_tointeger(lua.get(), -6), 5);
    EXPECT_STREQ(lua_tostring(lua.get(), -5), "directed");
    EXPECT_EQ(lua_tointeger(lua.get(), -4), 1);
    EXPECT_EQ(lua_tointeger(lua.get(), -3), 6);
    EXPECT_STREQ(lua_tostring(lua.get(), -2), "broadcast");
    EXPECT_EQ(lua_tointeger(lua.get(), -1), 4);

    const ChildResult directed = run_child("--integration-sgi-directed");
    ASSERT_EQ(directed.exit_code, 0) << directed.output;
    EXPECT_NE(
        directed.output.find("target_counters=00001"),
        std::string::npos);
    EXPECT_NE(
        directed.output.find("non_target_counters=0"),
        std::string::npos);

    const ChildResult broadcast = run_child("--integration-sgi-broadcast");
    ASSERT_EQ(broadcast.exit_code, 0) << broadcast.output;
    EXPECT_NE(
        broadcast.output.find("target_counters=01111"),
        std::string::npos);
    EXPECT_NE(broadcast.output.find("observed=1"), std::string::npos);
}

TEST(ApolloSiGicIrq, PfdiCl1SourceUsesViewTwoOnCanonicalBackend)
{
    Environment environment(kSingleMode);
    LuaState lua;
    run(lua.get(),
        graph_script() + route_contract_script() +
        "local route=cl0.irq_route(active,'si_cl1_pfdi')\n"
        "return route.architectural_intid,route.socket_index,"
        "route.owner_view,route.target_semantics,"
        "platform.si_cl1_pfdi_mhu_pbx.irq.bind,"
        "platform.si_cl1_gic==nil",
        6);

    EXPECT_EQ(lua_tointeger(lua.get(), -6), 82);
    EXPECT_EQ(lua_tointeger(lua.get(), -5), 50);
    EXPECT_STREQ(lua_tostring(lua.get(), -4), "View2");
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "shared");
    EXPECT_STREQ(
        lua_tostring(lua.get(), -2),
        "&si_gic_multiview.view2_spi_in_50");
    EXPECT_TRUE(lua_toboolean(lua.get(), -1));

    const ChildResult child = run_child("--integration-pfdi");
    ASSERT_EQ(child.exit_code, 0) << child.output;
    EXPECT_NE(
        child.output.find("owner_view=View2"),
        std::string::npos);
    EXPECT_NE(
        child.output.find("target_counters=01000"),
        std::string::npos);
    EXPECT_NE(
        child.output.find("wrong_owner_counters=00000"),
        std::string::npos);
}

TEST(ApolloSiGicIrq, SpiSplitRollbackKeepsOriginalControllers)
{
    Environment environment({});
    LuaState lua;
    run(lua.get(),
        graph_script() +
        "return platform.si_cl0_timer_cntbase.irq.bind,"
        "platform.si_cl1_pfdi_mhu_pbx.irq.bind,"
        "platform.si_cl0_gic~=nil,platform.si_cl1_gic~=nil",
        4);

    EXPECT_STREQ(lua_tostring(lua.get(), -4), "&si_cl0_gic.spi_in_2");
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "&si_cl1_gic.spi_in_50");
    EXPECT_TRUE(lua_toboolean(lua.get(), -2));
    EXPECT_TRUE(lua_toboolean(lua.get(), -1));
}

TEST(ApolloSiGicIrq, SpiValidatorRejectsDuplicateAndOffBy32)
{
    LuaState lua;
    run(lua.get(),
        route_contract_script() +
        "local route=cl0.irq_route(active,'si_cl0_system_timer')\n"
        "local ok_dup,msg_dup=pcall(cl0.validate_irq_routes,{route,route})\n"
        "local invalid={}\n"
        "for key,value in pairs(route) do invalid[key]=value end\n"
        "invalid.socket_index=invalid.architectural_intid\n"
        "local ok_raw,msg_raw=pcall(cl0.validate_irq_routes,{invalid})\n"
        "return ok_dup,tostring(msg_dup),ok_raw,tostring(msg_raw)",
        4);

    EXPECT_FALSE(lua_toboolean(lua.get(), -4));
    EXPECT_NE(
        std::string(lua_tostring(lua.get(), -3)).find("duplicate"),
        std::string::npos);
    EXPECT_FALSE(lua_toboolean(lua.get(), -2));
    EXPECT_NE(
        std::string(lua_tostring(lua.get(), -1)).find("INTID - 32"),
        std::string::npos);
}

}

int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    if (argc == 2) {
        const std::string selector = argv[1];
        if (selector == "--negative-duplicate" ||
            selector == "--negative-off-by-32") {
            return run_negative_route_validation(selector);
        }
        if (selector == "--negative-sgi-missing-marker" ||
            selector == "--negative-sgi-missing-release" ||
            selector == "--negative-sgi-missing-send-done" ||
            selector == "--negative-sgi-wrong-order") {
            return run_irq_integration(selector, broker);
        }
        if (selector == "--integration-ppi") {
            return run_canonical_gic_integration(selector, broker);
        }
        if (selector.find("--integration-") == 0) {
            try {
                return run_irq_integration(selector, broker);
            } catch (const std::exception& error) {
                std::cerr << "real IRQ integration diagnostic: "
                          << error.what() << "\n";
                return 72;
            }
        }
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
