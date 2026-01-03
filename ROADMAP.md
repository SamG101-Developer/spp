# ROADMAP

## Critical

## Definitely Planned

- Annotations that can accept arguments.
    - `@repr(C)`, `@align(8)`, etc.
    - Will need to work out analysis: arg count + literal/identifier value checking.
    - Seems very static; this annotation must have this type parameter etc.
    - Can we define an annotation system that allows this to be extensible in the future?
    - Like `annotation align(n: S32) { ... }`?
- New annotations that map to LLVM attributes or arguments.
    - `@packed`: Indicates that a struct should be packed without padding.
    - `@repr`: Accepts arguments like `C`, `transparent`, etc., to define struct layout.


- Destructuring with borrows (from [**HERE**](./_ideas/destructuring.md)).
    - Need to implement "loose pins" in memory analysis.
    - Update destructuring syntax and semantics to support borrows.
    - Ensure safety and correctness in borrow destructuring scenarios.


- Const Upgrades (from [**HERE**](./_ideas/const.md)).
    - Implement compile-time function execution.
    - Extend `cmp` support to function statements and variables.
    - Create a framework for compile-time code generation.
    - Ensure seamless integration with existing runtime code.


- The `in` Expression (from [**HERE**](./_ideas/in_expr.md)).
    - Implement the `std::ops::contains::Contains` operator class.
    - Update syntax to support `in` expressions in various contexts.
    - Rename looping constructs to avoid conflicts with `in`.
    - Ensure proper semantic analysis for `in` expressions.
    - Embed `in` into the pattern matching system.

## Considered

## Long Term Ideas

- Potentially remove `cmp` values entirely, and replace with standard expressions.
    - Then semantic analysis can determine if the expression is a valid `cmp` value.
    - This allows `cmp` functions to be called and used as `cmp` values.

## Bugs

- The `cmp` expression doesn't work with `cmp` values on types (in `sup` blocks).
    - Example: `func[cmp n: USize = Struct::inner_val] foo() { ... }` gives an error.
    - Need to extend `cmp` to include limited postfix expressions like field access.
