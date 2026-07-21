# Arrays

S++ has an array type, which maps down to low level array intrinsics. Arrays are fixed size, have 2 different literals
that can be used (inspired by Rust), and a typ shorthand too.

## Explicit Element Literal

For explicit elements, the normal array literal syntax can be used like:

- `[1, 2, 3]` creates an array of 3 integers with the values 1, 2, and 3.
- `["a", "b", "c"]` creates an array of 3 strings views with the values "a", "b", and "c".

## Repeated Element Literal

For a single element that is repeated, the syntax is `[value; count]`, where `value` is the element to be repeated and
`count` is the number of times it should be repeated. For example:

- `[0; 5]` creates an array of 5 integers, all initialized to 0.
- `["x"; 3]` creates an array of 3 string views, all initialized to "x".

The size must be `cmp` known, meaning it is known or calculable at compile time. This means that the size must be a
constant expression, such as a literal or a const variable.

## Type Shorthand

S++ also has a shorthand for array types, which is `[T; n]`, where `T` is the type of the elements and `n` is the number
of elements in the array. For example:

- `[S32; 4]` is the type of an array of 4 integers.
- `[StrView; 2]` is the type of an array of 2 string views.
