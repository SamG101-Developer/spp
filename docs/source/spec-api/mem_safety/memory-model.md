# Memory model

## General model

S++ uses a "partially automatic memory management" model. The programmer never manually allocates or deallocates memory:
"scope-bound resource management" controls the lifetime of all values, tying the lifetime of a value to the scope of its
definition. A value can propagate out of the scope that created it, but a variable still initialized at the end of its
scope gets dropped. Put briefly:

- **Allocation**: Uniform object initialization, which calls the internal constructor.
- **Deallocation**: Scope-bound resource management, which calls the destructor.

Allocation and deallocation happen automatically, but the programmer takes borrows manually with the `&` or `&mut`
operator, which marks where a borrow occurs. S++ could take the C++ route and derive borrows automatically by type
checking the parameters, but that leaves the borrow implicit at the function call site. S++ instead
inherits the Rust-like explicit borrow syntax, so the code shows exactly when it borrows a value and when it creates a
new one.

Four key techniques ensure memory safety in S++:

1. **Ownership tracking**: This tracks the ownership, or initialization state, of objects. At any given line of the
   program, the compiler knows whether a symbol holds an initialized, partially initialized, or uninitialized value.
   This prevents use-after-free and double-free errors. Restrictions govern uninitialized and partially initialized
   values: they can't appear in expressions, or pass to functions, until they're fully initialized.

2. **Second-class borrows**: Code can borrow objects immutably or mutably without moving them, and so without changing
   ownership. A value can pass temporarily into another function, get changed or read, and still see use after that
   call returns. Second-class borrows constrain **where** a borrow can originate: either at a function call site, or at
   a generator yield point. This simplifies lifetime management.

3. **The law of exclusivity**: At any given time, overlapping regions of memory admit either one mutable reference, xor
   n immutable borrows. This prevents data races and invalid memory access in concurrent contexts. The compiler checks
   this at compile time and raises an error when it detects a violation.

4. **Memory pinning**: This prevents values from moving in memory, which keeps borrows valid in asynchronous or
   coroutine contexts. A pinned value can't move, so its borrows stay valid over extended periods. Pinning is fully
   automatic, and scope rules unpin values.

## Ownership tracking

Ownership tracking monitors every operation of the program and integrates with the symbol table to track the ownership
state of every variable. Knowing the initialization state of every symbol lets the compiler enforce restrictions on
certain operations at compile time. For example, the compiler prohibits borrowing a partially initialized value, and
prohibits using a value or its fields after it moves to the uninitialized state. This mitigates common memory errors
such as:

- **Use-after-free**: This occurs when a program keeps using a value after the runtime deallocates the memory it points
  to. Ownership tracking prevents it: once a value goes out of scope and gets deallocated, or moves in another
  operation, the compiler rejects any further use before reinitialization.

- **Double free**: This occurs when a program tries to deallocate the same memory more than once. The "scope-bound
  resource management" part of S++ ownership tracking deallocates each value exactly once, when it goes out of scope, so
  the destruction code never runs twice for the same value.

### Moving versus copying

All operations use move-default semantics. When an expression uses a value, such as an assignment or a function call
argument, ownership transfers to the new variable and the original variable becomes uninitialized.

This is the default behaviour for all types except copyable types. Copyable types copy rather than move, so both the
original and the new variable stay initialized and usable after the operation. This matches the behaviour of Rust's
`Copy` trait.

In S++, the `Copy` type must superimpose over the target type. Numbers, booleans, the string view type and some others
in the STL do this. Specialization also makes optionals copyable if and only if their inner type is copyable, and the
same applies to vectors, arrays and similar types. The following example makes a normal type copyable.

```s++
cls MyType {
    a: S32
    b: S32
}

sup MyType ext Copy { }
```

A shorthand annotation exists for extension, for when no abstract or virtual methods need overriding:

```s++
!extends[Copy]()
cls MyType {
    a: S32
    b: S32
}
```

To make a type copyable only when a generic parameter is copyable, use the generic constraint syntax:

```s++
cls MyType[T] {
    a: T
}

sup [T: Copy] MyType[T] ext Copy { }
```

### Moving and uninitialized values

Like Rust, S++ uses move semantics by default, except for copyable types. All operations that use a value, such as
assignments, variable declarations and function call arguments, **move** values: ownership transfers to the new
variable and the original variable becomes uninitialized. Functions can borrow arguments, but without a borrow operator
the value moves.

Types can opt into copying, as the section on copyable types describes, but non-copyable types use move semantics by
default. In the following example, the `let` statement declares `x` and initializes it with the string
`"hello world"`. The `Str` type is non-copyable, so move semantics apply. This matches Rust's string type ownership
semantics.

After the declaration of `x`, the assignment into `y` moves the value of `x` into `y`, leaving `x` uninitialized. Any
further use of `x`, such as the `print` call, causes a compile time error, because the compiler tracks `x` as
uninitialized at that point and prevents its use. The only valid operation on `x` is the left-hand side of an
assignment, or a variable redeclaration.

```s++
fun main() -> Void {
    let x = Str::from("hello world")
    let y = x              # x is now uninitialized, and ownership has moved to y
    std::io::print("${x}") # compile time error: use of uninitialized variable `x`
}
```

### Partial movement and partial initialization

When a value contains fields, those fields can move off the object, leaving the rest of it valid for use. The result is
a **partially initialized** value, because only some fields stay initialized. This restricts the operations available on
that value: it can't move, nothing can borrow from it, and the moved fields can't move again.

Partial moves can't come from borrowed objects, not even mutably borrowed ones. This mirrors the rule against borrowing
from partially initialized memory. Together the two rules guarantee that a borrow is always fully initialized, and
confine partial moves to a single scope so they never cross boundaries. This heavily simplifies ownership tracking.

The following example demonstrates partial movement and partial initialization. The `Point` struct has two fields, `x`
and `y`. When the `x` field moves into the variable `x`, the `Point` value becomes partially initialized, with only the
`y` field still initialized. The `print` call on `point` causes a compile time error, because the compiler tracks
`point` as partially initialized there and prevents its use.

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

Copying versus moving semantics also apply to partial moves. A copyable attribute copies from the object rather than
moving off it, leaving the object fully initialized, provided it started fully initialized.

### Uninitialized values

An uninitialized value has no location in memory, and the model treats it that way. Setting attributes on an
uninitialized variable, with the aim of making it partially initialized, is invalid for that reason: the variable has no
memory location, so it can't have fields.

One quirk exists. Moving every attribute off an object through partial moves leaves the value partially initialized
rather than uninitialized, because the object still has a pointer in memory, it just has no fields on it. This follows
the memory model, because the move takes its _contents_, not the object itself.

To use a variable that became uninitialized or moved from, initialize it through assignment, or redeclare it with a
`let` statement and a value.

### Moving rules in brief

1. **Moves**: A value moves out of a variable when its type doesn't superimpose `Copy` and the value appears as an
   assignment value, move-convention function argument, object initialization argument, move-convention yield, or
   return value. The move leaves the original variable uninitialized, and the new variable takes ownership.

2. **Partial moves**: A partial move requires that the value isn't borrowed, not even mutably, and that the attribute
   itself is fully initialized. More than one partial move can come from the same object, as long as the moved regions
   don't overlap. This places the object in the **partially initialized** state.

3. **Uninitialized variables**: An uninitialized variable can appear only on the left-hand side of an assignment, or in
   a variable redeclaration. It has no memory location, so it can't have fields, and nothing can borrow from it or move
   from it again.

## Second-class borrows

S++ uses second-class borrows, which closely resemble Rust's borrows, except that restrictions govern _where_ code can
take a borrow. These restrictions heavily simplify lifetime management without compromising the expressiveness or
runtime performance of the language. The compiler statically guarantees that every borrow is valid and that no dangling
references exist, with no lifetime annotations, purely through scoping rules.

Borrows can originate in two places:

- Function call sites: the `&` operator borrows an argument immutably, and the `&mut` operator borrows it mutably. The
  function can then read or change the value without taking ownership. The borrow lasts for the duration of the call,
  and the value stays usable after the call returns.

- Generator yield points: the same two operators borrow a value yielded from a generator. The generator can then yield
  a reference without the receiving context taking ownership. Once control reaches the next yield point, the yielded
  value is no longer borrowed and the generator can use it again.

### Function call site borrows

A borrow taken at a function call site is always safe, because the borrow lasts only for the duration of the call, and
the value stays usable after the call returns. A function call occupies an inner stack frame, which guarantees a shorter
lifetime than the caller.

Borrows live on the stack and disappear when the call finishes. Borrows carry no runtime overhead: they're plain stack
values, with no heap allocation and no reference counting. Coroutines and asynchronously called functions that accept
borrows add complexity, because their call lifetime isn't guaranteed to be shorter than the caller's. The "memory
pinning" section describes how S++ handles this.

### Yielding borrows

The iteration model of S++ relies on coroutines yielding borrows through the generator object. This closely matches
Rust's `gen fn` technique, and adds the ability to specify the return type, which superimposes the generator type.
Safety holds because control always returns to the coroutine. Memory pinning rules handle much of yield safety too, as
described below.

### Stacking borrows

A borrow used as a normal move argument becomes uninitialized after the move:

```s++
fun func(x: &Str) -> Void {
    other_func(x) # x is now uninitialized, as the borrow has been moved into other_func
}
```

Borrows can also stack, meaning code can take a borrow of a borrow, and the symbol's type automatically
coalesces to a single borrow. For example:

```s++
fun other_func(x: &Str) -> Void { }

fun func(x: &Str) -> Void {
    other_func(&x) # this is valid, as the type of x coalesces to a single borrow, and the borrow is still valid after the function call
}
```

On mutability, an inner function can take a mutable borrow either mutably or immutably, but it can only reborrow an
immutable borrow immutably. A mutable borrow guarantees exclusive access to the value, so either reborrow stays safe. An
immutable borrow guarantees no such thing, so it reborrows immutably only, which prevents data races.

### Mutable borrows vs mutable values

The mutability of a value and the mutability of a borrow can differ, because they carry different semantics. Borrow
mutability refers to whether the contents behind the borrow can change, whereas the mutability of the borrowed value
describes whether the code can rebind it:

| Borrow | Example                        | Value mutability     | Borrow mutability |
|--------|--------------------------------|----------------------|-------------------|
| `N`    | `fun f(x: T) -> Void`          | `x` isn't rebindable  | `x` is immutable |
| `N`    | `fun f(mut x: T) -> Void`      | `x` is rebindable     | `x` is mutable   |
| `Y`    | `fun f(x: &T) -> Void`         | `x` isn't rebindable  | `x` is immutable |
| `Y`    | `fun f(mut x: &T) -> Void`     | `x` is rebindable     | `x` is immutable |
| `Y`    | `fun f(x: &mut T) -> Void`     | `x` isn't rebindable  | `x` is mutable   |
| `Y`    | `fun f(mut x: &mut T) -> Void` | `x` is rebindable     | `x` is mutable   |

### Destructuring borrows

Destructuring a borrow is normally invalid, because it requires moving a field off a borrow. The memory model forbids
that, because it induces a partial move on a borrowed object. In `case` statements, though, pattern destructuring can
borrow attributes, because the symbols become available in a guaranteed inner scope, so scoping enforces the safety. For
example:

```s++
cls Point {
    x: Str
    y: Str
}

fun func(p: &Point) -> Void {
    case p is Point(&x, &y) { } # this is valid, as the pattern destructuring allows for attribute borrowing, and the symbols x and y are available in an inner scope
}
```

## The law of exclusivity

To prevent data races, the law of exclusivity states that overlapping regions of memory admit either one mutable
reference, xor n immutable borrows, at any given time. The compiler enforces this at compile time and raises an error
when it detects a violation.

### Overlapping regions of memory

Two regions of memory overlap if they refer to the same value, or to values that belong to the same object. Take `p`, a
`Point` struct with fields `x` and `y`: a mutable borrow of `p` and an immutable borrow of `p.x` overlap, because both
cover the `x` field of `p`. In contrast, `p.x` and `p.y` don't overlap, because they share no region of memory.

## Memory pinning

Memory pinning is an important, and normally invisible, part of the S++ memory model that keeps borrows valid in
asynchronous or coroutine contexts. Borrows passed into asynchronously called functions, or into coroutines, count as
"escaping borrows" because they don't definitely live in a smaller frame than the caller. They live in a concurrent
frame instead.

To enforce the memory rules, memory pinning stops the outer context from using the value underlying the borrow. The
scoping rules that destroy coroutines and futures release the borrow again.

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

In this example, the `my_coro` coroutine takes an immutable borrow of the string `s`. The borrow lasts for the duration
of the coroutine, so `s` stays pinned until the end of the scope, in this case the `my_func` function block. Pinning
stops `s` from moving, partially moving, or mutating while the coroutine holds an immutable borrow of it.

The generator `g` contains `s`, so `g` stays pinned for the same duration as `s`, and can't move or partially move until
the end of the `my_func` function block. This stops the generator object from quietly propagating the internal borrow
out of the scope that defined it.
