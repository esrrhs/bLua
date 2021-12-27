#include "luabridge.h"
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

    luabridge::reg_global_func(L, "newA", newA);
    luabridge::reg_global_func(L, "printA", printA);

    luabridge::reg_class<A>(L);
    luabridge::reg_class_func(L, "get_this", &A::get_this);
    luabridge::reg_class_func(L, "get_int", &A::get_int);
    luabridge::reg_class_func(L, "set_int", &A::set_int);
    luabridge::reg_class_func(L, "get_string", &A::get_string);
    luabridge::reg_class_func(L, "set_string", &A::set_string);

    luaL_dofile(L, "test.lua");

    int cost = 0;
    std::string message;
    uint64_t sum = 0;
    A *retA = 0;
    auto ret = luabridge::call_lua_global_func(L, "benchmark",
                                               std::tie(cost, message, sum, retA),
                                               100000, "test");
    printf("%d %s %llu\n", cost, message.c_str(), sum);
    printA(retA);
    printf("ret %d %s\n", ret.first, ret.second.c_str());

    lua_close(L);
    return 0;
}
