/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace {

const char* const kSingleGic = "QBOX_APOLLO_FULL_SI_SINGLE_GIC";
const char* const kSiAccel = "QBOX_APOLLO_FULL_SI_ACCEL";
const char* const kSiTcgMode = "QBOX_APOLLO_FULL_SI_TCG_MODE";
const char* const kSiSyncPolicy = "QBOX_APOLLO_FULL_SI_SYNC_POLICY";
const char* const kCl0Accel = "QBOX_APOLLO_FULL_SI_CL0_ACCEL";
const char* const kCl0TcgMode = "QBOX_APOLLO_FULL_SI_CL0_TCG_MODE";
const char* const kCl0SyncPolicy = "QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY";
const char* const kCl1Accel = "QBOX_APOLLO_FULL_SI_CL1_ACCEL";
const char* const kCl1TcgMode = "QBOX_APOLLO_FULL_SI_CL1_TCG_MODE";
const char* const kCl1SyncPolicy = "QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY";

const std::vector<const char*> kSiEnvironment = {
    kSingleGic, kSiAccel, kSiTcgMode, kSiSyncPolicy, kCl0Accel,
    kCl0TcgMode, kCl0SyncPolicy, kCl1Accel, kCl1TcgMode, kCl1SyncPolicy,
};

class Environment {
public:
    explicit Environment(
        const std::map<std::string, std::string>& assignments = {})
    {
        for (const char* name : kSiEnvironment) {
            const char* value = std::getenv(name);
            if (value != nullptr) {
                original_.emplace(name, value);
            }
            unsetenv(name);
        }
        for (const auto& assignment : assignments) {
            setenv(assignment.first.c_str(), assignment.second.c_str(), 1);
        }
    }

    ~Environment()
    {
        for (const char* name : kSiEnvironment) {
            unsetenv(name);
        }
        for (const auto& assignment : original_) {
            setenv(assignment.first.c_str(), assignment.second.c_str(), 1);
        }
    }

private:
    std::map<std::string, std::string> original_;
};

class LuaState {
public:
    LuaState() : state_(luaL_newstate())
    {
        if (state_ == nullptr) {
            throw std::runtime_error("failed to create Lua state");
        }
        luaL_openlibs(state_);
    }

    ~LuaState() { lua_close(state_); }

    lua_State* get() const { return state_; }

private:
    lua_State* state_;
};

std::string lua_quote(const std::string& value)
{
    std::string result = "\"";
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            result += '\\';
        }
        result += character;
    }
    return result + "\"";
}

void run(lua_State* state, const std::string& script, int results)
{
    if (luaL_loadstring(state, script.c_str()) != LUA_OK ||
        lua_pcall(state, 0, results, 0) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        throw std::runtime_error(message == nullptr ? "Lua error" : message);
    }
}

std::string config_create_script()
{
    const std::string config_path =
        lua_quote(std::string(APOLLO_HW_BLOCK_DIR) + "/config.lua");
    return "local module=dofile(" + config_path + ")\n"
           "local contract={range=function() return {base=1,size=2} end}\n"
           "local machine={topology={si_gic_mode="
           "(os.getenv('" + std::string(kSingleGic) +
           "')=='true' and 'single' or 'split')}}\n"
           "local ctx=module.create('',contract,machine)\n";
}

std::string config_result_script()
{
    return config_create_script() +
           "return ctx.config.si.single_gic,ctx.config.si.gic_mode,"
           "ctx.config.si.accel,ctx.config.si.tcg_mode,"
           "ctx.config.si.sync_policy,machine.topology.si_gic_mode";
}

std::string topology_mode()
{
    LuaState lua;
    run(lua.get(),
        "return dofile(" + lua_quote(
            std::string(APOLLO_HW_BLOCK_DIR) + "/topology.lua") +
            ").si_gic_mode",
        1);
    const char* mode = lua_tostring(lua.get(), -1);
    return mode == nullptr ? "" : mode;
}

TEST(ApolloSiSingleGicConfig, UnsetAndFalsePreserveLegacySplitConfiguration)
{
    for (const std::map<std::string, std::string> assignments :
         {std::map<std::string, std::string>{},
          {{kSingleGic, "false"},
           {kSiTcgMode, "UNUSED"},
           {kCl0TcgMode, "MULTI"},
           {kCl1TcgMode, "SINGLE"}}}) {
        Environment environment(assignments);
        LuaState lua;
        run(lua.get(), config_result_script(), 6);

        EXPECT_FALSE(lua_toboolean(lua.get(), -6));
        EXPECT_STREQ(lua_tostring(lua.get(), -5), "split");
        EXPECT_TRUE(lua_isnil(lua.get(), -4));
        EXPECT_TRUE(lua_isnil(lua.get(), -3));
        EXPECT_TRUE(lua_isnil(lua.get(), -2));
        EXPECT_STREQ(lua_tostring(lua.get(), -1), "split");
        EXPECT_EQ(topology_mode(), "split");
    }
}

TEST(ApolloSiSingleGicConfig, MatchingUnifiedConfigurationSelectsSingleMode)
{
    Environment environment({
        {kSingleGic, "true"},
        {kSiAccel, "tcg"},
        {kSiTcgMode, "MULTI"},
        {kSiSyncPolicy, "multithread-quantum"},
        {kCl0Accel, "tcg"},
        {kCl0TcgMode, "MULTI"},
        {kCl0SyncPolicy, "multithread-quantum"},
        {kCl1Accel, "tcg"},
        {kCl1TcgMode, "MULTI"},
        {kCl1SyncPolicy, "multithread-quantum"},
    });
    LuaState lua;
    run(lua.get(), config_result_script(), 6);

    EXPECT_TRUE(lua_toboolean(lua.get(), -6));
    EXPECT_STREQ(lua_tostring(lua.get(), -5), "single");
    EXPECT_STREQ(lua_tostring(lua.get(), -4), "tcg");
    EXPECT_STREQ(lua_tostring(lua.get(), -3), "MULTI");
    EXPECT_STREQ(lua_tostring(lua.get(), -2), "multithread-quantum");
    EXPECT_STREQ(lua_tostring(lua.get(), -1), "single");
    EXPECT_EQ(topology_mode(), "single");
}

class ApolloSiSingleGicConflictTest
    : public testing::TestWithParam<
          std::pair<std::map<std::string, std::string>, std::string>> {};

TEST_P(ApolloSiSingleGicConflictTest, FailsBeforeModelCreation)
{
    auto assignments = GetParam().first;
    assignments.emplace(kSingleGic, "true");
    Environment environment(assignments);
    LuaState lua;
    const std::string script =
        "local model_created=false\n"
        "local ok,message=pcall(function()\n" + config_create_script() +
        "model_created=true end)\n"
        "return ok,tostring(message),model_created";
    run(lua.get(), script, 3);

    EXPECT_FALSE(lua_toboolean(lua.get(), -3));
    const char* message = lua_tostring(lua.get(), -2);
    ASSERT_NE(message, nullptr);
    EXPECT_NE(std::string(message).find(GetParam().second), std::string::npos);
    EXPECT_FALSE(lua_toboolean(lua.get(), -1));
}

INSTANTIATE_TEST_SUITE_P(
    LegacyAndUnified,
    ApolloSiSingleGicConflictTest,
    testing::Values(
        std::make_pair(
            std::map<std::string, std::string>{
                {kCl0TcgMode, "MULTI"}, {kCl1TcgMode, "SINGLE"}},
            std::string(kCl1TcgMode)),
        std::make_pair(
            std::map<std::string, std::string>{
                {kSiSyncPolicy, "multithread-freerunning"},
                {kCl0SyncPolicy, "multithread-quantum"},
                {kCl1SyncPolicy, "multithread-quantum"}},
            std::string(kSiSyncPolicy))));

}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
