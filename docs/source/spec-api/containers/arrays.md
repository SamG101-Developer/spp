# Arrays

S++ has an array type, which maps down to low level array intrinsics. Arrays have a fixed size, two different literals,
inspired by Rust, and a type shorthand.

## Explicit element literal

The normal array literal syntax spells out each element:

- `[1, 2, 3]` creates an array of 3 integers with the values 1, 2, and 3.
- `["a", "b", "c"]` creates an array of 3 string views with the values `"a"`, `"b"`, and `"c"`.

## Repeated element literal

For a single repeated element, the syntax is `[value; count]`, where `value` gives the element and `count` gives the
number of copies. For example:

- `[0; 5]` creates an array of 5 integers, all initialized to 0.
- `["x"; 3]` creates an array of 3 string views, all initialized to `"x"`.

The size must be `cmp` known, meaning the compiler knows or can calculate it at compile time. This means the size must
be a constant expression, such as a literal or a const variable.

## Type shorthand

S++ also has a shorthand for array types, `[T; n]`, where `T` names the element type and `n` gives the number of
elements in the array. For example:

- `[S32; 4]` denotes an array of 4 integers.
- `[StrView; 2]` denotes an array of 2 string views.
