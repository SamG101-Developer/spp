#include "test_macros.hpp"

// An ffi package is for its owner's modules: "std" ships "sppc", so the project's own code cannot name it, however it
// is reached (N12.1).
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_call_into_a_dependencies_ffi_package,
  SppFfiPackageNotOwnedError, R"(
    fun f() -> Void {
        sppc::async_yield()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_use_of_a_dependencies_ffi_function,
  SppFfiPackageNotOwnedError, R"(
    use sppc::async_yield

    fun f() -> Void {
        async_yield()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_constant_of_a_dependencies_ffi_package,
  SppFfiPackageNotOwnedError, R"(
    fun f() -> Void {
        let x = sppc::eperm
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_use_of_a_dependencies_ffi_type,
  SppFfiPackageNotOwnedError, R"(
    use sppc::CTimespec
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_parameter_typed_by_a_dependencies_ffi_package,
  SppFfiPackageNotOwnedError, R"(
    fun f(t: sppc::CTimespec) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_attribute_typed_by_a_dependencies_ffi_package,
  SppFfiPackageNotOwnedError, R"(
    cls Holder {
        t: sppc::CTimespec
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  FfiOwnership,
  test_invalid_generic_argument_from_a_dependencies_ffi_package,
  SppFfiPackageNotOwnedError, R"(
    fun f() -> Void {
        let v = std::vector::Vec[sppc::CTimespec]()
    }
)");

// The owner's own code still calls it, here on the project's behalf, and its declarations' generics are read where
// the compiler needs them.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  FfiOwnership,
  test_valid_dependency_calls_its_own_ffi_package, R"(
    fun f() -> Void {
        std::console::println("a")
        let v = std::vector::Vec[U64]()
        std::mem::ops::drop(v)
    }
)");
