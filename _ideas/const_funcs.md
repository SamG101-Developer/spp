# Const Upgrades

## Const Functions

Functions will not be "markable" as `cmp`, the standard "compile time" keyword marker. What is going to happen, is that
every function will be assumed to be compile-time executable, in all contexts of that function being called. The
exception is if a function called a definite non-compile-time function. These definite non-compile-time functions
will **only** be ffi function calls, like `io`.

If all the arguments to a function are compile-time values (ie compile time variables or literals or results of
compile-time functions etc), then the function is automatically executed at compile time, and the result is injected
into the ASTs before codegen. Otherwise, standard IR is used to call the function. A variable is only non-compile-time
if is the result of a non-compile-time function, or it is a parameters, because there is no guarantee that the parameter
was a compile-time value or not.

If a parameter should always be `cmp`, use it as a non-type generic parameter, like
`fun[T, cmp n: Bool] () -> T { ... }`.

The numeric addition functions will not have any `cmp` parameters, but can be called either as compile time executable,
like `1 + 2`, or runtime executable, like `a + b`, where `a` and `b` are variables.

## Const Variables

Currently `cmp` can only be used at the module level, superimposition level, or to define generic comp parameters. They
could also be allowed as function statements, like `let`. If this were the case, they could then be used as generic comp
args, and when constant functions are ready, they can be manipulated as arguments to constant functions.

## Const Functions Framework

- Need to change `stage_10_codegen_2` to `stage_11_codegen_2`, and add a new `stage_10_codegen_const`. This stage will
  be used to generate compile time code, in C++, and modify symbol tables, before llvm codegen takes place.
- Not all ASTs will be valid in compile time codegen. For example, it is unlikely that loops and conditionals will not
  be valid, as they require runtime evaluation. Only straight line code with function calls to other compile time
  functions will be valid.
- Conditionals should be valid because the condition will be evaluable at compile time. Because all functions called are
  enforced to be `cmp` too, the branches can be evaluated at compile time.
- Where functions are annotated as `@compiler_builtin`, instead of the special llvm ir that is manually injected, there
  will be C++ that is manually called, with a 1-to-1 mapping with the llvm ir implementations. This will allow things
  like `+` operations to be done at compile time.
- Variable symbols will need a field in-case they are compile time values, so their value can be stored. Need to work
  out _how_ the value will be stored, because it could be any type. Look at `std::any`? We know that type of the
  variable because it's stored on the variable symbol.
- Look at `cmp` generics (non-type generics) vs `cmp` function parameters - don't they become the same thing? If a
  function parameter is `cmp`, then it must be a compile time value, and if a generic parameter is `cmp`, then it must
  also be a compile time value. The only difference is that generic parameters are known at compile time, whereas
  function parameters may not be known until runtime. But if they are `cmp`, they must be known at compile time. So
  maybe `cmp` function parameters should be converted to `cmp` generic parameters at compile time codegen time? On the
  other hand, `cmp` functions _can be called at runtime_, it is just that the code executed will be the llvm ir
  implementation. So I guess we stay with both `cmp` generics and function parameters.
