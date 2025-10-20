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
expressions look like `gen &mut 123`, then the generator type would be `Gen[&mut S32]`. This allows for the full
convention and type knowledge of the generated values to be accessible from the function signature alone.

The `Send` generic parameter represents the type being sent back to the coroutine. This defaults to `Void`, disallowing
data to be sent back (cannot have a `Void` variable), but can be set to any type. Only owned objects can be sent back
into a coroutine. Note that having `Void` as the parameter type causes it to be removed, so the `.res()` method will
take no arguments, producing an argument free `.send()` internal call.

## Advancing a Generator

A generator is advanced by using the `.res()` method. As this requires a special compiler intrinsic, `res` is a callable
postfix keyword, rather than a method alone. Internally, this calls the private `.send()` method on the generator,
passing in any data if the `Send` type is not `Void`.

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&S32] {
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

## Passing Data Out of a Coroutine

As seen above, data is passed out of a coroutine using the `gen` expression, including an optional convention. Allowing
borrows to be yielded from coroutines is the basis for iteration, as it allows elements of a vector, for example, to be
borrowed and used in the caller.

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&S32] {
    gen &a
    gen &b
    gen &c
}
```

## Invalidating Borrows [SECTION SUBJECT TO CHANGE]

Yielded borrowed belong to the generator they are yielded from. Like function arguments coming _into_ a function, the
law of exclusivity is applied to them, to prevent accessing multiple mutable parts of a generator, which could be
overlapping. So a mutably borrowed yield will invalidate the previous mutably borrowed yield.

Further to this, a borrow could be yielded, then its corresponding owned object consumed in the next resuming of the
caller. Therefore, each yield must be isolated, and invalidate the previous yield.

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&S32] {
    gen &a
    gen &b  # control of "a" is maintained here (immutable borrow) + pinned
    gen &c  # control of "b" is maintained here (immutable borrow) + pinned
    
    # The variables "a", "b" and "c" are pinned here as they may be used in the yieldee context.
}


fun main() -> Void {
    let generator = coroutine(1, 2, 3)
    let a = generator.res()
    let b = generator.res()  # a internal value still valid here (immutable borrow) + pin "a" released
    let c = generator.res()  # b internal value still valid here (immutable borrow) + pin "b" released
}
```

With immutable borrows, multiple yielded values can be taken and all remain valid, as they cannot be used to mutate the
underlying data, and so overlaps are fine; there will never be a conflict with mutability. In the "yielder" context,
these values are pinned, so they cannot be consumed in the yielder whilst being borrowed in the yieldee.

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&mut S32] {
    gen &mut a
    gen &mut b  # control of "a" is regained here
    gen &mut c  # control of "b" is regained here
    
    # Because control is regained, "a" is pinned until the next yield ("b").
}


fun main() -> Void {
    let generator = coroutine(1, 2, 3)
    let a = generator.res()
    let b = generator.res()  # a is invalidated here (mutable borrow)
    let c = generator.res()  # b is invalidated here (mutable borrow)
}
```

For mutable borrows, there is a stricter invalidation policy. Yielding a mutable borrow invalidates the previous
mutable borrow that was yielded, otherwise there is no guarantee that there isn't an overlap in data being yielded.
There is an argument to check all yields in the yielder are non-overlapping, and then the yieldee will never have to
invalidate previous yields, but this would prevent the same object being yielded twice from the yielder, which is a
common use-case. However, because control is regained per yield in the yielder, it is known that the object is singly
owned in the coroutine, meaning yielded symbols are only pinned until the following yield.

## Passing Data Into a Coroutine

The `.res()` method takes a single argument, whose type matches the `Send` generic parameter of the generator (
coroutine return type). Because substituting `Void` as a generic parameter causes function parameters of that type to be
removed from the signature, `.res()` can be used for th default `Send=Void` generic parameter.

Values are received by placing the `gen` expression on the right-hand-side of a variable definition statement. Variables
are always moved into a coroutine, not borrowed. This means that receiving a second value into the coroutine doesn't
invalidate the first one.

```S++
cor coroutine() -> Gen[Yield=S32, Send=S32] {
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

All borrows into a coroutine are automatically pinned. This prevents them from being consumed in the caller context,
until they are manually released. Releasing a pin will invalidate any object relying on the pin. Therefore, if a
coroutine received a borrow, and the memory of that borrow is released, the coroutine is marked as consumed and is
non-usable.

## Chaining Coroutines

Coroutine chaining allows a coroutine to yield the entirety of another coroutine in a one-line expression; without a
loop. This is identical to Pythons `yield from` statement. In S++, `gen with` is used, to generate values with another
coroutine.

The following example shows equivalent code using a loop and coroutine chaining:

```S++
cor coroutine() -> Gen[Yield=S32] {
    gen 1
    gen 2
    gen 3
}

cor coroutine_chain() -> Gen[Yield=S32] {
    gen 0
    for i in coroutine() {
        gen i
    }
}
```

```S++
cor coroutine() -> Gen[Yield=S32] {
    gen 1
    gen 2
    gen 3
}

cor coroutine_chain() -> Gen[Yield=S32] {
    gen 0
    gen with coroutine()
}
```

## The `GenOnce` type.

The generator types `Gen`, `GenOpt` and `GenRes` all create wrapper `GeneratedXXX` types, that may contain the yielded
value, and can be inspected in the `iter` block, as mentioned earlier. However, methods like `.index_ref()`, where there
will only be one value yielded, which may be a borrow, and would always require an `iter` block matching on a present
value, cause inconvenience. Therefore, the `GenOnce` type exists to allow for a single value to be yielded and
automatically unwrapped. Automatic unwrapping also bypasses the need for `.res()`. Without and with `GenOnce`:

```S++
cor get_value() -> GenOnce[Yield=&S32] {
    gen 42
}

fun main() -> Void {
    let val: &Str
    iter t = get_value().res() of {
        val { val }
    }
}
```
```S++
cor get_value() -> GenOnce[Yield=&S32] {
    gen 42
}

fun main() -> Void {
    let val = get_value()
}
```

In order to make this compatible with the memory model, the pinning is based not just off the generator (ie directly
left of `.res()`), but the entire expression the generator is part of. This means that the following code is valid:

```S++
cls MyType {}

sup MyType {
    cor get_value(&mut self) -> GenOnce[Yield=&mut S32] {
        let value: S32 = 42
        gen &mut value
    }
}

fun main() -> Void {
    let t = MyType()
    let val_1 = t.get_value()  # t is pinned here and marked as an extended borrow
    let val_2 = t.get_value()  # t is a conflicting borrow here, so this is invalid
}
```
