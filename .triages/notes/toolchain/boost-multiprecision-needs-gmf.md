---
name: boost-multiprecision-needs-gmf
description: "TUs doing arithmetic on boost::BigInt/BigDec must include the boost headers in their own global module fragment; `import boost;` alone silently binds to the wrong eval_* overloads"
updated: 2026-07-26
---

`import boost;` (the `headers/wrappers/boost.ixx` wrapper) is enough to **name, hold and pass** `boost::BigInt` /
`boost::BigDec`, but NOT to do arithmetic, conversion or streaming on them. Any TU that computes with them must include
the headers in its own global module fragment:

```cpp
module;
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
module spp.utils.numbers;   // body then compiles and runs correctly
```

**Why:** boost.multiprecision's operators are templates that do ADL *at instantiation time* in the consumer TU for
internal helpers (`eval_left_shift`, `eval_floor`, `eval_ldexp`, `eval_right_shift`, `eval_convert_to`, …). Those are
attached to the global module via the wrapper's GMF and are unreachable from an importing TU, so instantiation either
fails (`no matching function for call to 'eval_floor(cpp_dec_float<100>&, …)'`) or — if you hand-re-export a subset of
the `eval_*` helpers — compiles against a truncated overload set, binds a generic fallback, and returns a number with
garbage internals that crashes later in a copy constructor on malformed data. Diagnosed 2026-07-26 on
`spp::utils::numbers::FloatBounds`, where the inputs still debugged as correct (`two`=2, `x`=1024).

**How to apply:** never grow a list of `using ::boost::multiprecision::backends::eval_*;` in the wrapper — that is the
symptom, not the fix; the overload set cannot be enumerated by hand. Keep `boost.ixx` to the type aliases. Note also
that ADL for these types searches `boost::multiprecision`, not `boost`, so any using-declaration parked in
`export namespace boost { }` is invisible to operator lookup anyway. Same trap applies to any header-only template
library wrapped this way. See [[third-party-module-wrappers]].
