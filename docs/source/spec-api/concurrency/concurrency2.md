# Concurrency

The main concurrency model of s++ is based on stackless, lightweight coroutines. They acts in the same way as Rust's
unstable `gen fn` syntax. The idea behind this is to enable extremely high performance iteration based on generators,
which are extremely simple to use and maintain. The final codegen simply uses jump points against the yield points,
enabling extremely fast context switching.

To use a coroutine, it must be defined with thekeyword `cor`, instead of `fun`. This is the only syntactic difference
between function (actually subroutines), and coroutines. There are type restructions on the return type, explored below.

```s++
cor count_to_n(n: U32) -> Gen[Yield=U32] {
    loop i in Range::between(0, n) {
        gen i
    }
}
```

To allow for consistent keyword length, along with `ret` for returning values, `gen` was chosen, representing the
_generation_ of values into a generator.

## Coroutine return types

Rust's unstable `gen fn` syntax uses a special type to represent generators, using an API that is effectively
`impl IntoIterator<Item = T>`. S++ takes this a step further, by allowing for the return type to be any type that
superimposes a generator type. This allows for methods to be superimposed over the return type, which is foundaitonal to
the iteration model.

The `Iterator[T]` type superimposes `Gen[Yield=T | None]`, allowing for coroutines to return iterators directly. This is
the most common use case.

## The Generator type

The generator ype is defined as `cls Generator[Yield, Send=Void]`. This makes a generator that is optionally falible,
and supports two way data exchange. The `Yield` type is the type of value that is yielded from the generator, ie the
type inferred from the expression attached to `gen`. No expression means that `Yield` must be `Void`.

It is commonly seen that `Gen[T | None]` is used. This is because `Gen[Opt[T]]` is problematic, as if `T` is a borrow
type, then we get `Some[&T]`, which requires a borrowed attribute, violating second class borrows. Therefore, we define
as alias there `T` remains unwrapped, allowing borrows to work.

The `Send` type is used to define what type of data, if any, is being sent _into_ the coroutine. This is done by simply
extending the `gen` expression into `let a = gen expr`. This is because yields are only accelerated past with
`.send(value)`, meaning a value must always be provided, in order for the yield point to be moved on from. Calling a
coroutine will immediately provide the `Gen` object, and move to the first yield point.

## One-hit coroutines

There are a class of coroutines that only have one yield point, and are used for returning a borrow, with the additional
guarantee that control will return to the coroutine, preventing dangling borrows. These are called one-hit coroutines,
and use the return type `GenOnce[Yield]`. There is no `Send` generic parameter, as there is no need to send data into a
one-hit coroutine: a one-hit coroutine will always auto-unwrap the yield, effectively making the yield point invisible
to the user.

This is seen especially in the index api on collections, when a borrow is being returned:

```s++
cor index_ref(&self, index: USize) -> GenOnce[&T] {
    ...
}
```

## Coroutine chaining

Stolen from Python, the extended `gen with` syntax allows for one generator to yield all values from another generator,
without the loop beign required. This means that generator chaining is extremely simple to read and write:

```s++
cor count_to_n_twice(n: U32) -> Gen[Yield=U32] {
    gen with count_to_n(n)
    gen with count_to_n(n)
}
