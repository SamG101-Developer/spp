# Spp Compiler

Translating the S++ compiler from Python to C++ for performance reasons.

## Todo:

- [ ] Remove `ast_cast` -> use `dynamic_cast` and `std::dynamic_pointer_cast` instead.
- [ ] Remove `ast_clone` -> use `->clone()` method instead.
- [ ] Add `->as<T>()` method to AST nodes for easier casting.
- [ ] Add `->as_shared()` method to AST nodes for `T*` to `std::shared_ptr<T>` casting.