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
        let t = (1_u64, "hello", false)
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

        let t2 = (1_u64, 2_u32, 3_u16, "hello", false)
        t2.f()

        let t3 = (1_u64, 2_u32, 3_u16, "hello", false, 10.5)
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
