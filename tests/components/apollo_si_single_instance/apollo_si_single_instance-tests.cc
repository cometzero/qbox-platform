/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lua_test_support.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

namespace {

using apollo_si_test::Environment;
using apollo_si_test::LuaState;
using apollo_si_test::quote;
using apollo_si_test::run;

const std::map<std::string, std::string> kMatchingSingle = {
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

std::string graph_script()
{
    const std::string directory = APOLLO_HW_BLOCK_DIR;
    return "local config=dofile(" + quote(directory + "/config.lua") +
           ")\n"
           "local cl0=dofile(" +
           quote(directory + "/si_cl0.lua") +
           ")\n"
           "local cl1=dofile(" +
           quote(directory + "/si_cl1.lua") +
           ")\n"
           "local contract={range=function() return {base=1,size=2} end}\n"
           "local machine={topology={}}\n"
           "local ctx=config.create('',contract,machine)\n"
           "local platform={apollo_system_reset_fanout={reset_out={bind="
           "'&si_cl0_qemu_inst.reset;&si_cl1_qemu_inst.reset'}}}\n"
           "local cl0_instance=cl0.define_qemu_instance(ctx,platform)\n"
           "local cl1_instance=cl1.define_qemu_instance(ctx,platform)\n"
           "cl0.define_cpu_reset_hooks(ctx,platform)\n"
           "cl1.define_cpu_reset_hooks(ctx,platform)\n";
}

TEST(ApolloSiSingleInstance, MatchingPolicyCreatesOneSharedInstance)
{
    // Given: matching unified and legacy SI acceleration/sync settings.
    Environment environment(kMatchingSingle);
    LuaState lua;
    const std::string script = graph_script() +
                               "cl1.finalize_single_instance(ctx,platform)\n"
                               "local managers,instances=0,0\n"
                               "for _,v in pairs(platform) do\n"
                               " if v.moduletype=='QemuInstanceManager' then managers=managers+1 end\n"
                               " if v.moduletype=='QemuInstance' then instances=instances+1 end\n"
                               "end\n"
                               "return managers,instances,cl0_instance,cl1_instance,"
                               "platform.si_qemu_inst.tcg_mode,platform.si_qemu_inst.sync_policy,"
                               "platform.si_cl0_cpu_0_reset.args[1],"
                               "platform.si_cl1_cpu_3_reset.args[1],"
                               "platform.apollo_system_reset_fanout.reset_out.bind";

    // When: both cluster modules describe their QEMU ownership.
    run(lua.get(), script, 9);

    // Then: one lifecycle is shared while every reset remains CPU-local.
    EXPECT_EQ(lua_tointeger(lua.get(), -9), 1);
    EXPECT_EQ(lua_tointeger(lua.get(), -8), 1);
    EXPECT_STREQ(lua_tostring(lua.get(), -7), "&platform.si_qemu_inst");
    EXPECT_STREQ(lua_tostring(lua.get(), -6), "&platform.si_qemu_inst");
    EXPECT_STREQ(lua_tostring(lua.get(), -5), "MULTI");
    EXPECT_STREQ(lua_tostring(lua.get(), -4), "multithread-quantum");
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "&platform.si_cl0_cpu_0");
    EXPECT_STREQ(lua_tostring(lua.get(), -2), "&platform.si_cl1_cpu_3");
    const std::string reset_targets = lua_tostring(lua.get(), -1);
    EXPECT_EQ(reset_targets.find("si_cl0_qemu_inst"), std::string::npos);
    EXPECT_EQ(reset_targets.find("si_cl1_qemu_inst"), std::string::npos);
}

TEST(ApolloSiSingleInstance, SplitModeKeepsTwoIndependentInstances)
{
    // Given: split mode with intentionally different CL0/CL1 policies.
    Environment environment({
        { "QBOX_APOLLO_FULL_SI_SINGLE_GIC", "false" },
        { "QBOX_APOLLO_FULL_SI_CL0_TCG_MODE", "MULTI" },
        { "QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY", "multithread-quantum" },
        { "QBOX_APOLLO_FULL_SI_CL1_TCG_MODE", "SINGLE" },
        { "QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY", "multithread-freerunning" },
    });
    LuaState lua;
    const std::string script = graph_script() +
                               "local managers,instances=0,0\n"
                               "for _,v in pairs(platform) do\n"
                               " if v.moduletype=='QemuInstanceManager' then managers=managers+1 end\n"
                               " if v.moduletype=='QemuInstance' then instances=instances+1 end\n"
                               "end\n"
                               "return managers,instances,cl0_instance,cl1_instance,"
                               "platform.si_cl0_qemu_inst.tcg_mode,"
                               "platform.si_cl1_qemu_inst.tcg_mode,"
                               "platform.si_cl1_qemu_inst.sync_policy";

    // When: both legacy cluster backends are described.
    run(lua.get(), script, 7);

    // Then: the original two-instance policy remains independent.
    EXPECT_EQ(lua_tointeger(lua.get(), -7), 2);
    EXPECT_EQ(lua_tointeger(lua.get(), -6), 2);
    EXPECT_STREQ(lua_tostring(lua.get(), -5), "&platform.si_cl0_qemu_inst");
    EXPECT_STREQ(lua_tostring(lua.get(), -4), "&platform.si_cl1_qemu_inst");
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "MULTI");
    EXPECT_STREQ(lua_tostring(lua.get(), -2), "SINGLE");
    EXPECT_STREQ(lua_tostring(lua.get(), -1), "multithread-freerunning");
}

TEST(ApolloSiSingleInstance, Cl1ResetCannotRetargetCl0Context)
{
    // Given: a single-instance graph whose CL1 reset hook is corrupted to CL0.
    Environment environment(kMatchingSingle);
    LuaState lua;
    const std::string script = graph_script() +
                               "platform.si_cl0_cpu_0={rvbar=0x1000,request_origin_id=0x2000}\n"
                               "platform.si_cl1_cpu_0_reset.args[1]='&platform.si_cl0_cpu_0'\n"
                               "local rvbar=platform.si_cl0_cpu_0.rvbar\n"
                               "local origin=platform.si_cl0_cpu_0.request_origin_id\n"
                               "local ok,message=pcall(function()\n"
                               " cl1.finalize_single_instance(ctx,platform)\n"
                               " platform.si_cl0_cpu_0.rvbar=0xdead\n"
                               " platform.si_cl0_cpu_0.request_origin_id=0xbeef\n"
                               "end)\n"
                               "return ok,tostring(message),platform.si_cl0_cpu_0.rvbar==rvbar,"
                               "platform.si_cl0_cpu_0.request_origin_id==origin";

    // When: the graph is validated before model creation/reset dispatch.
    run(lua.get(), script, 4);

    // Then: validation fails before CL0 vector or request context can change.
    EXPECT_FALSE(lua_toboolean(lua.get(), -4));
    ASSERT_NE(lua_tostring(lua.get(), -3), nullptr);
    EXPECT_NE(std::string(lua_tostring(lua.get(), -3)).find("si_cl1_cpu_0_reset"), std::string::npos);
    EXPECT_TRUE(lua_toboolean(lua.get(), -2));
    EXPECT_TRUE(lua_toboolean(lua.get(), -1));
}

} // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
