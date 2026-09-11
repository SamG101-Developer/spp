#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  GenericTypeAliasAst,
  test_valid_generic_alias_as_a_generic_constraint, R"(
    type ZzClosure[Ts: std::tuple::TupLike, R] = std::function::FunMov[Ts, R]

    fun g[F: ZzClosure[(), Void]](f: F) -> Void {
        f()
    }

    fun f() -> Void {
        g(() -> Void { })
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  GenericTypeAliasAst,
  test_valid_generic_alias_with_a_non_empty_argument_tuple, R"(
    type ZzClosure[Ts: std::tuple::TupLike, R] = std::function::FunMov[Ts, R]

    fun g[F: ZzClosure[(S32), S32]](f: F) -> S32 {
        ret f(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  GenericTypeAliasAst,
  test_valid_generic_alias_as_a_parameter_type, R"(
    type ZzBox[T] = std::single::Single[T]

    fun g(b: ZzBox[S32]) -> S32 {
        ret b.read()
    }
)");

// Todo: Commented out - this crashes the compiler rather than failing.
//  segfaults in GenericParameterGroupAst::Stage4_QualifyTypes (generic_parameter_group_ast.cpp:265) - a non-generic
//  alias used as a generic parameter's constraint.
// SPP_TEST_SHOULD_PASS_SEMANTIC(
//   GenericTypeAliasAst,
//   test_valid_non_generic_alias_keeps_the_target_arguments, R"(
//     type ZzPtr = std::mem::pointer::Ptr[U8]
//
//     fun g[T: ZzPtr]() -> Void { }
// )");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  GenericTypeAliasAst,
  test_invalid_generic_alias_parameter_violates_the_target_constraint,
  SppGenericConstraintError, R"(
    type ZzClosure[Ts, R] = std::function::FunMov[Ts, R]
)");
