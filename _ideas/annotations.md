# Annotations

Lots of new annotations needed, primarily for llvm lowering. Also will add the ability to add arguments into some
annotations. This might look like `@repr(C)`

## Structs

- `@packed` - to indicate that a struct should be packed without padding. This will be used in the LLVM backend whilst
  calling `setBody(..., packed=)`.
- `@repr` - arguments include `C`, `transparent`, and some other predefined layouts. **TODO**: Where to apply this.

## Functions

- There is no need for `@noreturn` as this can be inferred from the return type being `!`. It will automatically be
  applied when using the `!` type. For non-`!` functions, the attribute `willreturn` can be used. Type checking will
  prevent `!` from being used incorrectly, meaning the attribute usage will always be correct.
- The `@nounwind` attribute is applied to every function defined in S++. This is because S++ does not have exceptions,
  and therefore functions cannot unwind the stack. There is `std::abort::abort`, like Rust's `panic!`, but this does not
  unwind the stack either, just terminates. As this attribute is an optimization, it is simply applied everywhere, and
  not required to be specified by the user. Single exception: `@ffi` functions - not ran in S++ runtime, so cannot
  assume no unwind. https://chatgpt.com/s/t_694046d9503c81918411d033d9b61a89
- The `readonly`, `writeonly` and `readnone` attributes are very difficult to apply automatically. While stores, loads
  and allocas can be tracked by inspecting the target and questioning whether they are applying to a borrowed type, the
  main issue is calling functions which might be tagged, due to order agnostic behaviour. **This needs more
  investigation.** https://chatgpt.com/s/t_694046ace5348191934260e27ed6399c
    - `@readonly` - indicates that a function does not read or write to memory. So if no store/load/alloca instructions
      are present in the codegen, (after function body analysis), and no functions calls to non-`readonly` functions are
      present, this can be applied. **But it could be the case that a function call target hasn't been analysed
      for `readnone` yet and may get marked as readonly.** Potential solution: Add a `stage_11_optimizer` stage after
      standard codegen. What this does, is forms a graph of all function calls, and can then determine which functions
      are `readonly` by propagating the information through the call graph. This can be done iteratively until a fixed
      point is reached. The actual graph can be formed during `stage_10_code_gen_2`, by `llvm_func` but the analysis
      must wait until completion. Also we need to handle recursion and cycles in the call graph.
    - `@readonly` - indicates that a function only reads from memory, and does not write. Similar analysis to
      `readnone`, but loads from borrowed types are allowed. Function calls to `readonly` and `readnone` functions are
      allowed.
    - `@writeonly` - indicates that a function only writes to memory, and does not read. Similar analysis to
      `readnone`, but stores to borrowed types are allowed. Function calls to `writeonly` and `readnone` functions are
      allowed.
- Unsure about `mustprogress` - **needs more research**.
- The inline attributes `alwaysinline` and `noinline` can be applied by the user, but no automatic application is done.
  A lot of small functions in the STL are marked `alwaysinline` manually.
- The `argmemonly` can be applied to functions which only read/write memory through their arguments. This can be
  determined by checking for loads/stores/allocas which do not target arguments. If none are found, this attribute can
  be applied. Function calls to non-`argmemonly` functions will prevent this attribute from being applied. Requires
  graph analysis similar to `readnone`/`readonly`/`writeonly`.
- The `inaccessiblememonly` can be applied to functions which only read/write memory internal to the function - no
  globals or arguments. This can be determined by checking for loads/stores/allocas which target globals or arguments.
  If none are found, this attribute can be applied. Function calls to non-`inaccessiblememonly` functions will prevent
  this attribute from being applied. Requires graph analysis similar to `readnone`/`readonly`/`writeonly`.
- The `nonnull` attribute is always applied, because S++ borrows cannot be null. This applies to both immutable and
  mutable borrows.
- The `noundef` attribute is always applied, because S++ does not have undefined values. Every value must be
  initialized before use.
- The `nocallback, nosync` attributes: idrk, needs more research.

## Parameters

- Nullability and undefiniteness are handled at the parameter level _as well as_ at the function level:
    - A immutable borrow parameter: `&T`: `nonnull dereferenceable(size_of(T)) readonly nocapture`
    - A mutable borrow parameter: `&mut T`: `nonnull dereferenceable(size_of(T)) noalias nocapture`
    - An owned parameter: `T`: `noundef`
- For `readonly/writeonly`: track through function call, including aliasing. If a parameter is only read from, it can be
  marked `readonly`. If only written to, it can be marked `writeonly`. If neither, it can be marked `readnone`. This
  requires whole program analysis, similar to function level `readonly/writeonly/readnone` analysis.
- For `alianas(N)`: idrk, needs more research.
- The `byval` attribute is applied to non-borrowed non-primitive types (ie owned structs). This indicates that the
  parameter is passed by value, can't fit in a register, and allows LLVM to make optimizations based on this knowledge.
- The `inreg` attribute is applied to primitive owned types (ints, floats, bools, etc). This indicates that the
  parameter is passed in a register, allowing LLVM to optimize based on this knowledge.