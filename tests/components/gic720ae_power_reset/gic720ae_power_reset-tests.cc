/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <cci/utils/broker.h>
#include <cortex-r82.h>
#include <gic720ae_messreg.h>
#include <gic720ae_power_bridge.h>
#include <gicx00_multiview.h>
#include <gs_memory.h>
#include <gtest/gtest.h>
#include <qemu-instance.h>
#include <qemu_device_cold_reset.h>
#include <reset_fanout.h>
#include <router.h>
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include "../apollo_si_single_instance/lua_test_support.h"

#include <arm_gicv3.h>
#include <ports/initiator-signal-socket.h>
#include <ports/target-signal-socket.h>

namespace {

using apollo_si_test::Environment;
using apollo_si_test::LuaState;
using apollo_si_test::quote;
using apollo_si_test::run;

constexpr uint64_t kGicdBase = 0x100000;
constexpr uint64_t kGicrBase = 0x200000;
constexpr uint64_t kReadyBase = 0x300000;
constexpr unsigned int kIntid = 34;
constexpr unsigned int kSpi = kIntid - 32;
constexpr uint32_t kBit = 1u << (kIntid % 32);
constexpr uint64_t kGicrWaker = 0x14;
constexpr uint64_t kGicrPwrr = 0x24;
constexpr uint32_t kProcessorSleep = 1u << 1;
constexpr uint32_t kChildrenAsleep = 1u << 2;
constexpr uint32_t kSleep = 1u << 0;
constexpr uint32_t kQuiescent = 1u << 31;
constexpr uint32_t kRedistributorPowerDown = 1u << 0;

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
    unsigned int rises = 0;
    bool value = false;
    gs::async_event changed;

    explicit SignalTap(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , input("input")
        , output("output")
        , changed("changed")
    {
        input.register_value_changed_cb([this](bool asserted) {
            value = asserted;
            rises += asserted ? 1u : 0u;
            changed.async_notify();
            output->write(asserted);
        });
    }
};

class ReadyTarget : public sc_core::sc_module
{
public:
    tlm_utils::simple_target_socket<ReadyTarget> socket;
    std::atomic<bool> ready;
    gs::async_event changed;

    explicit ReadyTarget(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , ready(false)
        , changed("changed")
    {
        socket.register_b_transport(this, &ReadyTarget::b_transport);
    }

    void b_transport(
        tlm::tlm_generic_payload& transaction, sc_core::sc_time&)
    {
        uint64_t value = 0;
        if (transaction.get_command() != tlm::TLM_WRITE_COMMAND ||
            transaction.get_address() != 0 ||
            transaction.get_data_length() != sizeof(value)) {
            transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        std::memcpy(&value, transaction.get_data_ptr(), sizeof(value));
        ready.store(value == 1);
        changed.async_notify();
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class TlmAccess : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<TlmAccess> socket;
    bool ok = true;

    explicit TlmAccess(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket")
    {
    }

    template <typename T>
    void write(uint64_t address, T value)
    {
        transfer(address, tlm::TLM_WRITE_COMMAND, value);
    }

    template <typename T>
    T read(uint64_t address)
    {
        T value = 0;
        transfer(address, tlm::TLM_READ_COMMAND, value);
        return value;
    }

private:
    template <typename T>
    void transfer(uint64_t address, tlm::tlm_command command, T& value)
    {
        tlm::tlm_generic_payload transaction;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        transaction.set_command(command);
        transaction.set_address(address);
        transaction.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value));
        transaction.set_data_length(sizeof(value));
        transaction.set_streaming_width(sizeof(value));
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(transaction, delay);
        ok &= transaction.is_response_ok();
    }
};

struct ResetObservation {
    uint32_t owner = UINT32_MAX;
    uint32_t message = UINT32_MAX;
    uint32_t enabled = UINT32_MAX;
    uint32_t pending = UINT32_MAX;
    uint32_t active = UINT32_MAX;
    uint32_t pwrr = UINT32_MAX;
    uint32_t waker = UINT32_MAX;
    unsigned int late_irq_count = UINT32_MAX;
};

struct PowerObservation {
    uint32_t waker_asleep = 0;
    uint32_t waker_quiescent = 0;
    uint32_t pwrr_down = 0;
    uint32_t pending_while_down = 0;
    uint32_t pwrr_up = UINT32_MAX;
    uint32_t waker_awake = UINT32_MAX;
    unsigned int delivery_while_down = UINT32_MAX;
    unsigned int delivery_after_up = 0;
    unsigned int sleep_polls = 0;
    unsigned int quiescent_polls = 0;
    unsigned int down_polls = 0;
    unsigned int up_polls = 0;
    unsigned int wake_polls = 0;
};

class AtomicResetBench : public sc_core::sc_module
{
public:
    SC_HAS_PROCESS(AtomicResetBench);

    QemuInstanceManager manager;
    QemuInstance instance;
    cpu_arm_cortexR82 cpu;
    gs::router<> router;
    gs::gs_memory<> memory;
    ReadyTarget ready;
    arm_gicv3 gic;
    gicx00_multiview multiview;
    gic720ae_power_bridge power_bridge;
    gic720ae_messreg messreg;
    reset_fanout reset_transaction;
    qemu_device_cold_reset gic_reset;
    qemu_device_cold_reset cpu_reset;
    SignalSource irq_source;
    SignalSource reset_source;
    SignalTap irq_tap;
    TlmAccess view0;
    TlmAccess view1;
    TlmAccess redist;
    TlmAccess message;
    gs::async_event keepalive;
    std::array<ResetObservation, 2> prepared {};
    std::array<ResetObservation, 2> observations {};
    PowerObservation power {};
    unsigned int rises_before_reset = 0;
    bool invalid_powerdown_order = false;

    AtomicResetBench(sc_core::sc_module_name name, bool reset_systemc = true,
                     bool invalid_order = false)
        : sc_core::sc_module(name)
        , manager("manager")
        , instance("instance", &manager, QemuInstance::Target::AARCH64)
        , cpu("cpu", instance)
        , router("router")
        , memory("memory", 0x10000)
        , ready("ready")
        , gic("gic", instance, 1)
        , multiview("multiview")
        , power_bridge("power_bridge")
        , messreg("messreg")
        , reset_transaction("reset_transaction")
        , gic_reset("gic_reset", &gic)
        , cpu_reset("cpu_reset", &cpu)
        , irq_source("irq_source")
        , reset_source("reset_source")
        , irq_tap("irq_tap")
        , view0("view0")
        , view1("view1")
        , redist("redist")
        , message("message")
        , keepalive("keepalive")
        , invalid_powerdown_order(invalid_order)
    {
        keepalive.async_attach_suspending();
        router.add_target(memory.socket, 0, 0x10000);
        router.add_target(gic.dist_iface, kGicdBase, 0x10000);
        router.add_target(gic.redist_iface[0], kGicrBase, 0x20000);
        router.add_target(ready.socket, kReadyBase, 0x1000);
        router.add_initiator(cpu.socket);
        cpu.p_mp_affinity = 0;
        cpu.p_start_in_reset = true;
        cpu.p_reset_power_on = true;
        gic.irq_out[0].bind(power_bridge.irq_in[0]);
        power_bridge.irq_out[0].bind(irq_tap.input);
        irq_tap.output.bind(cpu.irq_in);
        memory.load.ptr_load(
            reinterpret_cast<uint8_t*>(
                const_cast<uint32_t*>(kFirmware.data())),
            0, sizeof(kFirmware));

        multiview.backend_socket.bind(power_bridge.target_socket);
        power_bridge.backend_socket.bind(router.target_socket);
        multiview.spi_out[kSpi].bind(gic.spi_in[kSpi]);
        irq_source.signal.bind(multiview.view1_spi_in[kSpi]);
        view0.socket.bind(multiview.view0_dist);
        view1.socket.bind(multiview.view1_dist);
        redist.socket.bind(multiview.view0_redist_0);
        message.socket.bind(messreg.target_socket);

        reset_source.signal.bind(reset_transaction.reset_in);
        if (reset_systemc) {
            reset_transaction.reset_out.bind(multiview.reset);
            reset_transaction.reset_out.bind(power_bridge.reset);
            reset_transaction.reset_out.bind(gic_reset.reset);
            reset_transaction.reset_out.bind(messreg.reset);
        }
        reset_transaction.reset_out.bind(cpu_reset.reset);
        SC_THREAD(run);
    }

private:
    uint32_t poll_redist(uint64_t offset, uint32_t mask, bool asserted,
                         unsigned int& polls)
    {
        uint32_t value = 0;
        for (polls = 1; polls <= 32; ++polls) {
            value = redist.read<uint32_t>(offset);
            if (((value & mask) != 0) == asserted) {
                break;
            }
            wait(sc_core::sc_time(1, sc_core::SC_PS));
        }
        return value;
    }

    void exercise_power_cycle()
    {
        const uint32_t register_index = kIntid / 16;
        const uint32_t owner = 1u << ((kIntid % 16) * 2);
        const uint64_t enable = 0x100 + (kIntid / 32) * sizeof(uint32_t);
        const uint64_t pending_set = 0x200 +
            (kIntid / 32) * sizeof(uint32_t);
        const uint64_t pending_clear = 0x280 +
            (kIntid / 32) * sizeof(uint32_t);

        view0.write(0xf600 + register_index * sizeof(uint32_t), owner);
        view1.write(pending_clear, kBit);
        view1.write(0x80 + (kIntid / 32) * sizeof(uint32_t), kBit);
        view1.write(enable, kBit);
        view1.write<uint8_t>(0x400 + kIntid, 0x80);
        view1.write<uint64_t>(0x6000 + kIntid * sizeof(uint64_t), 0);
        redist.write<uint32_t>(kGicrWaker, kProcessorSleep);
        power.waker_asleep = poll_redist(
            kGicrWaker, kChildrenAsleep, true, power.sleep_polls);
        redist.write<uint32_t>(
            kGicrWaker, kSleep | kProcessorSleep);
        power.waker_quiescent = poll_redist(
            kGicrWaker, kQuiescent, true, power.quiescent_polls);
        redist.write<uint32_t>(kGicrPwrr, kRedistributorPowerDown);
        power.pwrr_down = poll_redist(
            kGicrPwrr, kRedistributorPowerDown, true, power.down_polls);

        const unsigned int before_down_irq = irq_tap.rises;
        irq_source.write(true);
        wait(sc_core::sc_time(1, sc_core::SC_MS));
        power.pending_while_down = view1.read<uint32_t>(pending_set);
        power.delivery_while_down = irq_tap.rises - before_down_irq;

        redist.write<uint32_t>(kGicrPwrr, 0);
        power.pwrr_up = poll_redist(
            kGicrPwrr, kRedistributorPowerDown, false, power.up_polls);
        const unsigned int before_wake_irq = irq_tap.rises;
        redist.write<uint32_t>(kGicrWaker, kProcessorSleep);
        redist.write<uint32_t>(kGicrWaker, 0);
        power.waker_awake = poll_redist(
            kGicrWaker, kChildrenAsleep, false, power.wake_polls);
        sc_core::sc_event delivery_timeout;
        delivery_timeout.notify(sc_core::sc_time(10, sc_core::SC_MS));
        while (irq_tap.rises == before_wake_irq) {
            wait(irq_tap.changed | delivery_timeout);
            if (delivery_timeout.triggered()) {
                break;
            }
        }
        power.delivery_after_up = irq_tap.rises - before_wake_irq;

        std::cout << "power_cycle"
                  << " pwrr_down=" << (power.pwrr_down & kRedistributorPowerDown)
                  << " children_asleep=" << (power.waker_asleep & kChildrenAsleep)
                  << " quiescent=" << (power.waker_quiescent & kQuiescent)
                  << " delivery_while_down=" << power.delivery_while_down
                  << " pending_while_down=" << (power.pending_while_down & kBit)
                  << " pwrr_up=" << (power.pwrr_up & kRedistributorPowerDown)
                  << " children_awake=" << (power.waker_awake & kChildrenAsleep)
                  << " delivery_after_up=" << power.delivery_after_up
                  << " polls=" << power.sleep_polls << ","
                  << power.quiescent_polls << "," << power.down_polls
                  << "," << power.up_polls << "," << power.wake_polls
                  << "\n";

        irq_source.write(false);
        view1.write(pending_clear, kBit);
        wait(sc_core::sc_time(2, sc_core::SC_PS));
    }

    void exercise_invalid_powerdown_order()
    {
        redist.write<uint32_t>(kGicrPwrr, 0);
        poll_redist(
            kGicrPwrr, kRedistributorPowerDown, false, power.up_polls);
        redist.write<uint32_t>(kGicrWaker, 0);
        poll_redist(
            kGicrWaker, kChildrenAsleep, false, power.wake_polls);
        redist.write<uint32_t>(kGicrPwrr, kRedistributorPowerDown);
        power.pwrr_down = poll_redist(
            kGicrPwrr, kRedistributorPowerDown, true, power.down_polls);
        std::cerr << "negative_fixture=pwrr-before-children-asleep"
                  << " reason=pwrr_timeout_before_children_asleep"
                  << " pwrr=" << (power.pwrr_down & kRedistributorPowerDown)
                  << " polls=" << power.down_polls << "\n";
    }

    void configure_state(unsigned int cycle)
    {
        const uint32_t register_index = kIntid / 16;
        const uint32_t owner = 1u << ((kIntid % 16) * 2);
        view0.write(0xf600 + register_index * sizeof(uint32_t), owner);
        view1.write(0x80 + (kIntid / 32) * sizeof(uint32_t), kBit);
        view1.write(0x100 + (kIntid / 32) * sizeof(uint32_t), kBit);
        view1.write(0x200 + (kIntid / 32) * sizeof(uint32_t), kBit);
        view1.write(0x300 + (kIntid / 32) * sizeof(uint32_t), kBit);
        message.write<uint32_t>(0x10, 0xa5a50000u | cycle);
        irq_source.write(false);
    }

    void pulse_reset()
    {
        rises_before_reset = irq_tap.rises;
        reset_source.write(true);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        irq_source.write(true);
        wait(sc_core::sc_time(1, sc_core::SC_PS));
        irq_source.write(false);
        wait(sc_core::sc_time(2, sc_core::SC_PS));
        reset_source.write(false);
        wait(sc_core::sc_time(2, sc_core::SC_PS));
    }

    ResetObservation read_state()
    {
        ResetObservation observed;
        observed.owner = view0.read<uint32_t>(
            0xf600 + (kIntid / 16) * sizeof(uint32_t));
        observed.message = message.read<uint32_t>(0x10);
        observed.enabled = view1.read<uint32_t>(
            0x100 + (kIntid / 32) * sizeof(uint32_t));
        observed.pending = view1.read<uint32_t>(
            0x200 + (kIntid / 32) * sizeof(uint32_t));
        observed.active = view1.read<uint32_t>(
            0x300 + (kIntid / 32) * sizeof(uint32_t));
        observed.pwrr = redist.read<uint32_t>(kGicrPwrr);
        observed.waker = redist.read<uint32_t>(kGicrWaker);
        return observed;
    }

    void observe(unsigned int cycle)
    {
        ResetObservation& observed = observations[cycle];
        wait(sc_core::sc_time(2, sc_core::SC_PS));
        observed = read_state();
        const bool stale_deliverable =
            (observed.enabled & observed.pending & kBit) != 0;
        observed.late_irq_count = irq_tap.rises - rises_before_reset +
            (irq_tap.value || stale_deliverable ? 1u : 0u);
        std::cout << (cycle == 0 ? "cold" : "warm")
                  << " owner=" << observed.owner
                  << " message=" << observed.message
                  << " qemu_enable=" << (observed.enabled & kBit)
                  << " qemu_pending=" << (observed.pending & kBit)
                  << " qemu_active=" << (observed.active & kBit)
                  << " pwrr=" << (observed.pwrr & kRedistributorPowerDown)
                  << " waker=" << (observed.waker &
                      (kProcessorSleep | kChildrenAsleep))
                  << " late_irq_count=" << observed.late_irq_count
                  << " reset_epoch=" << cycle + 1 << "\n";
    }

    void run()
    {
        sc_core::sc_unsuspendable();
        cpu.reset_cb(false);
        while (!ready.ready.load()) {
            wait(ready.changed);
        }
        if (invalid_powerdown_order) {
            exercise_invalid_powerdown_order();
            keepalive.async_detach_suspending();
            sc_core::sc_stop();
            sc_core::sc_suspendable();
            return;
        }
        exercise_power_cycle();
        for (unsigned int cycle = 0; cycle < observations.size(); ++cycle) {
            configure_state(cycle);
            wait(sc_core::SC_ZERO_TIME);
            prepared[cycle] = read_state();
            pulse_reset();
            observe(cycle);
            irq_source.write(false);
            cpu.reset_cb(false);
        }
        keepalive.async_detach_suspending();
        sc_core::sc_stop();
        sc_core::sc_suspendable();
    }
};

void set_presets(cci::cci_broker_handle broker)
{
    broker.set_preset_cci_value(
        "bench.gic.num_spi", cci::cci_value(960u));
    broker.set_preset_cci_value(
        "bench.gic.redist_region.0", cci::cci_value(1u));
    broker.set_preset_cci_value(
        "bench.gic.has_security_extensions",
        cci::cci_value(false));
    broker.set_preset_cci_value(
        "bench.instance.tcg_mode", cci::cci_value("SINGLE"));
    broker.set_preset_cci_value(
        "bench.instance.sync_policy",
        cci::cci_value("multithread-quantum"));
    broker.set_preset_cci_value(
        "bench.multiview.spi_count", cci::cci_value(960u));
    broker.set_preset_cci_value(
        "bench.multiview.backend_dist_base",
        cci::cci_value(kGicdBase));
    broker.set_preset_cci_value(
        "bench.multiview.backend_redist_base",
        cci::cci_value(kGicrBase));
    broker.set_preset_cci_value(
        "bench.multiview.backend_redist_count",
        cci::cci_value(1u));
    broker.set_preset_cci_value(
        "bench.power_bridge.backend_redist_base",
        cci::cci_value(kGicrBase));
    broker.set_preset_cci_value(
        "bench.power_bridge.backend_redist_stride",
        cci::cci_value(0x20000u));
    broker.set_preset_cci_value(
        "bench.power_bridge.redistributor_count",
        cci::cci_value(1u));
}

int run_negative_without_systemc_reset()
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("gic720ae_power_reset_negative"));
    set_presets(broker);
    tlm_utils::tlm_quantumkeeper::set_global_quantum(
        sc_core::sc_time(1, sc_core::SC_MS));
    AtomicResetBench* bench = new AtomicResetBench("bench", false);
    sc_core::sc_start();
    const ResetObservation& observed = bench->observations[0];
    const bool stale = observed.owner != 0 || observed.message != 0 ||
        observed.late_irq_count != 0;
    std::cerr << "negative_fixture=without_systemc_reset"
              << " stale_delivery=" << stale
              << " owner=" << observed.owner
              << " message=" << observed.message
              << " late_irq_count=" << observed.late_irq_count << "\n";
    return stale ? 73 : 74;
}

int run_negative_pwrr_before_children_asleep()
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("gic720ae_power_order_negative"));
    set_presets(broker);
    tlm_utils::tlm_quantumkeeper::set_global_quantum(
        sc_core::sc_time(1, sc_core::SC_MS));
    AtomicResetBench* bench = new AtomicResetBench("bench", true, true);
    sc_core::sc_start();
    const bool rejected =
        (bench->power.pwrr_down & kRedistributorPowerDown) == 0 &&
        bench->power.down_polls > 32;
    return rejected ? 75 : 76;
}

std::string graph_script()
{
    const std::string directory = APOLLO_PLATFORM_DIR;
    return "enable_ap_cpus=true\n"
           "dofile(" + quote(directory + "/apollo-qvp.lua") + ")\n";
}

}

TEST(Gic720aePowerReset, SingleTopologyOrdersSharedStateBeforeCpuReset)
{
    Environment environment({
        {"QBOX_APOLLO_FULL_SI_SINGLE_GIC", "true"},
        {"QBOX_RDASPEN_ENABLE_AP_CPUS", "true"},
    });
    LuaState lua;
    run(lua.get(), graph_script() +
        "return platform.apollo_system_reset_fanout.reset_out.bind,"
        "platform.si_gic_reset.args[1],"
        "platform.si_gic_power_bridge.moduletype", 3);
    const std::string targets = lua_tostring(lua.get(), -3);
    EXPECT_EQ(targets.find(
        "&si_gic_multiview.reset;&si_gic_power_bridge.reset;"
        "&si_gic_reset.reset;"
        "&ap_rgic2lgic_messreg.reset;&si_cl0_cpu_0_reset.reset"), 0u);
    EXPECT_STREQ(lua_tostring(lua.get(), -2), "&platform.si_cl0_gic");
    EXPECT_STREQ(lua_tostring(lua.get(), -1), "gic720ae_power_bridge");
}

TEST(Gic720aePowerReset, SplitTopologyKeepsQemuCpuInterfaceResetDomain)
{
    Environment environment({
        {"QBOX_RDASPEN_ENABLE_AP_CPUS", "true"},
    });
    LuaState lua;
    run(lua.get(), graph_script() +
        "return platform.apollo_system_reset_fanout.reset_out.bind", 1);
    const std::string targets = lua_tostring(lua.get(), -1);
    EXPECT_EQ(targets.find(
        "&si_gic_multiview.reset;&si_cl0_qemu_inst.reset;"
        "&si_cl1_qemu_inst.reset;&ap_rgic2lgic_messreg.reset"), 0u);
}

TEST(Gic720aePowerReset, ColdAndWarmResetAreAtomicAcrossOwners)
{
    auto broker = cci::cci_get_global_broker(
        cci::cci_originator("gic720ae_power_reset"));
    set_presets(broker);
    tlm_utils::tlm_quantumkeeper::set_global_quantum(
        sc_core::sc_time(1, sc_core::SC_MS));
    AtomicResetBench* bench = new AtomicResetBench("bench");
    sc_core::sc_start();

    ASSERT_TRUE(bench->view0.ok);
    ASSERT_TRUE(bench->view1.ok);
    ASSERT_TRUE(bench->redist.ok);
    ASSERT_TRUE(bench->message.ok);
    EXPECT_NE(bench->power.waker_asleep & kChildrenAsleep, 0u);
    EXPECT_NE(bench->power.waker_quiescent & kQuiescent, 0u);
    EXPECT_NE(bench->power.pwrr_down & kRedistributorPowerDown, 0u);
    EXPECT_EQ(bench->power.delivery_while_down, 0u);
    EXPECT_NE(bench->power.pending_while_down & kBit, 0u);
    EXPECT_EQ(bench->power.pwrr_up & kRedistributorPowerDown, 0u);
    EXPECT_EQ(bench->power.waker_awake & kChildrenAsleep, 0u);
    EXPECT_NE(bench->power.delivery_after_up, 0u);
    EXPECT_LE(bench->power.sleep_polls, 32u);
    EXPECT_LE(bench->power.quiescent_polls, 32u);
    EXPECT_LE(bench->power.down_polls, 32u);
    EXPECT_LE(bench->power.up_polls, 32u);
    EXPECT_LE(bench->power.wake_polls, 32u);
    for (unsigned int cycle = 0; cycle < bench->observations.size(); ++cycle) {
        const ResetObservation& before = bench->prepared[cycle];
        const ResetObservation& observed = bench->observations[cycle];
        EXPECT_EQ(before.owner, 1u << ((kIntid % 16) * 2));
        EXPECT_EQ(before.message, 0xa5a50000u | cycle);
        EXPECT_NE(before.enabled & kBit, 0u);
        EXPECT_NE(before.pending & kBit, 0u);
        EXPECT_NE(before.active & kBit, 0u);
        EXPECT_EQ(observed.owner, 0u);
        EXPECT_EQ(observed.message, 0u);
        EXPECT_EQ(observed.enabled & kBit, 0u);
        EXPECT_EQ(observed.pending & kBit, 0u);
        EXPECT_EQ(observed.active & kBit, 0u);
        EXPECT_NE(observed.pwrr & kRedistributorPowerDown, 0u);
        EXPECT_NE(observed.waker & kProcessorSleep, 0u);
        EXPECT_NE(observed.waker & kChildrenAsleep, 0u);
        EXPECT_EQ(observed.late_irq_count, 0u);
    }
}

int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    if (argc == 2 &&
        std::strcmp(argv[1], "--negative-without-systemc-reset") == 0) {
        return run_negative_without_systemc_reset();
    }
    if (argc == 2 &&
        std::strcmp(
            argv[1], "--negative-pwrr-before-children-asleep") == 0) {
        return run_negative_pwrr_before_children_asleep();
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
