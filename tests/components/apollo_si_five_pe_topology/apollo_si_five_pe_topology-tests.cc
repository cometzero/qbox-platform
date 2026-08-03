/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../apollo_si_single_instance/lua_test_support.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

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

std::string five_pe_script()
{
    const std::string directory = APOLLO_HW_BLOCK_DIR;
    return "local config=dofile(" + quote(directory + "/config.lua") +
           ")\n"
           "local cl0=dofile(" + quote(directory + "/si_cl0.lua") + ")\n"
           "local cl1=dofile(" + quote(directory + "/si_cl1.lua") + ")\n"
           "local contract={range=function() return {base=1,size=2} end}\n"
           "local machine={topology={}}\n"
           "local ctx=config.create('',contract,machine)\n"
           "local platform={si_cl0_gic={},si_cl1_gic={}}\n"
           "local shared=cl0.define_qemu_instance(ctx,platform)\n"
           "assert(shared==cl1.define_qemu_instance(ctx,platform))\n"
           "cl0.define_loader_and_cpu(ctx,platform,shared,'cl0.bin')\n"
           "cl1.define_loader_and_cpus(ctx,platform,shared,'cl1.bin')\n"
           "cl0.define_cpu_reset_hooks(ctx,platform)\n"
           "cl1.define_cpu_reset_hooks(ctx,platform)\n";
}

TEST(ApolloSiFivePeTopology, SingleModeCreatesExactFivePeContract)
{
    // Given: one shared Safety Island QEMU instance and distinct images.
    Environment environment(kSingleMode);
    LuaState lua;
    const std::string script =
        five_pe_script() +
        "local pes=cl1.validate_five_pe_topology(ctx,platform)\n"
        "return #pes,pes[1].mp_affinity,pes[2].mp_affinity,"
        "pes[3].mp_affinity,pes[4].mp_affinity,pes[5].mp_affinity,"
        "pes[1].image,pes[2].image,pes[1].router,pes[5].router,"
        "pes[1].reset,pes[5].reset";

    // When: the production Lua helpers build and validate the PE graph.
    run(lua.get(), script, 12);

    // Then: CPU0 is CL0 and CPUs1-4 are CL1 with unique FVP affinities.
    EXPECT_EQ(lua_tointeger(lua.get(), -12), 5);
    EXPECT_EQ(lua_tointeger(lua.get(), -11), 0x00000);
    EXPECT_EQ(lua_tointeger(lua.get(), -10), 0x10000);
    EXPECT_EQ(lua_tointeger(lua.get(), -9), 0x10100);
    EXPECT_EQ(lua_tointeger(lua.get(), -8), 0x10200);
    EXPECT_EQ(lua_tointeger(lua.get(), -7), 0x10300);
    EXPECT_STREQ(lua_tostring(lua.get(), -6), "cl0.bin");
    EXPECT_STREQ(lua_tostring(lua.get(), -5), "cl1.bin");
    EXPECT_STREQ(
        lua_tostring(lua.get(), -4),
        "&si_cl0_ni710ae_primary_nci.protected_target_socket");
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "&si_cl1_router.target_socket");
    EXPECT_STREQ(
        lua_tostring(lua.get(), -2),
        "&platform.si_cl0_cpu_0");
    EXPECT_STREQ(
        lua_tostring(lua.get(), -1),
        "&platform.si_cl1_cpu_3");
}

TEST(ApolloSiFivePeTopology, DuplicateMpidrIsRejected)
{
    // Given: a five-PE graph with CL1 CPU1 duplicating CL1 CPU0 affinity.
    Environment environment(kSingleMode);
    LuaState lua;
    const std::string script =
        five_pe_script() +
        "platform.si_cl1_cpu_1.mp_affinity="
        "platform.si_cl1_cpu_0.mp_affinity\n"
        "local ok,message=pcall(function()\n"
        " cl1.validate_five_pe_topology(ctx,platform)\n"
        "end)\n"
        "return ok,tostring(message)";

    // When: the graph crosses the single-instance validation boundary.
    run(lua.get(), script, 2);

    // Then: duplicate affinity is rejected before model construction.
    EXPECT_FALSE(lua_toboolean(lua.get(), -2));
    ASSERT_NE(lua_tostring(lua.get(), -1), nullptr);
    EXPECT_NE(
        std::string(lua_tostring(lua.get(), -1)).find("duplicate MPIDR"),
        std::string::npos);
}

TEST(ApolloSiFivePeTopology, Cl1ImageOnCpu0IsRejected)
{
    // Given: CPU0's CL0 loader is corrupted to load the CL1 image.
    Environment environment(kSingleMode);
    LuaState lua;
    const std::string script =
        five_pe_script() +
        "platform.si_cl0_loader[1].bin_file="
        "platform.si_cl1_loader[1].bin_file\n"
        "local ok,message=pcall(function()\n"
        " cl1.validate_five_pe_topology(ctx,platform)\n"
        "end)\n"
        "return ok,tostring(message)";

    // When: image ownership is validated for the shared instance.
    run(lua.get(), script, 2);

    // Then: CPU0 cannot silently receive the CL1 image.
    EXPECT_FALSE(lua_toboolean(lua.get(), -2));
    ASSERT_NE(lua_tostring(lua.get(), -1), nullptr);
    EXPECT_NE(
        std::string(lua_tostring(lua.get(), -1)).find(
            "CL1 image must not be assigned to CPU0"),
        std::string::npos);
}

} // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
