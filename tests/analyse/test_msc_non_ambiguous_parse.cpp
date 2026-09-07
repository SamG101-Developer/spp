#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_calling_with_cmp_integer, R"(
    fun v[cmp n: U64]() -> Bool {
        ret true
    }

    fun g() -> Void {
        let mut x = v[1_u64]()
        x = false
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_indexing_vector_of_callables, R"(
    fun g() -> Void {
        let v = Vec[FunMov[(), Str]]()
        let mut x = (v[1_uz])()
        x = Str::from("hello")
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_tuple_literal_on_new_line_is_not_a_call, R"(
    fun g() -> Void {
        let mut x = {
            let a = 1
            (a, 9)
        }
        x = (2, 3)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_array_literal_on_new_line_is_not_an_index, R"(
    fun g() -> Void {
        let mut x = {
            let a = 1
            [a]
        }
        x = [2]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_call_arguments_span_lines, R"(
    fun v(a: S32, b: S32) -> S32 {
        ret a
    }

    fun g() -> Void {
        let mut x = v(
            1,
            2)
        x = 3
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_member_access_on_new_line, R"(
    cls Foo { }

    sup Foo {
        !public fun bar(&self) -> S32 { ret 1 }
    }

    fun g() -> Void {
        let foo = Foo()
        let mut x = foo
            .bar()
        x = 2
        std::mem::ops::drop(foo)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNonAmbiguousParse,
  test_valid_index_on_new_line_is_a_new_statement, R"(
    fun g() -> Void {
        let mut x = 1
        std::mem::ops::drop([x])
        x = 2
    }
)");

// Todo: should really be an unknown identifier error but due
//  to how func sigs are resolved it'll throw a "no valid
//  signatures error", with 0 suggested alternatives.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNonAmbiguousParse,
  test_invalid_sibling_method_called_without_self,
  SppFunctionCallNoValidSignaturesError, R"(
    cls T { }

    sup T {
        fun helper(&self) -> Void { }

        fun caller(&self) -> Void {
            helper()
        }
    }
)");
