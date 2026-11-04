# bLua

[<img src="https://img.shields.io/github/license/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/languages/top/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/actions/workflow/status/esrrhs/bLua/c-cpp.yml?branch=master">](https://github.com/esrrhs/bLua/actions)

> **A lightweight, header-only C++17 binding library for Lua.**

[English](README.md) | [Chinese](README_CN.md)

---

## Overview
A header-only C++ and Lua glue/bridge library (`b` stands for bridge). Built on modern C++17, **bLua** provides a clean, zero-overhead, and non-intrusive interface for seamless interoperability between C++ and Lua.

## Features
* **Modern C++17**: Leverages fold expressions and template metaprogramming for efficient compilation and zero runtime overhead.
* **Header-only**: Single header file `bLua.h` with no extra libraries to build—just drop it into your project and use.
* **Member Function Support**: Easily binds regular functions, member functions, and `const` member functions.
* **Rich Type Support**: Built-in support for `std::string_view`, `std::string`, primitive types, custom user classes, plus `std::vector` / `std::map` ↔ Lua table helpers.
* **Non-intrusive**: No need to inherit from any base class or decorate classes with intrusive macros.
* **Configurable ownership**: Owning / non-owning / custom deleter so Lua `__gc` does not free C++-owned pointers.
* **Stable type names**: User-provided metatable names (demangled `typeid` fallback on GCC/Clang).
* **API DX**: Constructors, properties, enums, and arity-based free-function overloads.
* **Modern CMake**: First-class support for `find_package(bLua)`, `FetchContent`, and CTest unit testing.

## Lua version matrix

| Runtime | Status | Notes |
|---------|--------|-------|
| Lua 5.4 | Supported (CI) | Primary target |
| Lua 5.3 | Supported (CI) | |
| Lua 5.2 | Compatible | Compatibility shims present; not in CI matrix |
| Lua 5.1 / LuaJIT | Compatible (CI: LuaJIT) | `lua_rawgetp` / `lua_rawlen` / `luaL_setmetatable` shims |

## Usage

### 1. Lua Calling C++
Register a class and its member functions (supports global functions, member functions, const member functions, etc.):

```cpp
#include "bLua.h"

// Register global functions
bLua::reg_global_func(L, "newA", newA);
bLua::reg_global_func(L, "printA", printA);

// Register class with a stable name (recommended); default ownership is owning (delete on __gc)
bLua::reg_class<A>(L, "A");
bLua::reg_class_func(L, "get_this", &A::get_this);
bLua::reg_class_func(L, "get_int", &A::get_int);
bLua::reg_class_func(L, "get_int_const", &A::get_int_const);
bLua::reg_class_func(L, "set_int", &A::set_int);
bLua::reg_class_func(L, "get_string", &A::get_string);
bLua::reg_class_func(L, "set_string", &A::set_string);

// Non-owning (C++ keeps ownership; __gc does not delete)
// bLua::reg_class_non_owning<A>(L, "A");
// Custom deleter:
// bLua::reg_class_with_deleter<A>(L, my_free_A, "A");

// Constructor / property / enum / overload
bLua::reg_constructor<A, int>(L, "A_new");
bLua::reg_property(L, "a", &A::m_a);           // get_a / set_a
bLua::reg_enum<Color>(L, "Color", {{"Red", Color::Red}, {"Green", Color::Green}});
bLua::reg_global_overload(L, "add", add1, add2); // dispatch by argument count
```

Then invoke them in Lua:

```lua
local a = newA()
a:set_int(123)
print("a:get_int() ", a:get_int())
a:set_string("abc")
print("a:get_string() ", a:get_string())
```

### 2. Containers
```cpp
std::vector<int> v = bLua::to_vector<int>(L, index);
bLua::push_vector(L, v);

std::map<std::string, int> m = bLua::to_map<std::string, int>(L, index);
bLua::push_map(L, m);
```

### 3. C++ Calling Lua
#### Call Global Function
```cpp
int output_int = 0;
std::string output_str;
uint64_t output_int64 = 0;

int input_int = 123;
std::string input_str = "test";

auto err = bLua::call_lua_global_func(L, "global_func_name",
    std::tie(output_int, output_str, output_int64),
    input_int, input_str);

if (err) {
    printf("ret error: %s\n", err.value().c_str());
} else {
    printf("%d %s %llu\n", output_int, output_str.c_str(), output_int64);
}
```

#### Call Nested Table Function
```lua
function _G.test.func.test(a, b)
    return a - b
end
```
```cpp
auto err = bLua::call_lua_table_func(L, {"_G", "test", "func"}, "test",
    std::tie(output_int),
    input_int, input_int2);
```

For complete examples, refer to [test/test.cpp](test/test.cpp) and [test/test.lua](test/test.lua). See also [CHANGELOG.md](CHANGELOG.md).

## Integration

### Option 1: Direct Header Inclusion
Copy `bLua.h` into your project directory and `#include "bLua.h"`.

### Option 2: CMake FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
    bLua
    GIT_REPOSITORY https://github.com/esrrhs/bLua.git
    GIT_TAG master
)
FetchContent_MakeAvailable(bLua)

target_link_libraries(your_target PRIVATE bLua::bLua)
```

### Option 3: CMake find_package
After installing, use it directly in `CMakeLists.txt`:
```cmake
find_package(bLua REQUIRED)
target_link_libraries(your_target PRIVATE bLua::bLua)
```

## Build & Test
Build and run tests using modern CMake:
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build test executable
cmake --build build

# Run unit tests
ctest --test-dir build --output-on-failure
```

---

## Related Projects
* [Lua Family Bucket](https://github.com/esrrhs/lua-family-bucket)
