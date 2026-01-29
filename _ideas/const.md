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
- Doing structs as for `cmp` context might be a bit weird - start initial support for non-struct types ie
  numbers/bools/strings etc, disallowing postfix operator access primarily because it is more complex to implement.

## Cmp constants

- Need to extend to allow `::` postfix access for `cmp` values, so we can do things like
  `fun f(a: U8 = Type::constant) -> Void`

## NEW

The idea of `cmp` variables is that they can be used inside functions, such that these values can be computed at compile
time and not waste runtime resources. That being said, there will, at least initially, be limitations on what can be
done: both he operations that can be done as `cmp`, and where these `cmp` values can be present. These are all the
palces that `cmp` variables need to be known at compile time.

- Comptime declaration statements: `cmp x: s32 = 1`
- Comptime generic parameter default value: `fun f[cmp x: S32 = 1]() { ... }`
- Comptime generic argument value: `f[1]() { ... }`
- Function parameter default value: `fun f(x: S32 = 1) { ... }`
- Class attribute default value: `cls C { x: S32 = 1 }`

For resolution, we build a "tree" of the comptime linking. This enables 2 key things:

- Are there cycles in the comptime resolution; do we have `cmp x: S32 = y + 1` and `cmp y: S32 = x + 1`? This involved
  deeper analysis, such as `cmp x: S32 = f()`, and the function `f` uses the variable `x` too.
- Substituting the value into the variable. This needs to be done, ultimately, before the final codegen stage, so that
  any uses of the variable can be replaced with the literal value.

When do we do the substitution. Initially, this will be restricted. What this means, is that we won't be able to use
them in types being used for superimposition, or function parameters etc, just like with "postfix" types, due to order
of resolution. This is okay for now, and a special error will be raised, likely referenceing "not yet implemented".
  