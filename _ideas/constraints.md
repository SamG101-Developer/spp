# Generic Constraits

Where generics are used:

- `ClassPrototypeAst`
- `FunctionPrototypeAst`
- `SupPrototypeFunctionsAst`
- `SupPrototypeExtensionAst`
- `TypeStatementAst`

## Usage: Class Prototype

- Check that the generic constraints are satisfied when instantiating a class with specific type arguments.
- **Handler**: `type_utils::infer_generics`

## Usage: Function Prototype

- Check that the generic constraints are satisfied when calling a function with specific type arguments.
- **Handler**: `type_utils::infer_generics`

## Usage: Type Statement

- Check that the generic constraints are satisfied when using a type alias with specific type arguments.
- **Handler**: `ClsPrototypeAst` (copy constraints over to the type alias definition?)

## Usage: Sup Prototype Functions

- Only apply the super scopes to the type if the constraints are satisfied
- **Handler**: `type_utils::symbolic_eq`
