/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace apollo_si_test {

const std::vector<const char*> kEnvironmentNames = {
    "QBOX_APOLLO_FULL_SI_SINGLE_GIC",      "QBOX_APOLLO_FULL_SI_ACCEL",     "QBOX_APOLLO_FULL_SI_TCG_MODE",
    "QBOX_APOLLO_FULL_SI_SYNC_POLICY",     "QBOX_APOLLO_FULL_SI_CL0_ACCEL", "QBOX_APOLLO_FULL_SI_CL0_TCG_MODE",
    "QBOX_APOLLO_FULL_SI_CL0_SYNC_POLICY", "QBOX_APOLLO_FULL_SI_CL1_ACCEL", "QBOX_APOLLO_FULL_SI_CL1_TCG_MODE",
    "QBOX_APOLLO_FULL_SI_CL1_SYNC_POLICY",
};

class Environment
{
public:
    explicit Environment(const std::map<std::string, std::string>& assignments)
    {
        for (const char* name : kEnvironmentNames) {
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
        for (const char* name : kEnvironmentNames) {
            unsetenv(name);
        }
        for (const auto& assignment : original_) {
            setenv(assignment.first.c_str(), assignment.second.c_str(), 1);
        }
    }

private:
    std::map<std::string, std::string> original_;
};

class LuaState
{
public:
    LuaState(): state_(luaL_newstate())
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

inline std::string quote(const std::string& value)
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

inline void run(lua_State* state, const std::string& script, int results)
{
    if (luaL_loadstring(state, script.c_str()) != LUA_OK || lua_pcall(state, 0, results, 0) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        throw std::runtime_error(message == nullptr ? "Lua error" : message);
    }
}

} // namespace apollo_si_test
