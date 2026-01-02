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

## Memory

Introduce the idea of "loose pins".

- When a borrow is made, the scope is also saved in the memory information of that symbol.
- When an initialization or move is made, the scope is also saved.
- A pinned object cannot be moved in any way, such as borrows being moved into a coroutine call.
- But a loose pin can only be moved so long as the initialization scope of the target variable is of a greater or equal
  scope depth to the current scope (the borrow scope). Then, we loosely pin the target with the borrow scope.
- This allows for destructuring with borrows, as long as the borrows cannot leave the scope of the destructuring.

