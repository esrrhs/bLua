# bLua

[<img src="https://img.shields.io/github/license/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/languages/top/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/actions/workflow/status/esrrhs/bLua/c-cpp.yml?branch=master">](https://github.com/esrrhs/bLua/actions)

> **A lightweight, header-only C++17 binding library for Lua.**

[English](#english) | [中文说明](#中文说明)

---

<span id="english"></span>
## English

### Overview
A header-only C++ and Lua glue/bridge library (`b` stands for bridge). Built on modern C++17, **bLua** provides a clean, zero-overhead, and non-intrusive interface for seamless interoperability between C++ and Lua.

### Features
* **Modern C++17**: Leverages fold expressions and template metaprogramming for efficient compilation and zero runtime overhead.
* **Header-only**: Single header file `bLua.h` with no extra libraries to build—just drop it into your project and use.
* **Member Function Support**: Easily binds regular functions, member functions, and `const` member functions.
* **Rich Type Support**: Built-in support for `std::string_view`, `std::string`, primitive types, and custom user classes.
* **Non-intrusive**: No need to inherit from any base class or decorate classes with intrusive macros.
* **Safe Lifetime Management**: Uses Lua weak tables and userdata to track and manage C++ pointer lifecycles, preventing duplicate userdata creation and memory leaks.
* **Modern CMake**: First-class support for `find_package(bLua)`, `FetchContent`, and CTest unit testing.

### Usage

#### 1. Lua Calling C++
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

#### 2. C++ Calling Lua
##### Call Global Function
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

##### Call Nested Table Function
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

### Integration

#### Option 1: Direct Header Inclusion
Copy `bLua.h` into your project directory and `#include "bLua.h"`.

#### Option 2: CMake FetchContent
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

#### Option 3: CMake find_package
After installing, use it directly in `CMakeLists.txt`:
```cmake
find_package(bLua REQUIRED)
target_link_libraries(your_target PRIVATE bLua::bLua)
```

### Build & Test
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

<span id="中文说明"></span>
## 中文说明

### 简介
C++ 与 Lua 的胶水层（Header-only），`b` 代表 bridge。基于现代 C++17 标准设计，为 C++ 与 Lua 之间的互相调用提供简洁、高效、非侵入式的绑定能力。

### 特性
* **现代 C++17**：充分利用折叠表达式与模板元编程，编译高效，无运行时开销。
* **Header-only**：单个头文件 `bLua.h`，无须编译库文件，直接包含即用。
* **支持 const / 非 const 成员函数**：类成员函数及常成员函数均可便捷绑定。
* **支持 std::string_view / std::string / 常见基本类型**。
* **无侵入**：C++ 类无需继承任何基类或添加宏。
* **生命周期安全**：利用 Lua Weak Table 与 Userdata 管理 C++ 指针生命周期，避免重复创建与内存泄漏。
* **现代 CMake**：支持 `find_package(bLua)`、`FetchContent` 以及 CTest 单元测试。

### 用法
#### 1. Lua 调用 C++
首先注册类及需要的成员函数（支持普通函数、成员函数、常成员函数等）：
```cpp
#include "bLua.h"

// 注册全局函数
bLua::reg_global_func(L, "newA", newA);
bLua::reg_global_func(L, "printA", printA);

// 注册类及成员函数
bLua::reg_class<A>(L);
bLua::reg_class_func(L, "get_this", &A::get_this);
bLua::reg_class_func(L, "get_int", &A::get_int);
bLua::reg_class_func(L, "get_int_const", &A::get_int_const); // 支持 const 成员函数
bLua::reg_class_func(L, "set_int", &A::set_int);
bLua::reg_class_func(L, "get_string", &A::get_string);
bLua::reg_class_func(L, "set_string", &A::set_string);
```

然后在 Lua 中使用即可：
```lua
-- 创建一个对象
local a = newA()

-- 调用对象函数
a:set_int(123)
print("a:get_int() ", a:get_int())

-- 调用对象函数
a:set_string("abc")
print("a:get_string() ", a:get_string())
```

#### 2. C++ 调用 Lua
##### 调用全局函数
```cpp
// 接收多个返回值
int output_int = 0;
std::string output_str;
uint64_t output_int64 = 0;

// 输入参数
int input_int = 123;
std::string input_str = "test";

// 调用（也支持 0 个入参）
auto err = bLua::call_lua_global_func(L, "global_func_name", 
    std::tie(output_int, output_str, output_int64), 
    input_int, input_str);

// 调用是否成功
if (err) {
    // 出错，输出错误信息
    printf("ret error: %s\n", err.value().c_str());
} else {
    // 成功则输出函数返回结果
    printf("%d %s %llu\n", output_int, output_str.c_str(), output_int64);
}
```

##### 调用嵌套 Table 函数
若 Lua 函数定义在嵌套 table 中：
```lua
function _G.test.func.test(a, b)
    return a - b
end
```
C++ 可直接通过层级数组调用：
```cpp
auto err = bLua::call_lua_table_func(L, {"_G", "test", "func"}, "test", 
    std::tie(output_int), 
    input_int, input_int2);
```

具体完整示例可参考 [test/test.cpp](test/test.cpp) 和 [test/test.lua](test/test.lua)。

### 引入项目

#### 方式一：直接包含头文件
将 `bLua.h` 复制到您的项目中，包含即可使用。

#### 方式二：CMake FetchContent
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

#### 方式三：CMake find_package
安装后直接在 `CMakeLists.txt` 中：
```cmake
find_package(bLua REQUIRED)
target_link_libraries(your_target PRIVATE bLua::bLua)
```

### 编译与测试
使用现代 CMake 命令：
```bash
# 配置工程
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译测试程序
cmake --build build

# 运行单元测试
ctest --test-dir build --output-on-failure
```

---

## Related Projects / 其他
* [lua全家桶 / Lua Family Bucket](https://github.com/esrrhs/lua-family-bucket)
