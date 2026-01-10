# Destructuring

- Maybe allow destructuring with borrows, if the bororws are pinned so can not leave the scope. This sould allow for
  array, tuple or object destructuring without partially moving the object.
- Example syntax:

```s++
fun f() -> Void {
  case my_object of {
      is MyClass (&field1, &field2) { }  # Destructure with borrows
      is MyClass (field1, field2) { }    # Destructure with moves
      else { }
    }
}
```

## Syntax Upgrades

- Alter the single identifier destructure pattern to allow for borrows.
- Make the mutability/convention rules on it follow the `self` parameter.
- Either `var`, `mut var` or `&/&mut var` (ie not `mut &mut var`).

## Implementation Steps

- Ammend the parser. It needs to be able to parse `LocalVariableSingleIdentifierAst` to have both `kw_mut` and `conv`.
  Look at the `FunctionParameterSelfAst` to see how it has successfully been implemented there.
- Next, in `LocalVariableSingleIdentifierAst`, ammend the local variable conversion; produce the cooresponding
  `LocalVariableSingleIdentifier`, without a convention even if present in the AST. **BUT**, snapshot the memory
  information of the outer symbol first.
- After running the semantic analysis on the new `LetStatementInitializedAst`, (containing the translated local
  variable), restore the memory information of the outer symbol. This is done on a per-variable basis, not before and
  after all destructures of the outer variable, because some attributes might still be "moved" in the destructure.
- Follow the steps seen in `FunctionParameterSelfAst` to ensure the convention is applied (as a borrow), in the inner
  symbol's memory information.
- Assignment scope lifetime check will prevent these borrows being propagated outside of the destructure scope.
