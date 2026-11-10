#pragma once

#if defined(__has_include)
  #if __has_include(<lua.hpp>)
    #include <lua.hpp>
  #elif __has_include(<lua.h>)
    extern "C" {
    #include <lua.h>
    #include <lualib.h>
    #include <lauxlib.h>
    }
  #else
    extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
    }
  #endif
#else
  extern "C" {
  #include "lua.h"
  #include "lualib.h"
  #include "lauxlib.h"
  }
#endif

#include <typeinfo>
#include <type_traits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <memory>
#include <functional>
#include <initializer_list>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <limits>
#include <cmath>

#if defined(__GNUG__) && !defined(_MSC_VER)
#include <cxxabi.h>
#endif

namespace bLua {
    enum class ownership { owning, non_owning };

    namespace internal {
        inline void ensure_stack(lua_State *L, int n) {
            if (!lua_checkstack(L, n)) throw std::runtime_error("Lua stack capacity exhausted");
        }
        inline int absindex(lua_State *L, int i) {
            return i > 0 || i <= LUA_REGISTRYINDEX ? i : lua_gettop(L) + i + 1;
        }
        inline size_t rawlen(lua_State *L, int i) {
#if LUA_VERSION_NUM < 502
            return lua_objlen(L, i);
#else
            return lua_rawlen(L, i);
#endif
        }
        inline void rawgetp(lua_State *L, int i, const void *p) {
            ensure_stack(L, 2);
            i = absindex(L, i);
            lua_pushlightuserdata(L, const_cast<void *>(p));
            lua_rawget(L, i);
        }
        inline void rawsetp(lua_State *L, int i, const void *p) {
            ensure_stack(L, 2);
            i = absindex(L, i);
            lua_pushlightuserdata(L, const_cast<void *>(p));
            lua_insert(L, -2);
            lua_rawset(L, i);
        }
        inline void rawfield(lua_State *L, int i, const char *name) {
            ensure_stack(L, 2);
            i = absindex(L, i);
            lua_pushstring(L, name);
            lua_rawget(L, i);
        }
        inline void globals(lua_State *L) {
#if LUA_VERSION_NUM < 502
            lua_pushvalue(L, LUA_GLOBALSINDEX);
#else
            lua_pushglobaltable(L);
#endif
        }
        struct stack_guard {
            lua_State *L;
            int top;
            explicit stack_guard(lua_State *s) : L(s), top(lua_gettop(s)) {}
            ~stack_guard() { lua_settop(L, top); }
            stack_guard(const stack_guard &) = delete;
            stack_guard &operator=(const stack_guard &) = delete;
        };
        [[noreturn]] inline void fail(const char *s) { throw std::runtime_error(s); }

        // Never luaL_error while an exception / argument tuple is alive. All validation
        // below throws C++; only this outermost callback converts it to a Lua error.
        template<int (*Fn)(lua_State *)>
        int boundary(lua_State *L) noexcept {
            char error[1024];
            try { return Fn(L); }
            catch (const std::exception &e) { std::snprintf(error, sizeof(error), "%s", e.what()); }
            catch (...) { std::snprintf(error, sizeof(error), "unknown C++ exception"); }
            return luaL_error(L, "%s", error);
        }

        template<typename T> inline char type_key;
        template<typename T> inline char meta_key;
        template<typename T> inline char table_key;
        inline char cache_key;

        template<typename T> const std::string &fallback_name() {
            static const std::string name = [] {
#if defined(__GNUG__) && !defined(_MSC_VER)
                int status = 0;
                std::unique_ptr<char, decltype(&std::free)> demangled(
                    abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status), &std::free);
                if (status == 0 && demangled) return std::string(demangled.get());
#endif
                return std::string(typeid(T).name());
            }();
            return name;
        }
        template<typename T> void delete_object(T *p) { delete p; }
        template<typename T> struct class_state {
            std::string name = fallback_name<T>();
            ownership own = ownership::owning;
            void (*deleter)(T *) = &delete_object<T>;
            bool registered = false;
        };
        template<typename T> int free_state(lua_State *L) {
            // Private metatable, not exposed through any object.
            static_cast<class_state<T> *>(lua_touserdata(L, 1))->~class_state<T>();
            return 0;
        }
        template<typename T> class_state<T> &state(lua_State *L) {
            stack_guard guard(L);
            ensure_stack(L, 6);
            rawgetp(L, LUA_REGISTRYINDEX, &meta_key<T>);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                auto p = static_cast<class_state<T> *>(lua_newuserdata(L, sizeof(class_state<T>)));
                new (p) class_state<T>();
                lua_newtable(L);
                lua_pushcfunction(L, free_state<T>);
                lua_setfield(L, -2, "__gc");
                lua_setmetatable(L, -2);
                lua_pushvalue(L, -1);
                rawsetp(L, LUA_REGISTRYINDEX, &meta_key<T>);
            }
            return *static_cast<class_state<T> *>(lua_touserdata(L, -1));
        }
        template<typename T> void push_metatable(lua_State *L) {
            rawgetp(L, LUA_REGISTRYINDEX, &table_key<T>);
            if (!lua_istable(L, -1)) { lua_pop(L, 1); fail("class is not registered"); }
        }
        struct userdata_base {
            const void *type;
            void *ptr;
            bool alive;
            bool owning;
        };
        template<typename T> struct userdata {
            userdata_base base;
            void (*const deleter)(T *); // fixed when this wrapper is created
        };
        template<typename T> userdata<T> *test_object(lua_State *L, int i) {
            ensure_stack(L, 3);
            i = absindex(L, i);
            if (lua_type(L, i) != LUA_TUSERDATA || rawlen(L, i) != sizeof(userdata<T>)) return nullptr;
            stack_guard guard(L);
            if (!lua_getmetatable(L, i)) return nullptr;
            rawgetp(L, LUA_REGISTRYINDEX, &table_key<T>);
            if (!lua_rawequal(L, -1, -2)) return nullptr;
            // Do not interpret foreign userdata's bytes before validating its metatable.
            auto ud = static_cast<userdata<T> *>(lua_touserdata(L, i));
            return ud->base.type == &type_key<T> ? ud : nullptr;
        }
        template<typename T> T *check_t(lua_State *L, int i) {
            auto ud = test_object<T>(L, i);
            if (!ud) fail("wrong userdata type (or class is not registered)");
            if (!ud->base.alive || !ud->base.ptr) fail("object has been destroyed");
            return static_cast<T *>(ud->base.ptr);
        }
        template<typename T> int free_class(lua_State *L) {
            auto ud = test_object<T>(L, 1);
            if (!ud || !ud->base.alive) return 0;
            auto p = static_cast<T *>(ud->base.ptr);
            ud->base.ptr = nullptr;
            ud->base.alive = false; // invalidate before any possibly re-entrant deleter
            if (ud->base.owning && p) ud->deleter(p);
            return 0;
        }
        template<typename T> void push_ptr(lua_State *L, T *p, ownership own, bool reuse_policy = false) {
            static_assert(!std::is_const_v<T>, "const object pointers are not supported");
            if (!p) { lua_pushnil(L); return; }
            auto &meta = state<T>(L);
            if (!meta.registered) fail("register class before pushing its objects");
            auto del = own == ownership::owning ? meta.deleter : nullptr;
            const int top = lua_gettop(L);
            ensure_stack(L, 6);
            try {
                rawgetp(L, LUA_REGISTRYINDEX, &cache_key);
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    lua_newtable(L);
                    lua_pushliteral(L, "v"); lua_setfield(L, -2, "__mode");
                    lua_setmetatable(L, -2);
                    lua_pushvalue(L, -1); rawsetp(L, LUA_REGISTRYINDEX, &cache_key);
                }
                rawgetp(L, -1, p);
                if (!lua_isnil(L, -1)) {
                    auto ud = test_object<T>(L, -1);
                    if (!ud) fail("pointer is already exposed with a different type");
                    if (ud->base.alive) {
                        if (!reuse_policy && (ud->base.owning != (own == ownership::owning) || ud->deleter != del))
                            fail("pointer ownership/deleter conflict");
                        lua_remove(L, -2);
                        return;
                    }
                }
                lua_pop(L, 1);
                auto ud = static_cast<userdata<T> *>(lua_newuserdata(L, sizeof(userdata<T>)));
                new (ud) userdata<T>{{&type_key<T>, p, true, own == ownership::owning}, del};
                push_metatable<T>(L); lua_setmetatable(L, -2);
                lua_pushvalue(L, -1); rawsetp(L, -3, p);
                lua_remove(L, -2);
            } catch (...) { lua_settop(L, top); throw; }
        }

        template<typename T, typename Enable = void> struct converter;
        template<typename T> T lua_to_native(lua_State *L, int i) { return converter<T>::get(L, i); }
        template<typename T> void native_to_lua(lua_State *L, const T &v) { converter<std::decay_t<T>>::push(L, v); }
        template<typename T> void native_to_lua(lua_State *L, T *v, ownership own) { push_ptr(L, v, own); }

        template<typename T> struct converter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>> {
            static T get(lua_State *L, int i) {
                if (lua_type(L, i) != LUA_TNUMBER) fail("number expected");
#if LUA_VERSION_NUM >= 503
                if (lua_isinteger(L, i)) {
                    const lua_Integer n = lua_tointeger(L, i);
                    if constexpr (std::is_unsigned_v<T>) {
                        if (n < 0 || static_cast<lua_Unsigned>(n) > std::numeric_limits<T>::max()) fail("integer out of range");
                    } else {
                        if (n < std::numeric_limits<T>::lowest() || n > std::numeric_limits<T>::max()) fail("integer out of range");
                    }
                    return static_cast<T>(n);
                }
#endif
                const lua_Number n = lua_tonumber(L, i);
                // Avoid undefined out-of-range floating-to-integer conversions, including NaN.
                const long double value = static_cast<long double>(n);
                const long double limit = std::ldexp(1.0L, std::numeric_limits<T>::digits);
                const long double lower = std::is_signed_v<T> ? -limit : 0.0L;
                if (!(value >= lower && value < limit)) fail("integer out of range");
                if (std::trunc(n) != n) fail("integer expected");
                return static_cast<T>(n);
            }
            static void push(lua_State *L, T v) {
                if constexpr (std::is_unsigned_v<T>) {
                    if (v > static_cast<std::make_unsigned_t<lua_Integer>>(std::numeric_limits<lua_Integer>::max()))
                        fail("integer does not fit lua_Integer");
                } else if constexpr (sizeof(T) > sizeof(lua_Integer)) {
                    if (v < std::numeric_limits<lua_Integer>::lowest() || v > std::numeric_limits<lua_Integer>::max())
                        fail("integer does not fit lua_Integer");
                }
                lua_pushinteger(L, static_cast<lua_Integer>(v));
            }
        };
        template<typename T> struct converter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
            static T get(lua_State *L, int i) {
                if (lua_type(L, i) != LUA_TNUMBER) fail("number expected");
                return static_cast<T>(lua_tonumber(L, i));
            }
            static void push(lua_State *L, T v) { lua_pushnumber(L, static_cast<lua_Number>(v)); }
        };
        template<> struct converter<bool> {
            static bool get(lua_State *L, int i) {
                if (lua_type(L, i) != LUA_TBOOLEAN) fail("boolean expected");
                return lua_toboolean(L, i) != 0;
            }
            static void push(lua_State *L, bool v) { lua_pushboolean(L, v); }
        };
        template<> struct converter<std::string> {
            static std::string get(lua_State *L, int i) {
                if (lua_type(L, i) != LUA_TSTRING) fail("string expected");
                size_t n = 0; const char *s = lua_tolstring(L, i, &n);
                return std::string(s, n);
            }
            static void push(lua_State *L, const std::string &s) { lua_pushlstring(L, s.data(), s.size()); }
        };
        template<> struct converter<std::string_view> {
            static std::string_view get(lua_State *L, int i) {
                if (lua_type(L, i) != LUA_TSTRING) fail("string expected");
                size_t n = 0; const char *s = lua_tolstring(L, i, &n);
                return std::string_view(s, n);
            }
            static void push(lua_State *L, std::string_view s) { lua_pushlstring(L, s.empty() ? "" : s.data(), s.size()); }
        };
        template<> struct converter<const char *> {
            static const char *get(lua_State *L, int i) {
                if (lua_isnil(L, i)) return nullptr;
                if (lua_type(L, i) != LUA_TSTRING) fail("string or nil expected");
                return lua_tostring(L, i);
            }
            static void push(lua_State *L, const char *s) { if (s) lua_pushstring(L, s); else lua_pushnil(L); }
        };
        template<> struct converter<char *> {
            static void push(lua_State *L, const char *s) { converter<const char *>::push(L, s); }
        };
        template<> struct converter<std::nullptr_t> {
            static void push(lua_State *L, std::nullptr_t) { lua_pushnil(L); }
        };
        template<typename T> struct converter<T *, std::enable_if_t<!std::is_same_v<std::remove_cv_t<T>, char>>> {
            static T *get(lua_State *L, int i) { return lua_isnil(L, i) ? nullptr : check_t<T>(L, i); }
            static void push(lua_State *L, T *p) { push_ptr(L, p, state<T>(L).own, true); }
        };
        template<typename E> struct converter<E, std::enable_if_t<std::is_enum_v<E>>> {
            using U = std::underlying_type_t<E>;
            static E get(lua_State *L, int i) { return static_cast<E>(converter<U>::get(L, i)); }
            static void push(lua_State *L, E v) { converter<U>::push(L, static_cast<U>(v)); }
        };
        template<typename T> struct converter<std::vector<T>> {
            static std::vector<T> get(lua_State *L, int i) {
                stack_guard guard(L); ensure_stack(L, 4); i = absindex(L, i);
                if (!lua_istable(L, i)) fail("table expected for vector");
                std::vector<T> result; const size_t n = rawlen(L, i); result.reserve(n);
                for (size_t k = 1; k <= n; ++k) {
                    lua_rawgeti(L, i, static_cast<int>(k));
                    result.push_back(lua_to_native<T>(L, -1)); lua_pop(L, 1);
                }
                return result;
            }
            static void push(lua_State *L, const std::vector<T> &v) {
                const int top = lua_gettop(L);
                ensure_stack(L, 4);
                try {
                    lua_createtable(L, static_cast<int>(v.size()), 0);
                    for (size_t k = 0; k < v.size(); ++k) {
                        if constexpr (std::is_same_v<T, bool>) native_to_lua(L, static_cast<bool>(v[k]));
                        else native_to_lua(L, v[k]);
                        lua_rawseti(L, -2, static_cast<int>(k + 1));
                    }
                } catch (...) { lua_settop(L, top); throw; }
            }
        };
        template<typename K, typename V> struct converter<std::map<K, V>> {
            static std::map<K, V> get(lua_State *L, int i) {
                stack_guard guard(L); ensure_stack(L, 4); i = absindex(L, i);
                if (!lua_istable(L, i)) fail("table expected for map");
                std::map<K, V> result;
                lua_pushnil(L);
                while (lua_next(L, i)) {
                    // Preserve lua_next's original key even if a converter manipulates its slot.
                    lua_pushvalue(L, -2);
                    K key = lua_to_native<K>(L, -1); lua_pop(L, 1);
                    V value = lua_to_native<V>(L, -1);
                    if (!result.emplace(std::move(key), std::move(value)).second) fail("duplicate converted map key");
                    lua_pop(L, 1);
                }
                return result;
            }
            static void push(lua_State *L, const std::map<K, V> &m) {
                const int top = lua_gettop(L);
                ensure_stack(L, 4);
                try {
                    lua_createtable(L, 0, static_cast<int>(m.size()));
                    for (const auto &kv : m) {
                        native_to_lua(L, kv.first);
                        if (lua_isnil(L, -1)) fail("nil map key");
                        if (lua_type(L, -1) == LUA_TNUMBER && std::isnan(lua_tonumber(L, -1))) fail("NaN map key");
                        native_to_lua(L, kv.second); lua_rawset(L, -3);
                    }
                } catch (...) { lua_settop(L, top); throw; }
            }
        };

        template<typename F> struct function_traits;
        template<typename R, typename... A> struct function_traits<R (*)(A...)> {
            using result = R; using args = std::tuple<std::decay_t<A>...>;
            static constexpr int offset = 1;
        };
        template<typename T, typename R, typename... A> struct function_traits<R (T::*)(A...)> {
            using result = R; using args = std::tuple<std::decay_t<A>...>; using object = T;
            static constexpr int offset = 2;
        };
        template<typename T, typename R, typename... A> struct function_traits<R (T::*)(A...) const>
            : function_traits<R (T::*)(A...)> {};
        template<typename Tuple, size_t... I> Tuple read_args([[maybe_unused]] lua_State *L, [[maybe_unused]] int offset, std::index_sequence<I...>) {
            return Tuple{lua_to_native<std::tuple_element_t<I, Tuple>>(L, offset + static_cast<int>(I))...};
        }
        template<typename F> int call_function(lua_State *L) {
            using traits = function_traits<F>; using args_type = typename traits::args;
            auto func = *static_cast<F *>(lua_touserdata(L, lua_upvalueindex(1)));
            auto args = read_args<args_type>(L, traits::offset, std::make_index_sequence<std::tuple_size_v<args_type>>{});
            auto invoke = [&](auto &...a) -> decltype(auto) {
                if constexpr (std::is_member_function_pointer_v<F>) return std::invoke(func, check_t<typename traits::object>(L, 1), a...);
                else return func(a...);
            };
            if constexpr (std::is_void_v<typename traits::result>) { std::apply(invoke, args); return 0; }
            else { native_to_lua(L, std::apply(invoke, args)); return 1; }
        }
        template<typename F> void push_function(lua_State *L, F func) {
            if (func == nullptr) fail("null native function");
            ensure_stack(L, 2);
            auto mem = static_cast<F *>(lua_newuserdata(L, sizeof(F))); new (mem) F(func);
            lua_pushcclosure(L, boundary<call_function<F>>, 1);
        }
        template<typename T, typename... A> int construct(lua_State *L) {
            using tuple = std::tuple<std::decay_t<A>...>;
            auto args = read_args<tuple>(L, 1, std::index_sequence_for<A...>{});
            auto obj = std::apply([](auto &...a) { return std::make_unique<T>(a...); }, args);
            push_ptr(L, obj.get(), ownership::owning); obj.release(); return 1;
        }
        template<typename T, typename M> int get_property(lua_State *L) {
            auto member = *static_cast<M T::**>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, check_t<T>(L, 1)->*member); return 1;
        }
        template<typename T, typename M> int set_property(lua_State *L) {
            auto member = *static_cast<M T::**>(lua_touserdata(L, lua_upvalueindex(1)));
            auto obj = check_t<T>(L, 1);
            obj->*member = lua_to_native<std::decay_t<M>>(L, 2); return 0;
        }
        inline int dispatch(lua_State *L) {
            const int n = lua_gettop(L);
            lua_rawgeti(L, lua_upvalueindex(1), n);
            if (!lua_isfunction(L, -1)) fail("no overload matches argument count");
            lua_insert(L, 1); lua_call(L, n, LUA_MULTRET); return lua_gettop(L);
        }
        inline void push_traceback(lua_State *L) {
            globals(L); rawfield(L, -1, "debug"); lua_remove(L, -2);
            if (lua_istable(L, -1)) { rawfield(L, -1, "traceback"); lua_remove(L, -2); }
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); lua_pushnil(L); }
        }
        template<typename Rets, typename Args> struct call_context {
            const std::vector<std::string> *tables;
            const char *name;
            Rets &rets;
            const Args &args;
        };
        template<typename Tuple, size_t... I> auto read_results([[maybe_unused]] lua_State *L, std::index_sequence<I...>) {
            return std::tuple<std::decay_t<std::tuple_element_t<I, Tuple>>...>{
                lua_to_native<std::decay_t<std::tuple_element_t<I, Tuple>>>(L, -static_cast<int>(sizeof...(I)) + static_cast<int>(I))...};
        }
        template<typename Rets, typename Args> int perform_call(lua_State *L) {
            auto &ctx = *static_cast<call_context<Rets, Args> *>(lua_touserdata(L, 1));
            lua_settop(L, 0);
            ensure_stack(L, static_cast<int>(std::tuple_size_v<Args> + std::tuple_size_v<Rets> + 8));
            push_traceback(L); const int handler = lua_isfunction(L, 1) ? 1 : 0;
            globals(L);
            if (ctx.tables) {
                if (ctx.tables->empty()) fail("no tables");
                for (const auto &name : *ctx.tables) {
                    rawfield(L, -1, name.c_str()); lua_remove(L, -2);
                    if (!lua_istable(L, -1)) fail("no table in path");
                }
            }
            rawfield(L, -1, ctx.name); lua_remove(L, -2);
            if (!lua_isfunction(L, -1)) fail("no function");
            std::apply([&](const auto &...a) { (native_to_lua(L, a), ...); }, ctx.args);
            // Only trivial locals cross Lua's error longjmp. Argument / result validation
            // uses C++ exceptions, not luaL_check*; user function errors stay in lua_pcall.
            if (lua_pcall(L, static_cast<int>(std::tuple_size_v<Args>), static_cast<int>(std::tuple_size_v<Rets>), handler))
                return lua_error(L);
            auto values = read_results<Rets>(L, std::make_index_sequence<std::tuple_size_v<Rets>>{});
            ctx.rets = std::move(values);
            return 0;
        }
        template<typename... R, typename... A>
        std::optional<std::string> call(lua_State *L, const std::vector<std::string> *tables, const char *name,
                                        std::tuple<R &...> &rets, const A &...args) {
            stack_guard guard(L);
            auto tuple = std::tie(args...);
            call_context<std::tuple<R &...>, decltype(tuple)> ctx{tables, name, rets, tuple};
            lua_pushcfunction(L, (boundary<perform_call<std::tuple<R &...>, decltype(tuple)>>));
            lua_pushlightuserdata(L, &ctx);
            if (lua_pcall(L, 1, 0, 0)) {
                if (lua_type(L, -1) == LUA_TSTRING) return std::string(lua_tostring(L, -1));
                return std::string("Lua error (non-string error object)");
            }
            return std::nullopt;
        }
    }

    template<typename T> void reg_class(lua_State *L, const char *name = nullptr,
                                        ownership own = ownership::owning, void (*deleter)(T *) = nullptr) {
        internal::stack_guard guard(L);
        auto &meta = internal::state<T>(L);
        std::string wanted = name ? name : meta.name;
        if (wanted.empty()) internal::fail("empty type name");
        auto del = deleter ? deleter : &internal::delete_object<T>;
        if (meta.registered) {
            if (meta.name != wanted || meta.own != own || meta.deleter != del)
                internal::fail("conflicting class registration");
            return;
        }
        lua_pushstring(L, wanted.c_str()); lua_rawget(L, LUA_REGISTRYINDEX);
        if (!lua_isnil(L, -1)) internal::fail("type name already registered");
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushcfunction(L, (internal::boundary<internal::free_class<T>>)); lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1); lua_setfield(L, -2, "__index");
        lua_pushlstring(L, wanted.data(), wanted.size()); lua_setfield(L, -2, "__name");
        lua_pushvalue(L, -1); internal::rawsetp(L, LUA_REGISTRYINDEX, &internal::table_key<T>);
        lua_pushstring(L, wanted.c_str()); lua_pushvalue(L, -2); lua_rawset(L, LUA_REGISTRYINDEX);
        meta.name = std::move(wanted); meta.own = own; meta.deleter = del; meta.registered = true;
    }
    template<typename T> void reg_class_non_owning(lua_State *L, const char *name = nullptr) {
        reg_class<T>(L, name, ownership::non_owning);
    }
    template<typename T> void reg_class_with_deleter(lua_State *L, void (*deleter)(T *), const char *name = nullptr) {
        reg_class<T>(L, name, ownership::owning, deleter);
    }
    // Names are state-local and copied. Changing a live registration is forbidden.
    template<typename T> void set_type_name(lua_State *L, const char *name) {
        if (!name || !*name) internal::fail("empty type name");
        auto &meta = internal::state<T>(L);
        if (meta.registered && meta.name != name) internal::fail("cannot rename registered class");
        meta.name = name;
    }
    template<typename T> const char *get_type_name(lua_State *L) { return internal::state<T>(L).name.c_str(); }
    template<typename T> void push(lua_State *L, const T &v) { internal::native_to_lua(L, v); }
    template<typename T> void push(lua_State *L, T *v, ownership own) { internal::push_ptr(L, v, own); }
    template<typename T> void push_vector(lua_State *L, const std::vector<T> &v) { push(L, v); }
    template<typename T> std::vector<T> to_vector(lua_State *L, int i) { return internal::lua_to_native<std::vector<T>>(L, i); }
    template<typename K, typename V> void push_map(lua_State *L, const std::map<K, V> &m) { push(L, m); }
    template<typename K, typename V> std::map<K, V> to_map(lua_State *L, int i) { return internal::lua_to_native<std::map<K, V>>(L, i); }
    template<typename F> void reg_global_func(lua_State *L, const char *name, F func) {
        internal::push_function(L, func); lua_setglobal(L, name);
    }
    template<typename F, typename... Fs> void reg_global_overload(lua_State *L, const char *name, F f, Fs...fs) {
        internal::stack_guard guard(L);
        lua_newtable(L);
        auto add = [&](auto func) {
            using args = typename internal::function_traits<decltype(func)>::args;
            constexpr int n = static_cast<int>(std::tuple_size_v<args>);
            lua_rawgeti(L, -1, n);
            const bool duplicate = !lua_isnil(L, -1); lua_pop(L, 1);
            if (duplicate) internal::fail("duplicate overload arity");
            internal::push_function(L, func); lua_rawseti(L, -2, n);
        };
        add(f); (add(fs), ...);
        lua_pushcclosure(L, internal::boundary<internal::dispatch>, 1); lua_setglobal(L, name);
    }
    template<typename F> void reg_class_func(lua_State *L, const char *name, F func) {
        using T = typename internal::function_traits<F>::object;
        internal::stack_guard guard(L); internal::push_metatable<T>(L);
        internal::push_function(L, func); lua_setfield(L, -2, name);
    }
    template<typename T, typename... A> void reg_constructor(lua_State *L, const char *name) {
        if (!internal::state<T>(L).registered) internal::fail("register class before constructor");
        lua_pushcfunction(L, (internal::boundary<internal::construct<T, A...>>)); lua_setglobal(L, name);
    }
    template<typename T, typename M> void reg_property(lua_State *L, const char *name, M T::*member) {
        internal::stack_guard guard(L); internal::push_metatable<T>(L);
        using member_type = M T::*;
        auto add = [&](const char *prefix, lua_CFunction fn) {
            std::string method = std::string(prefix) + name;
            auto mem = static_cast<member_type *>(lua_newuserdata(L, sizeof(member_type))); new (mem) member_type(member);
            lua_pushcclosure(L, fn, 1); lua_setfield(L, -2, method.c_str());
        };
        add("get_", internal::boundary<internal::get_property<T, M>>);
        if constexpr (!std::is_const_v<M>) add("set_", internal::boundary<internal::set_property<T, M>>);
    }
    template<typename T, typename R> void reg_property(lua_State *L, const char *name, R(T::*getter)() const) {
        const std::string method = std::string("get_") + name; reg_class_func(L, method.c_str(), getter);
    }
    template<typename T, typename R, typename A> void reg_property(lua_State *L, const char *name,
                                                                   R(T::*getter)() const, void(T::*setter)(A)) {
        reg_property(L, name, getter);
        const std::string method = std::string("set_") + name; reg_class_func(L, method.c_str(), setter);
    }
    template<typename E> void reg_enum(lua_State *L, const char *name, std::initializer_list<std::pair<const char *, E>> values) {
        static_assert(std::is_enum_v<E> || std::is_integral_v<E>, "enum or integer required");
        lua_newtable(L);
        for (const auto &kv : values) { lua_pushinteger(L, static_cast<lua_Integer>(kv.second)); lua_setfield(L, -2, kv.first); }
        lua_setglobal(L, name);
    }
    template<typename... R, typename... A> std::optional<std::string> call_lua_global_func(
        lua_State *L, const char *name, std::tuple<R &...> &&rets, const A &...args) {
        return internal::call(L, nullptr, name, rets, args...);
    }
    template<typename... R, typename... A> std::optional<std::string> call_lua_table_func(
        lua_State *L, const std::vector<std::string> &tables, const char *name, std::tuple<R &...> &&rets, const A &...args) {
        return internal::call(L, &tables, name, rets, args...);
    }
}
