# Code Gen

Need 2 stages

## Stage 1

- Load the `llvm_type` attribute on all type symbols during class generation
- Each class probably needs to maintain a list of generic implementations
- Also generate generic implementations' type symbols
- Declare all function prototypes (no definitions yet)
- This includes the list of generic implementations for each function
- Module/sup level constants also need to be declared here (functions may reference them)

## Stage 2

- Generate function definitions

## Materializing

- Need to materialize non-symbolic expressions in the following cases:
    - [x] Argument to a function call
    - [x] `ret` expression
    - [ ] `case` block condition
    - [ ] `loop` block condition
    - [ ] `iter` block condition
    - [x] `gen` expression?
    - [x] Postfix `?` expression lhs

## Assigning to variables

- Possible expressions:
    - Literal => generate llvm value directly
    - Postfix.FnCall =>
  