#include "../test_macros.hpp"

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_using_uninitialized_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p: Point
        let q = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_using_moved_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let q = p
        let r = p
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_partially_using_uninitialized_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p: Point
        let x = p.x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_partially_using_moved_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let q = p
        let x = p.x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_partially_using_partially_moved_symbol_same_part,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let x1 = p.x
        let x2 = p.x
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_using_partially_moved_symbol,
    SppPartiallyInitializedMemoryUseError, R"(
    cls Point {
        !public x: Str
        !public y: Str
    }

    fun f() -> Void {
        let p = Point(x=Str::from("5"), y=Str::from("5"))
        let x = p.x
        let q = p
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryUninitialized,
    test_valid_memory_multiple_partial_moves_different_parts, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let p = Point(x=5, y=5)
        let x = p.x
        let y = p.y
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_array_explicit,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Str
        let a = [elem]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_array_repeated,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: S32
        let a = [elem; 3_uz]
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_assignment,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let mut point: Point
        point.x = 5
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_binary_expression_lhs,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: S32
        let a = elem + 123
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_binary_expression_rhs,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: S32
        let a = 123 + elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_case_expression_condition,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: S32
        case elem of {
            == 1 { }
            == 2 { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_case_expression_branch,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: S32
        case 1 of {
            == elem { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_function_call_argument_unnamed,
    SppUninitializedMemoryUseError, R"(
    fun g(a: Str) -> Void { }
    fun f() -> Void {
        let elem: Str
        let a = g(elem)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_function_call_argument_named,
    SppUninitializedMemoryUseError, R"(
    fun g(a: Str) -> Void { }
    fun f() -> Void {
        let elem: Str
        let a = g(a=elem)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_gen_mov_expression,
    SppUninitializedMemoryUseError, R"(
    cor f() -> Gen[Str] {
        let elem: Str
        gen elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_gen_mut_expression,
    SppUninitializedMemoryUseError, R"(
    cor f() -> Gen[&mut Str] {
        let elem: Str
        gen &mut elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_gen_ref_expression,
    SppUninitializedMemoryUseError, R"(
    cor f() -> Gen[&Str] {
        let elem: Str
        gen &elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_generic_comp_argument_unnamed,
    SppUninitializedMemoryUseError, R"(
    fun g[cmp n: Str]() -> Void { }

    fun f() -> Void {
        let elem: Str
        let a = g[elem]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_generic_comp_argument_named,
    SppUninitializedMemoryUseError, R"(
    fun g[cmp n: Str]() -> Void { }

    fun f() -> Void {
        let elem: Str
        let a = g[n=elem]()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_is_expression,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        !public x: S32
        !public y: S32
    }

    fun f() -> Void {
        let elem: Point
        case elem of {
            is Point(x=0, y) { }
            is Point(x, y=0) { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_lambda_capture_mov,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Str
        let a = (caps elem) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_lambda_capture_mut,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let mut elem: Str
        let a = (caps &mut elem) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_lambda_capture_ref,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Str
        let a = (caps &elem) { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_let_statement,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Str
        let a = elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_parenthesis,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Str
        let a = (elem)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_loop_condition_boolean,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Bool
        loop elem { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_loop_condition_iterable,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let iterator: Gen[Str]
        loop elem in iterator { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_object_initializer_argument_unnamed,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public bytes: Vec[U8]
    }

    fun f() -> Void {
        let bytes: Vec[U8]
        let a = A(bytes)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_object_initializer_argument_named,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public bytes: Vec[U8]
    }

    fun f() -> Void {
        let bytes_vec: Vec[U8]
        let a = A(bytes=bytes_vec)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_pattern_guard,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Bool
        case true of {
            == false and elem { }
            else { }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_member_access_attribute,
    SppUninitializedMemoryUseError, R"(
    cls A {
        !public bytes: Vec[U8]
    }

    fun f() -> Void {
        let elem: A
        let x = elem.bytes
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_member_access_index,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: (Str, Str)
        elem.0
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_not_keyword,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Bool
        elem.not
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_res_keyword,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Gen[Bool, Bool]
        elem.res(false)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_func_call,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let func: FunRef[(Bool,), Void]
        func(false)
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_ret_statement,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Str {
        let elem: Str
        ret elem
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_unary_expression_operator_async,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: FunRef[(&StrView,), Void]
        let a = async elem("hello world")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_function_call,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: FunRef[(&StrView,), Void]
        let a = elem("hello world")
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_tuple,
    SppUninitializedMemoryUseError, R"(
    fun f() -> Void {
        let elem: Str
        let a = (elem,)
    }
)");
