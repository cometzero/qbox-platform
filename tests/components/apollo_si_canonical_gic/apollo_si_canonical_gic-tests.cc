/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../apollo_si_single_instance/lua_test_support.h"
#include "canonical_gic_integration.h"

#include <iostream>
#include <map>
#include <string>

#include <cci/utils/broker.h>
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

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
        return { 127, "pipe failed" };
    }

    const pid_t pid = fork();
    if (pid == 0) {
        dup2(output_pipe[1], STDOUT_FILENO);
        dup2(output_pipe[1], STDERR_FILENO);
        close(output_pipe[0]);
        close(output_pipe[1]);
        execl("/proc/self/exe", "apollo_si_canonical_gic-tests",
              scenario, nullptr);
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
    const int exit_code = WIFEXITED(status)
        ? WEXITSTATUS(status)
        : 128 + WTERMSIG(status);
    return { exit_code, output };
}

std::string production_graph_script()
{
    const std::string directory = APOLLO_PLATFORM_DIR;
    return "enable_ap_cpus=true\n"
           "dofile(" +
           quote(directory + "/apollo-qvp.lua") +
           ")\n"
           "local cl1=dofile(" +
           quote(directory + "/hw-block/si_cl1.lua") +
           ")\n";
}

TEST(ApolloSiCanonicalGic, SingleModeUsesOneQemuOwnedFivePeController)
{
    // Given: the opt-in production Apollo graph in single-GIC mode.
    Environment environment(kSingleMode);
    LuaState lua;
    const std::string script =
        production_graph_script() +
        "local metrics=cl1.validate_canonical_gic(platform)\n"
        "return metrics.gic_count,metrics.redistributor_count,"
        "metrics.cpu_interface_count,metrics.normal_spi_count,"
        "metrics.forbidden_symbol_count,"
        "platform.si_cl0_gic.redist_region[1],"
        "platform.si_cl0_gic.redist_region[2],"
        "platform.si_cl0_gic.args[1]";

    // When: production construction and canonical realize validation run.
    run(lua.get(), script, 8);

    // Then: one QEMU GIC owns five interfaces and 960 normal SPIs.
    EXPECT_EQ(lua_tointeger(lua.get(), -8), 1);
    EXPECT_EQ(lua_tointeger(lua.get(), -7), 5);
    EXPECT_EQ(lua_tointeger(lua.get(), -6), 5);
    EXPECT_EQ(lua_tointeger(lua.get(), -5), 960);
    EXPECT_EQ(lua_tointeger(lua.get(), -4), 0);
    EXPECT_EQ(lua_tointeger(lua.get(), -3), 1);
    EXPECT_EQ(lua_tointeger(lua.get(), -2), 4);
    EXPECT_STREQ(lua_tostring(lua.get(), -1), "&platform.si_qemu_inst");

    std::cout << "canonical_gic_metrics gic=1 redistributors=5 "
                 "cpu_interfaces=5 normal_spi=960 "
                 "forbidden_second_gic_or_shadow_state=0\n";
}

TEST(ApolloSiCanonicalGic, SingleModePinsSharedQemuCpuConstructionBeforeGic)
{
    // Given: the production Apollo graph in shared SI QEMU/GIC mode.
    Environment environment(kSingleMode);
    LuaState lua;
    const std::string script =
        production_graph_script() +
        "return platform.si_qemu_inst_mgr.construction_priority or 0,"
        "platform.si_qemu_inst.construction_priority or 0,"
        "platform.si_cl0_cpu_0.construction_priority or 0,"
        "platform.si_cl1_cpu_0.construction_priority or 0,"
        "platform.si_cl1_cpu_1.construction_priority or 0,"
        "platform.si_cl1_cpu_2.construction_priority or 0,"
        "platform.si_cl1_cpu_3.construction_priority or 0,"
        "platform.si_cl0_gic.construction_priority or 0";

    // When: the factory consumes the graph's ascending priorities.
    run(lua.get(), script, 8);

    // Then: QEMU CPU indices 0..4 are constructed before the shared GIC.
    EXPECT_EQ(lua_tointeger(lua.get(), -8), -300);
    EXPECT_EQ(lua_tointeger(lua.get(), -7), -299);
    EXPECT_EQ(lua_tointeger(lua.get(), -6), -200);
    EXPECT_EQ(lua_tointeger(lua.get(), -5), -199);
    EXPECT_EQ(lua_tointeger(lua.get(), -4), -198);
    EXPECT_EQ(lua_tointeger(lua.get(), -3), -197);
    EXPECT_EQ(lua_tointeger(lua.get(), -2), -196);
    EXPECT_EQ(lua_tointeger(lua.get(), -1), 0);
}

TEST(ApolloSiCanonicalGic, DefaultSplitModeKeepsConstructionOrderUnchanged)
{
    // Given: the default split SI QEMU/GIC production graph.
    Environment environment({});
    LuaState lua;
    const std::string script =
        production_graph_script() +
        "return platform.si_cl0_qemu_inst_mgr.construction_priority or 0,"
        "platform.si_cl0_qemu_inst.construction_priority or 0,"
        "platform.si_cl0_cpu_0.construction_priority or 0,"
        "platform.si_cl0_gic.construction_priority or 0,"
        "platform.si_cl1_qemu_inst_mgr.construction_priority or 0,"
        "platform.si_cl1_qemu_inst.construction_priority or 0,"
        "platform.si_cl1_cpu_0.construction_priority or 0,"
        "platform.si_cl1_gic.construction_priority or 0";

    // When: the factory reads construction priorities in split mode.
    run(lua.get(), script, 8);

    // Then: every split lifecycle retains its established default ordering.
    for (int index = -8; index <= -1; ++index) {
        EXPECT_EQ(lua_tointeger(lua.get(), index), 0);
    }
}

TEST(ApolloSiCanonicalGic, CpuZeroAndCpuFourPpisStayCpuLocal)
{
    // Given: a real five-CPU QEMU arm-gicv3 component graph.
    const ChildResult child = run_child("--integration-ppi");

    // When: CPU0 and CPU4 physical-timer PPIs assert and deassert.
    ASSERT_EQ(child.exit_code, 0) << child.output;

    // Then: all five real IRQ outputs prove owner-only delivery.
    EXPECT_NE(child.output.find("cpu0_assert=10000"), std::string::npos);
    EXPECT_NE(child.output.find("cpu0_deassert=00000"), std::string::npos);
    EXPECT_NE(child.output.find("cpu4_assert=00001"), std::string::npos);
    EXPECT_NE(child.output.find("cpu4_deassert=00000"), std::string::npos);
    EXPECT_NE(child.output.find("observed=1"), std::string::npos);
}

TEST(ApolloSiCanonicalGic, CpuOutsideSharedInstanceFailsRealizeValidation)
{
    // Given: four CPUs share the GIC instance and CPU4 belongs elsewhere.
    const ChildResult child = run_child("--integration-foreign");

    // When: the real QBox/QEMU component graph reaches GIC realization.
    ASSERT_EQ(child.exit_code, 64) << child.output;

    // Then: component realization fails explicitly instead of crashing QEMU.
    EXPECT_NE(child.output.find("real_component_diagnostic"),
              std::string::npos);
    EXPECT_NE(child.output.find("realize error"), std::string::npos);
    EXPECT_NE(child.output.find("owns 4 CPU(s)"), std::string::npos);
    EXPECT_NE(child.output.find("index range [0, 4]"), std::string::npos);
}

TEST(ApolloSiCanonicalGic, UnknownIntegrationSelectorFailsClosed)
{
    // Given: an unsupported integration selector.
    const ChildResult child = run_child("--integration-does-not-exist");

    // When: the native test entrypoint validates the selector.
    ASSERT_NE(child.exit_code, 0) << child.output;

    // Then: it rejects the selector before initializing QEMU.
    EXPECT_NE(child.output.find("unknown integration selector"),
              std::string::npos);
    EXPECT_EQ(child.output.find("Initializing QEMU instance"),
              std::string::npos);
}

TEST(ApolloSiCanonicalGic, DefaultSplitModeKeepsTwoIndependentControllers)
{
    // Given: the default production Apollo graph without the opt-in.
    Environment environment({});
    LuaState lua;
    const std::string script =
        production_graph_script() +
        "local gics=0\n"
        "for name,module in pairs(platform) do\n"
        " if name:match('^si_.*gic$') and module.moduletype=='arm_gicv3' "
        "then gics=gics+1 end\n"
        "end\n"
        "return gics,platform.si_cl0_gic.num_cpus,"
        "platform.si_cl0_gic.num_spi,platform.si_cl1_gic.num_cpus,"
        "platform.si_cl1_gic.num_spi,"
        "platform.si_cl1_cpu_3.irq_timer_phys_out.bind";

    // When: the unchanged split graph is constructed.
    run(lua.get(), script, 6);

    // Then: its original two GICs and local CPU indices remain intact.
    EXPECT_EQ(lua_tointeger(lua.get(), -6), 2);
    EXPECT_EQ(lua_tointeger(lua.get(), -5), 1);
    EXPECT_EQ(lua_tointeger(lua.get(), -4), 384);
    EXPECT_EQ(lua_tointeger(lua.get(), -3), 4);
    EXPECT_EQ(lua_tointeger(lua.get(), -2), 128);
    EXPECT_STREQ(
        lua_tostring(lua.get(), -1),
        "&si_cl1_gic.ppi_in_cpu_3_20");
}

} // namespace

int sc_main(int argc, char** argv)
{
    cci_utils::consuming_broker broker("global_broker");
    cci_register_broker(broker);
    if (argc == 2) {
        const std::string selector = argv[1];
        if (selector == "--integration-ppi" ||
            selector == "--integration-foreign") {
            return run_canonical_gic_integration(selector, broker);
        }
        if (selector.find("--integration-") == 0) {
            std::cerr << "unknown integration selector: " << selector << '\n';
            return 64;
        }
    }
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
