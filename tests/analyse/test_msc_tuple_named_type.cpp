#include "../test_macros.hpp"

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_as_parameter, R"(
    fun f(a: Tup[U64, U32]) -> Void { }

    fun g() -> Void {
        f((1_u64, 2_u32))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_as_return_type, R"(
    fun f() -> Tup[U64, U32] {
        ret (1_u64, 2_u32)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_as_let_annotation, R"(
    fun f() -> Void {
        let a: Tup[Bool, Bool] = (true, false)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_matches_literal_type, R"(
    fun f(a: Tup[U64, U32]) -> Void { }

    fun g() -> Void {
        let a = (1_u64, 2_u32)
        f(a)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleNamedType,
    test_invalid_named_tuple_type_wrong_element_types,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Tup[U64, U32]) -> Void { }

    fun g() -> Void {
        f((1_u64, 2_u64))
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestTupleNamedType,
    test_invalid_named_tuple_type_wrong_element_count,
    SppFunctionCallNoValidSignaturesError, R"(
    fun f(a: Tup[U64, U32]) -> Void { }

    fun g() -> Void {
        f((1_u64, 2_u32, 3_u16))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_nested, R"(
    fun f(a: Tup[Tup[U64, U32], Bool]) -> Void { }

    fun g() -> Void {
        f(((1_u64, 2_u32), true))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_as_generic_argument, R"(
    fun f(a: std::vector::Vec[Tup[U64, U32]]) -> Void { }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_behind_type_alias, R"(
    type MyPair = Tup[U64, U32]

    fun f(a: MyPair) -> Void { }

    fun g() -> Void {
        f((1_u64, 2_u32))
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestTupleNamedType,
    test_valid_named_tuple_type_in_generic_function, R"(
    fun f[T](a: Tup[T, Bool]) -> Void { }

    fun g() -> Void {
        f((1_u64, true))
    }
)");
