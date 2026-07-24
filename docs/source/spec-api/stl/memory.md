# Memory

The STL includes the types `Memory`, `Buffer`, `Vec`, `Str`, `StrView`, `Single` and `Ptr`. Each has a specific use,
described below.

## Types

### Memory

The `Memory[T]` type is a low-level representation of a contiguous block of memory. It holds the head pointer as a
`Single[T]`, a uniquely owned pointer into the memory block, and the capacity of the memory block in bytes as a `USize`.
The `Memory` type doesn't track the length of initialized elements within the memory block.

### Buffer

The `Buffer[T]` type is a higher-level abstraction over `Memory[T]`. It tracks both the capacity **and** the length of
initialized elements within the memory block, and provides methods that safely read elements, write elements and resize
the buffer.

### Vec

The `Vec[T]` type is a dynamic array built on `Buffer[T]`. It adds methods that append elements, remove elements and
iterate over the vector. It wraps the `Buffer[T]` type directly, giving a friendlier interface for dynamic arrays, and
resizes automatically when elements exceed the current capacity.

### Str

The `Str` type is a UTF-8 encoded string type representing a sequence of characters. It builds on `Vec[U8]`, and
provides methods for string manipulation such as concatenation, substring extraction and searching. `Vec` makes it a
heap allocated string.

### `StrView`

The `StrView` type is a static-lifetime immutable string, representing a view into a UTF-8 encoded string. It holds a
raw pointer and a length, and maps directly onto a specialized LLVM type for peak performance. String literals and
other static strings that need no heap allocation typically use `StrView`.

### Single

The `Single[T]` type is a uniquely owned pointer to a single instance of type `T`. It provides methods that safely
read and update the value it points to, and provides one-hit generators that view the internal value under the
pointer, following the S++ memory rules.

### Ptr

The `Ptr[T]` type is a non-owning pointer to a single instance of type `T`. It appears rarely, and never in user code.
It's always non-null, except when it comes from an FFI function.

## `Str` versus `StrView`

Use the `StrView` type for _inputting_ values into functions, where the function neither modifies nor owns the string
data. `StrView` needs no heap allocation, which makes it more efficient for read-only access.

Use the `Str` type for _outputting_ values from functions, and when the function modifies or owns the string data. `Str`
sits on the heap, which allows dynamic resizing and modification of the string data. Even then, it belongs in a return
value rather than in a parameter.

APIs in the STL typically accept only `StrView` for input parameters, and return `Str` types for output values.
