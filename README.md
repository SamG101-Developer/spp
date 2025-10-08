# Spp Compiler

Translating the S++ compiler from Python to C++ for performance reasons.

## Todo:

- [ ] Remove `ast_cast` -> add `->as<T>()` method instead.
- [ ] Remove `ast_clone` -> use `->clone()->as<T>()` methods instead.
- [ ] Add `->as_shared<T>()` method to AST nodes for `T*` to `std::shared_ptr<T>` casting.

## Notes:

- `ast_cast` doesn't work properly for shared pointers; if one dies then they all die, because I copy the raw pointer
  in, instead of casting it using `dynamic_pointer_cast`.

## Optimizations:

- [ ] Where can `T&` and `T*` be used instead of `std::shared_ptr<T>`?
- [ ] Reduce cloning where possible (move and put back, shared_ptr -- i think this is minimal anyway)
- [ ] Remove move `genex::to<std::vector>` (requires modifications to genex).
- [ ] Remove `orig_name`, use `name` everywhere.
- [ ] Check `auto &&` vs `auto const &` for unique pointer vector iteration.
- [ ] Change `not all_of` to the new `none_of` in genex.

## Profile determined optimizations:

- [ ] Use the `robin_map` for symbol tables instead of `std::map`.
- [ ] Remove most of `genex::to<std::vector>()` where it isn't needed.