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
* **Rich Type Support**: Built-in support for `std::string_view`, `std::string`, primitive types, and custom user classes.
* **Non-intrusive**: No need to inherit from any base class or decorate classes with intrusive macros.
* **Safe Lifetime Management**: Uses Lua weak tables and userdata to track and manage C++ pointer lifecycles, preventing duplicate userdata creation and memory leaks.
* **Modern CMake**: First-class support for `find_package(bLua)`, `FetchContent`, and CTest unit testing.

## Usage

### 1. Lua Calling C++
Register a class and its member functions (supports global functions, member functions, const member functions, etc.):

```cpp
#include "bLua.h"

// Register global functions
bLua::reg_global_func(L, "newA", newA);
bLua::reg_global_func(L, "printA", printA);

// Register class and member functions
bLua::reg_class<A>(L);
bLua::reg_class_func(L, "get_this", &A::get_this);
bLua::reg_class_func(L, "get_int", &A::get_int);
bLua::reg_class_func(L, "get_int_const", &A::get_int_const); // Supports const member functions
bLua::reg_class_func(L, "set_int", &A::set_int);
bLua::reg_class_func(L, "get_string", &A::get_string);
bLua::reg_class_func(L, "set_string", &A::set_string);
```

Then invoke them in Lua:

```lua
-- Create an instance
local a = newA()

-- Call member functions
a:set_int(123)
print("a:get_int() ", a:get_int())

-- Call string functions
a:set_string("abc")
print("a:get_string() ", a:get_string())
```

### 2. C++ Calling Lua
#### Call Global Function
```cpp
// Receive multiple return values
int output_int = 0;
std::string output_str;
uint64_t output_int64 = 0;

// Input arguments
int input_int = 123;
std::string input_str = "test";

// Call the function (supports 0 or more arguments)
auto err = bLua::call_lua_global_func(L, "global_func_name", 
    std::tie(output_int, output_str, output_int64), 
    input_int, input_str);

// Check if call succeeded
if (err) {
    // Error occurred, print error message
    printf("ret error: %s\n", err.value().c_str());
} else {
    // Succeeded, print returned values
    printf("%d %s %llu\n", output_int, output_str.c_str(), output_int64);
}
```

#### Call Nested Table Function
If the Lua function is located inside nested tables:
```lua
function _G.test.func.test(a, b)
    return a - b
end
```
Call it from C++ via a path of table keys:
```cpp
auto err = bLua::call_lua_table_func(L, {"_G", "test", "func"}, "test", 
    std::tie(output_int), 
    input_int, input_int2);
```

For complete examples, refer to [test/test.cpp](test/test.cpp) and [test/test.lua](test/test.lua).

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
