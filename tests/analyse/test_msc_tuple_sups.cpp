#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_any_3_tuple, R"(
    sup [T, U, V] Tup[T, U, V] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_wrong_number_elems_1,
    SppIdentifierUnknownError, R"(
    sup [T, U, V] Tup[T, U, V] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_wrong_number_elems_2,
    SppIdentifierUnknownError, R"(
    sup [T, U] Tup[T, U] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_specific_3_tuple_mismatch_types,
    SppIdentifierUnknownError, R"(
    sup Tup[U64, U32, U16] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u64, 3_u64)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_specific_3_tuple_correct_types, R"(
    sup Tup[U64, U32, U16] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_specific_and_generic_3_tuple_correct_types, R"(
    sup [P, Q] Tup[U64, P, Q] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, Str::from("hello"), false)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_generics, R"(
    sup [..T] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t1 = (1_u64, 2_u32, 3_u16)
        t1.f()

        let t2 = (1_u64, 2_u32, 3_u16, Str::from("hello"), false)
        t2.f()

        let t3 = (1_u64, 2_u32, 3_u16, Str::from("hello"), false, 10.5)
        t3.f()

        let t4 = (false,)
        t4.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_constraint_satisfied, R"(
    sup [..T: Copy] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_constraint_unsatisfied,
    SppIdentifierUnknownError, R"(
    sup [..T: Copy] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, Str::from("hello"))
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_empty_tuple, R"(
    sup [..T] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = ()
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_fixed_prefix_and_variadic, R"(
    sup [First, ..Rest] Tup[First, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_covers_2_tuple, R"(
    sup [..T] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_fixed_prefix_swallows_nothing, R"(
    sup [First, ..Rest] Tup[First, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64,)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_two_fixed_prefix_and_variadic, R"(
    sup [P, Q, ..Rest] Tup[P, Q, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16, false)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_fewer_elements_than_fixed_prefix,
    SppIdentifierUnknownError, R"(
    sup [P, Q, ..Rest] Tup[P, Q, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64,)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_specific_prefix_and_variadic, R"(
    sup [..Rest] Tup[U64, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_specific_prefix_mismatch_with_variadic,
    SppIdentifierUnknownError, R"(
    sup [..Rest] Tup[U64, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (false, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_fixed_and_variadic_blocks_coexist, R"(
    sup [..T] Tup[T] {
        !public fun any_len(&self) -> Void { }
    }

    sup [T, U, V] Tup[T, U, V] {
        !public fun exactly_3(&self) -> Void { }
    }

    fun f() -> Void {
        let t3 = (1_u64, 2_u32, 3_u16)
        t3.any_len()
        t3.exactly_3()

        let t2 = (1_u64, 2_u32)
        t2.any_len()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_fixed_block_not_reached_by_wrong_arity,
    SppIdentifierUnknownError, R"(
    sup [..T] Tup[T] {
        !public fun any_len(&self) -> Void { }
    }

    sup [T, U, V] Tup[T, U, V] {
        !public fun exactly_3(&self) -> Void { }
    }

    fun f() -> Void {
        let t2 = (1_u64, 2_u32)
        t2.exactly_3()
    }
)");

// --------------------------------------------------------------------------------------------------------------
// Variadic packs and constraints.
//
// A variadic parameter constrains each of the types it stands for rather than the list as a whole, so the block
// applies only when every element the pack swallowed satisfies it. The element that breaks the constraint is varied
// by position, because checking only the first would pass the simplest case by accident.
// --------------------------------------------------------------------------------------------------------------

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_constraint_single_element, R"(
    sup [..T: Copy] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64,)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_constraint_broken_by_last_element,
    SppIdentifierUnknownError, R"(
    sup [..T: Copy] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, Str::from("hello"))
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_constraint_broken_by_middle_element,
    SppIdentifierUnknownError, R"(
    sup [..T: Copy] Tup[T] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, Str::from("hello"), 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_constrained_prefix_and_constrained_variadic, R"(
    sup [First: Copy, ..Rest: Copy] Tup[First, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, false)
        t.f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_constrained_prefix_unsatisfied,
    SppIdentifierUnknownError, R"(
    sup [First: Copy, ..Rest] Tup[First, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (Str::from("hello"), 2_u32)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_unconstrained_variadic_takes_non_copy, R"(
    sup [First: Copy, ..Rest] Tup[First, Rest] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, Str::from("hello"))
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_param_name_shared_with_stdlib, R"(
    sup [T, A] Tup[T, A] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_variadic_param_name_shared_with_stdlib, R"(
    sup [T, ..A] Tup[T, A] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32, 3_u16)
        t.f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_tuple_superimposition_constrained_param_name_shared_with_stdlib, R"(
    sup [T: Copy, A: Copy] Tup[T, A] {
        !public fun f(&self) -> Void { }
    }

    fun f() -> Void {
        let t = (1_u64, 2_u32)
        t.f()
    }
)");

// A tuple owns nothing beyond its elements, so it is copyable exactly when all of them are - the same shape as
// "sup [..Variants: Copy] Var[Variants] ext Copy". Without this a variadic pack of numbers arrived as a linear value.
SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleSuperimpositions,
    test_valid_tuple_of_copyable_elements_is_copy, R"(
    fun f() -> Void {
        let t = (1, 2, 3)
    }
)");

// ...and is not copyable when one of them is not, so it still has to be taken apart.
SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleSuperimpositions,
    test_invalid_tuple_with_a_non_copyable_element,
    SppLinearValueNotConsumedError, R"(
    cls T { }

    fun f() -> Void {
        let t = (1, T())
    }
)");
