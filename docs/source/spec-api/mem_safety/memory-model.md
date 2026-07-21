# Memory Model

## General Model

S++ uses a "partially automatic memory management" model. The programmer never manually allocates or deallocates memory,
as "scope-bound resource management" controls the lifetime of all values, by tying the lifetime of a value to the scope
of its definition. There are ways to propagate a value out of the scope it was created in, but if the variable is
initialized at the end of the scope, it will be dropped. Simply put:

- **Allocation**: Uniform object initialization (internal constructor called).
- **Deallocation**: Scope-bound resource management (destructor called).

Whilst allocation and decallocation are done automatically, borrows must be taken manually, in order to clearly indicate
when a value is being created, using either the `&` or `&mut` operator. It is possible to take the C++ route, and allow
the borrows (references) to be tken automatically by type checking the parameters, but this becomes implicit, and harder
to see, from the function call site, so S++ inherits the Rust-like explicit borrow syntax, to make it clear when a value
is being borrowed, and when a new value is being created.

There are three key techniques that are used to ensure memory safety in S++:

1. **Ownership tracking**: This tracks the ownership (initialization state) of objects, and at any given line of the
   program, the compiler knows if a symbol is holding an initialized, partially initialized, or uninitialized value.
   This is used to prevent use after free, and double free errors. There are a number of restrictions on the usage of
   uninitialized and partially initialized values, such as they cannot be used in expressions, or passed to functions,
   until they are fully initialized.


2. **Second-class borrows**: Objects can be borrowed immutable or mutable, without moving the object (ie without
   changing the ownership). This allows a value to be temporarily shared into another function, modified or just read
   from, and then used after the function call has finished executing. Second class borrows enforce **where** a borrow
   can be made from: either ata function call site, or a generator yield point. This simplifies lifetime management.


3. **The law of exclusivity**: This states that at any given time, there can only be one mutable reference, xor n
   immutable borrows, to overlapping regions of memory at a given time. This is used to prevent data races and invalid
   memory access in concurrent contexts. The compiler checks for this at compile time, and will throw an error if it
   detects a violation of the law of exclusivity.


4. **Memory pinning**: This is a technique used to prevent values from being moved in memory, which is important for
   ensuring the validity of borrows in asynchronous or coroutine contexts. When a value is pinned, it cannot be moved in
   memory, enforcing that borrows are valid for extended periods of time. This is fully automatic, with scope rules used
   to unpin values.

## Ownership Tracking

Ownership tracking is a concept in the S++ memory model that monitors every operation of the program, and integrates
with the symbol table to track the ownership state of every individual variable in the program. By knowing the
initialization state of every symbol, the program can, at compile time, enforce restructions on certain operations. For
example, borrowing a partially initialized value is prohibited, as is using a value or its fields after it has moved to
the uninitialized state. This mitigates common memory errors such as:

- **Use-after-free**: This occurs when a program continues to use a value after the memory it points to has been
  deallocated. Ownership tracking prevents this by ensuring that once a value goes out of scope and is deallocated, or
  is moved in another operation, than further uses of the value prior to reinitialization are prohibited.


- **Double free**: This occurs when a program attempts to deallocate the same memory more than once. The "scope-bound
  resource management" part of S++ ownership tracking ensures that each value is deallocated exactly once when it goes
  out of scope, preventing the destruction code from ever being called more than once for the same value.

### Moving vs Copying

All operations use move-default semantics. This means that when a value is used in an expression, such as an assignment,
function call argument, etc, the ownership of the value is transferred to the new variable, and the original variable is
left in an uninitialized state.

This is the default behaviour for all types, except for copyable types. For copyable types, the value is copied instead
of moved, meaning that both the original and new variable remain initialized and valid for use after the operation. This
is identical to the behaviour Rust's `Copy` trait.

In S++, the `Copy` type must be superimposed over the target type. This is seen for numbers, booleans, the string view
type and some other in the STL. Due to specialization, we can also specialise optionals to be copyable, if and only if
their inner type is also copyable. This applies to vectors and arrays, etc. The following example shows making a normal
type copyable.

```s++
cls MyType {
    a: S32
    b: S32
}

sup MyType ext Copy { }
```

There is a shorthand annotation for extension, when no abstract method need overriding, or no virtual methods are being
overridden:

```s++
!extends[Copy]()
cls MyType {
    a: S32
    b: S32
}
```

To make a type copyable depending on a generic parameter being copyable, the generic constraint syntax can be used:

```s++
cls MyType[T] {
    a: T
}

sup [T: Copy] MyType[T] ext Copy { }
```

### Moving and Uninitialized Values

S++ uses move semantics by default, like Rust, except for copyable types. All operations that use a value, such as
assignment, variable declarations, function call arguments etc, **move** values. This means that the ownership of the
value is transferred to the new variable, and the original variable is left in an uninitialized state. Functions can
borrow arguments, but with no borrow operator, the value is moved.

There is a way to make types copyable, which is explored further down in the section on copyable types, but for
non-copyable types, move semantics are used by default. In the following example, the variable `x` is declared by the
`let` statement, and is initialized with the string `"hello world"`. The `Str` type is non-copyable, so move semantics
are used. This matches Rust's string type ownership semantics.

Following the declaration of `x` with the `Str` type, the subsequent assignment into the variable `y` moves the value of
`x` into `y`, leaving `x` in an uninitialized state. Any further use of `x`, such as the `print` function call, will
cause a compile time error, as the compiler knows that `x` is uninitialized at this point, and prevents its use. The
only operation value for `x` is on the left-hand-side of an assignment operation, or a variable redeclaration.

```s++
fun main() -> Void {
    let x = Str::from("hello world")
    let y = x              # x is now uninitialized, and ownership has moved to y
    std::io::print("${x}") # compile time error: use of uninitialized variable `x`
}
```

### Partial Movement and Partial Initialization

When a value contains fields, the fields can be moved off of the object, leaving the remainder of the object valid for
use. This is a **partially initialized** value, because only some fields are left initialized. This restricts what
operations can be done on this value. For example, it cannot be moved, borrowed from, and the moved fields cannot be
moved from again.

Partial moves can not be taken from borrowed objects even mutably borrowed objects. This is effectively the inverse of
not being able to borrow from partially initialized memory. This guarantees that a borrow will always be completely
initialized, and also forces partial moves to be bound to a single scope, and never cross boundaries. Tis heavily
simplifies ownership tracking.

The following example demonstrates partial movement and partial initialization. The `Point` struct has two fields, `x`
and `y`. When the `x` field is moved into the variable `x`, the `Point` value is left in a partially initialized state,
with only the `y` field remaining initialized. The `print` function call on the `point` variable will cause a compile
time error, as the compiler knows that `point` is partially initialized at this point, and prevents its use.

```s++
cls Point {
    x: Str
    y: Str
}

fun main() -> Void {
    let mut point = Point()
    let x = point.x              # point is now partially initialized, with only the y field remaining initialized
    std::io::print("${point}")   # compile time error: use of partially initialized variable `point`
    std::io::print("${point.y}") # this is valid, as x is fully initialized and has been moved out of point
}
```

Note that copying vs moving semantics also apply to partial moves. If an attribute is copyable, then it will be copied
from the object, not moved off of it, levbing the object in the fully initialized state (provided it started in the
fully initialised state).

### Uninitialised Values

When a value is uninitialized, it is modelled as if it has no location in memory, which semantically is true. Therefore,
even setting attributes to an uninitlized variable, with the idea of making is partially initialized, is invalid. This
is because the variable doesn't have a memory location, so it can't have fields.

However, there is a quirk where moving all attributes off an object by partially moving, leaves the value in the
partially-initialized state; This is because the object still has a pointer in memory, it just has no fields on it. This
follows the memory model, as its _contents_ have been moved, not the object itself.

To use a variable that has been uninitialized or moved from, it must be initialized via assignment, or redeclared with a
`let` statement and a value.

### TLDR Moving Rules

1. **Moves**: A value is moved out of a variable when its type doesn't superimpose `Copy`, and it is used as an
   assignment value, move-convention function argument, object initialization argument, move-convention yield, or a
   return value. Moving a value leaves the original variable in an uninitialized state, and the new variable takes
   ownership of the value.


2. **Partial Moves**: For a partial move to be taken from a value, the value must not be borrowed, even mutably.
   Furthermore, the attribute itself must be fully initialized. Multiple partial moves of non-overlapping regions can
   betake from the same object. This places the object in the **partially initialized** state.


3. **Uninitialized Variables**: The only operation an uninitialized variable can be used in is the left-hand-side of an
   assignment, or a variable redeclaration. This is because an uninitialized variable has no memory location, so it
   can't have fields, and it can't be borrowed from, or moved from again.

## Second-Class Borrows

S++ uses second class borrows, which are very similar to Rust's borrows, but there are restrictions about _where_
borrows can be taken. This is done to heavily simplify lifetime management, whilst not compromising on the
expressiveness, or runtime performance of the language. The compiler can statically guarantee that all borrows are
valid, and that there are no dangling references, without any lifetime annotations; purely be scoping rules.

Borrows can be taken at two places:

- Function call sites: When a value is passed as an argument to a function, it can be borrowed immutably with the `&`
  operator, or mutably with the `&mut` operator. This allows the function to read from or modify the value without
  taking ownership of it. The borrow is valid for the duration of the function call, and the value can be used after the
  function call has finished executing.


- Generator yield points: When a value is yielded from a generator, it can be borrowed immutably with the `&` operator,
  or mutably with the `&mut` operator. This allows the generator to yield a reference to a value without the receiving
  context taking ownership of it. Once the next yield point is reached, the previously yielded value is no longer
  borrowed, and can be used again in the generator.

### Function Call Site Borrows

Taking a borrow at a function call site is guaranteed to be safe, because the borrow is only valid for the duration of
the function call, and the value can be used after the function call has finished executing. This is because a function
call exists as as inner stack frame, and therefore has a guaranteed shorter lifetime than the caller.

Borrows are stored on the stack, and are deallocated when the function call finishes executing. This means that there is
no runtime overhead for borrows, as they are just stack values, and there is no heap allocation or reference counting
involved. There are additional complexities when considering coroutines and functions called asynchronously that accept
borrows, because the lifetime of the function call is not guaranteed to be shorter than the caller. The "memory pinning"
section discusses how this is handed.

### Yielding Borrows

The iteration model of S++ relies on borrowed being able to be yielded from coroutines, using the generator object. This
is almost identical to Rust's `gen fn` technique, with the additional capability of being able to specify the return
type (that superimposes the generator type). Safety is enforced due to the fact that control will always return to the
coroutine; memory pinning rules also handle a lot of yield safety, discussed below.

### Stacking Borrows

If a borrow is used as a normal move argument, then the borrow becomes uninitialized after the move:

```s++
fun func(x: &Str) -> Void {
    other_func(x) # x is now uninitialized, as the borrow has been moved into other_func
}
```

However, borrows can be "stacked" ie by taking a borrow of a borrow, but the type of the symbol automatically coalesces
to a single borrow. For example:

```s++
fun other_func(x: &Str) -> Void { }

fun func(x: &Str) -> Void {
    other_func(&x) # this is valid, as the type of x coalesces to a single borrow, and the borrow is still valid after the function call
}
```

Regarding mutability, a mutable borrow can either be taken mutable or immutably into an inner function, but an immutable
borrow can only be reborrowed immutably. This is because a mutable borrow guarantees exclusive access to the value, so
it can be safely reborrowed as either mutable or immutable. However, an immutable borrow does not guarantee exclusive
access, so it can only be reborrowed immutably to prevent potential data races.

### Mutable Borrows vs Mutable Values

The mutability of a value, and a borrow, can be different, as they provide different semantics. The mutability of a
borrow refers to the contexts of the borrow being mutable or not, where-as the mutability of a value, who is borrowed,
describes whether it can be rebound or not:

| Borrow | Example                        | Value Mutability           | Borrow Mutability      |
|--------|--------------------------------|----------------------------|------------------------|
| `N`    | `fun f(x: T) -> Void`          | `x` can not be re-assigned | `x` can not be mutated |
| `N`    | `fun f(mut x: T) -> Void`      | `x` can be re-assigned     | `x` can be mutated     |
| `Y`    | `fun f(x: &T) -> Void`         | `x` can not be re-assigned | `x` can not be mutated |
| `Y`    | `fun f(mut x: &T) -> Void`     | `x` can be re-assigned     | `x` can not be mutated |
| `Y`    | `fun f(x: &mut T) -> Void`     | `x` can not be re-assigned | `x` can be mutated     |
| `Y`    | `fun f(mut x: &mut T) -> Void` | `x` can be re-assigned     | `x` can be mutated     |

### Destructuring Borrows

Destructuring a borrow is typically not allowed, because it requires moving a field off of a borrow. This is not allowed
under the memory model, becauuse it attempts to induce a partial move on a borrowed object. However, in `case`
statements, pattern destructuring allows for attribute borrowing, because the symbols become available in an inner scope
(guaranteed), so the safety is scope-enforced. For example:

```s++
cls Point {
    x: Str
    y: Str
}

fun func(p: &Point) -> Void {
    case p is Point(&x, &y) { } # this is valid, as the pattern destructuring allows for attribute borrowing, and the symbols x and y are available in an inner scope
}
```

## The Law of Exclusivity

To prevent data races, the law of exclusivity states that at any given time, there can only be one mutable reference,
xor n immutable borrows, to overlapping regions of memory. This is enforced by the compiler at compile time, and will
throw an error if it detects a violation of the law of exclusivity.

### Overlapping Regions of Memory

Two regions of memory are considered overlapping if they refer to the same value, or if they refer to values that are
part of the same object. For example, if we have `p`, a struct `Point`, with fields `x` and `y`, then a mutable borrow
of `p` and an immutable borrow of `p.x` would be considered overlapping, because they overlap on the `x` field of `p`.
However, `p.x` and `p.y` do not overlap, as they do not share any region of memory.

## Memory Pinning

Memory pinning is an important (and largely hidden) part of the S++ memory model, that is used to ensure the validity of
borrows in asynchronous or coroutine contexts. Borrows passed into asynchronously called function, or coroutines, are
described as "escaping borrows". This is because they don't, for definite, live in a smaller frame than the caller.
Rather, they live in a concurrent frame.

In order to enforce memory rules, "memory pinning" prevents the outer context from using the value that underlies the
borrow. Scoping rules that destroy coroutines and futures allow for the borrow to be unlocked.

```s++
cor my_coro(s: &Str) -> Gen[S32] {
    gen 1
    gen 2
}

fun my_func() -> Void {
    let s = Str::from("hello world")
    let g = my_coro(&s)

    std::io::println("${g.res()}")
}
```

In this example, the `my_coro` coroutine takes an immutable borrow of the string `s`. The borrow is valid for the
duration of the coroutine, meaning that `s` gets pinned until the end of the scope; in this case the `my_func` function
block. This prevents `s` from being moved, partially moved, or mutated, whilst being immutable borrowed by the
coroutine.

Additionally, because the generator `g` contains `s`, `g` is pinned for the same duration as `s`, meaning that `g` also
cannot be moved or partially moved until the end of the `my_func` function block. This ensures that the generator object
doesn't secretly propagate the internal borrow out of the scope it was defined in.
