# Compiled Safety

## Compiler Features

- Stack canaries
- Address space layout randomization
- Read-only relocations
- Non-executable areas
- Fortified functions
- Immutable function layouts
    - Randomize function order per build
- Shadow stacks
- Safe/split stacks
    - Separates "safe" objects from "unsafe" ones
    - Unsafe objects include buffers and arrays
- Indirect branch tracking
- Digital signatures
    - Sign binaries
    - Sign packages on github
- Control flow integrity
    - Forward and backward edge protection
    - Backward edge uses shadow stacks
- Pointer authentication
    - Signing pointers cryptographically
- Memory tagging (MTE)
    - Assigns tags to memory allocations/pointers
    - Checks for matching tags
- Compile time bounds checks
    - Arrays and tuples are compile-time bounds checked
    - Vectors are runtime bounds checked
- Const propagation
    - Write all `cmp` data to `.rodata`
- Data flow integrity
    - Variables can only be written from specific sites
- FFI Safety
