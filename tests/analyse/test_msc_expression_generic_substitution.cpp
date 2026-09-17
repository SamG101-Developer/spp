#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_literal_default_is_carried_through, R"(
    fun f[T](t: T, n: S32 = 5) -> S32 {
        std::mem::ops::drop(t)
        ret n
    }

    fun g() -> Void {
        let x = f(true)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_type_named_in_a_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](w: Wrap[T] = Wrap[T]::new()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_identifier_naming_a_sibling_comp_parameter, R"(
    fun f[cmp a: U8, cmp b: U8 = a]() -> U8 {
        ret b
    }

    fun g() -> Void {
        let x = f[a=5_u8]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_parenthesised_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](w: Wrap[T] = (Wrap[T]::new())) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_tuple_literal_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](p: (S32, Wrap[T]) = (1, Wrap[T]::new())) -> Void {
        let (n, w) = p
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_explicit_element_array_default, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](a: Arr[Wrap[T], 2_uz] = [Wrap[T]::new(), Wrap[T]::new()]) -> Void {
        let [p, q] = a
        std::mem::ops::drop(p)
        std::mem::ops::drop(q)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_repeated_element_array_default, R"(
    use std::num::sized_integer_unsigned::SizedIntegerUnsigned

    fun f[cmp w: U32](a: Arr[SizedIntegerUnsigned[w], 2_uz] = [SizedIntegerUnsigned[w]::from(0); 2_uz]) -> Void {
        let [p, q] = a
    }

    fun g() -> Void {
        f[32_u32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_object_initializer_default, R"(
    cls Holder[T] {
        !public n: S32
    }

    fun f[T](h: Holder[T] = Holder[T](n=1)) -> Void {
        std::mem::ops::drop(h)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_binary_expression_default, R"(
    fun f[cmp n: S32](x: S32 = n + 1) -> S32 {
        ret x
    }

    fun g() -> Void {
        let y = f[4]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_generic_arguments_of_a_call_in_a_default, R"(
    cls Wrap[T] { }

    fun make[T]() -> Wrap[T] {
        ret Wrap[T]()
    }

    fun f[T](w: Wrap[T] = make[T]()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_call_arguments_of_a_call_in_a_default, R"(
    fun twice(x: S32) -> S32 {
        ret x * 2
    }

    fun f[cmp n: S32](x: S32 = twice(n)) -> S32 {
        ret x
    }

    fun g() -> Void {
        let y = f[4]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_invalid_async_default,
  SppInvalidDefaultValueError, R"(
    cls Wrap[T] { }

    fun make[T]() -> Wrap[T] {
        ret Wrap[T]()
    }

    fun f[T](w: Fut[Wrap[T]] = async make[T]()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_one_default_substituted_two_ways, R"(
    cls Wrap[T] { }

    sup [T] Wrap[T] {
        fun new() -> Wrap[T] { ret Wrap[T]() }
    }

    fun f[T](w: Wrap[T] = Wrap[T]::new()) -> Void {
        std::mem::ops::drop(w)
    }

    fun g() -> Void {
        f[S32]()
        f[Bool]()
    }
)");

// The four below pin down that a comp parameter is substituted by the scope that declares it, even where two nested
// scopes spell theirs the same. Type substitution matches a parameter by "ParamId" and comp substitution still matches
// by spelling ("IdentifierAst::SubstituteGenericsExpr"), which looks like a hygiene hole and is not one: every driver
// of "SubstituteGenericsExpr" substitutes an expression against the arguments of the very scope that wrote it, and one
// name is one parameter within a scope. Each same-spelling test is paired with a distinct-spelling control, so a
// failure says whether the spelling is what broke it. The comp value is surfaced through a type, because that is what
// makes a wrong binding observable to the analyser rather than only to a running program.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_method_comp_parameter_spelled_like_its_sup_blocks, R"(
    !public cls A[cmp n: U8] { }
    !public cls H[cmp n: U8] { }

    sup [cmp n: U8] H[n] {
        !public fun m[cmp n: U8](&self) -> A[n] { ret A[n]() }
    }

    fun g() -> Void {
        let h = H[3_u8]()
        let x: A[7_u8] = h.m[7_u8]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_method_comp_parameter_spelled_unlike_its_sup_blocks, R"(
    !public cls A[cmp n: U8] { }
    !public cls H[cmp n: U8] { }

    sup [cmp n: U8] H[n] {
        !public fun m[cmp q: U8](&self) -> A[q] { ret A[q]() }
    }

    fun g() -> Void {
        let h = H[3_u8]()
        let x: A[7_u8] = h.m[7_u8]()
        std::mem::ops::drop(x)
        std::mem::ops::drop(h)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_callee_comp_parameter_spelled_like_its_callers, R"(
    !public cls A[cmp n: U8] { }

    fun inner[cmp n: U8]() -> A[n] { ret A[n]() }
    fun outer[cmp n: U8]() -> A[4_u8] { ret inner[4_u8]() }

    fun g() -> Void {
        let x: A[4_u8] = outer[9_u8]()
        std::mem::ops::drop(x)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestExpressionGenericSubstitution,
  test_valid_callee_comp_parameter_spelled_unlike_its_callers, R"(
    !public cls A[cmp n: U8] { }

    fun inner[cmp k: U8]() -> A[k] { ret A[k]() }
    fun outer[cmp n: U8]() -> A[4_u8] { ret inner[4_u8]() }

    fun g() -> Void {
        let x: A[4_u8] = outer[9_u8]()
        std::mem::ops::drop(x)
    }
)");
