// SPDX-License-Identifier: BSD-3-Clause
#include <cstdio>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s TEST.lua APOLLO_DIR/\n", argv[0]);
        return 2;
    }
    lua_State* state = luaL_newstate();
    if (!state) return 1;
    luaL_openlibs(state);
    lua_pushstring(state, argv[2]);
    lua_setglobal(state, "apollo_dir");
    int result = luaL_dofile(state, argv[1]);
    if (result != LUA_OK) {
        std::fprintf(stderr, "%s\n", lua_tostring(state, -1));
    }
    lua_close(state);
    return result == LUA_OK ? 0 : 1;
}
