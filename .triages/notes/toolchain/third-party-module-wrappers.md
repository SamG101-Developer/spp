---
name: third-party-module-wrappers
description: "spp wraps third-party libs as C++ modules in headers/wrappers/*.ixx; wrapper module names must not collide with the library's own module"
---

`spp` consumes header-only third-party libs as C++20 modules via thin wrappers in `headers/wrappers/*.ixx`: each does `module; #include <lib/header>; export module NAME; export namespace ... { using ...; }`. Examples: `ankerl.ixx` (`export module ankerl;`, wraps `unordered_dense.h`), `tomlpp.ixx` (`export module tomlpp;`), `cli11.ixx` (`export module cli11;`).

**Convention/gotcha:** the wrapper module name must be DISTINCT from any module the upstream library itself ships. `ankerl`/`tomlpp` were chosen to differ from upstream `unordered_dense`/`tomlplusplus`. `cli11.ixx` reused the name `cli11`, which is exactly CLI11's own module name — so when CMake also built+linked CLI11's module (`CLI11_MODULES ON` + link `CLI11_Module`), the link failed with `multiple definition of 'initializer for module cli11'` (two primary interface units for one module).

**Resolution (2026-07-14, wrapper-only approach):** the wrappers ARE the modules, so don't also build/link the upstream module libs. In CMakeLists.txt: removed `set(ANKERL_ENABLE_MODULES/TOMLPLUSPLUS_BUILD_MODULES/CLI11_MODULES ON)`, and link the plain namespaced targets instead of the `_module` ones: `unordered_dense::unordered_dense`, `tomlplusplus::tomlplusplus`, `magic_enum::magic_enum`, `CLI11::CLI11` (dropped `unordered_dense_module`, `tomlplusplus_modules`, `CLI11_Module`). The bare names `unordered_dense`/`magic_enum`/etc. had also been failing as `-lX not found` because they aren't real imported target names.

Related: [[genex-view-gotchas]] (the other big class of build breakage on the optimize-all branch, genex-side).
