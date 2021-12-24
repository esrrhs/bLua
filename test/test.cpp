#include "luabridge.h"
#include "lua.hpp"

class A {
public:
    A() {}

    int get_int() { return 1; }
};

int main(int argc, char *argv[]) {
    auto L = luaL_newstate();
    printf("top: %d\n", lua_gettop(L));

    luabridge::reg_class<A>(L);
    luabridge::reg_class_func<A>(L, "get_int", &A::get_int);

    printf("top: %d\n", lua_gettop(L));
    lua_close(L);
    return 0;
}
