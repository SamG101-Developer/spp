# Enums

Enums would be very basic data structures that allow you to define a set of named values. They are useful for
representing a fixed set of related constants in a more readable and maintainable way. They would have names and values,
but no additional functionality. Sum types come from the `Var` variant type class, where there is no type wrapping:
`type Opt[T] = Some[T] or None`.

Enums would allow for better type safety and code clarity when dealing with a limited set of options. They would be
defined using a simple syntax, and could be used in various contexts such as function parameters, return types,
andvariable declarations.

## Example Syntax

```s++
enum Color {
    red,
    green,
    blue
}
```
