# Generators

## Ambiguous

Currently, if a type is iterated and there is more than one inherited generator type, it is an error. This is to prevent
ambiguity. However, if there is a return known, then the generator may be selectable using return type overloading
resolution rules.

## Generators V2

- Iterators are defined as generators, still, but the `Send` parameter is no longer `Void`. Instead, we use a `IterDir`
  type, which is aliased to `type IterDir = IterFwd or IterBwd or IterRng`.
- Then, we define 3 helper methods to wrap around `.res(...)` calls:
    - `next()` which calls `.res(IterFwd())`
    - `prev()` which calls `.res(IterBwd())`
    - `jump(n: SSize)` which calls `.res(IterRng(n))`
- This allows for more flexible generator definitions, where a generator can support multiple iteration directions,
  without needing to restart the iteration.

```s++
let v = Vec[S32]::from([1, 2, 3])
let it = v.iter_ref()

loop val in it {
    std::io::print(val)  # prints 1, 2, 3
}

let it2 = v.iter_ref()
let val = it2.jump(2)@
```
