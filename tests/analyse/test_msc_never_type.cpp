#include "../test_macros.hpp"

// Declaring a "!" function: the body has to diverge.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_function_ending_in_abort, R"(
    use std::abort::abort
    fun f() -> ! {
        abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_function_ending_in_infinite_loop_with_skip, R"(
    fun f() -> ! {
        loop true { skip }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_function_inner_loop_exit_leaves_outer_infinite, R"(
    fun f() -> ! {
        loop true {
            loop true { exit }
        }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_never_function_body_completes,
  SppFunctionSubroutineMissingReturnStatementError, R"(
    fun f() -> ! { }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_never_function_loop_with_exit,
  SppFunctionSubroutineMissingReturnStatementError, R"(
    fun f() -> ! {
        loop true { exit }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_never_function_conditional_loop,
  SppFunctionSubroutineMissingReturnStatementError, R"(
    fun f(b: Bool) -> ! {
        loop b { }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_never_function_returning_a_value,
  SppTypeMismatchError, R"(
    fun f() -> ! {
        ret 1
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_by_type_name, R"(
    use std::abort::abort
    fun f() -> std::never::Never {
        abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_through_an_alias, R"(
    use std::abort::abort
    type Diverge = !
    fun f() -> Diverge {
        abort()
    }
    fun g() -> S32 {
        f()
    }
)");

// A "!" value where another type is expected: it never exists, so it fits anywhere.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_body_in_valued_function, R"(
    use std::abort::abort
    fun f() -> S32 {
        abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_infinite_loop_body_in_valued_function, R"(
    fun f() -> Str {
        loop true { }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_returning_method_ends_valued_function, R"(
    use std::abort::abort
    cls A { }
    sup A {
        !public
        fun fail(&self) -> ! { abort() }
    }
    fun f(a: &A) -> S32 {
        a.fail()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_closure_ends_valued_function, R"(
    use std::abort::abort
    fun f() -> S32 {
        let g = () abort()
        g()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_in_typed_let, R"(
    use std::abort::abort
    fun f() -> Void {
        let x: S32 = abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_as_argument, R"(
    use std::abort::abort
    fun g(x: S32) -> Void { }
    fun f() -> Void {
        g(abort())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_returned, R"(
    use std::abort::abort
    fun f() -> S32 {
        ret abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_assigned, R"(
    use std::abort::abort
    fun f() -> Void {
        let mut x = 1
        x = abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_into_variant, R"(
    use std::abort::abort
    fun f() -> Void {
        let x: S32 or Bool = abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_in_binary_expression, R"(
    use std::abort::abort
    fun f() -> Void {
        let x: S32 = 1 + abort()
    }
)");

// Wherever the value is consumed, the dead code after "!" still has to lower to valid IR.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_as_object_field, R"(
    use std::abort::abort
    cls A { !public x: S32 }
    fun f() -> Void {
        let a = A(x=abort())
        std::mem::ops::drop(a)
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_as_tuple_element, R"(
    use std::abort::abort
    fun f() -> Void {
        let t: (S32, Bool) = (1, abort())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_as_array_element, R"(
    use std::abort::abort
    fun f() -> Void {
        let a: [S32; 2_uz] = [1, abort()]
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_value_as_loop_exit_value, R"(
    use std::abort::abort
    fun f(b: Bool) -> Void {
        let x: S32 = loop b { exit abort() } else { 1 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_exit_before_valued_exit, R"(
    use std::abort::abort
    fun f(b: Bool, c: Bool) -> Void {
        let x: S32 = loop b {
            case c { exit abort() } else { exit 1 }
        } else { 2 }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_valued_exit_before_never_exit, R"(
    use std::abort::abort
    fun f(b: Bool, c: Bool) -> Void {
        let x: S32 = loop b {
            case c { exit 1 } else { exit abort() }
        } else { 2 }
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_exits_still_mismatch,
  SppTypeMismatchError, R"(
    fun f(b: Bool, c: Bool) -> Void {
        let x = loop b {
            case c { exit 1 } else { exit false }
        } else { 2 }
    }
)");

// The name, an alias, and an alias of an alias are all "!" too.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_by_type_name_value_in_typed_let, R"(
    use std::abort::abort
    fun f() -> std::never::Never { abort() }
    fun g() -> Void {
        let x: Bool = f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_qualified_call_value_in_typed_let, R"(
    fun g() -> Void {
        let x: S32 = std::abort::abort()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_alias_value_in_typed_let, R"(
    use std::abort::abort
    type Diverge = !
    fun f() -> Diverge { abort() }
    fun g() -> Void {
        let x: S32 = f()
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_alias_by_name_value_as_argument, R"(
    use std::abort::abort
    type Diverge = std::never::Never
    fun f() -> Diverge { abort() }
    fun h(x: S32) -> Void { }
    fun g() -> Void {
        h(f())
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_alias_of_alias_value_returned, R"(
    type A = !
    type B = A
    fun f() -> B { loop true { } }
    fun g() -> S32 {
        ret f()
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_value_in_never_alias_typed_let,
  SppTypeMismatchError, R"(
    type Diverge = !
    fun f() -> Void {
        let x: Diverge = 1
    }
)");

// "!" fits every parameter type, so it cannot choose between overloads.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_never_value_ambiguous_overload,
  SppFunctionCallOverloadAmbiguousError, R"(
    use std::abort::abort
    fun g(x: S32) -> Void { }
    fun g(x: Bool) -> Void { }
    fun f() -> Void {
        g(abort())
    }
)");

// The checks that ask "is this !" still say no to every other type.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_valued_function_without_ret_still_rejected,
  SppFunctionSubroutineMissingReturnStatementError, R"(
    fun f() -> S32 {
        let x = 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_discarded_value_still_rejected,
  SppDiscardedValueError, R"(
    fun f() -> Void {
        1
        let y = 2
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_case_branches_still_mismatch,
  SppTypeMismatchError, R"(
    fun f(b: Bool) -> Void {
        let x = case b { 1 } else { false }
    }
)");

// Nothing fits into "!" except another "!".

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_value_in_never_typed_let,
  SppTypeMismatchError, R"(
    fun f() -> Void {
        let x: ! = 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_value_as_never_argument,
  SppFunctionCallNoValidSignaturesError, R"(
    fun g(x: !) -> Void { }
    fun f() -> Void {
        g(1)
    }
)");

// "case" branches: a diverging branch takes no part in the case's type.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_else_branch, R"(
    use std::abort::abort
    fun f(b: Bool) -> Void {
        let x = case b { 1 } else { abort() }
        let y: S32 = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_first_branch, R"(
    use std::abort::abort
    fun f(b: Bool) -> Void {
        let x = case b { abort() } else { 1 }
        let y: S32 = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_into_typed_let, R"(
    use std::abort::abort
    fun f(b: Bool) -> Void {
        let x: S32 = case b { 1 } else { abort() }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_every_branch_diverges, R"(
    use std::abort::abort
    fun f(b: Bool) -> ! {
        case b { abort() } else { loop true { } }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_every_branch_diverges_in_valued_function, R"(
    use std::abort::abort
    fun f(b: Bool) -> S32 {
        case b { abort() } else { abort() }
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_nested, R"(
    use std::abort::abort
    fun f(a: Bool, b: Bool) -> Void {
        let x = case a { abort() } else { case b { 1 } else { abort() } }
        let y: S32 = x
    }
)");

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_case_infinite_loop_branch, R"(
    fun f(b: Bool) -> S32 {
        ret case b { 1 } else { loop true { } }
    }
)");

// Statement position: a "!" value may be discarded; nothing after it can run.

SPP_TEST_SHOULD_PASS_SEMANTIC(
  TestNeverType,
  test_valid_never_call_discarded_in_void_function, R"(
    use std::abort::abort
    fun f() -> Void {
        abort()
    }
)");

// Todo: a call returning "!" and a "loop true" without an "exit" both diverge, but neither answers "Terminates", so
//  the statements after them are not reported. Expected red until they do.

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_statement_after_never_call,
  SppUnreachableCodeError, R"(
    use std::abort::abort
    fun f() -> Void {
        abort()
        let x = 1
    }
)");

SPP_TEST_SHOULD_FAIL_SEMANTIC(
  TestNeverType,
  test_invalid_statement_after_infinite_loop,
  SppUnreachableCodeError, R"(
    fun f() -> Void {
        loop true { }
        let x = 1
    }
)");
