#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiGenericParameters,
  test_invalid_type_generic_on_an_ffi_function,
  SppFfiGenericParameterError, R"(
    !ffi(symbol="zz_absent")
    fun g[T](x: T) -> S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiGenericParameters,
  test_invalid_borrowed_type_generic_on_an_ffi_function,
  SppFfiGenericParameterError, R"(
    !ffi(symbol="zz_absent")
    fun g[T](x: &T) -> S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiGenericParameters,
  test_invalid_comp_generic_on_an_ffi_function,
  SppFfiGenericParameterError, R"(
    !ffi(symbol="zz_absent")
    fun g[cmp n: U32](x: S32) -> S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiGenericParameters,
  test_invalid_variadic_ffi_function,
  SppFfiGenericParameterError, R"(
    !ffi(symbol="zz_absent")
    fun g[..V](a: S32, ..rest: V) -> S32 { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiGenericParameters,
  test_invalid_closure_constrained_generic_on_an_ffi_function,
  SppFfiGenericParameterError, R"(
    !ffi(symbol="zz_absent")
    fun g[F: std::function::FunMov[(), Void]](f: F) -> S32 { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiGenericParameters,
  test_valid_closure_taken_as_the_concrete_c_pair, R"(
    !ffi(symbol="zz_absent")
    fun g(f: std::cffi::CClosure[(), Void]) -> S32 { }

    fun f() -> Void {
        let n = g(std::cffi::CClosure[(), Void]::from(() -> Void { }))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiGenericParameters,
  test_valid_non_generic_ffi_function, R"(
    !ffi(symbol="zz_absent")
    fun g(a: S32, b: &Str) -> S32 { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiGenericParameters,
  test_valid_generic_parameters_on_a_native_function, R"(
    fun g[T](x: T) -> T {
        ret x
    }
)");
