# Destructuring

- Maybe allow destructuring with borrows, if the bororws are pinned so can not leave the scope. This sould allow for
  array, tuple or object destructuring without partially moving the object.
- Example syntax:

```spp
case my_object of {
    MyClass { &field1, &field2 } { ... }  // Destructure with borrows
    MyClass { field1, field2 } { ... }    // Destructure with moves
    else { ... }
}
```

## Syntax Upgrades

- Alter the single identifier destructure pattern to allow for borrows.
- Make the mutability/convention rules on it follow the `self` parameter.
- Either `var`, `mut var` or `&/&mut var` (ie not `mut &mut var`).
