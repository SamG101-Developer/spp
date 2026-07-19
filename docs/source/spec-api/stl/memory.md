# Memory

Within the STL there are types including `Memory`, `Buffer`, `Vec`, `Str`, `StrView`, `Single`, `Ptr`. They all have
specific uses, described below.

## Types

### Memory

The `Memory[T]` type is a low-level representation of a contiguous block of memory. It contains the "head" pointer, as a
`Single[T]`. This is a uniquely owned pointer into the memory block. The `Memory[T]` type also contains the capacity of
the memory block, in bytes, as a `USize`. The `Memory` type does not track the length of initialized elements within the
memory block.

### Buffer

The `Buffer[T]` type is a higher-level abstraction over `Memory[T]`. It tracks both the capacity **and** the length of
initialized elements within the memory block. The `Buffer[T]` type provides methods for safely reading and writing
elements, as well as resizing the buffer.

### Vec

The `Vec[T]` type is a dynamic array that builds upon `Buffer[T]`. It provides additional functionality such as
methods for adding and removing elements, as well as iterating over the elements in the vector. It directly wraps the
`Buffer[T]` type, providing a more user-friendly interface for working with dynamic arrays. Automatic resizing is also
handled when elements are added beyond the current capacity.

### Str

The `Str` type is a UTF-8 encoded string type that represents a sequence of characters. It is built on top of
`Vec[U8]`, providing methods for string manipulation, such as concatenation, substring extraction, and searching. This
is a heap allocated string (via `Vec`).

### StrView

The `StrView` is a static-lifetime immutable string, representing a view into a UTF-8 encoded string. It contains a raw
pointer and a length, and maps directly into a specialized llvm type, for maximum performance. `StrView` is typically
used for string literals or other static strings that do not require heap allocation.

### Single

The `Single[T]` type is a uniquely owned pointer to a single instance of type `T`. It provides methods for safely
accessing and modifying the value it points to. It provided one-hit generators to view the internal value under the
pointer, abiding by the memory rules within S++.

### Ptr

The `Ptr[T]` type is a non-owning pointer to a single instance of type `T`. It is used very rarely, and not exposed to
the user code. It is always non-null, unless being used from an ffi function.

## Str vs StringView

The `StrView` type should be used for _inputting_ values into functions, where the string data does not need to be
modified or owned by the function. This is because `StrView` does not require heap allocation, making it more efficient
for read-only access.

The `Str` type should be used for _outputting_ values from functions, or when the string data needs to be modified or
owned by the function. This is because `Str` is heap allocated, allowing for dynamic resizing and modification of the
string data. Even then, it should rarely be used for parameters, preferring a return value instead.

Typically APIs in the STL will only accept `StrView` for input parameters, and return `Str` types for output values.
