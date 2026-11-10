#include <bLua.h>
#include <cstdio>
const void *other_translation_unit_key();
int main() {
    if (other_translation_unit_key() != &bLua::internal::type_key<int>) return 1;
    auto L = luaL_newstate();
    if (!L) return 2;
    const int rc = luaL_dostring(L, "function twice(x) return x*2 end");
    int n = 0;
    auto err = bLua::call_lua_global_func(L, "twice", std::tie(n), 21);
    lua_close(L);
    if (rc || err || n != 42) return 3;
    std::puts("Relocated bLua package consumer passed");
}
