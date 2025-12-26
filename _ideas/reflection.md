# Reflection

The reflection API will be builtin to the std library, and will allow compile-time introspection of types, functions,
variables, modules, etc. Will probably need `cmp` function support first, to allow compile-time execution of reflection
functions.

## API

```
fun f(a: S32, b: Bool) -> F32 {
    ret 0.0
}

cls MyStruct {
    x: S32
    y: F32
}

fun test() -> Void {
    # Functions
    type RetType = std::reflect::MetaFun::RetType              # F32
    type Param1Type = std::reflect::MetaFun::ParamType[0]      # S32
    type Param2Type = std::reflect::MetaFun::ParamType[1]      # Bool
    type ParamAllTypes = std::reflect::MetaFun[f]::ParamTypes  # as a tuple type
    
    # Structs
    type Field1Type = std::reflect::MetaType[MyStruct]::FieldTypes[0]  # S32
    type Field2Type = std::reflect::MetaType[MyStruct]::FieldTypes[1]  # F32
    let field_1_name = std::reflect::MetaType[MyStruct]::FieldName[0]  # "x"
    let field_2_name = std::reflect::MetaType[MyStruct]::FieldName[1]  # "y"
}
```

```
cls MetaType[T] {
    type FieldTypes[cmp n: S32] = MetaFieldType[T, n]
    type FieldName[cmp n: S32] = MetaFieldName[T, n]
    cmp FieldCount: S32 = ...  # implementation defined ?
}


cls MetaFun[cmp f: F, F] {
    type RetType = MetaRetType[F]
    type ParamType[cmp n: S32] = MetaParamType[F, n]
    type ParamTypes = MetaParamTypes[F]
}


@compiler_builtin
type MetaFieldType[T, cmp n: S32] = ...  # implementation defined

@compiler_builtin
type MetaFieldName[T, cmp n: S32] = ...  # implementation defined

@compiler_builtin
type MetaRetType[F] = ...  # implementation defined

@compiler_builtin
type MetaParamType[F, cmp n: S32] = ...  # implementation defined

@compiler_builtin
type MetaParamTypes[F] = ...  # implementation defined
```
