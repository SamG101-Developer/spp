# Iteration

## Iterators

S++ uses coroutines for iteration, which gives universal iterator classes rather than a new iterator type per container.
Iteration adaptors such as `Filter`, `Transform` and `Take` need no types of their own either: each one is a coroutine
on the `Iterator` type. This heavily simplifies the STL, which needs no set of iterator types at all.

The `BidirectionalIterator` type contains methods for reverse iteration. The key method is the `rev` adaptor, which
flips the coroutine and then iterates forwards again, yielding values in reverse. `BidirectionalIterator` superimposes
the `Iterator` type, which makes all other adaptor methods available.

## Accessing iterators

To reach the `Iterator` type, a class can superimpose any of these types:

- `IterRef[I]`
- `IterMut[I]`
- `IterMov[I]`

The `IterRef` type gives iteration by reference, yielding references to the values in the container. It contains the
method `iter_ref`, a coroutine that yields values into `Iterator[&T]`, which in turn superimposes `GenOpt[&T]`. This
method borrows the container immutably.

The `IterMut` type gives iteration by mutable reference, yielding mutable references to the values in the container. It
contains the method `iter_mut`, a coroutine that yields values into `Iterator[&mut T]`, which in turn superimposes
`GenOpt[&mut T]`. This method borrows the container mutably.

The `IterMov` type gives iteration by value, yielding values moved out of the container. It contains the method
`iter_mov`, a coroutine that yields values into `Iterator[T]`, which in turn superimposes `GenOpt[T]`. This method takes
ownership of the container and consumes it.

## Sized iterators

The `SizedIterator` type optimizes iteration for types like `Vec`, which have a known size. It provides a `size_hint`
method that returns the number of elements in the collection, which enables iteration optimizations.

## Example

This is the iteration code for the `Vec` type, which provides all six iterator methods: by value, by mutable reference,
and by immutable reference, both forwards and backwards.

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

## How the compiler transforms iteration

For iterators, meaning types that superimpose a `Gen` variant, the iteration variant of `loop` uses a plain `loop-in`
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

In S++, the transformation stays fully internal to the compiler, because coroutines drive external iteration. Each
iteration injects LLVM IR with the same semantics as the following S++:

```s++
loop it in generator {
    iter it of {
        variable { ... }
        !! { exit }
    }
}
```

This uses the same number of instructions as the C++ iterator comparison per iteration, but the iterator itself is a
coroutine state machine. Raw LLVM IR manages the generator internals, so the `!!` exhaustion check hooks into the IR
state tracker. Rather than comparing two pointers, the check reads the state of the coroutine to see whether a flag
marks it exhausted.
