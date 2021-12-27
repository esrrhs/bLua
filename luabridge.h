#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <typeinfo>
#include <string>
#include <tuple>
#include <iostream>
#include <array>
#include <utility>
#include <vector>

namespace luabridge {

    template<typename T>
    T *check_t(lua_State *L, int i) noexcept {
        auto name = typeid(T).name();
        void *user_data = luaL_checkudata(L, i, name);
        luaL_argcheck(L, user_data != NULL, i, (std::string(name) + " expected").c_str());
        return *static_cast<T **>(user_data);
    }

    template<typename T>
    T lua_to_native(lua_State *L, int i) {
        static_assert(std::is_pointer<T>::value, "T should be pointer");
        typedef typename std::remove_pointer<T>::type t;
        return check_t<t>(L, i);
    }

    template<>
    bool lua_to_native<bool>(lua_State *L, int i) {
        return lua_toboolean(L, i) != 0;
    }

    template<>
    char lua_to_native<char>(lua_State *L, int i) {
        return (char) lua_tointeger(L, i);
    }

    template<>
    unsigned char lua_to_native<unsigned char>(lua_State *L, int i) {
        return (unsigned char) lua_tointeger(L, i);
    }

    template<>
    short lua_to_native<short>(lua_State *L, int i) {
        return (short) lua_tointeger(L, i);
    }

    template<>
    unsigned short lua_to_native<unsigned short>(lua_State *L, int i) {
        return (unsigned short) lua_tointeger(L, i);
    }

    template<>
    int lua_to_native<int>(lua_State *L, int i) {
        return (int) lua_tointeger(L, i);
    }

    template<>
    unsigned int lua_to_native<unsigned int>(lua_State *L, int i) {
        return (unsigned int) lua_tointeger(L, i);
    }

    template<>
    long lua_to_native<long>(lua_State *L, int i) {
        return (long) lua_tointeger(L, i);
    }

    template<>
    unsigned long lua_to_native<unsigned long>(lua_State *L, int i) {
        return (unsigned long) lua_tointeger(L, i);
    }

    template<>
    long long lua_to_native<long long>(lua_State *L, int i) {
        return lua_tointeger(L, i);
    }

    template<>
    unsigned long long
    lua_to_native<unsigned long long>(lua_State *L, int i) {
        return (unsigned long long) lua_tointeger(L, i);
    }

    template<>
    float lua_to_native<float>(lua_State *L, int i) {
        return (float) lua_tonumber(L, i);
    }

    template<>
    double lua_to_native<double>(lua_State *L, int i) {
        return lua_tonumber(L, i);
    }

    template<>
    const char *lua_to_native<const char *>(lua_State *L, int i) {
        return lua_tostring(L, i);
    }

    template<>
    std::string lua_to_native<std::string>(lua_State *L, int i) {
        const char *str = lua_tostring(L, i);
        return str == nullptr ? "" : str;
    }

    template<typename T>
    void native_to_lua(lua_State *L, T *v) {
        static_assert(!std::is_pointer<T>::value, "T should not be pointer");

        if (!v) {
            lua_pushnil(L);
            return;
        }

        if (lua_getfield(L, LUA_REGISTRYINDEX, "luabridge_pointer") != LUA_TTABLE) {
            lua_pop(L, 1);

            lua_newtable(L);

            lua_newtable(L);
            lua_pushstring(L, "v");
            lua_setfield(L, -2, "__mode");
            lua_setmetatable(L, -2);

            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_REGISTRYINDEX, "luabridge_pointer");
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

    void native_to_lua(lua_State *L, bool v) {
        lua_pushboolean(L, v);
    }

    void native_to_lua(lua_State *L, char v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, unsigned char v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, short v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, unsigned short v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, int v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, unsigned int v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, long v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, unsigned long v) {
        lua_pushinteger(L, v);
    }

    void native_to_lua(lua_State *L, long long v) {
        lua_pushinteger(L, (lua_Integer) v);
    }

    void native_to_lua(lua_State *L, unsigned long long v) {
        lua_pushinteger(L, (lua_Integer) v);
    }

    void native_to_lua(lua_State *L, float v) {
        lua_pushnumber(L, v);
    }

    void native_to_lua(lua_State *L, double v) {
        lua_pushnumber(L, v);
    }

    void native_to_lua(lua_State *L, const char *v) {
        lua_pushstring(L, v);
    }

    void native_to_lua(lua_State *L, char *v) {
        lua_pushstring(L, v);
    }

    void native_to_lua(lua_State *L, const std::string &v) {
        lua_pushstring(L, v.c_str());
    }

    template<typename T>
    int free_class(lua_State *L) {
        auto t = check_t<T>(L, 1);
        delete t;
        return 0;
    }

    template<typename T>
    void reg_class(lua_State *L) {
        auto name = typeid(T).name();
        luaL_newmetatable(L, name);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, free_class<T>);
        lua_settable(L, -3);

        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */

        lua_pop(L, 1);
    }

    template<typename T, typename return_type, size_t... I, typename... arg_types>
    return_type
    class_func_call_helper(lua_State *L, T *obj, return_type(T::*func)(arg_types...), std::index_sequence<I...> &&) {
        return ((obj)->*func)(lua_to_native<arg_types>(L, I + 2)...);
    }

    template<typename T, typename return_type, typename... arg_types>
    int call_class_func(lua_State *L) {
        auto obj = check_t<T>(L, 1);
        auto func = *(return_type(T::* *)(arg_types...)) lua_touserdata(L, lua_upvalueindex(1));
        native_to_lua(L, class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    }

    template<typename T, typename... arg_types>
    int call_class_void_func(lua_State *L) {
        auto obj = check_t<T>(L, 1);
        auto func = *(void (T::* *)(arg_types...)) lua_touserdata(L, lua_upvalueindex(1));
        class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    }

    template<typename T, typename return_type, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, return_type(T::*func)(arg_types...)) {
        auto name = typeid(T).name();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        auto func_mem = new char[sizeof(func)];
        new(func_mem)(return_type(T::*)(arg_types...))(func);

        lua_pushstring(L, func_name);
        lua_pushlightuserdata(L, func_mem);
        lua_pushcclosure(L, call_class_func<T, return_type, arg_types...>, 1); /* closure with those upvalues */
        lua_settable(L, -3);

        lua_pop(L, 1);
    }

    template<typename T, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, void(T::*func)(arg_types...)) {
        auto name = typeid(T).name();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        auto func_mem = new char[sizeof(func)];
        new(func_mem)(void (T::*)(arg_types...))(func);

        lua_pushstring(L, func_name);
        lua_pushlightuserdata(L, func_mem);
        lua_pushcclosure(L, call_class_void_func<T, arg_types...>, 1); /* closure with those upvalues */
        lua_settable(L, -3);

        lua_pop(L, 1);
    }

    template<typename return_type, size_t... I, typename... arg_types>
    return_type
    global_func_call_helper(lua_State *L, return_type(*func)(arg_types...), std::index_sequence<I...> &&) {
        return (*func)(lua_to_native<arg_types>(L, I + 1)...);
    }

    template<typename return_type, typename... arg_types>
    int call_global_func(lua_State *L) {
        auto func = (return_type(*)(arg_types...)) lua_touserdata(L, lua_upvalueindex(1));
        native_to_lua(L, global_func_call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>()));
        return 1;
    }

    template<typename... arg_types>
    int call_global_void_func(lua_State *L) {
        auto func = (void (*)(arg_types...)) lua_touserdata(L, lua_upvalueindex(1));
        global_func_call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>());
        return 0;
    }

    template<typename return_type, typename... arg_types>
    void reg_global_func(lua_State *L, const char *func_name, return_type(*func)(arg_types...)) {
        lua_pushlightuserdata(L, (void *) func);
        lua_pushcclosure(L, call_global_func<return_type, arg_types...>, 1); /* closure with those upvalues */
        lua_setglobal(L, func_name);
    }

    template<typename... arg_types>
    void reg_global_func(lua_State *L, const char *func_name, void(*func)(arg_types...)) {
        lua_pushlightuserdata(L, (void *) func);
        lua_pushcclosure(L, call_global_void_func<arg_types...>, 1); /* closure with those upvalues */
        lua_setglobal(L, func_name);
    }

    template<typename head, typename... arg_types>
    void lua_func_call_helper(lua_State *L, head arg) {
        native_to_lua(L, arg);
    }

    template<typename head, typename... arg_types>
    void lua_func_call_helper(lua_State *L, head arg, arg_types... args) {
        native_to_lua(L, arg);
        lua_func_call_helper(L, args...);
    }

    template<size_t I = 0, typename... ret_types>
    inline typename std::enable_if<I == sizeof...(ret_types), void>::type
    lua_func_ret_helper(lua_State *L, std::tuple<ret_types &...> &rets) {

    }

    template<size_t I = 0, typename... ret_types>
    inline typename std::enable_if<I < sizeof...(ret_types), void>::type
    lua_func_ret_helper(lua_State *L, std::tuple<ret_types &...> &rets) {
        typedef typename std::remove_reference<std::tuple_element_t<I, std::tuple<ret_types &...>>>::type t;
        std::get<I>(rets) = lua_to_native<t>(L, -(int(sizeof...(ret_types) - I)));
        lua_func_ret_helper<I + 1, ret_types...>(L, rets);
    }

    struct lua_stack_protector {
        lua_stack_protector(lua_State *L) {
            m_L = L;
            m_top = lua_gettop(L);
        }

        ~lua_stack_protector() {
            lua_settop(m_L, m_top);
        }

        lua_stack_protector(const lua_stack_protector &other) = delete;

        lua_stack_protector(lua_stack_protector &&other) = delete;

        lua_stack_protector &operator=(const lua_stack_protector &) = delete;

        lua_State *m_L;
        int m_top;
    };

    template<typename... ret_types, typename... arg_types>
    std::pair<bool, std::string>
    call_lua_global_func(lua_State *L, const char *func_name, std::tuple<ret_types &...> &&rets, arg_types... args) {
        lua_stack_protector lp(L);

        auto ret_num = sizeof...(ret_types);
        auto arg_num = sizeof...(args);

        lua_getglobal(L, "debug");
        lua_getfield(L, -1, "traceback");
        lua_remove(L, -2);

        lua_getglobal(L, func_name);
        if (!lua_isfunction(L, -1)) {
            return {false, std::string("no function ") + func_name};
        }

        lua_func_call_helper(L, args...);

        if (lua_pcall(L, arg_num, ret_num, -(arg_num + 2))) {
            return {false, lua_tostring(L, -1)};
        }

        lua_func_ret_helper(L, rets);

        return {true, ""};
    }

    template<typename... ret_types, typename... arg_types>
    std::pair<bool, std::string>
    call_lua_table_func(lua_State *L, std::vector<std::string> tables, const char *func_name,
                        std::tuple<ret_types &...> &&rets, arg_types... args) {
        lua_stack_protector lp(L);

        auto ret_num = sizeof...(ret_types);
        auto arg_num = sizeof...(args);

        lua_getglobal(L, "debug");
        lua_getfield(L, -1, "traceback");
        lua_remove(L, -2);

        if (tables.empty()) {
            return {false, "no tables"};
        }

        lua_getglobal(L, tables[0].c_str());
        if (!lua_istable(L, -1)) {
            return {false, std::string("no table ") + tables[0]};
        }

        for (int i = 1; i < tables.size(); ++i) {
            lua_getfield(L, -1, tables[i].c_str());
            lua_remove(L, -2);
            if (!lua_istable(L, -1)) {
                return {false, std::string("no table ") + tables[i - 1]};
            }
        }

        lua_getfield(L, -1, func_name);
        if (!lua_isfunction(L, -1)) {
            return {false, std::string("no function ") + func_name};
        }

        lua_func_call_helper(L, args...);

        if (lua_pcall(L, arg_num, ret_num, -(arg_num + 2))) {
            return {false, lua_tostring(L, -1)};
        }

        lua_func_ret_helper(L, rets);

        return {true, ""};
    }
}
