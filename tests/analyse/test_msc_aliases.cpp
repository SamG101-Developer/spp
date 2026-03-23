#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_simple, R"(
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
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    cls A {
        b: MyVec[Bool]
    }

    fun f() -> std::void::Void {
        let a = A(b=MyVec[Bool]::from([true, false, true]))
        let b = A(b=std::vector::Vec[Bool]::from([false, false]))
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestClassAttributeAlias,
    test_valid_number, R"(
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
    sup Bool {
        fun test(&self) -> Bool {
            ret self@
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
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    sup [XX] MyVec[XX] {
        fun test(&self) -> USize {
            ret self.length()
        }
    }

    fun f() -> std::void::Void {
        let v = MyVec[S32]::from([1, 2, 3, 4, 5])
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
    sup S32 {
        fun test(&self) -> S32 {
            ret self@ + 1
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
    sup S32 ext From[S32, std::string::Str] {
        fun from(that: std::string::Str) -> S32 {
            ret 0
        }
    }

    fun f() -> std::void::Void {
        let s = std::string::Str::from("123")
        let mut n = S32::from(s)
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    sup MyVec[S32] ext From[MyVec[S32], std::string::Str] {
        fun from(that: std::string::Str) -> MyVec[S32] {
            ret MyVec[S32]::new()
        }
    }

    fun f() -> std::void::Void {
        let s = std::string::Str::from("1,2,3")
        let mut v = MyVec[S32]::from(s)
        v = std::vector::Vec[S32]::new()

        let s = std::string::Str::from("1,2,3")
        let mut v = std::vector::Vec[S32]::from(s)
        v = MyVec[S32]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestSupExtensionSuperClassAlias,
    test_valid_number, R"(
    sup Str ext From[Str, S32] {
        fun from(that: S32) -> Str {
            ret Str::from("test")
        }
    }

    fun f() -> std::void::Void {
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
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun f() -> MyVec[Bool] {
        ret case true {
            MyVec[Bool]::from([true, false])
        }
        else {
            std::vector::Vec[Bool]::from([false, true])
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
    fun g(b: Bool) -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g(true)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun g(v: MyVec[Bool]) -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g(MyVec[Bool]::from([true, false, true]))
        g(std::vector::Vec[Bool]::from([false, true]))
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFunctionParameterTypeAlias,
    test_valid_alias_number, R"(
    fun g(n: S32) -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g(123)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_simple, R"(
    fun g[cmp b: Bool]() -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g[true]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun g[cmp v: MyVec[Bool]]() -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g[MyVec[Bool]()]()
        g[std::vector::Vec[Bool]()]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterCompTypeAlias,
    test_valid_alias_number, R"(
    fun g[cmp n: S32]() -> std::void::Void {
    }

    fun f() -> std::void::Void {
        g[123]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_simple, R"(
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
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun g[T=MyVec[Bool]]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut v1 = g()
        v1 = MyVec[Bool]::new()

        let mut v2 = g()
        v2 = std::vector::Vec[Bool]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericParameterTypeOptionalDefaultTypeAlias,
    test_valid_alias_number, R"(
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
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun g[T]() -> T {
        ret T()
    }

    fun f() -> std::void::Void {
        let mut v1 = g[MyVec[Bool]]()
        v1 = MyVec[Bool]::new()

        let mut v2 = g[MyVec[Bool]]()
        v2 = std::vector::Vec[Bool]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestGenericArgumentTypeTypeAlias,
    test_valid_number, R"(
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
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun f(v: MyVec[U8], v2: MyVec[U8]) -> std::void::Void {
        case v is MyVec[U8](mut buf) {
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
    fun f(n: S32) -> std::void::Void {
        case n is S32() { }
        case n is std::number::S32() { }
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_simple, R"(
    fun f(s: Str) -> std::void::Void {
        let Str(mut data) = s
        data = std::vector::Vec[std::number::U8]::new()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun f(v: MyVec[U8]) -> std::void::Void {
        let MyVec[U8](mut buf) = v
        buf = Slice[U8]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLocalVariableDestructureObjectTypeAlias,
    test_valid_alias_number, R"(
    fun f(n: S32) -> std::void::Void {
        let S32() = n
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementTypeAlias,
    test_valid_simple, R"(
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
    type Tool = S32

    fun f(mut n: Tool) -> std::void::Void {
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_simple, R"(
    cmp value: Bool = true

    fun f() -> std::void::Void {
        let mut x = value
        x = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    cmp value: MyVec[Bool] = Vec[Bool]()

    fun f() -> std::void::Void {
        let mut l = value.length()
        l = 0_uz
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestCmpStatementTypeAlias,
    test_valid_number, R"(
    cmp value: S32 = 123

    fun f() -> std::void::Void {
        let mut n = value
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_simple, R"(
    fun f() -> std::void::Void {
        let mut b: Bool
        b = true
        b = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun f() -> std::void::Void {
        let mut v: MyVec[Bool]
        v = MyVec[Bool]::from([true, false])
        v = std::vector::Vec[Bool]::from([false, true])
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementUninitializedTypeAlias,
    test_valid_number, R"(
    fun f() -> std::void::Void {
        let mut n: S32
        n = 123
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_simple, R"(
    fun f() -> std::void::Void {
        let mut b: Bool = true
        b = false
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun f() -> std::void::Void {
        let mut v: MyVec[Bool] = MyVec[Bool]::from([true, false])
        v = std::vector::Vec[Bool]::from([false, true])
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestLetStatementInitializedTypeAlias,
    test_valid_number, R"(
    fun f() -> std::void::Void {
        let mut n: S32 = 123
        n = 0
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_simple, R"(
    fun f() -> std::void::Void {
        let b: Bool = Bool()
        let c: std::boolean::Bool = b
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_complex, R"(
    type MyVec[ZZ] = std::vector::Vec[ZZ]

    fun f() -> std::void::Void {
        let v1: MyVec[Bool] = MyVec[Bool]::new()
        let v2: std::vector::Vec[Bool] = v1
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestObjectInitializerTypeAlias,
    test_valid_number, R"(
    fun f() -> std::void::Void {
        let n: S32 = S32()
        let m: std::number::S32 = n
    }
)");
