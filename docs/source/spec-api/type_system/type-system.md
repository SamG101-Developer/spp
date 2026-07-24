# Type system

S++ uses a static, strong type system. At any point in the program, the compiler knows every variable's type, and no
implicit casting occurs. The type system draws on a range of features to maximize expressiveness and flexibility, while
still providing one way to achieve each task. A brief overview of the features that form the S++ type system:

- **First-class types**: All types in S++ are first-class, so they behave consistently in all forms, including the types
  that other languages treat as primitive: booleans, numbers, void, arrays, tuples and functions. LLVM optimizes them
  down to its own known types, but within S++ every type behaves the same.

- **Type inference**: S++ has powerful type inference. The compiler can fully infer any expression, which makes type
  hints on `let` statements redundant unless the code uses variant types. Type hints remain necessary in three places:
  function parameters and return types, which form the function signature; uninitialized `let` statements, where
  nothing else gives the type; and class attributes.

- **Variant types**: S++ uses variants as first-class union types, with no syntactic wrapping. For example, the STL
  defines the optional type as `type Opt[T] = Some[T] or None`. Type checking matches variant types to their inner
  types, when the inner type sits on the right-hand side.

- **Flow typing**: Pattern destructuring of variant types gives flow typing. For example, destructuring a variable of
  type `Opt[T]` as the `Some[T]` variant flow types the variable to `Some[T]` within the scope of that pattern, and the
  destructure exposes the inner variable directly.

- **Generic types**: Functions, classes, type aliases and superimpositions all accept required, optional and variadic
  generic types. The compiler monomorphizes generics at compile time, and infers generics for function calls and object
  initialisations.

- **Custom types**: A class definition lists the fields of the type, and the uniform default initialisation syntax
  instantiates it. A class has a name, and optionally generics and fields.

- **Superimposition**: The `sup` block implements behaviour on a type, whether that means attaching functions, aliases
  and constants, or extending an existing type. It comes in two variants: `sup MyType { ... }` and
  `sup MyType ext OtherType`. The superimposition section describes them in more detail.

- **Type forwarding**: Lets one type stand in for another without inheritance, in the same way as Rust's `Deref` family
  of types. `Str` can act as `StrView`, for example, and use its methods, while the value never takes the `StrView`
  format.

## "Non-primitive" types

S++ declares the types that other languages treat as primitive as classes with ordinary methods, and the compiler
substitutes raw LLVM for those methods under the direction of the `!compiler_builtin` annotation. The types then behave
consistently with every other type, and still optimize down to the raw LLVM types. S++ has these "non-primitive" types:

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
- `std::array::Arr[T, n, A]` - fixed-size array type, where `T` is the element type and `n` is the number of elements
- `std::tuple::Tup[..Ts]` - tuple type, where `Ts` is a variadic list of element types
