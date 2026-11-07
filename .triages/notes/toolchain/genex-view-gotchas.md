---
name: genex-view-gotchas
description: "Recurring genex (GenEx lib) view/pipeline compile pitfalls on the spp compiler and how to fix them"
updated: 2026-11-01
---

The `spp` compiler uses the GenEx range library (installed at `/usr/local/lib/cmake/GenEx/modules/genex/`, its own `genex::` concepts, NOT `std::ranges`). Several genex limitations cause hard compile errors when migrating pipelines; each has a standard fix:

- **Multi-range views (`genex::views::zip`, `genex::views::concat`) don't satisfy `sentinel_for`** (their sentinel isn't weakly-eq-comparable with their iterator). Piping them into `transform`/`filter`/`all_of`/etc. fails. **Fix:** materialize first — `genex::views::concat(a, b) | genex::to<Vec>() | ...`. Already-correct sites: `expr_utils.cpp:73`, `fn_values.cpp:329`.

- **`iota | transform | to<Vec>` breaks**: `to<Vec>` probes `has_member_size`, instantiating `transform_view::size()` → `distance()` on `iota_iterator`, which advertises random-access but has no working `operator-`. **Fix:** materialize the iota first — `genex::views::iota(a,b) | genex::to<Vec>() | genex::views::transform(...) | genex::to<Vec>()`. Note `iota | to<Vec>` alone is fine (iota_view::size uses integer bounds).

- **Can't chain `transform` onto a *capturing-lambda* `transform_view`**: genex requires the source iterator to be `semiregular` (default-constructible), but a boxed capturing lambda isn't. **Fix:** collapse into one transform, or materialize to `Vec` between stages. For pure upcasts (`Unique<Derived>` → `Unique<Base>`) just make the transform return `Unique<Base>` directly (unique_ptr converting move) instead of adding a cast stage.

- **Folds hand the op `*first`, so over a `Vec<Unique<X>>` a lambda taking `Unique<X> &&` fails the `invocable` constraint**, and `fold_right` rejects `views::move` because `move_iterator` isn't forward. **Fix:** `v | genex::views::move` with `fold_left`, or `v | genex::views::move_reverse` for a reverse fold.

- **`join_with` needs a non-const range of containers** (GenEx `b5c0fc1`): its `inner_it` is `iterator_t<iter_value_t<I>>`, which drops `const`, so a `const Vec<Str>`, or a temporary `Vec<Str>` piped in (`owning_view::begin` runs through `const`), fails with "no match for `operator=`" inside `join_with.cpp`. **Fix:** hold the `Vec` in a non-const named local, then pipe that (`llvm_mangle.cpp`). The real fix is `iter_reference_t<I>` in GenEx.

- **Since `b5c0fc1`, `views::cast_dynamic<T*>` and `actions::pop_front` are closure objects**: write them without `()`. `cast_dynamic<T*>()` is "no match for call", and the variables it fed then report `position` and "expected primary-expression" errors further down.

`Unique<T>` = `std::unique_ptr<T>` (headers/spp/utils/types.ixx); a custom `Unique` with capital `.Release()` is commented out — don't write `.Release()`, use `.release()`.
