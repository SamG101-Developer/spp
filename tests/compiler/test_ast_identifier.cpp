#include "test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IdentifierAst,
  test_invalid_identifier,
  SppIdentifierUnknownError, R"(
  fun f() -> Void {
    let mut x = y
    x = z
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IdentifierAst,
  test_valid_identifier, R"(
  fun f() -> Void {
    let mut x = 1
    let y = 2
    x = y
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IdentifierAst,
  test_invalid_self_identifier_in_free_function,
  SppSelfIdentifierInvalidContextError, R"(
  fun f() -> Void {
    let x = self
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IdentifierAst,
  test_valid_self_identifier_in_method, R"(
  cls A { }
  sup A {
    fun f(self) -> Void {
      let x = self
    }
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IdentifierAst,
  test_invalid_namespace_as_a_value,
  SppIdentifierUnknownError, R"(
  fun f() -> Void {
    let x = std
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IdentifierAst,
  test_invalid_namespace_as_an_object_argument,
  SppIdentifierUnknownError, R"(
  cls P { !public std: U32 }

  fun f() -> Void {
    let p = P(std)
  }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  IdentifierAst,
  test_invalid_uncaptured_name_matching_a_namespace_in_a_closure,
  SppIdentifierUnknownError, R"(
  cls P { !public std: U32 }

  fun f(std: U32) -> Void {
    let c = || P(std)
  }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  IdentifierAst,
  test_valid_namespace_as_a_static_member_access_lhs, R"(
  fun f() -> Void {
    let v = std::vector::Vec[U8]()
  }
)");
