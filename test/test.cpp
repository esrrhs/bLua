#include "bLua.h"
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <atomic>

static std::atomic<int> g_alive{0};
static std::atomic<int> g_custom_deleted{0};

class A {
public:
    A() : m_a(0) { ++g_alive; }
    explicit A(int v) : m_a(v) { ++g_alive; }
    A(int v, const char *s) : m_a(v), m_b(s ? s : "") { ++g_alive; }
    ~A() { --g_alive; }

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
        m_b = i ? i : "";
    }

    A *get_this() {
        return this;
    }

    // public for property binding
    int m_a;
    std::string m_b;
};

class B {
public:
    explicit B(int v) : value(v) { ++g_alive; }
    ~B() { --g_alive; }
    int value;
};

enum class Color : int {
    Red = 1,
    Green = 2,
    Blue = 3
};

A *newA() {
    return new A;
}

void printA(A *a) {
    printf("A(%p,%d,%s)\n", (void *) a, a->get_int(), a->get_string());
}

const char *maybe_null_string(bool return_null) {
    return return_null ? nullptr : "ok";
}

int add1(int x) { return x + 1; }
int add2(int x, int y) { return x + y; }
int add3(int x, int y, int z) { return x + y + z; }

std::vector<int> double_vec(std::vector<int> v) {
    for (auto &x : v) x *= 2;
    return v;
}

std::map<std::string, int> bump_map(std::map<std::string, int> m) {
    for (auto &kv : m) kv.second += 1;
    return m;
}

void custom_delete_B(B *p) {
    ++g_custom_deleted;
    delete p;
}

static void expect_lua_ok(lua_State *L, int rc, const char *what) {
    if (rc != LUA_OK) {
        printf("FAIL %s: %s\n", what, lua_tostring(L, -1));
        assert(false && what);
    }
}

static void test_nullptr_string(lua_State *L) {
    bLua::reg_global_func(L, "maybe_null_string", maybe_null_string);
    expect_lua_ok(L, luaL_dostring(L,
        "assert(maybe_null_string(false) == 'ok')\n"
        "assert(maybe_null_string(true) == nil)\n"
    ), "nullptr string");
    printf("[ok] nullptr string -> nil\n");
}

static void test_type_error(lua_State *L) {
    expect_lua_ok(L, luaL_dostring(L,
        "local ok, err = pcall(function()\n"
        "  local a = newA()\n"
        "  a:set_int('not_a_number_but_coerced')\n"  // lua coerces
        "end)\n"
        // Wrong userdata type: pass a plain table as self
        "local ok2, err2 = pcall(function()\n"
        "  local a = newA()\n"
        "  local mt = getmetatable(a)\n"
        "  mt.get_int({})\n"
        "end)\n"
        "assert(ok2 == false, 'expected type error for wrong self')\n"
        "assert(type(err2) == 'string' and #err2 > 0)\n"
    ), "type error");
    printf("[ok] type error on wrong userdata\n");
}

static void test_ownership_gc() {
    // Isolated state so alive counters are not polluted by other tests.
    auto L = luaL_newstate();
    luaL_openlibs(L);
    g_alive = 0;

    bLua::reg_class<A>(L, "A");
    bLua::reg_constructor<A>(L, "newA");
    bLua::reg_class_func(L, "set_int", &A::set_int);

    expect_lua_ok(L, luaL_dostring(L,
        "do local a = newA(); a:set_int(1) end\n"
        "collectgarbage('collect')\n"
        "collectgarbage('collect')\n"
    ), "owning GC");
    assert(g_alive.load() == 0 && "owning A should be deleted by Lua GC");
    printf("[ok] owning GC deletes A\n");

    // Non-owning: C++ stack object must survive Lua GC
    bLua::reg_class_non_owning<B>(L, "B");
    {
        B stack_b(42);
        assert(g_alive.load() == 1);
        bLua::push(L, &stack_b, bLua::ownership::non_owning);
        lua_setglobal(L, "stack_b");
        expect_lua_ok(L, luaL_dostring(L,
            "assert(stack_b ~= nil)\n"
            "stack_b = nil\n"
            "collectgarbage('collect')\n"
            "collectgarbage('collect')\n"
        ), "non-owning GC");
        assert(g_alive.load() == 1 && "non-owning must not delete");
        printf("[ok] non-owning GC does not delete\n");
    }
    assert(g_alive.load() == 0);

    lua_close(L);
}

static void test_custom_deleter(lua_State *L2) {
    g_alive = 0;
    g_custom_deleted = 0;
    bLua::reg_class_with_deleter<B>(L2, custom_delete_B, "BOwned");
    bLua::reg_constructor<B, int>(L2, "newB");
    expect_lua_ok(L2, luaL_dostring(L2,
        "do local b = newB(7) end\n"
        "collectgarbage('collect')\n"
        "collectgarbage('collect')\n"
    ), "custom deleter");
    assert(g_custom_deleted.load() == 1 && "custom deleter should run once");
    assert(g_alive.load() == 0);
    printf("[ok] custom deleter\n");
}

static void test_stable_type_name(lua_State * /*L*/) {
    const char *name = bLua::get_type_name<A>();
    assert(name != nullptr);
    assert(std::strcmp(name, "A") == 0);
    printf("[ok] stable type name: %s\n", name);
}

static void test_constructor_property_enum_overload(lua_State *L) {
    bLua::reg_constructor<A>(L, "A_new");
    bLua::reg_constructor<A, int>(L, "A_new1");
    bLua::reg_constructor<A, int, const char *>(L, "A_new2");
    bLua::reg_property(L, "a", &A::m_a);
    bLua::reg_property(L, "b", &A::m_b);
    bLua::reg_enum<Color>(L, "Color", {
        {"Red", Color::Red},
        {"Green", Color::Green},
        {"Blue", Color::Blue},
    });
    bLua::reg_global_overload(L, "add", add1, add2, add3);

    expect_lua_ok(L, luaL_dostring(L,
        "local a = A_new()\n"
        "a:set_a(10)\n"
        "assert(a:get_a() == 10)\n"
        "a:set_b('prop')\n"
        "assert(a:get_b() == 'prop')\n"
        "local a1 = A_new1(5)\n"
        "assert(a1:get_int() == 5)\n"
        "local a2 = A_new2(9, 'xy')\n"
        "assert(a2:get_int() == 9 and a2:get_string() == 'xy')\n"
        "assert(Color.Red == 1 and Color.Green == 2 and Color.Blue == 3)\n"
        "assert(add(10) == 11)\n"
        "assert(add(3, 4) == 7)\n"
        "assert(add(1, 2, 3) == 6)\n"
        "local ok, err = pcall(function() add(1,2,3,4) end)\n"
        "assert(ok == false)\n"
    ), "ctor/property/enum/overload");
    printf("[ok] constructor / property / enum / overload\n");
}

static void test_containers(lua_State *L) {
    bLua::reg_global_func(L, "double_vec", double_vec);
    bLua::reg_global_func(L, "bump_map", bump_map);

    expect_lua_ok(L, luaL_dostring(L,
        "local v = double_vec({1, 2, 3})\n"
        "assert(v[1] == 2 and v[2] == 4 and v[3] == 6)\n"
        "local m = bump_map({a = 1, b = 2})\n"
        "assert(m.a == 2 and m.b == 3)\n"
    ), "containers");

    // C++ side helpers
    lua_newtable(L);
    lua_pushinteger(L, 10);
    lua_rawseti(L, -2, 1);
    lua_pushinteger(L, 20);
    lua_rawseti(L, -2, 2);
    auto vec = bLua::to_vector<int>(L, -1);
    lua_pop(L, 1);
    assert(vec.size() == 2 && vec[0] == 10 && vec[1] == 20);

    std::map<std::string, int> src{{"x", 1}, {"y", 2}};
    bLua::push_map(L, src);
    auto back = bLua::to_map<std::string, int>(L, -1);
    lua_pop(L, 1);
    assert(back["x"] == 1 && back["y"] == 2);

    printf("[ok] vector/map table conversion\n");
}

static void test_multi_return(lua_State *L) {
    expect_lua_ok(L, luaL_dostring(L,
        "function multi_ret()\n"
        "  return 1, 'two', 3\n"
        "end\n"
        "function no_ret()\n"
        "end\n"
        "function one_ret()\n"
        "  return 99\n"
        "end\n"
    ), "define multi_ret");

    int a = 0;
    std::string b;
    int c = 0;
    auto err = bLua::call_lua_global_func(L, "multi_ret", std::tie(a, b, c));
    assert(!err);
    assert(a == 1 && b == "two" && c == 3);

    // Request fewer returns than provided
    int only = 0;
    err = bLua::call_lua_global_func(L, "multi_ret", std::tie(only));
    assert(!err);
    assert(only == 1);

    // Zero returns
    err = bLua::call_lua_global_func(L, "no_ret", std::tie());
    assert(!err);

    // Function returns nil-ish / nothing while we ask for a value -> converted as default
    int z = 123;
    err = bLua::call_lua_global_func(L, "no_ret", std::tie(z));
    assert(!err);
    // lua_tointeger on nil -> 0
    assert(z == 0);

    int one = 0;
    err = bLua::call_lua_global_func(L, "one_ret", std::tie(one));
    assert(!err && one == 99);

    printf("[ok] multi-return edge cases\n");
}

int main(int /*argc*/, char * /*argv*/[]) {
    auto L = luaL_newstate();
    luaL_openlibs(L);

    bLua::reg_global_func(L, "newA", newA);
    bLua::reg_global_func(L, "printA", printA);

    bLua::reg_class<A>(L, "A");
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

    // Extended tests
    test_stable_type_name(L);
    test_nullptr_string(L);
    test_type_error(L);
    test_containers(L);
    test_multi_return(L);
    test_constructor_property_enum_overload(L);
    test_ownership_gc();

    lua_close(L);

    // Custom deleter needs a fresh Lua state (fresh metatable registration for B)
    auto L2 = luaL_newstate();
    luaL_openlibs(L2);
    test_custom_deleter(L2);
    lua_close(L2);

    printf("All bLua tests passed successfully!\n");
    return 0;
}
