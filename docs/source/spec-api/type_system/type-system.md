# Type System

S++ uses a static, strong type system. At any point in the program, the type of every variable is known, and no implicit
casting can occur. S++ uses a range of features within its type system to maximize expressiveness and flexibility, whilst
still only providing one way to achieve each task. The following is a brief overview of each of the features that forms
the S++ type system:

- **First-class types**: All types in S++ are first-class. This means that their behaviour is consistent in all forms,
  including typically primitive types like booleans, numbers, void, arrays, tuples and functions. LLVM optimizes them
  down to the specially known types, but for all purposes within S++, all types behave the same.


- **Type inference**: S++ has incredibly powerful type inference; any expression can be fully type inferred, such that
  type hints on `let` statements are completely useless, unless variant types are being used. Type hints are only
  required for function parameters / return type (for form function signatures), uninitialized `let` statements (what
  type will the value be?) and class attributes.


- **Variant types**: S++ uses variants as first-class union types, using no syntactic wrapping. For example, the STL
  defined the optional type as `type Opt[T] = Some[T] or None`. Type checking will match variant types to their inner
  types (when the inner type is on the right-hand-side).


- **Flow typing**: Pattern destructuring of variant types allows for flow typing. For example, if a variable of type
  `Opt[T]` is destructured as the `Some[T]` variant, then within the scope of that pattern, the variable will be flow
  typed to `Some[T]`, with the inner variable directly accessible via the destructure.


- **Generic types**: Required, optional and variadic generic types are supported for function, classes, type-aliases and
  superimpositions. Generics are monomorphized at compile time. Generic inference for function calls and object
  initializations are also supported.


- **Custom types**: Classes can be defined, listing the fields of the type. The type is instantiated using the uniform
  default initialization syntax. Classes have a name, optionally generics, and optionally fields.


- **Superimposition**: To implement behaviour on a type, whether this is attaching functions/aliases/constant, or
  extending another existing type, the `sup` block is used. There are 2 variants: `sup MyType { ... }` and
  `sup MyType ext OtherType`. The superimposition section describes these in more detail.


- **Type Forwarding**: Allow for a type to be interpreted as another type without using inheritance, in the same way
  that Rust's "Deref" family of types work. This allows `Str` to be interpreted as `StrView` for example, using the
  method without ever actually being stored in the `StrView` format.

## "Non-Primitive" Types

The typically primitive types in programming languages are declared as classes with standard method, which the compiler
substitutes raw llvm in for, directed by the `!compiler_builtin` annotation. This allows for the types to be used in a
consistent way with all other types, whilst still being optimized down to the raw llvm types. The following types are
the "non-primitive" types in S++:

- `std::boolean::Bool` - standard boolean type, with values `true` and `false`.
- `std::number::U8` - unsigned 8-bit integer type
- `std::number::S8` - signed 8-bit integer type
- `std::number::F8` - 8-bit floating point type
- `std::number::U16` - unsigned 16-bit integer type
- `std::number::S16` - signed 16-bit integer type
- `std::number::F16` - 16-bit floating point type
- `std::number::U32` - unsigned 32-bit integer type
- `std::number::S32` - signed 32-bit integer type
- `std::number::F32` - 32-bit floating point type
- `std::number::U64` - unsigned 64-bit integer type
- `std::number::S64` - signed 64-bit integer type
- `std::number::F64` - 64-bit floating point type
- `std::number::U128` - unsigned 128-bit integer type
- `std::number::S128` - signed 128-bit integer type
- `std::number::F128` - 128-bit floating point type
- `std::number::U256` - unsigned 256-bit integer type
- `std::number::S256` - signed 256-bit integer type
- `std::number::USize` - unsigned integer type with the same number of bits as the target platform's pointer size
- `std::number::SSize` - signed integer type with the same number of bits as the target platform's pointer size
- `std::array::Arr[T, n, A]` - fixed-size array type, where `T` is the element type and `N` is the number of elements
- `std::tuple::Tup[..Ts]` - tuple type, where `Ts` is a variadic list of element types
