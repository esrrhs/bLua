#include "bLua.h"
#include "check.h"
#include <cstring>
#include <limits>

namespace {
struct Lua {
    lua_State *L = luaL_newstate();
    Lua() { CHECK(L); luaL_openlibs(L); }
    ~Lua() { lua_close(L); }
    operator lua_State *() const { return L; }
};
void script(lua_State *L, const char *text) {
    const int top = lua_gettop(L);
    int rc = luaL_dostring(L, text);
    if (rc) {
        std::string error = lua_tostring(L, -1) ? lua_tostring(L, -1) : "Lua error";
        lua_settop(L, top); throw std::runtime_error(error);
    }
    CHECK(lua_gettop(L) == top);
}
template<typename F> void rejected(lua_State *L, F f, const char *message) {
    const int top = lua_gettop(L);
    bool caught = false;
    try { f(); } catch (const std::exception &e) {
        caught = true; CHECK(std::string(e.what()).find(message) != std::string::npos);
    }
    CHECK(caught); CHECK(lua_gettop(L) == top);
}
struct Obj {
    static inline int destroyed = 0;
    int value = 7;
    const int readonly = 11;
    ~Obj() { ++destroyed; }
    int get() const { return value; }
    void set(int v) { value = v; }
    Obj *self() { return this; }
};
struct Other { int value = 1; };
int deletes_a = 0, deletes_b = 0;
void del_a(Obj *p) { ++deletes_a; delete p; }
void del_b(Obj *p) { ++deletes_b; delete p; }
void test_states(bool reverse) {
    auto a = luaL_newstate(); auto b = luaL_newstate(); CHECK(a && b);
    luaL_openlibs(a); luaL_openlibs(b);
    deletes_a = deletes_b = 0;
    {
        std::string transient = "First";
        bLua::set_type_name<Obj>(a, transient.c_str());
        bLua::reg_class_with_deleter<Obj>(a, del_a);
        transient.assign(1000, 'x');
    }
    bLua::reg_class_with_deleter<Obj>(b, del_b, "Second");
    CHECK(std::strcmp(bLua::get_type_name<Obj>(a), "First") == 0);
    CHECK(std::strcmp(bLua::get_type_name<Obj>(b), "Second") == 0);
    for (auto state : {a, b}) {
        bLua::reg_class_func(state, "get", &Obj::get);
        bLua::push(state, new Obj); lua_setglobal(state, "obj");
        script(state, "assert(obj:get() == 7)");
    }
    if (reverse) { lua_close(b); CHECK(deletes_b == 1 && deletes_a == 0); lua_close(a); }
    else { lua_close(a); CHECK(deletes_a == 1 && deletes_b == 0); lua_close(b); }
    CHECK(deletes_a == 1 && deletes_b == 1);
}
void test_ownership() {
    Lua s; auto L = s.L; const int before = Obj::destroyed;
    bLua::reg_class<Obj>(L, "Obj");
    bLua::reg_class<Obj>(L, "Obj"); // identical registrations are idempotent
    rejected(L, [&] { bLua::reg_class_non_owning<Obj>(L, "Obj"); }, "conflicting");
    rejected(L, [&] { bLua::set_type_name<Obj>(L, "Renamed"); }, "rename");
    rejected(L, [&] { bLua::reg_class<Other>(L, "Obj"); }, "name already");
    bLua::reg_class<Other>(L, "Other");
    bLua::reg_class_func(L, "get", &Obj::get);
    bLua::reg_class_func(L, "self", &Obj::self);
    lua_pushcfunction(L, +[](lua_State *s) -> int {
        lua_newuserdata(s, sizeof(bLua::internal::userdata<Obj>));
        return 1;
    });
    lua_setglobal(L, "foreign_userdata");
    bLua::reg_property(L, "value", &Obj::value);
    bLua::reg_property(L, "readonly", &Obj::readonly);
    bLua::reg_property(L, "func", &Obj::get, &Obj::set);
    auto p = new Obj;
    bLua::push(L, p); lua_setglobal(L, "o");
    bLua::push(L, p); lua_getglobal(L, "o"); CHECK(lua_rawequal(L, -1, -2)); lua_pop(L, 2);
    rejected(L, [&] { bLua::push(L, p, bLua::ownership::non_owning); }, "ownership");
    rejected(L, [&] { bLua::push(L, reinterpret_cast<Other *>(p)); }, "different type");
    script(L, R"(
        assert(o:self() == o)
        o:set_func(33); assert(o:get_value() == 33)
        assert(o:get_readonly() == 11 and o.set_readonly == nil)
        local gc = getmetatable(o).__gc
        gc({}); gc(io.stdout); gc(foreign_userdata()) -- foreign userdata, including same-sized, is harmless
        gc(o); gc(o)
        assert(not pcall(function() return o:get() end))
        assert(not pcall(function() return o:get_value() end))
        assert(not pcall(function() o:set_value(2) end))
        o = nil; collectgarbage('collect'); collectgarbage('collect')
    )");
    CHECK(Obj::destroyed == before + 1);
    {
        Obj external;
        bLua::push(L, &external, bLua::ownership::non_owning); lua_setglobal(L, "external");
        script(L, "assert(external:self()==external); external=nil; collectgarbage('collect')");
        CHECK(Obj::destroyed == before + 1);
    }
    CHECK(Obj::destroyed == before + 2);
    // Non-owning class defaults are honored by push(L,p), not just explicit overrides.
    Lua borrowed;
    bLua::reg_class_non_owning<Obj>(borrowed, "Borrowed");
    {
        Obj stack;
        bLua::push(borrowed, &stack); lua_setglobal(borrowed, "stack");
        bLua::reg_class_func(borrowed, "get", &Obj::get);
        script(borrowed, "assert(stack:get()==7); getmetatable(stack).__gc(stack); assert(not pcall(function() stack:get() end)); stack=nil; collectgarbage('collect')");
        CHECK(Obj::destroyed == before + 2);
    }
    CHECK(Obj::destroyed == before + 3);
    bLua::reg_constructor<Obj>(borrowed, "make");
    bLua::reg_class_func(borrowed, "self", &Obj::self);
    script(borrowed, "do local created=make(); assert(created:self()==created) end; collectgarbage('collect')");
    CHECK(Obj::destroyed == before + 4); // constructors always own their new object
    rejected(L, [&] { bLua::push(L, reinterpret_cast<Lua *>(L)); }, "register class");
}
int f1(int x) { return x + 1; }
int f2(int a, int b) { return a + b; }
double same_arity(double x) { return x; }
int throw_nonstd() { throw 42; }
int guards = 0;
struct Guard { Guard() { ++guards; } ~Guard() { --guards; } };
int throwing(std::string) { Guard g; throw std::runtime_error("native boom"); }
struct ThrowCtor { explicit ThrowCtor(int) { throw std::runtime_error("constructor boom"); } };
void test_errors() {
    Lua s; auto L = s.L;
    bLua::reg_global_func(L, "throwing", throwing);
    bLua::reg_global_func(L, "throw_nonstd", throw_nonstd);
    bLua::reg_global_func(L, "f1", f1);
    bLua::reg_class<ThrowCtor>(L, "ThrowCtor");
    bLua::reg_constructor<ThrowCtor, int>(L, "ThrowCtor_new");
    script(L, R"(
        local ok,err=pcall(function() throwing('arg') end)
        assert(not ok and err:find('native boom'))
        ok,err=pcall(throw_nonstd)
        assert(not ok and err:find('unknown C++ exception',1,true))
        ok,err=pcall(function() ThrowCtor_new(1) end)
        assert(not ok and err:find('constructor boom'))
        assert(not pcall(function() f1('not number') end))
        assert(not pcall(function() f1(1.5) end))
        function good() return 12, 'hello' end
        function wrong() return 12, {} end
        function wrong_object() return {} end
        function many(...) return select('#',...) end
        function nil_object() return nil end
        function raises() error({reason='not a string'}) end
        function nothing() end
        function lua_boom() error('lua boom') end
        path={inner={good=good,wrong=wrong}}
    )");
    CHECK(guards == 0);
    int count = 0;
    auto many_error = bLua::call_lua_global_func(L, "many", std::tie(count),
        1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30);
    CHECK(!many_error && count == 30);
    int number = -1; std::string text = "old";
    lua_pushinteger(L, 987); // caller stack sentinel survives all successes and failures
    const int top = lua_gettop(L);
    auto err = bLua::call_lua_global_func(L, "good", std::tie(number, text));
    CHECK(!err && number == 12 && text == "hello"); CHECK(lua_gettop(L) == top);
    number = -1; text = "old";
    err = bLua::call_lua_global_func(L, "wrong", std::tie(number, text));
    CHECK(err && number == -1 && text == "old"); CHECK(lua_gettop(L) == top);
    Obj *obj = nullptr;
    err = bLua::call_lua_global_func(L, "wrong_object", std::tie(obj));
    CHECK(err && obj == nullptr); CHECK(lua_gettop(L) == top);
    err = bLua::call_lua_global_func(L, "nil_object", std::tie(obj)); CHECK(!err && !obj);
    for (const char *name : {"missing", "lua_boom", "raises", "nothing"}) {
        err = bLua::call_lua_global_func(L, name, std::tie(number));
        CHECK(err); CHECK(lua_gettop(L) == top);
    }
    err = bLua::call_lua_table_func(L, {"path", "inner"}, "good", std::tie(number, text)); CHECK(!err);
    err = bLua::call_lua_table_func(L, {"path", "inner"}, "wrong", std::tie(number, text)); CHECK(err);
    err = bLua::call_lua_table_func(L, {}, "good", std::tie()); CHECK(err);
    err = bLua::call_lua_table_func(L, {"missing"}, "good", std::tie()); CHECK(err);
    CHECK(lua_gettop(L) == top);
    for (const char *debug : {"nil", "{}", "{traceback=false}", "{traceback=function() return {} end}"}) {
        const std::string code = std::string("debug=") + debug;
        script(L, code.c_str());
        err = bLua::call_lua_global_func(L, "good", std::tie(number, text)); CHECK(!err);
        err = bLua::call_lua_global_func(L, "lua_boom", std::tie()); CHECK(err);
        CHECK(lua_gettop(L) == top);
    }
    script(L, "debug=nil");
    err = bLua::call_lua_global_func(L, "throwing", std::tie(number), std::string("abc"));
    CHECK(err && err->find("native boom") != std::string::npos); CHECK(guards == 0);
    lua_pop(L, 1);
    // No openlibs/debug at all, still usable for plain scripts.
    auto bare = luaL_newstate(); CHECK(bare);
    CHECK(luaL_dostring(bare, "function f() return 5 end") == 0);
    err = bLua::call_lua_global_func(bare, "f", std::tie(number)); CHECK(!err && number == 5);
    lua_close(bare);
}
void test_containers_overloads() {
    Lua s; auto L = s.L;
    bLua::reg_global_overload(L, "over", f1, f2);
    rejected(L, [&] { bLua::reg_global_overload(L, "over", f1, same_arity); }, "duplicate overload");
    script(L, "assert(over(1)==2 and over(2,3)==5); assert(not pcall(function() over() end))");
    script(L, "mixed={ [1]=10, ['1']=11, word=20 }; numeric={ [1]=10, [2]=20 }; strings={a=1,b=2}");
    lua_getglobal(L, "mixed");
    rejected(L, [&] { (void)bLua::to_map<std::string,int>(L,-1); }, "string expected");
    // Validation failed without mutating the source key or leaking stack entries.
    int count=0; lua_pushnil(L);
    while (lua_next(L,-2)) { ++count; lua_pop(L,1); }
    CHECK(count == 3); lua_pop(L,1);
    lua_getglobal(L, "numeric");
    auto map = bLua::to_map<int,int>(L,-1); CHECK(map.size()==2 && map.at(1)==10 && map.at(2)==20);
    auto vec = bLua::to_vector<int>(L,-1); CHECK(vec == std::vector<int>({10,20})); lua_pop(L,1);
    lua_getglobal(L, "strings");
    auto smap=bLua::to_map<std::string,int>(L,-1); CHECK(smap.at("a")==1); lua_pop(L,1);
    bLua::push(L,std::vector<std::vector<int>>{{1,2},{3}});
    auto nested=bLua::to_vector<std::vector<int>>(L,-1); CHECK(nested[1][0]==3); lua_pop(L,1);
    bLua::push(L,std::map<int,std::vector<int>>{{2,{3,4}}});
    auto nestedmap=bLua::to_map<int,std::vector<int>>(L,-1); CHECK(nestedmap.at(2)[1]==4); lua_pop(L,1);
    bLua::push(L,std::vector<bool>{true,false});
    auto booleans=bLua::to_vector<bool>(L,-1); CHECK(booleans[0] && !booleans[1]); lua_pop(L,1);
    lua_pushboolean(L,1); rejected(L,[&]{(void)bLua::to_vector<int>(L,-1);},"table expected"); lua_pop(L,1);
    script(L,"function echo(x) return x end");
    std::vector<std::vector<int>> result;
    auto err=bLua::call_lua_global_func(L,"echo",std::tie(result),nested); CHECK(!err && result==nested);
#if LUA_VERSION_NUM >= 503
    std::int64_t large=0;
    err=bLua::call_lua_global_func(L,"echo",std::tie(large),std::int64_t(9007199254740993LL));
    CHECK(!err && large==9007199254740993LL);
#endif
    CHECK(lua_gettop(L)==0);
}
}
int main() {
    try {
        test_states(false); test_states(true); test_ownership(); test_errors(); test_containers_overloads();
        std::puts("All lifetime/error/container regressions passed"); return 0;
    } catch (const std::exception &e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
