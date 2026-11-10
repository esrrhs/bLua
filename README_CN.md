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
* **现代 C++17**：充分利用折叠表达式与模板元编程，实现紧凑的绑定。
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
| Lua 5.2 | 支持（已配置 CI） | 使用版本无关的 getter 包装 |
| Lua 5.1 / LuaJIT | 支持（已配置 CI） | 私有兼容辅助，不覆盖 Lua API 宏 |

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

### 所有权与注册约定

```cpp
bLua::reg_class<A>(L, "A");  // 名字复制保存，每个 Lua 状态独立
bLua::push(L, cpp_owned, bLua::ownership::non_owning);
// 替代旧版进程全局接口：现在必须传 L
bLua::set_type_name<B>(L, "B"); // 在注册之前设置
bLua::reg_class<B>(L);
const char* name = bLua::get_type_name<B>(L); // 借用到状态关闭/名字变更
```

* 类型名、默认所有权和 deleter 按 Lua 状态隔离；userdata 创建时固定释放策略，
  其它状态的注册不会改变已有对象的释放方式。
* 相同注册可重复；注册后改名/改策略、不同 C++ 类型使用相同名字都会抛出
  `std::runtime_error`。跨编译器稳定的名字请显式指定，demangle 仅作回退。
* 同一状态中，同地址同类型复用活跃 userdata；显式 `push(L,ptr,ownership)`
  与已有策略冲突会报错，不会偷偷接管。隐式推送/函数返回指针复用已有策略，
  类默认策略仅用于新包装。同地址不同类型拒绝绑定（不提供继承转换）。
* 构造函数始终产生 Lua-owned 对象，即使类默认为 non-owning。
  自定义 deleter 必须匹配分配方式且不得抛异常；每个包装最多调用一次。
  重复 `__gc` 安全；已失效对象的方法/属性访问会报错。
* 非拥有对象必须比所有 Lua 引用活得更久，无法自动感知 C++ 外部析构。
  不要让多个 Lua 状态同时拥有同一分配。返回 C++ 的对象裸指针是借用，
  使用期间须保留 Lua 强引用；`const char*`/`string_view` 返回值也借用 Lua 存储。
* C++ 直接调用注册/栈辅助接口时，校验失败抛异常；不要从未保护的原生 C 回调中
  调用并让异常跨越 Lua。正常注册的回调会先析构 C++ 临时量，再转换为 Lua 错误。

### 错误与类型转换

`call_lua_global_func` / `call_lua_table_func` 将 Lua 错误、函数/路径缺失、
原生异常、参数/返回值转换失败转换为错误字符串，并恢复调用前的 Lua 栈。
全部返回值转换成功才写入输出，转换失败不会部分修改结果；用户自定义 C++
输出类型的赋值/分配异常不保证事务性。`debug.traceback` 可缺失，非字符串错误对象
也有兜底消息。函数和路径用原始 table key 查找，不触发 `__index`。

数字、布尔、字符串采用严格类型检查，不再静默强转；整数拒绝小数及越界，推送值须
能装入 `lua_Integer`。Lua 5.1/5.2、LuaJIT 的数值精度仍取决于其构建（通常 double）。
缺少数字/字符串返回值会报错，不再默认为 0/空串。类指针及 `const char*` 允许 `nil`，
接收空指针的 C++ 函数须自行处理。

保护边界覆盖常规 Lua 错误与 C++ 异常，**不承诺 Lua OOM 可恢复**，也不支持原生
回调在活跃 C++ RAII 对象上直接 `lua_error`、或恶意篡改 registry/metatable。
请使用按 C 编译、正常 protected-call ABI 的 Lua，开启 C++ 异常，并信任宿主回调和
释放函数。Lua 分配失败可能 longjmp 跳过 C++ 临时量析构，应视为致命错误而非继续运行。

### 重载与属性范围

`reg_global_overload` 仅按参数个数分发；重复 arity 在替换全局函数前报错。
同参数个数按类型选重载尚未实现，可使用不同 Lua 名；选择 C++ 重载函数时可显式
`static_cast`。`reg_property` 生成 `get_name`/`set_name` 方法，不是 Lua 字段访问器；
const 数据成员只生成 getter，const getter 方法可配 setter。

### 2. 容器转换
```cpp
std::vector<int> v = bLua::to_vector<int>(L, index);
bLua::push_vector(L, v);

std::map<std::string, int> m = bLua::to_map<std::string, int>(L, index);
bLua::push_map(L, m);
```
支持嵌套 vector/map 与已支持的基本/指针类型。vector 读取从 1 开始的序列
（`rawlen`），不承诺带洞/混合键 table 的密集数组语义。map 保留 `lua_next` 原始 key；
数字键使用 `map<int,V>`，`map<string,V>` 拒绝数字/混合键而不自动字符串化。
失败时恢复栈；空 table 转换为空容器。

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

安装后的 bLua 前缀可以移动，但 Lua 是**外部依赖**，不会随 bLua 打包。
消费端须自行找到与宿主 ABI 兼容、同运行时家族/版本的 Lua；多个版本或 LuaJIT
建议显式设置 `LUA_INCLUDE_DIR` / `LUA_LIBRARY`。导出目标引用 `Lua::Lua`，
不写入生产端 Lua 路径，由消费端 `find_dependency(Lua)` 重新解析。

## 编译与测试
使用现代 CMake 命令：
```bash
# 配置工程
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译测试程序
cmake --build build

# 运行单元测试
ctest --test-dir build --output-on-failure

# Debug + 仅测试目标开启 ASan/UBSan（GCC/Clang）
cmake -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DBLUA_ENABLE_SANITIZERS=ON
cmake --build build/sanitize
ctest --test-dir build/sanitize --output-on-failure
```

CTest 包含集成/回归、故意失败退出码检查，以及安装→移动前缀→独立 find_package
消费端编译运行。检查在 Release 中仍生效。CI 已配置 GCC/Clang × Lua 5.1–5.4、
LuaJIT、macOS、Windows，以及 Linux Debug / ASan+UBSan；Windows 使用固定官方 Lua 5.4.7 源码和 MSVC，LuaJIT 的外部异常展开会保留。
配置不等于尚未推送的远程 CI 已运行。

---

## 相关项目
* [lua全家桶](https://github.com/esrrhs/lua-family-bucket)
