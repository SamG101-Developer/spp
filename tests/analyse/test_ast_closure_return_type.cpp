#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_declared_return_type, R"(
    fun f() -> Void {
        let c = (x: S32) -> S32 { ret x + 1 }
        let a = c(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_declared_return_type_widens_to_a_variant, R"(
    fun f() -> Void {
        let c = (x: S32) -> std::option::Opt[S32] { ret std::option::Some(val=x) }
        let a = c(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_declared_void_return_type, R"(
    fun f() -> Void {
        let c = (x: S32) -> Void { }
        c(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_no_declared_return_type_is_still_accepted, R"(
    fun f() -> Void {
        let c = (x: S32) x + 1
        let a = c(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_declared_return_type_passed_as_an_argument, R"(
    fun g(func: std::function::FunRef[(S32), std::option::Opt[S32]]) -> Void { }

    fun f() -> Void {
        g((x: S32) -> std::option::Opt[S32] { ret std::option::Some(val=x) })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_declared_return_type_with_no_parameters, R"(
    fun f() -> Void {
        let c = () -> S32 { ret 1 }
        let a = c()
    }
)");
