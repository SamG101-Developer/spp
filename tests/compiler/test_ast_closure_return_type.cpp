#include "test_macros.hpp"

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
    fun g(func: std::function::FunRef[(S32,), std::option::Opt[S32]]) -> Void { }

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

SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_body_value_member_of_a_variant_return_type, R"(
    fun f() -> Void {
        let c = (x: S32) -> std::option::Opt[S32] { std::option::Some[S32](val=x) }
        let o = c(1)
    }
)");

// With no written return type, a closure's "ret"s (and its body's own value, when it can finish) agree on one type as a
// "case"'s branches do ("type_join::TypeJoin"): the widest variant among them, in either order.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_valid_inferred_rets_agree_on_the_widest_variant, R"(
    fun f(n: Opt[S32]) -> Void {
        let c = (b: Bool, m: Opt[S32]) {
            case b of {
                == true { ret Some(val=1) }
                else { ret m }
            }
        }
        let o: Opt[S32] = c(true, n)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_invalid_inferred_rets_of_unrelated_types,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        let c = (b: Bool) {
            case b of {
                == true { ret 1 }
                else { ret false }
            }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  ClosureExpressionReturnTypeAst,
  test_invalid_inferred_ret_and_body_value_of_unrelated_types,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        let c = (b: Bool) {
            case b of {
                == true { ret 1 }
                else { }
            }
            false
        }
    }
)");
