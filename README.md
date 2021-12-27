# bLua
C++与Lua的胶水层，b代表着bridge

# 特性
* 依赖C++17
* 只有一个头文件
* 接口简单轻量
* userdata的方式管理c++指针生命周期

# 用法
### lua调用c++
首先注册类及需要的成员函数
```c++
// 注册全局函数
bLua::reg_global_func(L, "newA", newA);
bLua::reg_global_func(L, "printA", printA);

// 注册类函数
bLua::reg_class<A>(L);
bLua::reg_class_func(L, "get_this", &A::get_this);
bLua::reg_class_func(L, "get_int", &A::get_int);
bLua::reg_class_func(L, "set_int", &A::set_int);
bLua::reg_class_func(L, "get_string", &A::get_string);
bLua::reg_class_func(L, "set_string", &A::set_string);
```
然后在lua中使用即可
```lua
-- 创建一个对象
local a = newA()

-- 调用对象函数
a:set_int(123)
print("a:get_int() ", a:get_int())

-- 调用对象函数
a:set_string("abc")
print("a:set_string() ", a:get_string())
```

### c++调用lua
c++调用lua全局函数
```c++
// 多个返回值
int output_int = 0;
std::string output_str;
uint64_t output_int64 = 0;

// 输入参数
int input_int = 123;
std::string input_str = "test";

// 调用
auto err = bLua::call_lua_global_func(L, "global_func_name", 
    std::tie(output_int, output_str, output_int64), 
    input_int, input_str);

// 调用是否成功
if (err) {
    // 出错，输出错误信息
    printf("ret error %s\n", err.value().c_str());
} else {
    // 成功则输出函数返回结果
    printf("%d %s %llu\n", output_int, output_str.c_str(), output_int64);
}
```
如果lua的函数在一层层table中，例如
```lua
function _G.test.func.test(a, b)
    return a - b
end
```
那么c++调用lua嵌套的table函数
```c++
// 调用
auto err = bLua::call_lua_table_func(L, {"_G", "test", "func"}, "test", 
    std::tie(output_int, output_str, output_int64), 
    input_int, input_str);
```
具体例子可以参考test目录的test.cpp和test.lua

# 编译
```
# mkdir build
# cd build
# cmake ..
# make
```
运行test程序
```
# ./bLua 
```

## 其他
[lua全家桶](https://github.com/esrrhs/lua-family-bucket)
