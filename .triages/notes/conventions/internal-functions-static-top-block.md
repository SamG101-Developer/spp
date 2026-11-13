---
name: internal-functions-static-top-block
description: "A .cpp's internal helpers are static declarations in one named-namespace block at the top, defined below at the margin as ns::Name; types alone take an anonymous namespace"
updated: 2026-11-12
---

A `.cpp` file's internal helpers do not sit in an anonymous namespace. Each file has **one** block of its own namespace
(the namespace its public definitions use, for example `spp::asts` for an AST file) at the top, below the imports and
any file-scope constants or macros, laid out like this:

```cpp
namespace spp::analyse::utils::pattern_solver {
  using X = ...;                  // aliases and namespace reductions: in the named namespace
  namespace {                     // types only
    struct Pat { ... };
  }
  using Row = Vec<Pat>;           // an alias of an internal type goes after that type
  static constexpr auto kN = 4;   // static constexpr where possible, else static const; mutable state plain static
  /// doc
  static auto Helper(...) -> T;   // the docstring stays on the declaration
}

/// [CHECKED]
auto spp::analyse::utils::pattern_solver::Helper(...) -> T { ... }
```

- Definitions follow in declaration order, at the left margin, fully qualified, without default arguments (those sit
  on the declaration only).
- `[CHECKED]` markers go above the definition.
- Both the block and the helper definitions sit **above** `SPP_MOD_BEGIN`, which wraps only the definitions of
  `SPP_EXP_CLS` entities (class methods, `extern "C++"` exported functions) that need global-module attachment. The
  helpers have internal linkage and need none. Keep a helper's declaration and definition on the same side, so they
  are not attached to two different modules. See [[spp-mod-begin-extern-cpp-linkage]].
  Outside the block, MSVC needs every AST type a helper uses imported directly
  ([[msvc-needs-member-type-imported]]).
- `static` goes after any leading attribute macro (`SPP_ATTR_HOT static auto`); the other order does not compile.
- Order inside the block is by dependency. A constant or type that names a helper (`kBinaryOps` taking
  `&ShiftRightOfType`, a class whose inline methods call one) goes after the function declarations, in a second
  `namespace { }` for types. A constant that a type names goes above the types.
- Tests (`tests/compiler/`) keep plain anonymous namespaces. They all link into one binary, and names such as
  `ScratchDir` repeat across files.

**Why:** the maintainer's style (2026-11-12). It keeps every definition at the margin, not nested, and puts a file's
private surface in one place at the top. Types stay anonymous because a type cannot be `static`, and outside an
anonymous namespace it would get module linkage (external in the plain pass TUs), which can clash.

**How to apply:** write new helpers this way. 94 files were converted by script on 2026-11-12. A file that still
nests helpers in `namespace { }`, or has a second namespace block of helpers further down, is unconverted.
