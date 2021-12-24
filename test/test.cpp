#include "luabridge.h"
#include "lua.hpp"

class A {
public:
    A() {}

    int get_int() { return m_a; }

    void set_int(int i) { m_a = i; }

private:
    int m_a;
};

int main(int argc, char *argv[]) {
    auto L = luaL_newstate();
    luaL_openlibs(L);
    printf("before top: %d\n", lua_gettop(L));

    luabridge::reg_class<A>(L);
    luabridge::reg_class_func(L, "get_int", &A::get_int);
    luabridge::reg_class_func(L, "set_int", &A::set_int);

    printf("after top: %d\n", lua_gettop(L));

    luaL_dofile(L, "test.lua");

    lua_close(L);
    return 0;
}
