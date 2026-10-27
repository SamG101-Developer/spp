---
name: import-utils-macro-convention
description: "AST method bodies use IMPORT_UTILS / IMPORT_UTILS_AND_UID and call util functions as module::Function; errors unqualified; the TU must import at least one errors and one utils module"
---

Since 2026-09-28, AST methods (`sources/spp/asts/**`) do not declare per-name `using analyse::utils::m::F;` or
`using analyse::errors::E;`. Each body opens with `IMPORT_UTILS;` (from `spp/analyse/macros.hpp`, which must be
included in the GMF). A body that calls `Uid()` bare opens with `IMPORT_UTILS_AND_UID;` instead.

Inside the body:
- Util functions are always called module-qualified: `type_compare::TypeEq(..)`, never bare `TypeEq(..)` and never
  fully qualified `analyse::utils::type_compare::TypeEq`.
- Error classes are unqualified: `Raise<SppTypeMismatchError>`.
- A util *type* may still be given a `using memory_state::ScopeSnapshot;` placed after the macro.

**Why:** the maintainer wants the util area visible at every call site. "It is clearer about the area being affected."

**How to apply:**
- The macro is a pair of using-directives, so the TU must import at least one `spp.analyse.errors.*` module and one
  `spp.analyse.utils.*` module. Otherwise you get "'errors'/'utils' is not a namespace-name". Add
  `import spp.analyse.errors.semantic_error;` where only utils are used.
- A body that only raises errors, in a file with no utils import, uses `using namespace analyse::errors;`. There were
  4 such functions at the time.
- Don't pre-declare `namespace spp::analyse::utils {}` in the macro header. It shadows `spp::utils` for code inside
  `spp::analyse` (semantic_error_builder's `utils::errors::AbstractErrorBuilder` broke).
- Constructors and function signatures keep full qualification: the directive is only in scope inside a body.
