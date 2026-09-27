#include "bLua.h"
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <string>

class A {
public:
    A() : m_a(0) {}

    int get_int() {
        return m_a;
    }

    int get_int_const() const {
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
    printf("A(%p,%d,%s)\n", (void *) a, a->get_int(), a->get_string());
}

int main(int /*argc*/, char * /*argv*/[]) {
    auto L = luaL_newstate();
    luaL_openlibs(L);

    // register global function
    bLua::reg_global_func(L, "newA", newA);
    bLua::reg_global_func(L, "printA", printA);

    // register class function
    bLua::reg_class<A>(L);
    bLua::reg_class_func(L, "get_this", &A::get_this);
    bLua::reg_class_func(L, "get_int", &A::get_int);
    bLua::reg_class_func(L, "get_int_const", &A::get_int_const);
    bLua::reg_class_func(L, "set_int", &A::set_int);
    bLua::reg_class_func(L, "get_string", &A::get_string);
    bLua::reg_class_func(L, "set_string", &A::set_string);

    int dofile_res = luaL_dofile(L, "test.lua");
    if (dofile_res != LUA_OK) {
        printf("dofile error: %s\n", lua_tostring(L, -1));
        assert(false && "test.lua execution failed");
    }

    // call global lua function with args and returns
    int cost = 0;
    std::string message;
    uint64_t sum = 0;
    A *retA = nullptr;
    auto err = bLua::call_lua_global_func(L, "benchmark",
                                          std::tie(cost, message, sum, retA),
                                          100000, "test");
    printf("%d %s %llu\n", cost, message.c_str(), (unsigned long long) sum);
    assert(!err && "call benchmark failed");
    assert(message == "test");
    assert(sum == 5000050000ULL);
    assert(retA != nullptr);
    printA(retA);

    // call global lua function with 0 args
    int zero_arg_ret = 0;
    err = bLua::call_lua_global_func(L, "zero_arg_func", std::tie(zero_arg_ret));
    assert(!err && "call zero_arg_func failed");
    assert(zero_arg_ret == 42);
    printf("zero_arg_func returned: %d\n", zero_arg_ret);

    // call table lua function
    int testret = 0;
    err = bLua::call_lua_table_func(L, {"_G", "test", "func"}, "test",
                                    std::tie(testret),
                                    100000, 1);
    printf("%d\n", testret);
    assert(!err && "call table func failed");
    assert(testret == 99999);

    // test non-existent table error
    int dummy = 0;
    err = bLua::call_lua_table_func(L, {"_G", "non_existent_table"}, "test",
                                    std::tie(dummy), 1, 2);
    assert(err.has_value() && "expected error for non-existent table");
    printf("Expected table error caught: %s\n", err.value().c_str());

    // test non-existent function error
    err = bLua::call_lua_global_func(L, "non_existent_func", std::tie(dummy));
    assert(err.has_value() && "expected error for non-existent global function");
    printf("Expected global func error caught: %s\n", err.value().c_str());

    lua_close(L);
    printf("All bLua tests passed successfully!\n");
    return 0;
}
