# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Configurable object ownership: `ownership::owning` / `ownership::non_owning`, `reg_class_non_owning`, `reg_class_with_deleter`, and `push(L, ptr, ownership)` so non-Lua-owned pointers are not deleted by `__gc`.
- Stable type names: pass a name to `reg_class` / `set_type_name`, with demangled `typeid` fallback (GCC/Clang via `__cxa_demangle`).
- Container helpers: `std::vector` / `std::map` ↔ Lua table via `push_vector` / `to_vector` / `push_map` / `to_map` and matching `native_to_lua` / `lua_to_native` specializations for common element types.
- API DX: `reg_constructor`, `reg_property` (data member or getter/setter), `reg_enum`, and `reg_global_overload` (dispatch by argument count).
- Stronger tests: type errors, GC/ownership, custom deleter, nullptr string, multi-return edge cases, containers, ctor/property/enum/overload.
- CI: Windows (vcpkg Lua) and Linux LuaJIT jobs; existing Linux (GCC/Clang × Lua 5.3/5.4) and macOS jobs retained.
- Docs: Lua version compatibility matrix in README / README_CN.

### Fixed
- `native_to_lua(const char*)` / `char*`: push `nil` when the pointer is null (was undefined via `lua_pushstring`).
- Removed unused `<iostream>` include from `bLua.h`.

### Notes for maintainers
- Tagging / GitHub Release for this work still needs explicit approval; this file is prepared for a future `v1.1.0` (or similar) cut.

## [1.0.0] - prior

- Header-only C++17 Lua bridge with class/global binding, CMake package export, and basic CI.
