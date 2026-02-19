#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    test_subroutine_lambda_as_value, R"(
    use std::string_view::StrView
    use std::function::FunRef
    use std::void::Void

    fun g(x: FunRef[(), StrView]) -> Void {
        let mut y = x()
        y = "Goodbye, World!"
    }

    fun f() -> Void {
        let p = () "string"
        g(p)
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestFirstClassFunctions,
    test_subroutine_function_as_value, R"(
    use std::string_view::StrView
    use std::function::FunRef
    use std::void::Void

    fun h() -> StrView {
        ret "Hello, World!"
    }

    fun g(x: FunRef[(), StrView]) -> Void {
        let mut y = x()
        y = "Goodbye, World!"
    }

    fun f() -> Void {
        let p = h
        g(p)
    }
)");
