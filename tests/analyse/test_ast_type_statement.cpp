#include "../test_macros.hpp"
import testex;


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_old_type_convention_mut,
    SppSecondClassBorrowViolationError, R"(
    type MyType = &mut std::boolean::Bool
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTypeStatementAst,
    test_invalid_type_statement_old_type_convention_ref,
    SppSecondClassBorrowViolationError, R"(
    type MyType = &std::boolean::Bool
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_simple_alias, R"(
    type MyString = std::string::Str
    type MyBool = std::boolean::Bool

    fun f(a: MyString, b: MyBool) -> std::void::Void { }
    fun g() -> std::void::Void { f("hello", true) }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_simple_alias, R"(
    fun f() -> std::void::Void {
        type MyString = std::string::Str
        type MyBool = std::boolean::Bool

        let x: (MyString, MyBool)
        x = ("hello", true)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_variant, R"(
    type SomeType = std::string::Str or std::boolean::Bool
    fun f(a: SomeType) -> std::void::Void { }
    fun g() -> std::void::Void { f("hello") }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_variant, R"(
    fun f() -> std::void::Void {
        type SomeType = std::string::Str or std::boolean::Bool
        let x: SomeType
        x = "hello"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_generics_alias, R"(
    type MyVec[T] = std::vector::Vec[T]

    fun f[T](mut a: MyVec[T], replacement: T) -> std::void::Void {
        # let mut x = a.take_head()
        # x = replacement
    }

    fun g() -> std::void::Void {
        let x = std::vector::Vec[std::string::Str]()
        f(x, "test")
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_local_generics_alias, R"(
    fun f() -> std::void::Void {
        type MyVec[T] = std::vector::Vec[T]
        let x = MyVec[std::string::Str]()
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_nested_generic_alias, R"(
    fun f[T](a: std::option::Opt[T]) -> std::void::Void { }
    fun g() -> std::void::Void {
        let x = std::option::Some(val=123)
        f(x)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTypeStatementAst,
    test_valid_type_statement_reduction_type_generic, R"(
    type MyVec[T] = std::vector::Vec[T]

    fun f[T](a: MyVec[T]) -> std::void::Void { }
)");
