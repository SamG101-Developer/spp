# Concurrency

S++ uses coroutines as the primary concurrency mechanism. They can be suspended and resumed, and support bidirectional
data flow between the caller and the coroutine. They are defined in the same way as a function or method, but they
require the `cor` keyword rather than `fun`.

Coroutine return types are constrained to generators, as explored below. Typically, there will be any number of `gen`
expressions inside the coroutine, to generate or yield values to the caller. Assigning a value from the `gen` expression
allows for sending data back into the coroutine on resuming.

An important distinction is that coroutines are baked into control flow, rather than the type system purely. This means
whilst the multiplicity, optionality and fallibility of a coroutine is depicted in the type system, the actual
destructuring of a yielded value requires a specific block, designated for coroutines. This is because of second class
borrows; for example, `Opt[&T]` cannot exist as a type, because this requires `Some[&T]`, which requires a borrow type
attribute, invalidating the second class borrow rules. The `iter` block is explored in this [**section**]().

## Coroutine Return Types

There are four possible coroutine return types in S++:

- `Gen[Yield, Send=Void]`
- `GenOnce[Yield, Send=Void]`
- `GenOpt[Yield, Send=Void]`
- `GenRes[Yield, Err, Send=Void]`

The standard generator type, `Gen`, will only ever yield into 2 different states: yielding a value of type `Yield`, or
being exhausted with no more values. Every value yielded from a `Gen` coroutine is safely assumed to be valid, existing,
and inferred as the `Yield` type. The `iter` block can match on value and exhaustion.

The `GenOnce` type is used when the coroutine is designed to yield a single value, and then be exhausted. This is useful
for things like indexing a value in a vector. Calling `.res()` on a `GenOnce` coroutine will yield a value and
auto-unwrap it into the inner value. `GenOpt` and `GenRes` can't be one hit due to their no-value or error states
requiring consideration. If the one-hit generator is exhausted immediately, it will panic.

The `GenOpt` type is used when the coroutine might yield a "no-value". This is seen when "getting" an element from a
vector, as the bounds check might fail, and a "no-value" should be signified. A value yielded from a `GenOpt` coroutine
can be assumed to be either a value of type `Yield`, or a "no-value" (which is represented as `Void` in the typesystem).
The `iter` block can match on value, "no-value", and exhaustion.

Finally, the `GenRes` type is used when the coroutine might yield a value of type `Yield`, or an error of type `Err`.
This is used for fallible generators, where the result can be either a value or an error. A value yielded from a
`GenRes` coroutine can be assumed to be either a value of type `Yield`, or an error of type `Err`. The `iter` block can
match on value, error and exhaustion.

The `Yield` generic parameter's corresponding argument determines the type of value being yielded from the coroutine.
Whilst this is inferrable, a design decision was taken to mark it explicitly in the return type, in the same way that a
subroutine's return type must always be explicitly given, despite is being inferrable from `ret` statements.

A convention can be applied to the generic argument for the `Yield` parameter. Because borrows can be yielded from
coroutines, the yielding convention must match the convention of the `Yield` argument. For example, if the gen
expressions look like `gen &mut 123`, then the generator type would be `Gen[&mut BigInt]`. This allows for the full
convention and type knowledge of the generated values to be accessible from the function signature alone.

The `Send` generic parameter represents the type being sent back to the coroutine. This defaults to `Void`, disallowing
data to be sent back (cannot have a `Void` variable), but can be set to any type. Only owned objects can be sent back
into a coroutine.

## Advancing a Generator

A generator is advanced by using the `.res()` method. As this requires a special compiler intrinsic, `res` is a callable
postfix keyword, rather than a method.

```S++
cor coroutine(a: BigInt, b: BigInt, c: BigInt) -> Gen[Yield=&BigInt] {
    gen &a
    gen &b
    gen &c
}

fun main() -> Void {
    let generator = coroutine(1, 2, 3)
    let a = generator.res()
    let b = generator.res()
    let c = generator.res()
}
```

See the [](#invalidating-borrows) section for how and why earlier generated borrows are invalidated.

## Passing Data Out of a Coroutine

As seen above, data is passed out of a coroutine using the `gen` expression, including an optional convention. Allowing
borrows to be yielded from coroutines is the basis for iteration, as it allows elements of a vector, for example, to be
borrowed and used in the caller.

```S++
cor coroutine(a: BigInt, b: BigInt, c: BigInt) -> Gen[Yield=&BigInt] {
    gen &a
    gen &b
    gen &c
}
```

### Invalidating Borrows [SECTION SUBJECT TO CHANGE]

Yielded borrowed belong to the generator they are yielded from. That being said, the law of exclusivity is applied to
them, to prevent accessing multiple mutable parts of a generator, which could be overlapping. So a mutably borrowed
yield will invalidate the previous mutably borrowed yield.

Further to this, a borrow could be yielded, then its corresponding owned object consumed in the next resuming of the
caller. Therefore, each yield must be isolated, and invalidate the previous yield.

```S++

cor coroutine(a: BigInt, b: BigInt, c: BigInt) -> Gen[Yield=&BigInt] {
    gen &a

    let s = a + b
    gen &s

}


fun main() -> Void {
    let generator = coroutine(1, 2, 3)
    let a = generator.res()
    let b = generator.res()
    let c = generator.res()
}

```

In this example it can be seen that `a` is consumed in the coroutine. As there is no guarantee whether a variable is
consumed or not in the coroutine after being yielded as a borrow, it must be assumed that a worst-cast scenario occurs,
and that every variable yielded as a borrow might subsequently be consumed. As such, every time another borrowed value
is yielded, the previous borrow will be invalidated in the caller.

## Passing Data Into a Coroutine

The `.res()` method takes a single argument, whose type matches the `Send` generic parameter of the generator (
coroutine return type). Because substituting `Void` as a generic parameter causes function parameters of that type to be
removed from the signature, `.res()` can be used for th default `Send=Void` generic parameter.

Values are received by placing the `gen` expression on the right-hand-side of a variable definition statement. Variables
are always moved into a coroutine, not borrowed. This means that receiving a second value into the coroutine doesn't
invalidate the first one.

```S++
cor coroutine() -> Gen[Yield=BigInt, Send=BigInt] {
    let a = gen 1
    let b = gen 2
    let c = gen 3
    gen a + b + c
}

fun main() -> Void {
    let generator = coroutine()
    let a = generator.res(1)
    let b = generator.res(2)
    let c = generator.res(3)
    let t = generator.res(0)
}
```

## Borrowing Data Into a Coroutine

A problem is presented when trying to use borrows with coroutines; a borrow could be passed into a coroutine, the
coroutine suspends, the owned object in the caller context is consumed, and the borrow is subsequently used in the
coroutine. To prevent this, memory pinning is used.

All borrows into a coroutine must be pinned. This prevents them from being consumed in the caller context, until they
are manually released. Releasing a pin will invalidate any object relying on the pin. Therefore, if a coroutine received
a borrow, and the memory of that borrow is released, the coroutine is marked as consumed and is non-usable.

## Chaining Coroutines

Coroutine chaining allows a coroutine to yield the entirety of another coroutine in a one-line expression; without a
loop. This is identical to Pythons `yield from` statement. In S++, `gen with` is used, to generate values with another
coroutine.

The following example shows equivalent code using a loop and coroutine chaining:

```S++
cor coroutine() -> Gen[Yield=BigInt] {
    gen 1
    gen 2
    gen 3
}

cor coroutine_chain() -> Gen[Yield=BigInt] {
    gen 0
    for i in coroutine() {
        gen i
    }
}
```

```S++
cor coroutine() -> Gen[Yield=BigInt] {
    gen 1
    gen 2
    gen 3
}

cor coroutine_chain() -> Gen[Yield=BigInt] {
    gen 0
    gen with coroutine()
}
```