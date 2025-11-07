# Iteration

## Iterators

S++ uses coroutines for iteration. This allows universal iterator classes, rather than a new iterator type per a
container. Furthermore, all iteration adaptors, like `Filter`, `Transform`, `Take` etc, don't need their own types; they
are just implemented as coroutines on the `Iterator` type. This allows the STL to be heavily simplified, as there is no
need for a whole set of iterator types.

There is a `BidirectionalIterator` type, which contains methods that allow for reverse iteration. The key method is the
`rev` adaptor, allowing the coroutine to be flipped, and the iterated forwards again, yielding values in reverse. The
`BidirectionalIterator` superimposes the `Iterator` type, making all other adaptor methods available.

## Accessing Iterators

To access the `Iterator` type, the following types may be superimposed over a class:

- `IterRef[I]`
- `IterMut[I]`
- `IterMov[I]`

The `IterRef` type allows iteration by reference, yielding references to the values in the container. It contains the
method `iter_ref`, which is a coroutine that yields values into `Iterator[&T]`, which in turn superimposes `GenOpt[&T]`.
This method borrows the container immutably.

The `IterMut` type allows iteration by mutable reference, yielding mutable references to the values in the container. It
contains the method `iter_mut`, which is a coroutine that yields values into `Iterator[&mut T]`, which in turn
superimposes `GenOpt[&mut T]`. This method borrows the container mutably.

The `IterMov` type allows iteration by value, yielding values moved out of the container. It contains the method
`iter_mov`, which is a coroutine that yields values into `Iterator[T]`, which in turn superimposes `GenOpt[T]`. This
method takes ownership of the container (consumes it).

## Sized Iterators

To optimize iteration for types like `Vec`, which have a known size, there is the `SizedIterator` type. This type
provides a `size_hint` method, that returns a number of elements in the collection. This allows for iteration
optimizations.

## Example

This is the iteration code for the `Vec` type, which provides all six iterator methods: by value, by mutable
reference, and by immutable reference, both forwards and backwards.

```s++
sup [T, A] Vec[T, A] {
    cor iter_mov(self) -> Iterator[T] {
        gen with self.buf.iter_mov()
    }

    cor iter_mut(&mut self) -> Iterator[&mut T] {
        gen with self.buf.iter_mut()
    }

    cor iter_ref(&self) -> Iterator[&T] {
        gen with self.buf.iter_ref()
    }

    cor reverse_iter_mov(self) -> Iterator[T] {
        gen with self.buf.reverse_iter_mov()
    }

    cor reverse_iter_mut(&mut self) -> Iterator[&mut T] {
        gen with self.buf.reverse_iter_mut()
    }

    cor reverse_iter_ref(&self) -> Iterator[&T] {
        gen with self.buf.reverse_iter_ref()
    }
}



sup [T, A] Vec[T, A] ext SizedIterator[T] {
    fun size_hint(&self) -> USize {
        ret self.len
    }
}
```

## How Iteration is Transformed

For iterators (types that superimpose a `Gen` variant), the iteration variant of `loop` is used with a simple `loop-in`
syntax:

```s++
loop x in generator {
    ...
}
```

In C++, the transformation is:

```CPP
for (auto it: generator) {
    auto x = *it;
}

auto __begin = generator.begin();
auto __end = generator.end();
for (auto __it = __begin; __it != __end; ++__it) {
    auto x = *__it;
}
```

In S++, the transformation is fully internal to the compiler, because coroutines are used to do external iteration. Per
iteration, LLVM IR is injected, with the same semantics as the following S++:

```s++
loop it in generator {
    iter it of {
        variable { ... }
        !! { exit }
    }
}
```

This will use the same number of instructions as C++ comparing iterators per iteration, but the iterator itself is a
coroutine state machine.
