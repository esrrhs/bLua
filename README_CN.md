# bLua

[<img src="https://img.shields.io/github/license/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/languages/top/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/actions/workflow/status/esrrhs/bLua/c-cpp.yml?branch=master">](https://github.com/esrrhs/bLua/actions)

> **轻量级、Header-only 的 Lua C++17 绑定库。**

[English](README.md) | [中文说明](README_CN.md)

---

## 简介
C++ 与 Lua 的胶水层（Header-only），`b` 代表 bridge。基于现代 C++17 标准设计，为 C++ 与 Lua 之间的互相调用提供简洁、高效、非侵入式的绑定能力。

## 特性
* **现代 C++17**：充分利用折叠表达式与模板元编程，编译高效，无运行时开销。
* **Header-only**：单个头文件 `bLua.h`，无须编译库文件，直接包含即用。
* **支持 const / 非 const 成员函数**：类成员函数及常成员函数均可便捷绑定。
* **支持 std::string_view / std::string / 常见基本类型**，以及 `std::vector` / `std::map` ↔ Lua table 转换。
* **无侵入**：C++ 类无需继承任何基类或添加宏。
* **可配置所有权**：owning / non-owning / 自定义 deleter，避免非 Lua 拥有的指针被 `__gc` 误删。
* **稳定类型名**：可为类指定 metatable 名（GCC/Clang 上回退到 demangle 后的 typeid）。
* **API 体验**：构造函数、属性、枚举、按参数个数分发的重载注册。
* **现代 CMake**：支持 `find_package(bLua)`、`FetchContent` 以及 CTest 单元测试。

## Lua 版本兼容矩阵

| 运行时 | 状态 | 说明 |
|--------|------|------|
| Lua 5.4 | 支持（CI） | 主目标 |
| Lua 5.3 | 支持（CI） | |
| Lua 5.2 | 兼容 | 含兼容 shim，未进 CI |
| Lua 5.1 / LuaJIT | 兼容（CI：LuaJIT） | 含 `lua_rawgetp` / `lua_rawlen` / `luaL_setmetatable` shim |

## 用法
### 1. Lua 调用 C++
首先注册类及需要的成员函数（支持普通函数、成员函数、常成员函数等）：
```cpp
#include "bLua.h"

// 注册全局函数
bLua::reg_global_func(L, "newA", newA);
bLua::reg_global_func(L, "printA", printA);

// 注册类（建议传入稳定类型名）；默认 owning，即 __gc 时 delete
bLua::reg_class<A>(L, "A");
bLua::reg_class_func(L, "get_this", &A::get_this);
bLua::reg_class_func(L, "get_int", &A::get_int);
bLua::reg_class_func(L, "get_int_const", &A::get_int_const);
bLua::reg_class_func(L, "set_int", &A::set_int);
bLua::reg_class_func(L, "get_string", &A::get_string);
bLua::reg_class_func(L, "set_string", &A::set_string);

// 非拥有（C++ 保有所有权，__gc 不 delete）
// bLua::reg_class_non_owning<A>(L, "A");
// 自定义释放：
// bLua::reg_class_with_deleter<A>(L, my_free_A, "A");

// 构造函数 / 属性 / 枚举 / 重载
bLua::reg_constructor<A, int>(L, "A_new");
bLua::reg_property(L, "a", &A::m_a);           // get_a / set_a
bLua::reg_enum<Color>(L, "Color", {{"Red", Color::Red}, {"Green", Color::Green}});
bLua::reg_global_overload(L, "add", add1, add2); // 按参数个数分发
```

然后在 Lua 中使用即可：
```lua
local a = newA()
a:set_int(123)
print("a:get_int() ", a:get_int())
a:set_string("abc")
print("a:get_string() ", a:get_string())
```

### 2. 容器转换
```cpp
std::vector<int> v = bLua::to_vector<int>(L, index);
bLua::push_vector(L, v);

std::map<std::string, int> m = bLua::to_map<std::string, int>(L, index);
bLua::push_map(L, m);
```

### 3. C++ 调用 Lua
#### 调用全局函数
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

#### 调用嵌套 Table 函数
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

具体完整示例可参考 [test/test.cpp](test/test.cpp) 和 [test/test.lua](test/test.lua)。变更说明见 [CHANGELOG.md](CHANGELOG.md)。

## 引入项目

### 方式一：直接包含头文件
将 `bLua.h` 复制到您的项目中，包含即可使用。

### 方式二：CMake FetchContent
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

### 方式三：CMake find_package
安装后直接在 `CMakeLists.txt` 中：
```cmake
find_package(bLua REQUIRED)
target_link_libraries(your_target PRIVATE bLua::bLua)
```

## 编译与测试
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

## 相关项目
* [lua全家桶](https://github.com/esrrhs/lua-family-bucket)
