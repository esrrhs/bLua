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
#include <array>
#include <utility>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <cstddef>
#include <new>
#include <cstdlib>
#include <cstring>

#if defined(__GNUC__) || defined(__clang__)
  #include <cxxabi.h>
  #define BLUA_HAS_CXA_DEMANGLE 1
#endif

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

#ifndef LUA_OK
#define LUA_OK 0
#endif

#ifndef lua_rawlen
#define lua_rawlen lua_objlen
#endif

inline void blua_setmetatable(lua_State *L, const char *tname) {
    luaL_getmetatable(L, tname);
    lua_setmetatable(L, -2);
}
#define luaL_setmetatable(L, tname) blua_setmetatable(L, tname)
#endif

namespace bLua {

    // Ownership for C++ objects exposed to Lua.
    // owning:     __gc deletes (or runs a custom deleter)
    // non_owning: __gc is a no-op; C++ retains ownership
    enum class ownership {
        owning,
        non_owning
    };

    namespace internal {

        // Per-type registration state (name + default deleter).
        template<typename T>
        struct class_meta {
            static inline const char *name = nullptr;
            static inline void (*typed_deleter)(T *) = nullptr;
            // Erased deleter used in userdata; nullptr means non-owning.
            static inline void (*deleter)(void *) = nullptr;
            static inline bool registered = false;

            static void erase_deleter(void *p) {
                if (typed_deleter != nullptr) {
                    typed_deleter(static_cast<T *>(p));
                } else {
                    delete static_cast<T *>(p);
                }
            }
        };

        inline std::string demangle_type_name(const char *mangled) {
#if defined(BLUA_HAS_CXA_DEMANGLE)
            int status = 0;
            char *demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
            if (status == 0 && demangled != nullptr) {
                std::string result(demangled);
                std::free(demangled);
                return result;
            }
#endif
            return mangled ? std::string(mangled) : std::string();
        }

        // Stable metatable name: user-provided > demangled typeid > raw typeid.
        template<typename T>
        const char *type_name() {
            if (class_meta<T>::name != nullptr) {
                return class_meta<T>::name;
            }
            static std::string cached = demangle_type_name(typeid(T).name());
            return cached.c_str();
        }

        template<typename T>
        void default_delete_fn(void *p) {
            delete static_cast<T *>(p);
        }

        // Userdata layout: pointer + optional deleter (nullptr => non-owning).
        struct udata_base {
            void *ptr;
            void (*deleter)(void *);
        };

        template<typename T>
        T *check_t(lua_State *L, int i) {
            auto name = type_name<T>();
            void *user_data = luaL_checkudata(L, i, name);
            luaL_argcheck(L, user_data != nullptr, i, (std::string(name) + " expected").c_str());
            auto *ud = static_cast<udata_base *>(user_data);
            return static_cast<T *>(ud->ptr);
        }

        template<typename T>
        void push_ptr(lua_State *L, T *v, void (*del)(void *)) {
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

                auto *ud = static_cast<udata_base *>(lua_newuserdata(L, sizeof(udata_base)));
                ud->ptr = v;
                ud->deleter = del;
                luaL_setmetatable(L, type_name<T>());

                lua_pushvalue(L, -1);
                lua_rawsetp(L, -3, v);
            }

            lua_remove(L, -2);
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

        // Forward declare for recursive container conversion
        template<typename T>
        void native_to_lua(lua_State *L, T *v);

        inline void native_to_lua(lua_State *L, bool v);
        inline void native_to_lua(lua_State *L, char v);
        inline void native_to_lua(lua_State *L, signed char v);
        inline void native_to_lua(lua_State *L, unsigned char v);
        inline void native_to_lua(lua_State *L, short v);
        inline void native_to_lua(lua_State *L, unsigned short v);
        inline void native_to_lua(lua_State *L, int v);
        inline void native_to_lua(lua_State *L, unsigned int v);
        inline void native_to_lua(lua_State *L, long v);
        inline void native_to_lua(lua_State *L, unsigned long v);
        inline void native_to_lua(lua_State *L, long long v);
        inline void native_to_lua(lua_State *L, unsigned long long v);
        inline void native_to_lua(lua_State *L, float v);
        inline void native_to_lua(lua_State *L, double v);
        inline void native_to_lua(lua_State *L, const char *v);
        inline void native_to_lua(lua_State *L, char *v);
        inline void native_to_lua(lua_State *L, const std::string &v);
        inline void native_to_lua(lua_State *L, std::string_view v);

        template<typename T>
        void native_to_lua(lua_State *L, const std::vector<T> &v);

        template<typename K, typename V>
        void native_to_lua(lua_State *L, const std::map<K, V> &m);

        template<typename T>
        void native_to_lua(lua_State *L, T *v) {
            static_assert(!std::is_pointer_v<T>, "T should not be pointer");
            void (*del)(void *) = nullptr;
            if (class_meta<T>::deleter != nullptr) {
                del = class_meta<T>::deleter;
            }
            push_ptr(L, v, del);
        }

        // Push with explicit ownership (overrides class default for this push).
        template<typename T>
        void native_to_lua(lua_State *L, T *v, ownership own) {
            static_assert(!std::is_pointer_v<T>, "T should not be pointer");
            void (*del)(void *) = nullptr;
            if (own == ownership::owning) {
                del = class_meta<T>::deleter != nullptr
                          ? class_meta<T>::deleter
                          : &default_delete_fn<T>;
            }
            push_ptr(L, v, del);
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
            if (v == nullptr) {
                lua_pushnil(L);
            } else {
                lua_pushstring(L, v);
            }
        }

        inline void native_to_lua(lua_State *L, char *v) {
            if (v == nullptr) {
                lua_pushnil(L);
            } else {
                lua_pushstring(L, v);
            }
        }

        inline void native_to_lua(lua_State *L, const std::string &v) {
            lua_pushlstring(L, v.data(), v.size());
        }

        inline void native_to_lua(lua_State *L, std::string_view v) {
            lua_pushlstring(L, v.data(), v.size());
        }

        // ---- Container ↔ Lua table helpers ----

        template<typename T>
        void native_to_lua(lua_State *L, const std::vector<T> &v) {
            lua_createtable(L, static_cast<int>(v.size()), 0);
            for (size_t i = 0; i < v.size(); ++i) {
                native_to_lua(L, v[i]);
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
        }

        template<typename K, typename V>
        void native_to_lua(lua_State *L, const std::map<K, V> &m) {
            lua_createtable(L, 0, static_cast<int>(m.size()));
            for (const auto &kv : m) {
                native_to_lua(L, kv.first);
                native_to_lua(L, kv.second);
                lua_settable(L, -3);
            }
        }

        template<typename T>
        std::vector<T> lua_to_vector(lua_State *L, int index) {
            std::vector<T> result;
            if (!lua_istable(L, index)) {
                return result;
            }
            const auto len = static_cast<size_t>(lua_rawlen(L, index));
            result.reserve(len);
            for (size_t i = 1; i <= len; ++i) {
                lua_rawgeti(L, index, static_cast<lua_Integer>(i));
                result.push_back(lua_to_native<T>(L, -1));
                lua_pop(L, 1);
            }
            return result;
        }

        template<typename K, typename V>
        std::map<K, V> lua_to_map(lua_State *L, int index) {
            std::map<K, V> result;
            if (!lua_istable(L, index)) {
                return result;
            }
            index = (index > 0 || index <= LUA_REGISTRYINDEX)
                        ? index
                        : lua_gettop(L) + index + 1;
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                // stack: key at -2, value at -1
                K key = lua_to_native<K>(L, -2);
                V val = lua_to_native<V>(L, -1);
                result.emplace(std::move(key), std::move(val));
                lua_pop(L, 1); // pop value, keep key for next
            }
            return result;
        }

        template<>
        inline std::vector<int> lua_to_native<std::vector<int>>(lua_State *L, int i) {
            return lua_to_vector<int>(L, i);
        }

        template<>
        inline std::vector<double> lua_to_native<std::vector<double>>(lua_State *L, int i) {
            return lua_to_vector<double>(L, i);
        }

        template<>
        inline std::vector<std::string> lua_to_native<std::vector<std::string>>(lua_State *L, int i) {
            return lua_to_vector<std::string>(L, i);
        }

        template<>
        inline std::map<std::string, int> lua_to_native<std::map<std::string, int>>(lua_State *L, int i) {
            return lua_to_map<std::string, int>(L, i);
        }

        template<>
        inline std::map<std::string, std::string> lua_to_native<std::map<std::string, std::string>>(lua_State *L, int i) {
            return lua_to_map<std::string, std::string>(L, i);
        }

        template<typename T>
        int free_class(lua_State *L) {
            void *user_data = lua_touserdata(L, 1);
            if (user_data == nullptr) {
                return 0;
            }
            auto *ud = static_cast<udata_base *>(user_data);
            if (ud->ptr != nullptr && ud->deleter != nullptr) {
                ud->deleter(ud->ptr);
                ud->ptr = nullptr;
                ud->deleter = nullptr;
            }
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
            auto func = *static_cast<FuncType *>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
            return 1;
        }

        template<typename T, typename return_type, typename... arg_types>
        int call_class_const_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = return_type(T::*)(arg_types...) const;
            auto func = *static_cast<FuncType *>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>()));
            return 1;
        }

        template<typename T, typename... arg_types>
        int call_class_void_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = void(T::*)(arg_types...);
            auto func = *static_cast<FuncType *>(lua_touserdata(L, lua_upvalueindex(1)));
            class_func_call_helper(L, obj, func, std::make_index_sequence<sizeof...(arg_types)>());
            return 0;
        }

        template<typename T, typename... arg_types>
        int call_class_void_const_func(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using FuncType = void(T::*)(arg_types...) const;
            auto func = *static_cast<FuncType *>(lua_touserdata(L, lua_upvalueindex(1)));
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
            auto func = *static_cast<FuncType *>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, global_func_call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>()));
            return 1;
        }

        template<typename... arg_types>
        int call_global_void_func(lua_State *L) {
            using FuncType = void(*)(arg_types...);
            auto func = *static_cast<FuncType *>(lua_touserdata(L, lua_upvalueindex(1)));
            global_func_call_helper(L, func, std::make_index_sequence<sizeof...(arg_types)>());
            return 0;
        }

        // Constructor helper
        template<typename T, typename... arg_types, size_t... I>
        T *ctor_helper([[maybe_unused]] lua_State *L, std::index_sequence<I...>) {
            return new T(lua_to_native<std::decay_t<arg_types>>(L, static_cast<int>(I + 1))...);
        }

        template<typename T, typename... arg_types>
        int call_constructor(lua_State *L) {
            T *obj = ctor_helper<T, arg_types...>(L, std::make_index_sequence<sizeof...(arg_types)>());
            // Newly constructed objects are always Lua-owned (even if class default is non_owning).
            void (*del)(void *) = class_meta<T>::deleter != nullptr
                                      ? class_meta<T>::deleter
                                      : &default_delete_fn<T>;
            push_ptr(L, obj, del);
            return 1;
        }

        // Property getter / setter for data members
        template<typename T, typename M>
        int property_getter(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using MemberPtr = M T::*;
            auto member = *static_cast<MemberPtr *>(lua_touserdata(L, lua_upvalueindex(1)));
            native_to_lua(L, obj->*member);
            return 1;
        }

        template<typename T, typename M>
        int property_setter(lua_State *L) {
            auto obj = check_t<T>(L, 1);
            using MemberPtr = M T::*;
            auto member = *static_cast<MemberPtr *>(lua_touserdata(L, lua_upvalueindex(1)));
            obj->*member = lua_to_native<std::decay_t<M>>(L, 2);
            return 0;
        }

        template<typename T, typename R>
        int prop_func_getter_impl(lua_State *L, R(T::*getter)() const) {
            auto obj = check_t<T>(L, 1);
            native_to_lua(L, (obj->*getter)());
            return 1;
        }

        template<typename T, typename R>
        int prop_func_getter_impl(lua_State *L, R(T::*getter)()) {
            auto obj = check_t<T>(L, 1);
            native_to_lua(L, (obj->*getter)());
            return 1;
        }

        template<typename T, typename R>
        int prop_getter_cfunc(lua_State *L) {
            using Getter = R(T::*)();
            auto getter = *static_cast<Getter *>(lua_touserdata(L, lua_upvalueindex(1)));
            return prop_func_getter_impl(L, getter);
        }

        template<typename T, typename R>
        int prop_getter_const_cfunc(lua_State *L) {
            using Getter = R(T::*)() const;
            auto getter = *static_cast<Getter *>(lua_touserdata(L, lua_upvalueindex(1)));
            return prop_func_getter_impl(L, getter);
        }

        template<typename T, typename A>
        int prop_setter_cfunc(lua_State *L) {
            using Setter = void(T::*)(A);
            auto setter = *static_cast<Setter *>(lua_touserdata(L, lua_upvalueindex(1)));
            auto obj = check_t<T>(L, 1);
            (obj->*setter)(lua_to_native<std::decay_t<A>>(L, 2));
            return 0;
        }

        // Overload dispatch by argument count (excluding self for methods).
        // Upvalues: [1]=count, [2..count+1]=closures each tagged with arity in uservalue... 
        // Simpler: store array of {arity, function pointer as lightuserdata isn't enough}.
        // Use a table upvalue: map arity -> closure.
        inline int overload_dispatch(lua_State *L) {
            const int argc = lua_gettop(L);
            // upvalue 1 is a table: [arity] = closure
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_rawgeti(L, -1, argc);
            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 2);
                return luaL_error(L, "no overload matches argument count %d", argc);
            }
            lua_remove(L, -2); // remove table
            lua_insert(L, 1);  // move closure under args
            lua_call(L, argc, LUA_MULTRET);
            return lua_gettop(L);
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

        // Helper to push a free-function as a closure (used by overload registration).
        template<typename return_type, typename... arg_types>
        void push_global_closure(lua_State *L, return_type(*func)(arg_types...)) {
            using FuncType = return_type(*)(arg_types...);
            auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
            *func_mem = func;
            lua_pushcclosure(L, call_global_func<return_type, arg_types...>, 1);
        }

        template<typename... arg_types>
        void push_global_closure(lua_State *L, void(*func)(arg_types...)) {
            using FuncType = void(*)(arg_types...);
            auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
            *func_mem = func;
            lua_pushcclosure(L, call_global_void_func<arg_types...>, 1);
        }

        template<typename R, typename... Args>
        constexpr int func_arity_of(R(*)(Args...)) {
            return static_cast<int>(sizeof...(Args));
        }

    }

    // ---- Public: type name helpers ----

    // Set a stable metatable / type name for T (call before reg_class, or pass to reg_class).
    template<typename T>
    void set_type_name(const char *name) {
        internal::class_meta<T>::name = name;
    }

    template<typename T>
    const char *get_type_name() {
        return internal::type_name<T>();
    }

    // ---- Public: push helpers ----

    template<typename T>
    void push(lua_State *L, T *v, ownership own = ownership::owning) {
        internal::native_to_lua(L, v, own);
    }

    template<typename T>
    void push(lua_State *L, const T &v) {
        internal::native_to_lua(L, v);
    }

    // ---- Public: container helpers (also available via native_to_lua specializations) ----

    template<typename T>
    void push_vector(lua_State *L, const std::vector<T> &v) {
        internal::native_to_lua(L, v);
    }

    template<typename T>
    std::vector<T> to_vector(lua_State *L, int index) {
        return internal::lua_to_vector<T>(L, index);
    }

    template<typename K, typename V>
    void push_map(lua_State *L, const std::map<K, V> &m) {
        internal::native_to_lua(L, m);
    }

    template<typename K, typename V>
    std::map<K, V> to_map(lua_State *L, int index) {
        return internal::lua_to_map<K, V>(L, index);
    }

    // ---- Public: global function registration ----

    template<typename return_type, typename... arg_types>
    void reg_global_func(lua_State *L, const char *func_name, return_type(*func)(arg_types...)) {
        using FuncType = return_type(*)(arg_types...);
        auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
        *func_mem = func;
        lua_pushcclosure(L, internal::call_global_func<return_type, arg_types...>, 1);
        lua_setglobal(L, func_name);
    }

    template<typename... arg_types>
    void reg_global_func(lua_State *L, const char *func_name, void(*func)(arg_types...)) {
        using FuncType = void(*)(arg_types...);
        auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
        *func_mem = func;
        lua_pushcclosure(L, internal::call_global_void_func<arg_types...>, 1);
        lua_setglobal(L, func_name);
    }

    // Register multiple free-function overloads selected by Lua argument count.
    template<typename F, typename... Fs>
    void reg_global_overload(lua_State *L, const char *func_name, F f, Fs... fs) {
        lua_newtable(L);
        auto add = [&](auto func) {
            const int arity = internal::func_arity_of(func);
            internal::push_global_closure(L, func);
            lua_rawseti(L, -2, arity);
        };
        add(f);
        (add(fs), ...);
        lua_pushcclosure(L, internal::overload_dispatch, 1);
        lua_setglobal(L, func_name);
    }

    // ---- Public: class registration ----

    // Register class metatable.
    // name: stable type name (optional; falls back to demangled typeid).
    // own:  default ownership for objects pushed without an explicit override.
    //       owning installs __gc with delete (or custom deleter); non_owning installs no-op __gc.
    template<typename T>
    void reg_class(lua_State *L, const char *name = nullptr,
                   ownership own = ownership::owning,
                   void (*custom_deleter)(T *) = nullptr) {
        if (name != nullptr) {
            internal::class_meta<T>::name = name;
        }

        if (custom_deleter != nullptr) {
            internal::class_meta<T>::typed_deleter = custom_deleter;
            internal::class_meta<T>::deleter = &internal::class_meta<T>::erase_deleter;
        } else if (own == ownership::owning) {
            internal::class_meta<T>::typed_deleter = nullptr;
            internal::class_meta<T>::deleter = &internal::default_delete_fn<T>;
        } else {
            internal::class_meta<T>::typed_deleter = nullptr;
            internal::class_meta<T>::deleter = nullptr;
        }

        auto tname = internal::type_name<T>();
        luaL_newmetatable(L, tname);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, internal::free_class<T>);
        lua_settable(L, -3);

        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2);
        lua_settable(L, -3);

        lua_pop(L, 1);
        internal::class_meta<T>::registered = true;
    }

    // Convenience: non-owning registration (no delete on __gc).
    template<typename T>
    void reg_class_non_owning(lua_State *L, const char *name = nullptr) {
        reg_class<T>(L, name, ownership::non_owning, nullptr);
    }

    // Convenience: owning with custom deleter.
    template<typename T>
    void reg_class_with_deleter(lua_State *L, void (*deleter)(T *), const char *name = nullptr) {
        reg_class<T>(L, name, ownership::owning, deleter);
    }

    template<typename T, typename return_type, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, return_type(T::*func)(arg_types...)) {
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = return_type(T::*)(arg_types...);
        auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_func<T, return_type, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename T, typename return_type, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, return_type(T::*func)(arg_types...) const) {
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = return_type(T::*)(arg_types...) const;
        auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_const_func<T, return_type, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename T, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, void(T::*func)(arg_types...)) {
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = void(T::*)(arg_types...);
        auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_void_func<T, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    template<typename T, typename... arg_types>
    void reg_class_func(lua_State *L, const char *func_name, void(T::*func)(arg_types...) const) {
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using FuncType = void(T::*)(arg_types...) const;
        auto func_mem = static_cast<FuncType *>(lua_newuserdata(L, sizeof(FuncType)));
        new (func_mem) FuncType(func);

        lua_pushcclosure(L, internal::call_class_void_const_func<T, arg_types...>, 1);
        lua_setfield(L, -2, func_name);
        lua_pop(L, 1);
    }

    // Register a constructor as a global (or name) that returns an owning instance.
    template<typename T, typename... arg_types>
    void reg_constructor(lua_State *L, const char *func_name) {
        lua_pushcclosure(L, internal::call_constructor<T, arg_types...>, 0);
        lua_setglobal(L, func_name);
    }

    // Register a data-member property as get_<name> / set_<name> on the metatable.
    template<typename T, typename M>
    void reg_property(lua_State *L, const char *prop_name, M T::*member) {
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }

        using MemberPtr = M T::*;

        std::string getter = std::string("get_") + prop_name;
        auto get_mem = static_cast<MemberPtr *>(lua_newuserdata(L, sizeof(MemberPtr)));
        new (get_mem) MemberPtr(member);
        lua_pushcclosure(L, internal::property_getter<T, M>, 1);
        lua_setfield(L, -2, getter.c_str());

        std::string setter = std::string("set_") + prop_name;
        auto set_mem = static_cast<MemberPtr *>(lua_newuserdata(L, sizeof(MemberPtr)));
        new (set_mem) MemberPtr(member);
        lua_pushcclosure(L, internal::property_setter<T, M>, 1);
        lua_setfield(L, -2, setter.c_str());

        lua_pop(L, 1);
    }

    // Register getter/setter member functions under a property name prefix.
    template<typename T, typename R>
    void reg_property(lua_State *L, const char *prop_name, R(T::*getter)() const) {
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }
        using Getter = R(T::*)() const;
        std::string gname = std::string("get_") + prop_name;
        auto mem = static_cast<Getter *>(lua_newuserdata(L, sizeof(Getter)));
        new (mem) Getter(getter);
        lua_pushcclosure(L, internal::prop_getter_const_cfunc<T, R>, 1);
        lua_setfield(L, -2, gname.c_str());
        lua_pop(L, 1);
    }

    template<typename T, typename R, typename A>
    void reg_property(lua_State *L, const char *prop_name, R(T::*getter)() const, void(T::*setter)(A)) {
        reg_property(L, prop_name, getter);
        auto name = internal::type_name<T>();
        if (!luaL_getmetatable(L, name)) {
            lua_pop(L, 1);
            return;
        }
        using Setter = void(T::*)(A);
        std::string sname = std::string("set_") + prop_name;
        auto mem = static_cast<Setter *>(lua_newuserdata(L, sizeof(Setter)));
        new (mem) Setter(setter);
        lua_pushcclosure(L, internal::prop_setter_cfunc<T, A>, 1);
        lua_setfield(L, -2, sname.c_str());
        lua_pop(L, 1);
    }

    // Register an enum as a global table of name -> integer value.
    template<typename E>
    void reg_enum(lua_State *L, const char *enum_name,
                  std::initializer_list<std::pair<const char *, E>> values) {
        static_assert(std::is_enum_v<E> || std::is_integral_v<E>,
                      "reg_enum expects an enum or integral type");
        lua_newtable(L);
        for (const auto &kv : values) {
            lua_pushinteger(L, static_cast<lua_Integer>(kv.second));
            lua_setfield(L, -2, kv.first);
        }
        lua_setglobal(L, enum_name);
    }

    // ---- Public: call Lua from C++ ----

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

        if constexpr (sizeof...(ret_types) > 0) {
            internal::lua_func_ret_helper(L, rets);
        }

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

        if constexpr (sizeof...(ret_types) > 0) {
            internal::lua_func_ret_helper(L, rets);
        }

        return std::nullopt;
    }

}
