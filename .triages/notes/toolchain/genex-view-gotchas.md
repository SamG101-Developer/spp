---
name: genex-view-gotchas
description: "Recurring genex (GenEx lib) view/pipeline compile pitfalls on the spp compiler and how to fix them"
---

The `spp` compiler uses the GenEx range library (installed at `/usr/local/lib/cmake/GenEx/modules/genex/`, its own `genex::` concepts, NOT `std::ranges`). Several genex limitations cause hard compile errors when migrating pipelines; each has a standard fix:

- **Multi-range views (`genex::views::zip`, `genex::views::concat`) don't satisfy `sentinel_for`** (their sentinel isn't weakly-eq-comparable with their iterator). Piping them into `transform`/`filter`/`all_of`/etc. fails. **Fix:** materialize first — `genex::views::concat(a, b) | genex::to<Vec>() | ...`. Already-correct sites: `expr_utils.cpp:73`, `fn_values.cpp:329`.

- **`iota | transform | to<Vec>` breaks**: `to<Vec>` probes `has_member_size`, instantiating `transform_view::size()` → `distance()` on `iota_iterator`, which advertises random-access but has no working `operator-`. **Fix:** materialize the iota first — `genex::views::iota(a,b) | genex::to<Vec>() | genex::views::transform(...) | genex::to<Vec>()`. Note `iota | to<Vec>` alone is fine (iota_view::size uses integer bounds).

- **Can't chain `transform`/`cast_unique` onto a *capturing-lambda* `transform_view`**: genex requires the source iterator to be `semiregular` (default-constructible), but a boxed capturing lambda isn't. **Fix:** collapse into one transform, or materialize to `Vec` between stages. For pure upcasts (`Unique<Derived>` → `Unique<Base>`) just make the transform return `Unique<Base>` directly (unique_ptr converting move) instead of using `spp::views::cast_unique`.

`spp::views::cast_unique<To>()` ([[algorithms.ixx]]) does a `dynamic_cast` filter over a range of `Unique<X>`; it constrains on `genex::range_value_t<Rng>::element_type` and only works when the source range's iterators are semiregular (e.g. `Vec | genex::views::move`), not after a capturing-lambda transform.

`Unique<T>` = `std::unique_ptr<T>` (headers/spp/utils/types.ixx); a custom `Unique` with capital `.Release()` is commented out — don't write `.Release()`, use `.release()`.
