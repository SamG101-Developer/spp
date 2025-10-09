#include "../test_macros.hpp"


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_variable_target, R"(
        fun f() -> std::void::Void {
            let mut a = 1
            a = 2
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_owned_attribute_target, R"(
        cls A {
            b: std::boolean::Bool
        }

        fun f(mut a: A) -> std::void::Void {
            a.b = true
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_mutably_borrowed_attribute_target, R"(
        cls A {
            b: std::boolean::Bool
        }

        fun f(a: &mut A) -> std::void::Void {
            a.b = true
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_mutable_val_immutable_borrow, R"(
        fun f(mut a: &std::boolean::Bool, b: &std::boolean::Bool) -> std::void::Void {
            a = b
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_non_initialized_immutable_variable_target, R"(
        fun f() -> std::void::Void {
            let a: std::boolean::Bool
            a = false
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_non_initialized_mutbale_variable_targe, R"(
        fun f() -> std::void::Void {
            let mut a: std::boolean::Bool
            a = false
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_non_initialized_mutbale_variable_target_then_reassign, R"(
        fun f() -> std::void::Void {
            let mut a: std::boolean::Bool
            a = false
            a = true
        }
)")


SPP_TEST_SHOULD_PASS_SEMANTIC(
    AssignmentStatementAst,
    test_valid_convention_mismatch_but_valid_coercion, R"(
        fun f(b: &mut std::boolean::Bool) -> std::void::Void {
            let mut x: &std::boolean::Bool
            x = b
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_convention_mismatch_1,
    SppTypeMismatchError, R"(
        fun f(b: std::boolean::Bool) -> std::void::Void {
            let mut x: &mut std::boolean::Bool
            x = b
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_convention_mismatch_2,
    SppTypeMismatchError, R"(
        fun f(b: std::boolean::Bool) -> std::void::Void {
            let mut x: &std::boolean::Bool
            x = b
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_convention_mismatch_3,
    SppTypeMismatchError, R"(
        fun f(b: &mut std::boolean::Bool) -> std::void::Void {
            let mut x: std::boolean::Bool
            x = b
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_convention_mismatch_4,
    SppTypeMismatchError, R"(
        fun f(b: &std::boolean::Bool) -> std::void::Void {
            let mut x: &mut std::boolean::Bool
            x = b
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_convention_mismatch_5,
    SppTypeMismatchError, R"(
        fun f(b: &std::boolean::Bool) -> std::void::Void {
            let mut x: std::boolean::Bool
            x = b
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_type_mismatch_variable_target,
    SppTypeMismatchError, R"(
        fun f() -> std::void::Void {
            let mut a = 1
            a = "2"
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_type_mismatch_attribute_target,
    SppTypeMismatchError, R"(
        cls A {
            b: std::boolean::Bool
        }

        fun f(mut a: A) -> std::void::Void {
            a.b = "1"
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_immutable_variable_target,
    SppInvalidMutationError, R"(
        fun f() -> std::void::Void {
            let a = 1
            a = 2
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_immutable_attribute_target,
    SppInvalidMutationError, R"(
        cls A {
            b: std::boolean::Bool
        }

        fun f(a: A) -> std::void::Void {
            a.b = true
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_immutable_borrow_attribute_target,
    SppInvalidMutationError, R"(
        cls A {
            b: std::boolean::Bool
        }

        fun f(a: &A) -> std::void::Void {
            a.b = true
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_mutable_val_immutable_borrow_attribute_target,
    SppInvalidMutationError, R"(
        cls A {
            b: std::boolean::Bool
        }

        fun f(mut a: &A) -> std::void::Void {
            a.b = true
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_non_symbolic_target,
    SppCompoundAssignmentTargetError, R"(
        fun f() -> std::void::Void {
            1 = 2
        }
)")


SPP_TEST_SHOULD_FAIL_SEMANTIC(
    AssignmentStatementAst,
    test_invalid_non_initialized_immutable_variable_then_reassign,
    SppInvalidMutationError, R"(
        fun f() -> std::void::Void {
            let a: std::boolean::Bool
            a = false
            a = true
        }
)")
