# Concurrency

S++ builds its main concurrency model on stackless, lightweight coroutines. They behave in the same way as Rust's
unstable `gen fn` syntax. The goal is high-performance iteration built on generators that stay straightforward to use
and maintain. The final codegen uses jump points against the yield points, giving fast context switching.

To use a coroutine, declare it with the keyword `cor` instead of `fun`. This is the only syntactic difference between
functions, which are really subroutines, and coroutines. The return type carries restrictions, explored below.

```s++
cor count_to_n(n: U32) -> Gen[Yield=U32] {
    loop i in Range::between(0, n) {
        gen i
    }
}
```

To keep keyword length consistent with `ret` for returning values, S++ uses `gen`, representing the _generation_ of
values into a generator.

## Coroutine return types

Rust's unstable `gen fn` syntax uses a dedicated type to represent generators, through an API that's effectively
`impl IntoIterator<Item = T>`. S++ takes this a step further, and lets the return type be any type that superimposes a
generator type. This lets methods superimpose over the return type, which is foundational to the iteration model.

The `Iterator[T]` type superimposes `Gen[Yield=T | None]`, letting coroutines return iterators directly. This is the
most common use case.

## The generator type

S++ defines the generator type as `cls Generator[Yield, Send=Void]`. This makes a generator that's optionally fallible
and supports two-way data exchange. The `Yield` type is the type the generator yields, namely the type inferred from
the expression attached to `gen`. No expression means `Yield` must be `Void`.

`Gen[T | None]` appears commonly, because `Gen[Opt[T]]` is problematic: if `T` is a borrow type, the result is
`Some[&T]`, which requires a borrowed attribute and violates second-class borrows. S++ instead defines an alias where
`T` stays unwrapped, allowing borrows to work.

The `Send` type defines what data, if any, flows _into_ the coroutine. Extending the `gen` expression into
`let a = gen expr` does this. Only `.send(value)` advances past a yield point, so the caller must always supply a value
to move on from it. Calling a coroutine immediately provides the `Gen` object and moves to the first yield point.

## One-hit coroutines

One class of coroutines has a single yield point and returns a borrow, with the extra guarantee that control returns to
the coroutine, preventing dangling borrows. S++ calls these one-hit coroutines, and they use the return type
`GenOnce[Yield]`. They take no `Send` generic parameter, because nothing needs to send data into a one-hit coroutine: a
one-hit coroutine always auto-unwraps the yield, effectively making the yield point invisible to the user.

This behaviour appears especially in the index API on collections, when yielding a borrow:

```s++
cor index_ref(&self, index: USize) -> GenOnce[&T] {
    # IMPL
}

```

## Coroutine chaining

Stolen from Python, the extended `gen with` syntax lets one generator yield all values from another generator without
requiring a loop. This creates a concise generator chaining syntax.

```s++
cor count_to_n_twice(n: U32) -> Gen[Yield=U32] {
    gen with count_to_n(n)
    gen with count_to_n(n)
}
```
