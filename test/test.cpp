#include "bLua.h"
#include "lua.hpp"

class A {
public:
    A() {}

    int get_int() {
        return m_a;
    }

    void set_int(int i) {
        m_a = i;
    }

    const char *get_string() {
        return m_b.c_str();
    }

    void set_string(const char *i) {
        m_b = i;
    }

    A *get_this() {
        return this;
    }

private:
    int m_a;
    std::string m_b;
};

A *newA() {
    auto p = new A;
    return p;
}

void printA(A *a) {
    printf("A(%p,%d,%s)\n", a, a->get_int(), a->get_string());
}

int main(int argc, char *argv[]) {
    auto L = luaL_newstate();
    luaL_openlibs(L);

    // register global function
    bLua::reg_global_func(L, "newA", newA);
    bLua::reg_global_func(L, "printA", printA);

    // register class function
    bLua::reg_class<A>(L);
    bLua::reg_class_func(L, "get_this", &A::get_this);
    bLua::reg_class_func(L, "get_int", &A::get_int);
    bLua::reg_class_func(L, "set_int", &A::set_int);
    bLua::reg_class_func(L, "get_string", &A::get_string);
    bLua::reg_class_func(L, "set_string", &A::set_string);

    luaL_dofile(L, "test.lua");

    // call global lua function
    int cost = 0;
    std::string message;
    uint64_t sum = 0;
    A *retA = 0;
    auto err = bLua::call_lua_global_func(L, "benchmark",
                                          std::tie(cost, message, sum, retA),
                                          100000, "test");
    printf("%d %s %llu\n", cost, message.c_str(), sum);
    if (err) {
        printf("ret error %s\n", err.value().c_str());
    } else {
        printA(retA);
    }
 
    // call table lua function
    int testret = 0;
    err = bLua::call_lua_table_func(L, {"_G", "test", "func"}, "test",
                                    std::tie(testret),
                                    100000, 1);
    printf("%d\n", testret);
    if (err) {
        printf("ret error %s\n", err.value().c_str());
    }

    lua_close(L);
    return 0;
}
