# Annotations

Annotations are similar to Python's decorators, or Rust's attributes. They are a way to attach metadata or behaviour to
a construct, such as a function, class, or class field for example. Attributes are defined as functions, and have a
specific context type which provides functionality.

There are several annotations defined in the standard library, in `annotations.spp`, including:

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

## Defining an Annotation

To define an annotation, the `!annotation` attribute must be used. It is included in the prelude, so it is always
available. The `target` parameter specifies the context type of the annotation, which determines where the annotation
can be applied. The function must return `Void`, and can have any parameters.

Not only must an annotation be a function, it must be a compile-time function. This allows for the state changes to be
known at compile time, which is necessary for the compiler to be able to use the annotation for code generation and
other purposes. The `cmp` keyword can be used to define a compile-time function.

## Annotation Targets

The following targets are available for annotations:

| Target                   | Description                                                    |
|--------------------------|----------------------------------------------------------------|
| `Annotation::function`   | The annotation can be applied to free functions.               |
| `Annotation::method`     | The annotation can be applied to methods in a `sup` block.     |
| `Annotation::ext_method` | The annotation can be applied to methods in a `sup-ext` block. |
| `Annotation::class`      | The annotation can be applied to classes.                      |
| `Annotation::new_type`   | The annotation can be applied to type statement aliases.       |
| `Annotation::constant`   | The annotation can be applied to cmp constant declarations.    |

There are also some combined targets available:

```s++
cmp all: S32 = Self::function | Self::method | Self::ext_method | Self::class | Self::new_type | Self::constant
cmp mod: S32 = Self::function | Self::method | Self::class | Self::new_type | Self::constant
```

