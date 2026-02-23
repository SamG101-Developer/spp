#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_type_mismatch,
    SppTypeMismatchError, R"(
    cls MyType { }
    sup MyType {
        type X = std::string::Str
    }

    fun f() -> std::void::Void {
        let x: MyType::X
        x = 123
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_nested,
    SppTypeMismatchError, R"(
    cls MyType1 { }
    sup MyType1 {
        type Z = std::string::Str
    }

    cls MyType2 { }
    sup MyType2 {
        type Y = MyType1
    }

    cls MyType3 { }
    sup MyType3 {
        type X = MyType2
    }

    fun f() -> std::void::Void {
        let x: MyType3::X::Y::Z
        x = 123
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    SupTypeStatementAst,
    test_invalid_sup_type_statement_with_generic,
    SppTypeMismatchError, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        type X = T
    }

    fun f() -> std::void::Void {
        let x: MyType[std::number::S32]::X
        x = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement, R"(
    cls MyType { }
    sup MyType {
        type X = std::string_view::StrView
    }

    fun f() -> std::void::Void {
        let x: MyType::X
        x = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement_nested, R"(
    cls MyType1 { }
    sup MyType1 {
        type Z = std::string_view::StrView
    }

    cls MyType2 { }
    sup MyType2 {
        type Y = MyType1
    }

    cls MyType3 { }
    sup MyType3 {
        type X = MyType2
    }

    fun f() -> std::void::Void {
        let x: MyType3::X::Y::Z
        x = "hello world"
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    SupTypeStatementAst,
    test_valid_sup_type_statement_with_generic, R"(
    cls MyType[T] { }
    sup [T] MyType[T] {
        type X = T
    }

    fun f() -> std::void::Void {
        let x: MyType[std::number::S32]::X
        x = 123
    }
)");
