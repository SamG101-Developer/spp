#include "../test_macros.hpp"


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_using_uninitialized_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p: Point
        let q = p
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_using_moved_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
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
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p: Point
        let x = p.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_partially_using_moved_symbol,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
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
        x: std::string_view::Str
        y: std::string_view::Str
    }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        let x1 = p.x
        let x2 = p.x
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_using_partially_moved_symbol,
    SppPartiallyInitializedMemoryUseError, R"(
    cls Point {
        x: std::string_view::Str
        y: std::string_view::Str
    }

    fun f() -> std::void::Void {
        let p = Point(x=std::string::Str::from("5"), y=std::string::Str::from("5"))
        let x = p.x
        let q = p
    }
)");


SPP_TEST_SHOULD_PASS_SEMANTIC(
    TestAstMemoryUninitialized,
    test_valid_memory_multiple_partial_moves_different_parts, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let p = Point(x=5, y=5)
        let x = p.x
        let y = p.y
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_array_explicit,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = [elem]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_array_repeated,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::number::S32
        let a = [elem; 3_uz]
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_assignment,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
        let mut point: Point
        point.x = 5
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_binary_expression_lhs,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::number::S32
        let a = elem + 123
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_binary_expression_rhs,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::number::S32
        let a = 123 + elem
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_case_expression_condition,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::number::S32
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
    fun f() -> std::void::Void {
        let elem: std::number::S32
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
    fun g(a: std::string::Str) -> std::void::Void { }
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = g(elem)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_function_call_argument_named,
    SppUninitializedMemoryUseError, R"(
    fun g(a: std::string::Str) -> std::void::Void { }
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = g(a=elem)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_gen_mov_expression,
    SppUninitializedMemoryUseError, R"(
    cor f() -> std::generator::Gen[std::string::Str] {
        let elem: std::string::Str
        gen elem
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_gen_mut_expression,
    SppUninitializedMemoryUseError, R"(
    cor f() -> std::generator::Gen[&mut std::string::Str] {
        let elem: std::string::Str
        gen &mut elem
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_gen_ref_expression,
    SppUninitializedMemoryUseError, R"(
    cor f() -> std::generator::Gen[&std::string::Str] {
        let elem: std::string::Str
        gen &elem
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_generic_comp_argument_unnamed,
    SppUninitializedMemoryUseError, R"(
    fun g[cmp n: std::string::Str]() -> std::void::Void { }

    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = g[elem]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_generic_comp_argument_named,
    SppUninitializedMemoryUseError, R"(
    fun g[cmp n: std::string::Str]() -> std::void::Void { }

    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = g[n=elem]()
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_is_expression,
    SppUninitializedMemoryUseError, R"(
    cls Point {
        x: std::number::S32
        y: std::number::S32
    }

    fun f() -> std::void::Void {
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
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = (caps elem) { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_lambda_capture_mut,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let mut elem: std::string::Str
        let a = (caps &mut elem) { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_lambda_capture_ref,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = (caps &elem) { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_let_statement,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = elem
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_parenthesis,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = (elem)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_loop_condition_boolean,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::boolean::Bool
        loop elem { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_loop_condition_iterable,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let iterator: std::generator::Gen[std::string::Str]
        loop elem in iterator { }
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_object_initializer_argument_unnamed,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let data: std::vector::Vec[std::number::U8]
        let a = std::string::Str(data)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_object_initializer_argument_named,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let data_vec: std::vector::Vec[std::number::U8]
        let a = std::string::Str(data=data_vec)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_pattern_guard,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::boolean::Bool
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
    fun f() -> std::void::Void {
        let elem: std::string::Str
        elem.data
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_member_access_index,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: (std::string::Str, std::string::Str)
        elem.0
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_not_keyword,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::boolean::Bool
        elem.not
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_res_keyword,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::generator::Gen[std::boolean::Bool]
        elem.res(false)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_func_call,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let func: std::function::FunRef[(std::boolean::Bool,), std::void::Void]
        func(false)
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_ret_statement,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::string::Str {
        let elem: std::string::Str
        ret elem
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_unary_expression_operator_async,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::function::FunRef[(std::string_view::StrView,), std::void::Void]
        let a = async elem("hello world")
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_postfix_expression_operator_function_call,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::function::FunRef[(std::string_viwe::StrView,), std::void::Void]
        let a = elem("hello world")
    }
)");


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    TestAstMemoryUninitialized,
    test_invalid_memory_uninitialized_symbol_in_tuple,
    SppUninitializedMemoryUseError, R"(
    fun f() -> std::void::Void {
        let elem: std::string::Str
        let a = (elem,)
    }
)");



