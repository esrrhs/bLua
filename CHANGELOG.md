# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Safety / contract changes after PR #2
- Class registration data and copied type names are now state-local. `set_type_name<T>` /
  `get_type_name<T>` require `lua_State*`; captured userdata deleters no longer depend on mutable globals.
- Identical registrations are idempotent; conflicting registrations/names, explicit ownership
  changes and same-address/different-type wrappers are rejected. Implicit pointer returns reuse
  the existing wrapper policy. GC validates the type, invalidates before deletion, is idempotent,
  and methods/properties reject destroyed wrappers. Constructors always own their allocation.
- Strict scalar/container validation replaces silent coercion and missing-result defaults.
  Numeric `map<string,V>` keys are rejected; iteration keeps the original key intact.
  Generic nested vector/map conversions and const data-member getters are supported.
- C++ exceptions are translated at callback boundaries after unwinding. C++→Lua calls protect
  lookup/execution/conversion, work without debug/traceback, restore the stack and convert
  results before updating outputs. Lua OOM and hostile host longjmp/registry tampering remain
  outside the recoverable-error contract; borrowed pointers still require external lifetime care.
- Duplicate-arity overloads fail before replacing an existing binding (dispatch remains arity-only).
- Release checks no longer disappear under NDEBUG. Added independent state/GC/error/container
  regressions, deliberate C++/Lua failure-exit tests, Debug/ASan/UBSan configuration, and a
  moved-prefix installed `find_package` consumer test.
- Private Lua compatibility helpers avoid void-return getter assumptions in Lua 5.2 and avoid
  redefining public Lua macros. Linux CI explicitly selects Lua 5.1–5.4 plus existing LuaJIT jobs;
  macOS/Windows jobs are retained. Workflow changes are not claims of completed remote CI.
- Installed exports use a consumer-resolved `Lua::Lua` target, not producer Lua paths. Lua remains
  an external ABI-compatible dependency; moving bLua does not bundle or relocate Lua itself.

### Added
- Configurable object ownership: `ownership::owning` / `ownership::non_owning`, `reg_class_non_owning`, `reg_class_with_deleter`, and `push(L, ptr, ownership)` so non-Lua-owned pointers are not deleted by `__gc`.
- Stable type names: pass a name to `reg_class` / `set_type_name(L, ...)`, with demangled `typeid` fallback (GCC/Clang via `__cxa_demangle`).
- Container helpers: `std::vector` / `std::map` ↔ Lua table via `push_vector` / `to_vector` / `push_map` / `to_map` and matching `native_to_lua` / `lua_to_native` specializations for common element types.
- API DX: `reg_constructor`, `reg_property` (data member or getter/setter), `reg_enum`, and `reg_global_overload` (dispatch by argument count).
- Stronger tests: type errors, GC/ownership, custom deleter, nullptr string, multi-return edge cases, containers, ctor/property/enum/overload.
- CI: Windows (vcpkg Lua) and Linux LuaJIT jobs; existing Linux (GCC/Clang × Lua 5.1–5.4) and macOS jobs retained.
- Docs: Lua version compatibility matrix in README / README_CN.

### Fixed
- `native_to_lua(const char*)` / `char*`: push `nil` explicitly when the pointer is null (`lua_pushstring` also accepts null; this makes the contract explicit).
- Removed unused `<iostream>` include from `bLua.h`.

### Notes for maintainers
- Tagging / GitHub Release for this work still needs explicit approval; this file is prepared for a future `v1.1.0` (or similar) cut.

## [1.0.0] - prior

- Header-only C++17 Lua bridge with class/global binding, CMake package export, and basic CI.
