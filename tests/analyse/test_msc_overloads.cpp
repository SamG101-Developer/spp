#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_invalid_overload_parameter_conventions_1,
  SppFunctionPrototypeConflictError, R"(
    fun f(a: &Bool) -> Void { }
    fun f(a: &mut Bool) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_invalid_overload_parameter_conventions_2,
  SppFunctionPrototypeConflictError, R"(
    fun f(a: &mut Bool) -> Void { }
    fun f(a: &Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_different_return_type, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool) -> Bool { ret true }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_parameter_count, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: Bool, b: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_parameter_conventions_1, R"(
    fun f(a: &Bool) -> Void { }
    fun f(a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_parameter_conventions_2, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: &Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_parameter_conventions_3, R"(
    fun f(a: &mut Bool) -> Void { }
    fun f(a: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_parameter_conventions_4, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: &mut Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_parameter_types, R"(
    fun f(a: Bool) -> Void { }
    fun f(a: std::bignum::bigint::BigInt) -> Void {
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_invalid_overload_generics_same_name,
  SppFunctionPrototypeConflictError, R"(
    fun f[T](a: T) -> Void { }
    fun f[T](a: T) -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_invalid_overload_generics_different_name,
  SppFunctionPrototypeConflictError, R"(
    fun f[T](a: T) -> Void { }
    fun f[U](a: U) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_generics_usage_1, R"(
    fun f[T]() -> Void { }
    fun f[T](b: T) -> Void {
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_generics_usage_2, R"(
    fun f[T](a: T) -> Void {
        std::mem::ops::drop(a)
    }
    fun f[T](a: T, b: T) -> Void {
        std::mem::ops::drop(a)
        std::mem::ops::drop(b)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_generics_usage_3, R"(
    fun f[T]() -> Void { }
    fun f[T, U]() -> Void { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_different_parameter_names,
  SppFunctionPrototypeConflictError, R"(
    fun f(a: Bool) -> Void { }
    fun f(b: Bool) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_subroutine_and_coroutine, R"(
    fun f(a: Bool) -> Void { }
    cor f(a: Bool) -> Gen[Bool] { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_SupBlocks,
  test_invalid_overload_parameter_conventions_1,
  SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(a: &Bool) -> Void { }
    }

    sup A {
        fun f(a: &mut Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_SupBlocks,
  test_invalid_overload_parameter_conventions_2,
  SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(a: &mut Bool) -> Void { }
    }

    sup A {
        fun f(a: &Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_different_return_type, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool) -> Bool { ret true }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_parameter_count, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool, b: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_parameter_conventions_1, R"(
    cls A { }
    sup A {
        fun f(a: &Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_parameter_conventions_2, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: &Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_parameter_conventions_3, R"(
    cls A { }
    sup A {
        fun f(a: &mut Bool) -> Void { }
    }

    sup A {
        fun f(a: Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_parameter_conventions_4, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: &mut Bool) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_parameter_types, R"(
    cls A { }
    sup A {
        fun f(a: Bool) -> Void { }
    }

    sup A {
        fun f(a: std::bignum::bigint::BigInt) -> Void {
            std::mem::ops::drop(a)
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_SupBlocks,
  test_invalid_overload_generics_same_name,
  SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> Void { }
    }

    sup A {
        fun f[T](a: T) -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_SupBlocks,
  test_invalid_overload_generics_different_name,
  SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> Void { }
    }

    sup A {
        fun f[U](a: U) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_generics_usage_1, R"(
    cls A { }
    sup A {
        fun f[T]() -> Void { }
    }

    sup A {
        fun f[T](b: T) -> Void {
            std::mem::ops::drop(b)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_generics_usage_2, R"(
    cls A { }
    sup A {
        fun f[T](a: T) -> Void {
            std::mem::ops::drop(a)
        }
    }

    sup A {
        fun f[T](a: T, b: T) -> Void {
            std::mem::ops::drop(a)
            std::mem::ops::drop(b)
        }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_generics_usage_3, R"(
    cls A { }
    sup A {
        fun f[T]() -> Void { }
    }

    sup A {
        fun f[T, U]() -> Void { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_different_self_conventions,
  SppFunctionPrototypeConflictError, R"(
    cls A { }
    sup A {
        fun f(&self) -> Void { }
    }

    sup A {
        fun f(&mut self) -> Void { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_overload_subroutine_and_coroutine, R"(
    cls A { }
    sup A {
        fun f(&self, a: Bool) -> Void { }
    }

    sup A {
        cor f(&self, a: Bool) -> Gen[Bool] { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_overload_by_value_and_by_borrow, R"(
    fun f[T](a: T) -> Void { std::mem::ops::drop(a) }
    fun f[T](a: &T) -> Void { }
)");

// A function's mock is "$" + its name as written, so names differing only by "_" or case are separate overload sets.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_invalid_name_differing_by_an_underscore_is_another_function,
  SppFunctionCallNoValidSignaturesError, R"(
    fun foo1(x: S32) -> Void { }
    fun foo_1(x: Bool) -> Void { }

    fun f() -> Void {
        foo1(true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_FreeFuncs,
  test_valid_names_differing_by_case_are_not_ambiguous, R"(
    fun fooBar() -> S32 { ret 1_s32 }
    fun foobar() -> Bool { ret true }

    fun f() -> Void {
        let a = fooBar()
        let b = foobar()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestOverloads_SupBlocks,
  test_invalid_method_name_differing_by_an_underscore_is_another_method,
  SppFunctionCallNoValidSignaturesError, R"(
    cls A { }

    sup A {
        !public fun m1(&self, x: S32) -> Void { }
        !public fun m_1(&self, x: Bool) -> Void { }
    }

    fun f() -> Void {
        let x = A()
        x.m1(true)
        std::mem::ops::drop(x)
    }
)");

// A method's own generic may share a name with its "sup" block's: they are two parameters. The block's binding (from
// the receiver) is for the block's, and the method's is inferred from its own arguments.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_method_generic_shadowing_the_block_generic_inferred_from_the_argument, R"(
    cls Box[T] { !public v: T }

    sup [T] Box[T] {
        !public fun swap[T](&self, other: T) -> T { ret other }
    }

    fun f() -> Void {
        let b = Box[S32](v=1)
        let s: Bool = b.swap(true)
        std::mem::ops::drop(b)
    }
)");

// "ior_"'s own "E" once took the receiver's "E" ("Bool") by name, so "Res[S32, U8]" did not match.
SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestOverloads_SupBlocks,
  test_valid_result_ior_with_another_error_type, R"(
    fun f() -> Void {
        let r: Res[S32, Bool] = Fail(err=false)
        let o: Res[S32, U8] = Pass(val=1)
        let x: Res[S32, U8] = r.ior_(o)
    }
)");
