#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool

    cls A {
        b: Bool
    }

    fun f() -> std::void::Void {
        let a = A(b=true)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    cls A {
        b: Vec[Bool]
    }

    fun f() -> std::void::Void {
        let a = A(b=Vec[Bool]::from([true, false, true]))
        let b = A(b=std::vector::Vec[Bool]::from([false, false]))
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_number, R"(
    use std::number::S32

    cls A {
        b: S32
    }

    fun f() -> std::void::Void {
        let a = A(b=123)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupFunctionNameAlias,
    test_valid_simple, R"(
    use std::boolean::Bool

    sup Bool {
        fun test(&self) -> Bool {
            ret *self
        }
    }

    fun f() -> std::void::Void {
        let b = true
        let mut c = b.test()
        c = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupFunctionNameAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool
    use std::number::USize
    use std::number::S32

    sup [XX] Vec[XX] {
        fun test(&self) -> USize {
            ret self.length()
        }
    }

    fun f() -> std::void::Void {
        let v = Vec[S32]::from([1, 2, 3, 4, 5])
        let mut len = v.test()
        len = 0_uz

        let v = std::vector::Vec[Bool]::from([true, false])
        let mut len = v.test()
        len = 0_uz
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupFunctionNameAlias,
    test_valid_number, R"(
    use std::number::S32

    sup S32 {
        fun test(&self) -> S32 {
            ret *self + 1
        }
    }

    fun f() -> std::void::Void {
        let n = 42
        let mut m = n.test()
        m = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_simple, R"(
    use std::cast::From
    use std::number::S32

    sup S32 ext From[S32, std::string::Str] {
        fun from(that: std::string::Str) -> S32 {
            ret 0
        }
    }

    fun f() -> std::void::Void {
        let s = "123"
        let mut n = S32::from(s)
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::cast::From
    use std::number::S32

    sup Vec[S32] ext From[Vec[S32], std::string::Str] {
        fun from(that: std::string::Str) -> Vec[S32] {
            ret Vec[S32]::new()
        }
    }

    fun f() -> std::void::Void {
        let s = "1,2,3"
        let mut v = Vec[S32]::from(s)
        v = std::vector::Vec[S32]::new()

        let s = "1,2,3"
        let mut v = std::vector::Vec[S32]::from(s)
        v = Vec[S32]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_number, R"(
    use std::cast::From
    use std::number::S32
    use std::string::Str

    sup Str ext From[Str, S32] {
        fun from(that: S32) -> Str {
            ret "test"
        }
    }

    fun f() -> std::void::Void {
        let n = 42
        let mut s = Str::from(n)
        s = "changed"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionReturnTypeAlias,
    test_valid_alias_simple, R"(
    use std::boolean::Bool

    fun f() -> Bool {
        ret true
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionReturnTypeAlias,
    test_valid_alias_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun f() -> Vec[Bool] {
        ret case true {
            Vec[Bool]::from([true, false])
        }
        else {
            std::vector::Vec[Bool]::from([false, true])
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionReturnTypeAlias,
    test_valid_alias_number, R"(
    use std::number::S32

    fun f() -> S32 {
        ret 123
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_simple, R"(
    use std::boolean::Bool

    fun g(b: Bool) -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g(true)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun g(v: Vec[Bool]) -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g(Vec[Bool]::from([true, false, true]))
        g(std::vector::Vec[Bool]::from([false, true]))
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_number, R"(
    use std::number::S32

    fun g(n: S32) -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g(123)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_simple, R"(
    use std::boolean::Bool

    fun g[cmp b: Bool]() -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g[true]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun g[cmp v: Vec[Bool]]() -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g[Vec[Bool]()]()
        g[std::vector::Vec[Bool]()]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_number, R"(
    use std::number::S32

    fun g[cmp n: S32]() -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g[123]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_simple, R"(
    use std::boolean::Bool

    fun g[T=Bool]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut b = g()
        b = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun g[T=Vec[Bool]]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut v1 = g()
        v1 = Vec[Bool]::new()

        let mut v2 = g()
        v2 = std::vector::Vec[Bool]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_number, R"(
    use std::number::S32

    fun g[T=S32]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut n = g()
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool

    fun g[T]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut b = g[Bool]()
        b = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun g[T]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut v1 = g[Vec[Bool]]()
        v1 = Vec[Bool]::new()

        let mut v2 = g[Vec[Bool]]()
        v2 = std::vector::Vec[Bool]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_number, R"(
    use std::number::S32

    fun g[T]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut n = g[S32]()
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCaseDestructureObjectTypeAlias,
    test_valid_alias_simple, R"(
    use std::string::Str

    fun f(s: Str, t: Str) -> std::void::Void {
        case s is Str(mut data) {
            data = std::vector::Vec[std::number::U8]::new()
        }
        case t is std::string::Str(mut data) {
            data = std::vector::Vec[std::number::U8]::new()
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCaseDestructureObjectTypeAlias,
    test_valid_alias_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::string::Str
    use std::slice::Slice
    use std::number::U8

    fun f(v: Vec[U8], v2: Vec[U8]) -> std::void::Void {
        case v is Vec[U8](mut buf) {
            buf = Slice[U8]()
        }
        case v2 is std::vector::Vec[U8](mut buf) {
            buf = Slice[U8]()
        }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCaseDestructureObjectTypeAlias,
    test_valid_alias_number, R"(
    use std::number::S32

    fun f(n: S32) -> std::void::Void {
        case n is S32() { }
        case n is std::number::S32() { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_simple, R"(
    use std::string::Str

    fun f(s: Str) -> std::void::Void {
        let Str(mut data) = s
        data = std::vector::Vec[std::number::U8]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::string::Str
    use std::slice::Slice
    use std::number::U8

    fun f(v: Vec[U8]) -> std::void::Void {
        let Vec[U8](mut buf) = v
        buf = Slice[U8]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_number, R"(
    use std::number::S32

    fun f(n: S32) -> std::void::Void {
        let S32() = n
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool
    type Tool = Bool

    fun f(mut t: Tool) -> std::void::Void {
        t = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    type Tool = std::boolean::Bool
    type BoolVec = Vec[Tool]

    fun f(mut v: BoolVec) -> std::void::Void {
        v = std::vector::Vec[std::boolean::Bool]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_number, R"(
    use std::number::S32
    type Tool = S32

    fun f(mut n: Tool) -> std::void::Void {
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool
    cmp value: Bool = true

    fun f() -> std::void::Void {
        let mut x = value
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_complex, R"(
    use std::boolean::Bool
    type Vec[ZZ] = std::vector::Vec[ZZ]

    cmp value: Vec[Bool] = Vec[Bool]()

    fun f() -> std::void::Void {
        let mut l = value.length()
        l = 0_uz
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_number, R"(
    use std::number::S32
    cmp value: S32 = 123

    fun f() -> std::void::Void {
        let mut n = value
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool

    fun f() -> std::void::Void {
        let mut b: Bool
        b = true
        b = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun f() -> std::void::Void {
        let mut v: Vec[Bool]
        v = Vec[Bool]::from([true, false])
        v = std::vector::Vec[Bool]::from([false, true])
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_number, R"(
    use std::number::S32

    fun f() -> std::void::Void {
        let mut n: S32
        n = 123
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool

    fun f() -> std::void::Void {
        let mut b: Bool = true
        b = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun f() -> std::void::Void {
        let mut v: Vec[Bool] = Vec[Bool]::from([true, false])
        v = std::vector::Vec[Bool]::from([false, true])
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_number, R"(
    use std::number::S32

    fun f() -> std::void::Void {
        let mut n: S32 = 123
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_simple, R"(
    use std::boolean::Bool
    fun f() -> std::void::Void {
        let b: Bool = Bool()
        let c: std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_complex, R"(
    type Vec[ZZ] = std::vector::Vec[ZZ]
    use std::boolean::Bool

    fun f() -> std::void::Void {
        let v1: Vec[Bool] = Vec[Bool]::new()
        let v2: std::vector::Vec[Bool] = v1
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_number, R"(
    use std::number::S32

    fun f() -> std::void::Void {
        let n: S32 = S32()
        let m: std::number::S32 = n
    }
)");
