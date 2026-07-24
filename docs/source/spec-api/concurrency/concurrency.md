# Concurrency

S++ uses coroutines as the primary concurrency mechanism. A coroutine can suspend and resume, and supports
bidirectional data flow between the caller and the coroutine. Coroutines look the same as a function or method, except
that they require the `cor` keyword rather than `fun`.

Coroutine return types must be generators, as explored below. A coroutine typically contains any number of `gen`
expressions, which generate or yield values to the caller. Assigning a value from the `gen` expression sends data back
into the coroutine on resumption.

One important distinction: coroutines live in control flow, not purely in the type system. The type system still
captures the multiplicity, optionality, and fallibility of a coroutine, but destructuring a yielded value requires a
block designated for coroutines. Second-class borrows force this. For example, `Opt[&T]` can't exist as a type, because
it requires `Some[&T]`, which requires a borrow type attribute and breaks the second-class borrow rules. The
[**section**]() below covers the `iter` block.

## Coroutine return types

S++ has four possible coroutine return types:

- `Gen[Yield, Send=Void]`
- `GenOnce[Yield, Send=Void]`
- `GenOpt[Yield, Send=Void]`
- `GenRes[Yield, Err, Send=Void]`

The standard generator type, `Gen`, only ever reaches two states: it yields a value of type `Yield`, or it becomes
exhausted with no more values. Every value that a `Gen` coroutine yields is valid and present, and takes the `Yield`
type. The `iter` block can match on value and exhaustion.

The `GenOnce` type suits a coroutine that yields a single value and then becomes exhausted, such as indexing a value in
a vector. Calling `.res()` on a `GenOnce` coroutine yields a value and auto-unwraps it into the inner value. `GenOpt`
and `GenRes` can't be one-hit, because their no-value and error states need handling. A one-hit generator that starts
out exhausted panics.

The `GenOpt` type suits a coroutine that might yield a "no-value" state. Getting an element from a vector works this
way, because the bounds check might fail and needs to signal a "no-value" result. A value from a `GenOpt` coroutine is
either a value of type `Yield` or a "no-value" state, which the type system represents as `Void`. The `iter` block can
match on the value state, the "no-value" state, and exhaustion.

The `GenRes` type suits a coroutine that might yield a value of type `Yield`, or an error of type `Err`. Fallible
generators use it, where the result is either a value or an error. A value from a `GenRes` coroutine is either a value
of type `Yield` or an error of type `Err`. The `iter` block can match on value, error and exhaustion.

The argument for the `Yield` generic parameter determines what the coroutine yields. The compiler could infer this, but
S++ marks it explicitly in the return type, in the same way that a subroutine's return type always appears explicitly,
despite being inferable from `ret` statements.

The generic argument for the `Yield` parameter can carry a convention. Coroutines can yield borrows, so the yielding
convention must match the convention of the `Yield` argument. For example, `gen` expressions such as `gen &mut 123`
give the generator type `Gen[&mut S32]`. The function signature alone conveys both the convention and the yielded
type.

The `Send` generic parameter names what the caller sends back into the coroutine. It defaults to `Void`, which
forbids sending anything back, because no variable can hold `Void`, but it accepts any type. Only owned objects can go
back into a coroutine. A `Void` parameter type disappears from the signature, so `.res()` takes no arguments and
produces an argument-free internal `.send()` call.

## Advancing a generator

The `.res()` method advances a generator. This needs a compiler intrinsic, so `res` is a callable postfix keyword
rather than a plain method. Internally it calls the private `.send()` method on the generator, passing in data when the
`Send` type isn't `Void`.

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

## Passing data out of a coroutine

As shown in the preceding section, the `gen` expression passes data out of a coroutine, with an optional convention.
Yielding borrows from coroutines forms the basis for iteration: it lets the caller borrow and use elements of a vector,
for example.

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&S32] {
    gen &a
    gen &b
    gen &c
}
```

## Invalidating borrows

Yielded borrows belong to the generator that yields them, the **yielder** context. As with function arguments coming
_into_ a function, the law of exclusivity applies to yields, which prevents access to more than one mutable part of a
generator where those parts might overlap. A mutably borrowed yield invalidates the previous mutably borrowed yield in
the **receiver** context.

The yielder always owns immutable borrows, whereas ownership of mutable borrows transfers temporarily to the receiver,
following standard borrow semantics. Each consecutive mutable yield invalidates the previous one in the receiver and
hands ownership back to the yielder, which makes the symbol available in the yielder again. Immutable borrows don't
invalidate each other in the receiver, so they stay pinned in the yielder until the end of the coroutine.

---

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&S32] {
    gen &a
    gen &b  # symbol "a" is usable but is pinned.
    gen &c  # symbol "b" is usable but is pinned.

    # At this point, all 3 symbols, "a", "b" and "c" are pinned in the this
    # function. They remain fully initialized, and valid in both this function
    # (the yielder) and the receiver function.
}

fun main() -> Void {
    let generator = coroutine(1, 2, 3)
    let a = generator.res()
    let b = generator.res()  # "a" internal value still valid here (immutable borrow)
    let c = generator.res()  # "b" internal value still valid here (immutable borrow)
}
```

With immutable borrows, the receiver can take many yielded values and every one stays valid. None of them can mutate
the underlying data, so overlaps are harmless and no mutability conflict can arise. In the yielder context these values
stay pinned, so the yielder can't consume them while the receiver borrows them.

---

```S++
cor coroutine(a: S32, b: S32, c: S32) -> Gen[Yield=&mut S32] {
    gen &mut a
    gen &mut b  # control of "a" is regained here + pin "a" released
    gen &mut c  # control of "b" is regained here + pin "b" released

    # Because control is regained, "a" is pinned until the next yield ("b").
    # At this point, only "c" is pinned. It gets unpinned when the generator
    # for this coroutine is destroyed, but at that point "c" is no longer
    # usable anyway.
}

fun main() -> Void {
    let mut generator = coroutine(1, 2, 3)
    let a = generator.res()
    let b = generator.res()  # a is invalidated here (mutable borrow)
    let c = generator.res()  # b is invalidated here (mutable borrow)
}
```

Mutable borrows follow a stricter invalidation policy. Yielding a mutable borrow invalidates the last yielded
mutable borrow, because nothing else guarantees that the yielded data doesn't overlap. An alternative model would check
that all yields in the yielder are non-overlapping, and the receiver would then never invalidate previous yields, but
that model would stop the yielder from yielding the same object twice, which is a common use case. Because the yielder
regains control at each yield, the coroutine holds the object as singly owned, so yielded symbols stay pinned only
until the following yield.

## Passing data into a coroutine

The `.res()` method takes a single argument, whose type matches the `Send` generic parameter of the generator, meaning
the coroutine return type. Substituting `Void` as a generic parameter removes function parameters of that type from
the signature, so plain `.res()` works for the default `Send=Void` generic parameter.

A coroutine receives values by placing the `gen` expression on the right-hand side of a variable definition statement.
Variables always move into a coroutine rather than borrowing, so receiving a second value into the coroutine doesn't
invalidate the first.

```S++
cor coroutine() -> Gen[Yield=S32, Send=S32] {
    let a = gen 1
    let b = gen 2
    let c = gen 3
    gen a + b + c
}

fun main() -> Void {
    let mut generator = coroutine()
    let a = generator.res(1)
    let b = generator.res(2)
    let c = generator.res(3)
    let t = generator.res(0)
}
```

## Borrowing data into a coroutine

Borrows with coroutines raise a problem. A borrow passes into a coroutine, the coroutine suspends, the caller context
consumes the owned object, and the coroutine then uses the borrow. Memory pinning prevents this.

Every borrow into a coroutine gets pinned automatically. Pinning stops the caller context from consuming the borrow
until something releases the pin manually. Releasing a pin invalidates any object that relies on it, so when a coroutine
has received a borrow and the memory behind that borrow gets released, the coroutine becomes consumed and unusable.

## Chaining coroutines

Coroutine chaining lets a coroutine yield the entirety of another coroutine in a one-line expression, with no loop. This
matches Python's `yield from` statement. S++ spells it `gen with`, which generates values with another coroutine.

The following example shows the same code written with a loop and with coroutine chaining:

```S++
cor coroutine() -> Gen[Yield = S32] {
    gen 1
    gen 2
    gen 3
}

cor coroutine_chain() -> Gen[Yield = S32] {
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

## The `GenOnce` type

The generator types `Gen`, `GenOpt` and `GenRes` all create wrapper `GeneratedXXX` types, which may contain the yielded
value, and which the `iter` block can inspect, as described earlier. Methods like `.index_ref()` yield exactly one
value, possibly a borrow, and would always need an `iter` block matching on a present value, which is inconvenient. The
`GenOnce` type exists for that case: it yields a single value and unwraps it automatically. Automatic unwrapping also
removes the need for `.res()`. Without and with `GenOnce`:

```S++
cor get_value() -> GenOnce[Yield = &S32] {
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

To keep this compatible with the memory model, pinning covers not just the generator, meaning whatever sits directly
left of `.res()`, but the entire expression containing the generator. The following code shows the effect:

```S++
cls MyType { }

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
