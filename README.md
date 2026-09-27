# bLua

[<img src="https://img.shields.io/github/license/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/languages/top/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/actions/workflow/status/esrrhs/bLua/c-cpp.yml?branch=master">](https://github.com/esrrhs/bLua/actions)

C++ 与 Lua 的胶水层（Header-only），`b` 代表 bridge。

# 特性
* **现代 C++17**：充分利用折叠表达式与模板元编程，编译高效，无运行时开销。
* **Header-only**：单个头文件 `bLua.h`，无须编译库文件，直接包含即用。
* **支持 const / 非 const 成员函数**：类成员函数及常成员函数均可便捷绑定。
* **支持 std::string_view / std::string / 常见基本类型**。
* **无侵入**：C++ 类无需继承任何基类或添加宏。
* **生命周期安全**：利用 Lua Weak Table 与 Userdata 管理 C++ 指针生命周期，避免重复创建与内存泄漏。
* **现代 CMake**：支持 `find_package(bLua)`、`FetchContent` 以及 CTest 单元测试。

# 用法
### 1. Lua 调用 C++
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

### 2. C++ 调用 Lua
#### 调用全局函数
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

具体完整示例可参考 [test/test.cpp](file:///home/project/bLua/test/test.cpp) 和 [test/test.lua](file:///home/project/bLua/test/test.lua)。

# 引入项目

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

# 编译与测试
使用现代 CMake 命令：
```bash
# 配置工程
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译测试程序
cmake --build build

# 运行单元测试
ctest --test-dir build --output-on-failure
```

## 其他
[lua全家桶](https://github.com/esrrhs/lua-family-bucket)
