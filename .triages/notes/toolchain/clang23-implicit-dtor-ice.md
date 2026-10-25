---
name: clang23-implicit-dtor-ice
description: "clang 23.1 segfaults (GetVTTParameter) emitting an imported aggregate's implicit destructor; define special members out of line"
updated: 2026-10-14
---

clang 23.1.2 segfaulted compiling symbols.cpp while "Generating code for ... PlaceAt::~PlaceAt" (stack: destroyCXXObject -> GetVTTParameter -> castFromDeclContext). PlaceAt (regions.ixx) is a plain aggregate with an Opt<Place>; Place had just gained a defaulted operator<=>. GCC was fine. Fixed 2026-10-14 by declaring all special members of PlaceAt and defining them `= default` in regions.cpp.

**Why:** a clang modules merge bug; importers emitting the implicit dtor crash, the owning TU emitting it does not.

**How to apply:** same stack on another SPP_EXP_CLS type → same fix. Definitions must sit inside SPP_MOD_BEGIN/END ([[spp-mod-begin-extern-cpp-linkage]]), or both compilers reject them as a redeclaration in the named module. Declare all five plus ctors, else moves silently become copies. The old <release-clang build dir> tree can't link; see `local-clang-coverage-build` for the fixes (C flags, PIC gtest) that make a clang tree link.
