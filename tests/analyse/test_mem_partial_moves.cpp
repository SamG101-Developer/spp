#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPartialMoves,
    test_invalid_assign_attribute_to_non_initialized_value,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str
    use std::number::U8
    use std::vector::Vec

    fun f() -> std::void::Void {
        let mut x: Str
        x.data = Vec[U8]()
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPartialMoves,
    test_invalid_assign_to_non_initialized_attribute,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str

    cls A { str: Str }
    cls B { a: A }

    fun f() -> std::void::Void {
        let mut b = B()
        let a = b.a
        b.a.str = "hello"
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPartialMoves,
    test_invalid_assign_to_non_initialized_attributes_attribute,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str

    cls A { str: Str }
    cls B { a: A }
    cls C { b: B }

    fun f() -> std::void::Void {
        let mut c = C()
        let b = c.b
        c.b.a.str = "hello"
    }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryPartialMoves,
    test_invalid_assign_to_non_initialized_attributes_attribute_deep,
    SppUninitializedMemoryUseError, R"(
    use std::string::Str

    cls A { str: Str }
    cls B { a: A }
    cls C { b: B }

    fun f() -> std::void::Void {
        let mut c = C()
        let b = c.b.a
        c.b.a.str = "hello"
    }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryPartialMoves,
    test_valid_assign_attribute_on_non_initialized_attribute_4, R"(
    use std::string::Str

    cls A { str: Str }
    cls B { a: A }
    cls C { b: B }

    fun f() -> std::void::Void {
        let mut c = C()
        let x = c.b.a.str
        c.b.a.str = "hello"
    }
)")
