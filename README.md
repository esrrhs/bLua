# bLua

[<img src="https://img.shields.io/github/license/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/languages/top/esrrhs/bLua">](https://github.com/esrrhs/bLua)
[<img src="https://img.shields.io/github/actions/workflow/status/esrrhs/bLua/c-cpp.yml?branch=master">](https://github.com/esrrhs/bLua/actions)

> **A lightweight, header-only C++17 binding library for Lua.**

[English](README.md) | [Chinese](README_CN.md)

---

## Overview
A header-only C++ and Lua glue/bridge library (`b` stands for bridge). Built on modern C++17, **bLua** provides a small, non-intrusive interface for seamless interoperability between C++ and Lua.

## Features
* **Modern C++17**: Leverages fold expressions and template metaprogramming for compact bindings.
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
| Lua 5.2 | Supported (CI configuration) | Version-neutral getter wrappers |
| Lua 5.1 / LuaJIT | Supported (CI configuration) | Private compatibility helpers; no Lua API macro overrides |

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

### Ownership and registration contracts

```cpp
bLua::reg_class<A>(L, "A");                    // copied, state-local name
bLua::push(L, cpp_owned, bLua::ownership::non_owning);
// Names now require the state; replaces the former process-global API:
bLua::set_type_name<B>(L, "B");                // before registration
bLua::reg_class<B>(L);
const char* name = bLua::get_type_name<B>(L);    // borrowed until state closes / name changes
```

* Each Lua state has independent names and default ownership/deleters. The wrapper's
  destruction policy is fixed at creation, independent of subsequent registrations elsewhere.
* Identical class registrations are idempotent. Renaming/reconfiguring a registered class,
  or registering another type with the same name, throws `std::runtime_error`.
* Within one state, the same address and type reuse one live userdata. An explicit
  `push(L, ptr, ownership)` with conflicting ownership is rejected, never silently adopted.
  Implicit pushes/returned pointers reuse an existing wrapper's policy; class defaults apply
  only to new wrappers. Different types at the same address are rejected (no inheritance casts).
* Constructors always return Lua-owned objects, even for a non-owning class default.
  Custom deleters must match the allocation and must not throw; each wrapper calls its captured
  deleter at most once. Repeated `__gc` is harmless; methods/properties reject finalized objects.
* Borrowed C++ objects must outlive every Lua reference. This is not a weak pointer to C++:
  external destruction cannot be detected automatically. Never give multiple Lua states owning
  wrappers for the same C++ allocation. Raw pointers returned to C++ are borrowed; keep a Lua
  reference rooted while using them. `const char*` / `string_view` results also borrow Lua storage.
* Registration/stack helpers called directly from C++ throw on validation errors. Do not invoke
  them from an unguarded C callback and let C++ exceptions cross Lua. Normal registered callbacks
  translate native exceptions to Lua errors after C++ temporaries have unwound.

### Errors and type conversion

`call_lua_global_func` / `call_lua_table_func` return an error string for Lua execution,
missing functions/tables, native exceptions, and result/argument conversion failures. The
caller's Lua stack is restored. Result values are converted before updating outputs, so a
conversion failure does not partially overwrite them. Debug/traceback is optional; arbitrary
Lua error objects get a safe fallback message. Function/path lookup uses raw table keys,
not `__index` metamethods. Assignment/allocation failures in user-defined C++ result types are
not a transactional guarantee.

Numbers, booleans and strings are checked rather than silently coerced. Integer conversions
reject fractions/range overflow; values pushed as integers must fit `lua_Integer`. Lua 5.1/5.2
and LuaJIT number precision still depends on their runtime build (usually double).
Missing numeric/string results are errors, not zero/empty defaults. Nullable class pointers and
`const char*` accept `nil`; C++ callbacks receiving null pointers must handle them.

The protected boundary covers ordinary Lua errors and C++ exceptions, **not recoverable Lua
allocator exhaustion**, arbitrary host callbacks that call `lua_error` across live C++ RAII,
or hostile metatable/registry modification. Use a Lua library built as C with its normal
protected-call ABI, C++ exceptions enabled, and trusted host callbacks/deleters. Lua OOM can
longjmp over C++ temporaries during allocations; treat it as fatal rather than resume execution.

### Overload / property scope

`reg_global_overload` dispatches by argument count only and rejects duplicate arities before
replacing the global function. Same-arity, type-based overload resolution is not implemented;
use distinct Lua names or an explicit `static_cast` when selecting a C++ overloaded function.
`reg_property` creates `get_name`/`set_name` methods, not Lua field accessors. Const data members
are getter-only; const getter functions can be paired with a setter.

### 2. Containers
```cpp
std::vector<int> v = bLua::to_vector<int>(L, index);
bLua::push_vector(L, v);

std::map<std::string, int> m = bLua::to_map<std::string, int>(L, index);
bLua::push_map(L, m);
```
Conversions support nested vectors/maps and the supported scalar/pointer types. Vectors read
Lua's 1-based sequence (`rawlen`); holes and non-sequence fields are not a dense-array contract.
Maps preserve `lua_next`'s key. Key types are strict: numeric keys work with `map<int, V>`;
`map<string, V>` rejects numeric/mixed keys rather than stringifying them. Failures restore the
stack. Empty tables convert to empty containers.

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

The installed bLua prefix may be moved. Lua is an **external dependency**, not bundled with
bLua: the consumer must independently discover an ABI-compatible Lua installation (and the
same runtime family/version its host uses). Set `LUA_INCLUDE_DIR` / `LUA_LIBRARY` when needed,
especially for LuaJIT or machines with multiple versions. Exported bLua targets reference
`Lua::Lua`, not the producer's Lua paths; `find_dependency(Lua)` resolves them on the consumer.

## Build & Test
Build and run tests using modern CMake:
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build test executable
cmake --build build

# Run unit tests
ctest --test-dir build --output-on-failure

# Debug with test-only ASan/UBSan (GCC/Clang)
cmake -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DBLUA_ENABLE_SANITIZERS=ON
cmake --build build/sanitize
ctest --test-dir build/sanitize --output-on-failure
```

The CTest suite includes integration, regressions, intentional C++/Lua failure-exit checks,
and install → move prefix → separate `find_package` consumer build/run. Checks remain enabled
in Release. CI config covers GCC/Clang × Lua 5.1–5.4, LuaJIT, macOS, Windows, and Linux Debug /
ASan+UBSan. Windows builds pinned official Lua 5.4.7 sources with MSVC; LuaJIT foreign unwinding is preserved.
This describes the workflow configuration, not proof that unpushed CI has run.

---

## Related Projects
* [Lua Family Bucket](https://github.com/esrrhs/lua-family-bucket)
