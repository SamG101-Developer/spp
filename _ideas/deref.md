# Deref

- Allow deref on assignment, if the lhs is a borrow type.
- Example: `*a = 123`, if `a` is a `&mut S32`.
- Need to remove symbolic check on lhs then (see how this works with `meta->assignment_target`).
- This would make code more ergonomic, especially when working with borrow types.