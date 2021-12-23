#include "luabridge.h"
#include "lua.hpp"

int main(int argc, char *argv[]) {
    auto L = luaL_newstate();

    lua_close(L);
    return 0;
}
