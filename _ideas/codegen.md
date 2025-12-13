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