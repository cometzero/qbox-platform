/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "canonical_gic_integration.h"

#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <vector>

#include <async_event.h>
#include <cci_configuration>
#include <cortex-r82.h>
#include <gs_memory.h>
#include <router.h>
#include <systemc>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include <arm_gicv3.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>
#include <qemu-instance.h>

namespace {

constexpr uint64_t kGicdBase = 0x100000;
constexpr uint64_t kGicrBase = 0x200000;
constexpr uint64_t kGicrStride = 0x20000;
constexpr uint64_t kReadyBase = 0x300000;
constexpr unsigned kTimerPhysPpi = 20;

const std::array<uint32_t, 40> kFirmware = {
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

class SignalTap : public sc_core::sc_module
{
public:
    TargetSignalSocket<bool> input;
    InitiatorSignalSocket<bool> output;
    bool value = false;

    explicit SignalTap(sc_core::sc_module_name name)
        : sc_core::sc_module(name), input("input"), output("output")
    {
        input.register_value_changed_cb([this](bool asserted) {
            value = asserted;
            output->write(asserted);
        });
    }
};

class ReadyTarget : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<ReadyTarget> socket;
    std::atomic<unsigned> mask;
    gs::async_event changed;

    explicit ReadyTarget(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , mask(0)
        , changed("changed")
    {
        socket.register_b_transport(this, &ReadyTarget::b_transport);
    }

    void b_transport(
        tlm::tlm_generic_payload& transaction, sc_core::sc_time&)
    {
        const uint64_t address = transaction.get_address();
        uint64_t value = 0;
        if (transaction.get_command() != tlm::TLM_WRITE_COMMAND ||
            transaction.get_data_length() != sizeof(value) ||
            address % sizeof(value) != 0 || address / sizeof(value) >= 5) {
            transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        std::memcpy(
            &value, transaction.get_data_ptr(), sizeof(value));
        if (value != 1) {
            transaction.set_response_status(
                tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        mask.fetch_or(1u << (address / sizeof(value)));
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
        changed.async_notify();
    }
};

std::vector<bool> snapshot(const sc_core::sc_vector<SignalTap>& taps)
{
    std::vector<bool> values;
    for (const auto& tap : taps) {
        values.push_back(tap.value);
    }
    return values;
}

class CanonicalGicPpiBench : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(CanonicalGicPpiBench);

    QemuInstanceManager manager;
    QemuInstance instance;
    sc_core::sc_vector<cpu_arm_cortexR82> cpus;
    gs::router<> router;
    gs::gs_memory<> memory;
    ReadyTarget ready;
    arm_gicv3 gic;
    sc_core::sc_vector<SignalSource> ppi_sources;
    sc_core::sc_vector<SignalTap> irq_taps;
    gs::async_event keepalive;
    std::vector<bool> cpu0_asserted;
    std::vector<bool> cpu0_deasserted;
    std::vector<bool> cpu4_asserted;
    std::vector<bool> cpu4_deasserted;
    std::array<uint64_t, 5> ready_pc{};
    std::array<bool, 5> ready_runnable{};
    bool ready_observed = false;
    bool transition_timeout = false;

    explicit CanonicalGicPpiBench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , manager("manager")
        , instance("instance", &manager, QemuInstance::Target::AARCH64)
        , cpus("cpu", 5, [this](const char* n, int) {
            return new cpu_arm_cortexR82(n, instance);
        })
        , router("router")
        , memory("memory", 0x10000)
        , ready("ready")
        , gic("gic", instance, 5)
        , ppi_sources("ppi_source", 2)
        , irq_taps("irq_tap", 5)
        , keepalive("keepalive")
    {
        keepalive.async_attach_suspending();
        router.add_target(memory.socket, 0, 0x10000);
        router.add_target(gic.dist_iface, kGicdBase, 0x10000);
        router.add_target(gic.redist_iface[0], kGicrBase, kGicrStride);
        router.add_target(
            gic.redist_iface[1], kGicrBase + kGicrStride,
            4 * kGicrStride);
        router.add_target(ready.socket, kReadyBase, 0x1000);

        for (unsigned i = 0; i < cpus.size(); ++i) {
            cpus[i].p_mp_affinity = i;
            cpus[i].p_start_in_reset = true;
            cpus[i].p_reset_power_on = true;
            router.add_initiator(cpus[i].socket);
            gic.irq_out[i].bind(irq_taps[i].input);
            irq_taps[i].output.bind(cpus[i].irq_in);
        }
        memory.load.ptr_load(
            reinterpret_cast<uint8_t*>(
                const_cast<uint32_t*>(kFirmware.data())),
            0, sizeof(kFirmware));
        ppi_sources[0].signal.bind(gic.ppi_in[0][kTimerPhysPpi]);
        ppi_sources[1].signal.bind(gic.ppi_in[4][kTimerPhysPpi]);
        SC_THREAD(run);
    }

    bool wait_for_tap(unsigned cpu, bool value)
    {
        while (irq_taps[cpu].value != value) {
            wait(irq_taps[cpu].input->value_changed_event());
        }
        return true;
    }

    void run()
    {
        sc_core::sc_unsuspendable();
        for (auto& cpu : cpus) {
            cpu.reset_cb(false);
        }
        while (ready.mask.load() != 0x1f) {
            wait(ready.changed);
        }
        ready_observed = ready.mask.load() == 0x1f;
        for (unsigned i = 0; i < cpus.size(); ++i) {
            ready_pc[i] = cpus[i].get_cpu_aarch64().get_pc();
            ready_runnable[i] = cpus[i].can_run();
        }

        ppi_sources[0].write(true);
        transition_timeout |= !wait_for_tap(0, true);
        cpu0_asserted = snapshot(irq_taps);
        ppi_sources[0].write(false);
        transition_timeout |= !wait_for_tap(0, false);
        cpu0_deasserted = snapshot(irq_taps);

        ppi_sources[1].write(true);
        transition_timeout |= !wait_for_tap(4, true);
        cpu4_asserted = snapshot(irq_taps);
        ppi_sources[1].write(false);
        transition_timeout |= !wait_for_tap(4, false);
        cpu4_deasserted = snapshot(irq_taps);
        keepalive.async_detach_suspending();
        sc_core::sc_stop();
        sc_core::sc_suspendable();
    }
};

class ForeignCpuBench : public sc_core::sc_module
{
public:
    QemuInstanceManager manager;
    QemuInstance owner;
    QemuInstance foreign;
    sc_core::sc_vector<cpu_arm_cortexR82> owner_cpus;
    cpu_arm_cortexR82 foreign_cpu;
    arm_gicv3 gic;

    explicit ForeignCpuBench(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , manager("manager")
        , owner("owner", &manager, QemuInstance::Target::AARCH64)
        , foreign("foreign", &manager, QemuInstance::Target::AARCH64)
        , owner_cpus("owner_cpu", 4, [this](const char* n, int) {
            return new cpu_arm_cortexR82(n, owner);
        })
        , foreign_cpu("foreign_cpu", foreign)
        , gic("gic", owner, 5)
    {
        for (unsigned i = 0; i < owner_cpus.size(); ++i) {
            owner_cpus[i].p_mp_affinity = i;
        }
        foreign_cpu.p_mp_affinity = 4;
    }
};

void set_gic_presets(cci_utils::consuming_broker& broker)
{
    cci::cci_originator originator("canonical_gic_integration");
    broker.set_preset_cci_value(
        "bench.gic.num_spi", cci::cci_value(960u), originator);
    broker.set_preset_cci_value(
        "bench.gic.redist_region.0", cci::cci_value(1u), originator);
    broker.set_preset_cci_value(
        "bench.gic.redist_region.1", cci::cci_value(4u), originator);
    broker.set_preset_cci_value(
        "bench.instance.tcg_mode", cci::cci_value("SINGLE"),
        originator);
    broker.set_preset_cci_value(
        "bench.instance.sync_policy",
        cci::cci_value("multithread-quantum"),
        originator);
}

bool equals(const std::vector<bool>& actual,
            std::initializer_list<bool> expected)
{
    return actual == std::vector<bool>(expected);
}

std::string bits(const std::vector<bool>& values)
{
    std::string result;
    for (bool value : values) {
        result += value ? '1' : '0';
    }
    return result;
}

}

int run_canonical_gic_integration(
    const std::string& scenario,
    cci_utils::consuming_broker& broker)
{
    set_gic_presets(broker);
    try {
        if (scenario == "--integration-foreign") {
            ForeignCpuBench bench("bench");
            sc_core::sc_start(sc_core::SC_ZERO_TIME);
            std::cerr << "foreign CPU graph unexpectedly realized\n";
            return 65;
        }

        tlm_utils::tlm_quantumkeeper::set_global_quantum(
            sc_core::sc_time(1, sc_core::SC_MS));
        CanonicalGicPpiBench bench("bench");
        sc_core::sc_start();
        const bool pass =
            bench.ready_observed &&
            !bench.transition_timeout &&
            equals(bench.cpu0_asserted, {true, false, false, false, false}) &&
            equals(bench.cpu0_deasserted, {false, false, false, false, false}) &&
            equals(bench.cpu4_asserted, {false, false, false, false, true}) &&
            equals(bench.cpu4_deasserted, {false, false, false, false, false});
        std::cout << "real_gic_ppi cpu0_assert="
                  << bits(bench.cpu0_asserted)
                  << " cpu0_deassert=" << bits(bench.cpu0_deasserted)
                  << " cpu4_assert=" << bits(bench.cpu4_asserted)
                  << " cpu4_deassert=" << bits(bench.cpu4_deasserted)
                  << " ready_mask=0x" << std::hex << bench.ready.mask.load()
                  << " transition_timeout=" << std::dec
                  << bench.transition_timeout
                  << " observed=" << pass;
        for (unsigned i = 0; i < bench.cpus.size(); ++i) {
            std::cout << " cpu" << i << "_ready_pc=0x" << std::hex
                      << bench.ready_pc[i]
                      << " runnable=" << std::dec
                      << bench.ready_runnable[i];
        }
        std::cout << std::dec << "\n";
        return pass ? 0 : 66;
    } catch (const std::exception& error) {
        std::cerr << "real_component_diagnostic: " << error.what() << "\n";
        return scenario == "--integration-foreign" ? 64 : 67;
    }
}
