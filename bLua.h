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
#include <iostream>
#include <array>
#include <utility>
#include <vector>
#include <optional>
#include <cstdint>
#include <cstddef>
#include <new>

// Compatibility shims for Lua 5.1 / LuaJIT
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM < 502
inline int bLua_absindex(lua_State *L, int idx) {
    return (idx > 0 || idx <= LUA_REGISTRYINDEX) ? idx : lua_gettop(L) + idx + 1;
}

inline void lua_rawgetp(lua_State *L, int idx, const void *p) {
    idx = bLua_absindex(L, idx);
    lua_pushlightuserdata(L, const_cast<void*>(p));
    lua_rawget(L, idx);
}

inline void lua_rawsetp(lua_State *L, int idx, const void *p) {
    idx = bLua_absindex(L, idx);
    lua_pushlightuserdata(L, const_cast<void*>(p));
    lua_insert(L, -2);
    lua_rawset(L, idx);
}
#endif

namespace bLua {

    namespace internal {

        template<typename T>
        T *check_t(lua_State *L, int i) noexcept {
            auto name = typeid(T).name();
            void *user_data = luaL_checkudata(L, i, name);
            luaL_argcheck(L, user_data != nullptr, i, (std::string(name) + " expected").c_str());
            return *static_cast<T **>(user_data);
        }

        template<typename T>
        T lua_to_native(lua_State *L, int i) {
            static_assert(std::is_pointer_v<T>, "T should be pointer");
            using t = std::remove_pointer_t<T>;
            return check_t<t>(L, i);
        }

        template<>
        inline bool lua_to_native<bool>(lua_State *L, int i) {
            return lua_toboolean(L, i) != 0;
        }

        template<>
        inline char lua_to_native<char>(lua_State *L, int i) {
            return static_cast<char>(lua_tointeger(L, i));
        }

        template<>
        inline signed char lua_to_native<signed char>(lua_State *L, int i) {
            return static_cast<signed char>(lua_tointeger(L, i));
        }

        template<>
        inline unsigned char lua_to_native<unsigned char>(lua_State *L, int i) {
            return static_cast<unsigned char>(lua_tointeger(L, i));
        }

        template<>
        inline short lua_to_native<short>(lua_State *L, int i) {
            return static_cast<short>(lua_tointeger(L, i));
        }

        template<>
        inline unsigned short lua_to_native<unsigned short>(lua_State *L, int i) {
            return static_cast<unsigned short>(lua_tointeger(L, i));
        }

        template<>
        inline int lua_to_native<int>(lua_State *L, int i) {
            return static_cast<int>(lua_tointeger(L, i));
        }

        template<>
        inline unsigned int lua_to_native<unsigned int>(lua_State *L, int i) {
            return static_cast<unsigned int>(lua_tointeger(L, i));
        }

        template<>
        inline long lua_to_native<long>(lua_State *L, int i) {
            return static_cast<long>(lua_tointeger(L, i));
        }

        template<>
        inline unsigned long lua_to_native<unsigned long>(lua_State *L, int i) {
            return static_cast<unsigned long>(lua_tointeger(L, i));
        }

        template<>
        inline long long lua_to_native<long long>(lua_State *L, int i) {
            return static_cast<long long>(lua_tointeger(L, i));
        }

        template<>
        inline unsigned long long lua_to_native<unsigned long long>(lua_State *L, int i) {
            return static_cast<unsigned long long>(lua_tointeger(L, i));
        }

        template<>
        inline float lua_to_native<float>(lua_State *L, int i) {
            return static_cast<float>(lua_tonumber(L, i));
        }

        template<>
        inline double lua_to_native<double>(lua_State *L, int i) {
            return static_cast<double>(lua_tonumber(L, i));
        }

        template<>
        inline const char *lua_to_native<const char *>(lua_State *L, int i) {
            return lua_tostring(L, i);
        }

        template<>
        inline std::string lua_to_native<std::string>(lua_State *L, int i) {
            size_t len = 0;
            const char *str = lua_tolstring(L, i, &len);
            return str == nullptr ? std::string() : std::string(str, len);
        }

        template<>
        inline std::string_view lua_to_native<std::string_view>(lua_State *L, int i) {
            size_t len = 0;
            const char *str = lua_tolstring(L, i, &len);
            return str == nullptr ? std::string_view() : std::string_view(str, len);
        }

        template<typename T>
        void native_to_lua(lua_State *L, T *v) {
            static_assert(!std::is_pointer_v<T>, "T should not be pointer");

            if (!v) {
                lua_pushnil(L);
                return;
            }

            if (lua_getfield(L, LUA_REGISTRYINDEX, "blua_pointer") != LUA_TTABLE) {
                lua_pop(L, 1);

                lua_newtable(L);

                lua_newtable(L);
                lua_pushstring(L, "v");
                lua_setfield(L, -2, "__mode");
                lua_setmetatable(L, -2);

                lua_pushvalue(L, -1);
                lua_setfield(L, LUA_REGISTRYINDEX, "blua_pointer");
            }

            if (lua_rawgetp(L, -1, v) != LUA_TUSERDATA) {
                lua_pop(L, 1);

                auto userdata = static_cast<T **>(lua_newuserdata(L, sizeof(T *)));
                *userdata = v;
                auto name = typeid(T).name();
                luaL_setmetatable(L, name);

                lua_pushvalue(L, -1);
                lua_rawsetp(L, -3, v);
            }

            lua_remove(L, -2);
        }

        inline void native_to_lua(lua_State *L, bool v) {
            lua_pushboolean(L, v);
        }

        inline void native_to_lua(lua_State *L, char v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, signed char v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, unsigned char v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, short v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, unsigned short v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, int v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, unsigned int v) {
            lua_pushinteger(L, v);
        }

        inline void native_to_lua(lua_State *L, long v) {
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        }

        inline void native_to_lua(lua_State *L, unsigned long v) {
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        }

        inline void native_to_lua(lua_State *L, long long v) {
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        }

        inline void native_to_lua(lua_State *L, unsigned long long v) {
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        }

        inline void native_to_lua(lua_State *L, float v) {
            lua_pushnumber(L, v);
        }

        inline void native_to_lua(lua_State *L, double v) {
            lua_pushnumber(L, v);
        }

        inline void native_to_lua(lua_State *L, const char *v) {
            lua_pushstring(L, v);
        }

        inline void native_to_lua(lua_State *L, char *v) {
            lua_pushstring(L, v);
        }

        inline void native_to_lua(lua_State *L, const std::string &v) {
            lua_pushlstring(L, v.data(), v.size());
        }

        inline void native_to_lua(lua_State *L, std::string_view v) {
            lua_pushlstring(L, v.data(), v.size());
        }

        template<typename T>
        int free_class(lua_State *L) {
            auto t = check_t<T>(L, 1);
            delete t;
            return 0;
        }

        template<typename T, typename return_type, size_t... I, typename... arg_types>
        return_type
        class_func_call_helper([[maybe_unused]] lua_State *L, T *obj, return_type(T::*func)(arg_types...),
                               std::index_sequence<I...>) {
            return ((obj)->*func)(lua_to_native<std::decay_t<arg_types>>(L, static_cast<int>(I + 2))...);
        }

        template<typename T, typename return_type, size_t... I, typename... arg_types>
        return_type
        class_func_call_helper([[maybe_unused]] lua_State *L, T *obj, return_type(T::*func)(arg_types...) const,
                               std::index_sequence<I...>) {
            return ((obj)->*func)(lua_to_native<std::decay_t<arg_types>>(L, static_cast<int>(I + 2))...);
        }

        template<typename T, typename return_type, typename... arg_types>
        int call_class_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = return_type(T::*)(arg_types...);
            auto func = *static_cast<FuncType*>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
            return 1;
        }

        template<typename T, typename return_type, typename... arg_types>
        int call_class_const_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = return_type(T::*)(arg_types...) const;
            auto func = *static_cast<FuncType*>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
            return 1;
        }

        template<typename T, typename... arg_types>
        int call_class_void_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = void(T::*)(arg_types...);
            auto func = *static_cast<FuncType*>(lua_touserdata(L, lua_upvalueindex(1)));
            class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>());
            return 0;
        }

        template<typename T, typename... arg_types>
        int call_class_void_const_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = void(T::*)(arg_types...) const;
            auto func = *static_cast<FuncType*>(lua_touserdata(L, lua_upvalueindex(1)));
            class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>());
            return 0;
        }

        template<typename return_type, size_t... I, typename... arg_types>
        return_type
        global_func_call_helper([[maybe_unused]] lua_State *L, return_type(*func)(arg_types...), std::index_sequence<I...>) {
            return (*func)(lua_to_native<std::decay_t<arg_types>>(L, static_cast<int>(I + 1))...);
        }

        template<typename return_type, typename... arg_types>
        int call_global_func(lua_State *L) {
            using FuncType = return_type(*)(arg_types...);
            auto func = *static_cast<FuncType*>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, global_func_call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>()));
            return 1;
        }

        template<typename... arg_types>
        int call_global_void_func(lua_State *L) {
            using FuncType = void(*)(arg_types...);
            auto func = *static_cast<FuncType*>(lua_touserdata(L, lua_upvalueindex(1)));
            global_func_call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>());
            return 0;
        }

        inline void lua_func_call_helper([[maybe_unused]] lua_State *L) noexcept {}

        template<typename Head, typename... Tail>
        void lua_func_call_helper(lua_State *L, const Head &head, const Tail &... tail) {
            native_to_lua(L, head);
            if constexpr (sizeof...(tail) > 0) {
                lua_func_call_helper(L, tail...);
            }
        }

        template<typename Tuple, size_t... I>
        inline void lua_func_ret_impl(lua_State *L, Tuple &rets, std::index_sequence<I...>) {
            constexpr int total = static_cast<int>(sizeof...(I));
            (((void)(std::get<I>(rets) = lua_to_native<std::decay_t<std::tuple_element_t<I, Tuple>>>(L, -(total - static_cast<int>(I))))), ...);
        }

        template<typename... ret_types>
        inline void lua_func_ret_helper(lua_State *L, std::tuple<ret_types &...> &rets) {
            lua_func_ret_impl(L, rets, std::index_sequence_for<ret_types...>{});
        }

        struct lua_stack_protector {
            explicit lua_stack_protector(lua_State *L) noexcept : m_L(L), m_top(lua_gettop(L)) {}

            ~lua_stack_protector() noexcept {
                lua_settop(m_L, m_top);
            }

            lua_stack_protector(const lua_stack_protector &) = delete;
            lua_stack_protector(lua_stack_protector &&) = delete;
            lua_stack_protector &operator=(const lua_stack_protector &) = delete;
            lua_stack_protector &operator=(lua_stack_protector &&) = delete;

        private:
            lua_State *m_L;
            int m_top;
        };

    }

    template<typename return_type, typename... arg_types>
    void reg_global_func(lua_State *L, const char *func_name, return_type(*func)(arg_types...)) {
        using FuncType = return_type(*)(arg_types...);
        auto func_mem = static_cast<FuncType*>(lua_newuserdata(L, sizeof(FuncType)));
        *func_mem = func;
        lua_pushcclosure(L, internal::call_global_func<return_type, arg_types...>, 1);
        lua_setglobal(L, func_name);
    }

    template<typename... arg_types>
    void reg_global_func(lua_State *L, const char *func_name, void(*func)(arg_types...)) {
        using FuncType = void(*)(arg_types...);
        auto func_mem = static_cast<FuncType*>(lua_newuserdata(L, sizeof(FuncType)));
        *func_mem = func;
        lua_pushcclosure(L, internal::call_global_void_func<arg_types...>, 1);
        lua_setglobal(L, func_name);
    }

    template<typename T>
    void reg_class(lua_State *L) {
        auto name = typeid(T).name();
        luaL_newmetatable(L, name);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, internal::free_class<T>);
        lua_settable(L, -3);

        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */

        lua_pop(L, 1);
    }

    template<typename T, typename return_type, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, return_type(T::*func)(arg_types...)) {
        auto name = typeid(T).name();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = return_type(T::*)(arg_types...);
        auto func_mem = static_cast<FuncType*>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_func<T, return_type, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename T, typename return_type, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, return_type(T::*func)(arg_types...) const) {
        auto name = typeid(T).name();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = return_type(T::*)(arg_types...) const;
        auto func_mem = static_cast<FuncType*>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_const_func<T, return_type, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename T, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, void(T::*func)(arg_types...)) {
        auto name = typeid(T).name();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = void(T::*)(arg_types...);
        auto func_mem = static_cast<FuncType*>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_void_func<T, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename T, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, void(T::*func)(arg_types...) const) {
        auto name = typeid(T).name();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = void(T::*)(arg_types...) const;
        auto func_mem = static_cast<FuncType*>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_void_const_func<T, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename... ret_types, typename... arg_types>
    std::optional<std::string>
    call_lua_global_func(lua_State *L, const char *func_name, std::tuple<ret_types &...> &&rets, const arg_types &... args) {
        internal::lua_stack_protector lp(L);

        auto ret_num = static_cast<int>(sizeof...(ret_types));
        auto arg_num = static_cast<int>(sizeof...(args));

        lua_getglobal(L, "debug");
        lua_getfield(L, -1, "traceback");
        lua_remove(L, -2);

        lua_getglobal(L, func_name);
        if (!lua_isfunction(L, -1)) {
            return std::string("no function ") + func_name;
        }

        internal::lua_func_call_helper(L, args...);

        if (lua_pcall(L, arg_num, ret_num, -(arg_num + 2))) {
            return lua_tostring(L, -1);
        }

        internal::lua_func_ret_helper(L, rets);

        return std::nullopt;
    }

    template<typename... ret_types, typename... arg_types>
    std::optional<std::string>
    call_lua_table_func(lua_State *L, const std::vector<std::string> &tables, const char *func_name,
                        std::tuple<ret_types &...> &&rets, const arg_types &... args) {
        internal::lua_stack_protector lp(L);

        auto ret_num = static_cast<int>(sizeof...(ret_types));
        auto arg_num = static_cast<int>(sizeof...(args));

        lua_getglobal(L, "debug");
        lua_getfield(L, -1, "traceback");
        lua_remove(L, -2);

        if (tables.empty()) {
            return "no tables";
        }

        lua_getglobal(L, tables[0].c_str());
        if (!lua_istable(L, -1)) {
            return std::string("no table ") + tables[0];
        }

        for (size_t i = 1; i < tables.size(); ++i) {
            lua_getfield(L, -1, tables[i].c_str());
            lua_remove(L, -2);
            if (!lua_istable(L, -1)) {
                return std::string("no table ") + tables[i];
            }
        }

        lua_getfield(L, -1, func_name);
        if (!lua_isfunction(L, -1)) {
            return std::string("no function ") + func_name;
        }

        internal::lua_func_call_helper(L, args...);

        if (lua_pcall(L, arg_num, ret_num, -(arg_num + 2))) {
            return lua_tostring(L, -1);
        }

        internal::lua_func_ret_helper(L, rets);

        return std::nullopt;
    }

}
