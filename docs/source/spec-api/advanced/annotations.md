# Annotations

Annotations resemble Python's decorators, or Rust's attributes. They attach metadata or behaviour to a construct, such
as a function, a class, or a class field. Annotations take the form of functions, and carry a specific context type that
provides the behaviour.

The standard library defines these annotations in `annotations.spp`, among others:

```s++

!annotation(target=Annotation::function)
!compiler_builtin
!public
cmp fun annotation(target: S32) -> Void { }

!annotation(target=Annotation::function | Annotation::method | Annotation::ext_method | Annotation::class)
!compiler_builtin
!public
cmp fun compiler_builtin() -> Void { }

!annotation(target=Annotation::mod)
!compiler_builtin
!public
cmp fun public() -> Void { }
```

## Defining an annotation

Defining an annotation requires the `!annotation` attribute. The prelude includes it, so it's always available. The
`target` parameter gives the annotation's context type, which determines where the annotation can apply. The
function must return `Void`, and takes any parameters.

An annotation must be a function, and specifically a compile-time function. That keeps the state changes known at
compile time, which the compiler needs before it can use the annotation for code generation and other purposes. The
`cmp` keyword defines a compile-time function.

## Annotation targets

Annotations can use the following targets:

| Target                   | Description                                     |
|--------------------------|-------------------------------------------------|
| `Annotation::function`   | Applies to free functions.                      |
| `Annotation::method`     | Applies to methods in a `sup` block.            |
| `Annotation::ext_method` | Applies to methods in a `sup-ext` block.        |
| `Annotation::class`      | Applies to classes.                             |
| `Annotation::new_type`   | Applies to type statement aliases.              |
| `Annotation::constant`   | Applies to cmp constant declarations.           |

Some combined targets also exist:

```s++
cmp all: S32 = Self::function | Self::method | Self::ext_method | Self::class | Self::new_type | Self::constant
cmp mod: S32 = Self::function | Self::method | Self::class | Self::new_type | Self::constant
```
