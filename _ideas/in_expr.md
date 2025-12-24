# The `in` Expression

The `in` expression allows checking for membership within a collection, such as arrays, lists, or sets. It provides a
concise and readable way to determine if a specific element exists within a given collection. Create an operator class
called `std::ops::contains::Contains`, defined as:

```spp
cls Contains[T] {
    @abstract_method
    fun contains(&self, item: &T) -> Bool { }
}
```

This will open up the `in` syntax in non-looping contexts.

## Syntax

### Standard `in` Expression

```spp
element in collection
```

### Pattern Matching with `in`

```spp
case my_expression of {
    in some_collection { ... }
    in another_collection { ... }
}
```

### Renaming Constructs

Current `loop` allows for either `loop condition` or `loop iterator in iterable`. Now, there is a conflict, because
`loop iterator in iterable` may refer either to looping a value through the iterator, or looping whilst the value exists
in the collection. Offloading the change to semantic analysis is poor design. Therefore, we will rename the looping
construct to `for` and `while`, allowing for:

- `for iterator in iterable { ... }` for iterating over collections.
- `while condition { ... }` for looping based on a condition.
- `while value in collection { ... }` for looping while a value exists in a collection.

New idea:

- `loop` for boolean looping
- `loop each` for iterator looping
