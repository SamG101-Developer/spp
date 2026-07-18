#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_simple, R"(
    cls A {
        !public b: Bool
    }

    fun f() -> Void {
        let a = A(b=true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    cls A {
        !public b: MyVec[Bool]
    }

    fun f() -> Void {
        let a = A(b=MyVec[Bool]::from([true, false, true]))
        let b = A(b=Vec[Bool]::from([false, false]))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_number, R"(
    cls A {
        !public b: S32
    }

    fun f() -> Void {
        let a = A(b=123)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupFunctionNameAlias,
    test_valid_simple, R"(
    sup Bool {
        !public fun test(&self) -> Bool {
            ret self@
        }
    }

    fun f() -> Void {
        let b = true
        let mut c = b.test()
        c = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupFunctionNameAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    sup [XX] MyVec[XX] {
        !public fun test(&self) -> USize {
            ret self.len()
        }
    }

    fun f() -> Void {
        let v = MyVec[S32]::from([1, 2, 3, 4, 5])
        let mut len = v.test()
        len = 0_uz

        let v = Vec[Bool]::from([true, false])
        let mut len = v.test()
        len = 0_uz
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupFunctionNameAlias,
    test_valid_number, R"(
    sup S32 {
        !public fun test(&self) -> S32 {
            ret self@ + 1
        }
    }

    fun f() -> Void {
        let n = 42
        let mut m = n.test()
        m = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_simple, R"(
    sup S32 ext From[Str] {
        !public fun from(that: Str) -> Self {
            ret 0
        }
    }

    fun f() -> Void {
        let s = Str::from("123")
        let mut n = S32::from(s)
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    sup MyVec[S32] ext From[Str] {
        !public fun from(that: Str) -> Self {
            ret MyVec[S32]::new()
        }
    }

    fun f() -> Void {
        let s = Str::from("1,2,3")
        let mut v = MyVec[S32]::from(s)
        v = Vec[S32]::new()

        let s = Str::from("1,2,3")
        let mut v = Vec[S32]::from(s)
        v = MyVec[S32]::new()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_number, R"(
    sup Str ext From[S32] {
        !public fun from(that: S32) -> Self {
            ret Str::from("test")
        }
    }

    fun f() -> Void {
        let n = 42
        let mut s = Str::from(n)
        s = Str::from("changed")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionReturnTypeAlias,
    test_valid_alias_simple, R"(
    fun f() -> Bool {
        ret true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionReturnTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f() -> MyVec[Bool] {
        ret case true {
            MyVec[Bool]::from([true, false])
        }
        else {
            Vec[Bool]::from([false, true])
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionReturnTypeAlias,
    test_valid_alias_number, R"(
    fun f() -> S32 {
        ret 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_simple, R"(
    fun g(b: Bool) -> Void {
    }

    fun f() -> Void {
        g(true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun g(v: MyVec[Bool]) -> Void {
    }

    fun f() -> Void {
        g(MyVec[Bool]::from([true, false, true]))
        g(Vec[Bool]::from([false, true]))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_number, R"(
    fun g(n: S32) -> Void {
    }

    fun f() -> Void {
        g(123)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_simple, R"(
    fun g[cmp b: Bool]() -> Void {
    }

    fun f() -> Void {
        g[true]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun g[cmp v: MyVec[Bool]]() -> Void {
    }

    fun f() -> Void {
        g[MyVec[Bool]()]()
        g[Vec[Bool]()]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_number, R"(
    fun g[cmp n: S32]() -> Void {
    }

    fun f() -> Void {
        g[123]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_simple, R"(
    fun g[T=Bool]() -> T {
        ret T()
    }

    fun f() -> Void {
        let mut b = g()
        b = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun g[T=MyVec[Bool]]() -> T {
        ret T()
    }

    fun f() -> Void {
        let mut v1 = g()
        v1 = MyVec[Bool]::new()

        let mut v2 = g()
        v2 = Vec[Bool]::new()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_number, R"(
    fun g[T=S32]() -> T {
        ret T()
    }

    fun f() -> Void {
        let mut n = g()
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_simple, R"(
    fun g[T]() -> T {
        ret T()
    }

    fun f() -> Void {
        let mut b = g[Bool]()
        b = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun g[T]() -> T {
        ret T()
    }

    fun f() -> Void {
        let mut v1 = g[MyVec[Bool]]()
        v1 = MyVec[Bool]::new()

        let mut v2 = g[MyVec[Bool]]()
        v2 = Vec[Bool]::new()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_number, R"(
    fun g[T]() -> T {
        ret T()
    }

    fun f() -> Void {
        let mut n = g[S32]()
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCaseDestructureObjectTypeAlias,
    test_valid_alias_simple, R"(
    fun f(s: Str, t: Str) -> Void {
        case s is Str(mut data) {
            data = Vec[std::number::U8]::new()
        }
        case t is Str(mut data) {
            data = Vec[std::number::U8]::new()
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCaseDestructureObjectTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f(v: MyVec[U8], v2: MyVec[U8]) -> Void {
        case v is MyVec[U8](mut buf) {
            buf = Slice[U8]()
        }
        case v2 is Vec[U8](mut buf) {
            buf = Slice[U8]()
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCaseDestructureObjectTypeAlias,
    test_valid_alias_number, R"(
    fun f(n: S32) -> Void {
        case n is S32() { }
        case n is std::number::S32() { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_simple, R"(
    fun f(s: Str) -> Void {
        let Str(mut data) = s
        data = Vec[std::number::U8]::new()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f(v: MyVec[U8]) -> Void {
        let MyVec[U8](mut buf) = v
        buf = Slice[U8]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_number, R"(
    fun f(n: S32) -> Void {
        let S32() = n
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_simple, R"(
    type Tool = Bool

    fun f(mut t: Tool) -> Void {
        t = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_complex, R"(
    type VVec[ZZ] = Vec[ZZ]
    type Tool = Bool
    type BoolVec = VVec[Tool]

    fun f(mut v: BoolVec) -> Void {
        v = VVec[Bool]::new()
        v = Vec[Bool]::new()
        v = std::vector::Vec[Bool]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_number, R"(
    type Tool = S32

    fun f(mut n: Tool) -> Void {
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_simple, R"(
    cmp value: Bool = true

    fun f() -> Void {
        let mut x = value
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    cmp value: MyVec[Bool] = Vec[Bool]()

    fun f() -> Void {
        let mut l = value.len()
        l = 0_uz
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_number, R"(
    cmp value: S32 = 123

    fun f() -> Void {
        let mut n = value
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_simple, R"(
    fun f() -> Void {
        let mut b: Bool
        b = true
        b = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f() -> Void {
        let mut v: MyVec[Bool]
        v = MyVec[Bool]::from([true, false])
        v = Vec[Bool]::from([false, true])
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_number, R"(
    fun f() -> Void {
        let mut n: S32
        n = 123
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_simple, R"(
    fun f() -> Void {
        let mut b: Bool = true
        b = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f() -> Void {
        let mut v: MyVec[Bool] = MyVec[Bool]::from([true, false])
        v = Vec[Bool]::from([false, true])
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_number, R"(
    fun f() -> Void {
        let mut n: S32 = 123
        n = 0
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_simple, R"(
    fun f() -> Void {
        let b: Bool = Bool()
        let c: Bool = b
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f() -> Void {
        let v1: MyVec[Bool] = MyVec[Bool]::new()
        let v2: Vec[Bool] = v1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_number, R"(
    fun f() -> Void {
        let n: S32 = S32()
        let m: std::number::S32 = n
    }
)");

// --- Type-shorthand positions: an alias must work inside tuple/array/variant/function types. ---

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleTypeAlias,
    test_valid_simple, R"(
    fun f(mut t: (Bool, Bool)) -> Void {
        t = (true, false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f(mut t: (MyVec[Bool], Bool)) -> Void {
        t = (MyVec[Bool]::new(), false)
        t = (Vec[Bool]::new(), true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleTypeAlias,
    test_valid_number, R"(
    fun f(mut t: (S32, S32)) -> Void {
        t = (1, 2)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestArrayTypeAlias,
    test_valid_simple, R"(
    fun f(mut a: [Bool; 2_uz]) -> Void {
        a = [true, false]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestArrayTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f(mut a: [MyVec[Bool]; 2_uz]) -> Void {
        a = [MyVec[Bool]::new(), Vec[Bool]::new()]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestArrayTypeAlias,
    test_valid_number, R"(
    fun f(mut a: [S32; 3_uz]) -> Void {
        a = [1, 2, 3]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypeAlias,
    test_valid_simple, R"(
    fun f(mut x: Bool or S32) -> Void {
        x = true
        x = 123
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f(mut x: MyVec[Bool] or Bool) -> Void {
        x = MyVec[Bool]::new()
        x = Vec[Bool]::new()
        x = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestVariantTypeAlias,
    test_valid_number, R"(
    fun f(mut x: S32 or Bool) -> Void {
        x = 123
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionTypeAlias,
    test_valid_simple, R"(
    fun f(g: FunRef[(Bool,), Bool]) -> Void {
        let mut r = g(true)
        r = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = Vec[ZZ]

    fun f(g: FunRef[(MyVec[Bool],), Bool]) -> Void {
        let mut r = g(Vec[Bool]::new())
        r = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionTypeAlias,
    test_valid_number, R"(
    fun f(g: FunRef[(S32,), S32]) -> Void {
        let mut r = g(1)
        r = 0
    }
)");

// --- Generic constraint position: an alias must work as a `[T: Alias]` constraint. ---

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericConstraintTypeAlias,
    test_valid_simple, R"(
    fun g[T: Copy](t: T) -> Void { }

    fun f() -> Void {
        g(true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericConstraintTypeAlias,
    test_valid_complex, R"(
    cls Base { }
    type BaseAlias = Base

    cls Derived { }
    sup Derived ext Base { }

    fun g[T: BaseAlias](t: T) -> Void { }

    fun f() -> Void {
        let d = Derived()
        g(d)
    }
)");

// --- Nested type access through an alias: `Alias::Inner` must resolve like `Underlying::Inner`. ---

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestNestedTypeAccessAlias,
    test_valid_simple, R"(
    cls Holder { }
    sup Holder {
        !public type Inner = Bool
    }

    type HolderAlias = Holder

    fun f(mut x: HolderAlias::Inner) -> Void {
        x = true
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestNestedTypeAccessAlias,
    test_valid_complex, R"(
    cls Holder[T] { }
    sup [T] Holder[T] {
        !public type Inner = T
    }

    type HolderAlias[T] = Holder[T]

    fun f(mut x: HolderAlias[Bool]::Inner) -> Void {
        x = true
    }
)");
