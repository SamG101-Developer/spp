#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  VariadicFfi,
  test_invalid_call_to_a_variadic_ffi_function,
  SppFeatureNotYetSupportedError, R"(
    !ffi(symbol="zz_absent_symbol")
    fun g[..V](a: S32, ..rest: V) -> S32 { }

    fun f() -> Void {
        let x = g(1, 2, 3)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  VariadicFfi,
  test_invalid_call_to_a_variadic_ffi_function_with_no_trailing_arguments,
  SppFeatureNotYetSupportedError, R"(
    !ffi(symbol="zz_absent_symbol")
    fun g[..V](a: S32, ..rest: V) -> S32 { }

    fun f() -> Void {
        let x = g(1)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VariadicFfi,
  test_valid_declaring_a_variadic_ffi_function_without_calling_it, R"(
    !ffi(symbol="zz_absent_symbol")
    fun g[..V](a: S32, ..rest: V) -> S32 { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  VariadicFfi,
  test_valid_call_to_a_native_variadic_function, R"(
    fun g[..V](a: S32, ..rest: V) -> S32 {
        std::mem::ops::drop(rest)
        ret a
    }

    fun f() -> Void {
        let x = g(1, 2, 3)
    }
)");
