#pragma once

#include <spp/utils/errors.hpp>
#include <spp/asts/_fwd.hpp>

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>


namespace spp::analyse::errors {
    struct SppAnnotationInvalidApplicationError;
    struct SppAnnotationConflictError;
    struct SppAnnotationInvalidError;
    struct SppExpressionTypeInvalidError;
    struct SppTypeMismatchError;
    struct SppSecondClassBorrowViolationError;
    struct SppCompileTimeConstantError;
    struct SppInvalidMutationError;
    struct SppUninitializedMemoryUseError;
    struct SppPartiallyInitializedMemoryUseError;
    struct SppMoveFromBorrowedMemoryError;
    struct SppMoveFromPinnedMemoryError;
    struct SppMoveFromPinLinkedMemoryError;
    struct SppInconsistentlyInitializedMemoryUseError;
    struct SppInconsistentlyPinnedMemoryUseError;
    struct SppCompoundAssignmentTargetError;
    struct SppMemberAccessNonIndexableError;
    struct SppMemberAccessOutOfBoundsError;
    struct SppCaseBranchMultipleDestructuresError;
    struct SppCaseBranchElseNotLastError;
    struct SppCaseBranchMissingElseError;
    struct SppIdentifierDuplicateError;
    struct SppRecursiveTypeError;
    struct SppCoroutineInvalidReturnTypeError;
    struct SppFloatOutOfBoundsError;
    struct SppIntegerOutOfBoundsError;
    struct SppOrderInvalidError;
    struct SppExpansionOfNonTupleError;
    struct SppMemoryOverlapUsageError;
    struct SppMultipleSelfParametersError;
    struct SppSelfParamInFreeFunctionError;
    struct SppFunctionPrototypeConflictError;
    struct SppFunctionSubroutineContainsGenExpressionError;
    struct SppYieldedTypeMismatchError;
    struct SppIdentifierUnknownError;
    struct SppUnreachableCodeError;
    struct SppIterExpressionPatternTypeDuplicateError;
    struct SppIterExpressionPatternIncompatibleError;
    struct SppIterExpressionPatternMissingError;
    struct SppInvalidTypeAnnotationError;
    struct SppMultipleSkipMultiArgumentsError;
    struct SppVariableArrayDestructureArrayTypeMismatchError;
    struct SppVariableArrayDestructureArraySizeMismatchError;
    struct SppVariableTupleDestructureTupleTypeMismatchError;
    struct SppVariableTupleDestructureTupleSizeMismatchError;
    struct SppVariableObjectDestructureWithBoundMultiSkipError;
    struct SppExpressionNotBooleanError;
    struct SppLoopTooManyControlFlowStatementsError;
    struct SppObjectInitializerMultipleAutofillArgumentsError;
    struct SppArgumentNameInvalidError;
    struct SppArgumentMissingError;
    struct SppEarlyReturnRequiresTryTypeError;
    struct SppFunctionCallAbstractFunctionError;
    struct SppFunctionCallNotImplFunctionError;
    struct SppFunctionCallTooManyArgumentsError;
    struct SppFunctionFoldTupleElementTypeMismatchError;
    struct SppFunctionFoldTupleLengthMismatchError;
    struct SppFunctionCallNoValidSignaturesError;
    struct SppFunctionCallOverloadAmbiguousError;
    struct SppMemberAccessStaticOperatorExpectedError;
    struct SppMemberAccessRuntimeOperatorExpectedError;
    struct SppGenericTypeInvalidUsageError;
    struct SppAmbiguousMemberAccessError;
    struct SppFunctionCoroutineContainsRetExprExpressionError;

    auto add_header(std::size_t err_code, std::string &&msg) -> std::string;
    auto add_error(std::size_t pos, std::string &&tag) -> std::string;
    auto add_context_for_error(std::size_t pos, std::string &&msg) -> std::string;
    auto add_footer(std::string &&note, std::string &&help) -> std::string;
}


struct spp::analyse::errors::SppAnnotationInvalidApplicationError final : spp::utils::errors::SemanticError {
    explicit SppAnnotationInvalidApplicationError(asts::AnnotationAst const &annotation, asts::Ast const &ctx, std::string &&block_list);
};


struct spp::analyse::errors::SppAnnotationConflictError final : spp::utils::errors::SemanticError {
    explicit SppAnnotationConflictError(asts::AnnotationAst const &first_annotation, asts::AnnotationAst const &conflicting_annotation, asts::Ast const &ctx);
};


struct spp::analyse::errors::SppAnnotationInvalidError final : spp::utils::errors::SemanticError {
    explicit SppAnnotationInvalidError(asts::AnnotationAst const &annotation);
};


struct spp::analyse::errors::SppExpressionTypeInvalidError final : spp::utils::errors::SemanticError {
    explicit SppExpressionTypeInvalidError(asts::Ast const &expr);
};


struct spp::analyse::errors::SppTypeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty);
};


struct spp::analyse::errors::SppSecondClassBorrowViolationError final : spp::utils::errors::SemanticError {
    explicit SppSecondClassBorrowViolationError(asts::Ast const &expr, asts::ConventionAst const &conv, std::string_view ctx);
};


struct spp::analyse::errors::SppCompileTimeConstantError final : spp::utils::errors::SemanticError {
    explicit SppCompileTimeConstantError(asts::ExpressionAst const &expr);
};


struct spp::analyse::errors::SppInvalidMutationError final : spp::utils::errors::SemanticError {
    explicit SppInvalidMutationError(asts::Ast const &sym, asts::Ast const &mutator, asts::Ast const &initialization_location);
};


struct spp::analyse::errors::SppUninitializedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppUninitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location);
};


struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppPartiallyInitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &partial_move_location);
};


struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : spp::utils::errors::SemanticError {
    explicit SppMoveFromBorrowedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &move_location, asts::Ast const &borrow_location);
};


struct spp::analyse::errors::SppMoveFromPinnedMemoryError final : spp::utils::errors::SemanticError {
    explicit SppMoveFromPinnedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location, asts::Ast const &pin_location);
};


struct spp::analyse::errors::SppMoveFromPinLinkedMemoryError final : spp::utils::errors::SemanticError {
    explicit SppMoveFromPinLinkedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location, asts::Ast const &pin_location, asts::Ast const &pin_init_location);
};


struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppInconsistentlyInitializedMemoryUseError(asts::ExpressionAst const &ast, std::pair<asts::CaseExpressionBranchAst*, bool> branch_1, std::pair<asts::CaseExpressionBranchAst*, bool> branch_2, std::string_view what);
};


struct spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError final : spp::utils::errors::SemanticError {
    explicit SppInconsistentlyPinnedMemoryUseError(asts::ExpressionAst const &ast, std::pair<asts::CaseExpressionBranchAst*, bool> branch_1, std::pair<asts::CaseExpressionBranchAst*, bool> branch_2);
};


struct spp::analyse::errors::SppCompoundAssignmentTargetError final : spp::utils::errors::SemanticError {
    explicit SppCompoundAssignmentTargetError(asts::ExpressionAst const &lhs);
};


struct spp::analyse::errors::SppMemberAccessNonIndexableError final : spp::utils::errors::SemanticError {
    explicit SppMemberAccessNonIndexableError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, asts::Ast const &access_op);
};


struct spp::analyse::errors::SppMemberAccessOutOfBoundsError final : spp::utils::errors::SemanticError {
    explicit SppMemberAccessOutOfBoundsError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, asts::Ast const &access_op);
};


struct spp::analyse::errors::SppCaseBranchMultipleDestructuresError final : spp::utils::errors::SemanticError {
    explicit SppCaseBranchMultipleDestructuresError(asts::CasePatternVariantAst const &first_pattern, asts::CasePatternVariantAst const &second_pattern);
};


struct spp::analyse::errors::SppCaseBranchElseNotLastError final : spp::utils::errors::SemanticError {
    explicit SppCaseBranchElseNotLastError(asts::CaseExpressionBranchAst const &non_last_else_branch, asts::CaseExpressionBranchAst const &last_branch);
};


struct spp::analyse::errors::SppCaseBranchMissingElseError final : spp::utils::errors::SemanticError {
    explicit SppCaseBranchMissingElseError(asts::CaseExpressionAst const &case_expr, asts::CaseExpressionBranchAst const &last_branch);
};


struct spp::analyse::errors::SppIdentifierDuplicateError final : spp::utils::errors::SemanticError {
    explicit SppIdentifierDuplicateError(asts::IdentifierAst const &first_identifier, asts::IdentifierAst const &duplicate_identifier, std::string_view what);
};


struct spp::analyse::errors::SppRecursiveTypeError final : spp::utils::errors::SemanticError {
    explicit SppRecursiveTypeError(asts::ClassPrototypeAst const &type, asts::TypeAst const &recursion);
};


struct spp::analyse::errors::SppCoroutineInvalidReturnTypeError final : spp::utils::errors::SemanticError {
    explicit SppCoroutineInvalidReturnTypeError(asts::CoroutinePrototypeAst const &proto, asts::TypeAst const &return_type);
};


struct spp::analyse::errors::SppFloatOutOfBoundsError final : spp::utils::errors::SemanticError {
    explicit SppFloatOutOfBoundsError(asts::LiteralAst const &literal, boost::multiprecision::cpp_dec_float_100 value, boost::multiprecision::cpp_dec_float_100 lower, boost::multiprecision::cpp_dec_float_100 upper, std::string_view what);
};


struct spp::analyse::errors::SppIntegerOutOfBoundsError final : spp::utils::errors::SemanticError {
    explicit SppIntegerOutOfBoundsError(asts::LiteralAst const &literal, boost::multiprecision::cpp_int value, boost::multiprecision::cpp_int lower, boost::multiprecision::cpp_int upper, std::string_view what);
};


struct spp::analyse::errors::SppOrderInvalidError final : spp::utils::errors::SemanticError {
    explicit SppOrderInvalidError(std::string_view first_what, asts::Ast const &first, std::string_view second_what, asts::Ast const &second);
};


struct spp::analyse::errors::SppExpansionOfNonTupleError final : spp::utils::errors::SemanticError {
    explicit SppExpansionOfNonTupleError(asts::Ast const &ast, asts::TypeAst const &type);
};


struct spp::analyse::errors::SppMemoryOverlapUsageError final : spp::utils::errors::SemanticError {
    explicit SppMemoryOverlapUsageError(asts::Ast const &ast, asts::Ast const &overlap_ast);
};


struct spp::analyse::errors::SppMultipleSelfParametersError final : spp::utils::errors::SemanticError {
    explicit SppMultipleSelfParametersError(asts::FunctionParameterSelfAst const &first_self, asts::FunctionParameterSelfAst const &second_self);
};


struct spp::analyse::errors::SppSelfParamInFreeFunctionError final : spp::utils::errors::SemanticError {
    explicit SppSelfParamInFreeFunctionError(asts::FunctionPrototypeAst const &function_proto, asts::FunctionParameterSelfAst const &self_param);
};


// todo: remember to use m_orig for func names
struct spp::analyse::errors::SppFunctionPrototypeConflictError final : spp::utils::errors::SemanticError {
    explicit SppFunctionPrototypeConflictError(asts::FunctionPrototypeAst const &first_proto, asts::FunctionPrototypeAst const &second_proto);
};


struct spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError final : spp::utils::errors::SemanticError {
    explicit SppFunctionSubroutineContainsGenExpressionError(asts::TokenAst const &fun_tag, asts::TokenAst const &gen_expr);
};


struct spp::analyse::errors::SppYieldedTypeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppYieldedTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty, bool is_optional, bool is_Fallible, asts::TypeAst const &error_type);
};


struct spp::analyse::errors::SppIdentifierUnknownError final : spp::utils::errors::SemanticError {
    explicit SppIdentifierUnknownError(asts::Ast const &name, std::string_view what, std::optional<std::string> closest);
};


struct spp::analyse::errors::SppUnreachableCodeError final : spp::utils::errors::SemanticError {
    explicit SppUnreachableCodeError(asts::Ast const &member, asts::Ast const &next_member);
};


struct spp::analyse::errors::SppIterExpressionPatternTypeDuplicateError final : spp::utils::errors::SemanticError {
    explicit SppIterExpressionPatternTypeDuplicateError(asts::IterPatternVariantAst const &first_branch, asts::IterPatternVariantAst const &second_branch);
};


struct spp::analyse::errors::SppIterExpressionPatternIncompatibleError final : spp::utils::errors::SemanticError {
    explicit SppIterExpressionPatternIncompatibleError(asts::ExpressionAst const &cond, asts::TypeAst const &cond_type, asts::IterPatternVariantAst const &pattern, asts::TypeAst const &required_generator_type);
};


struct spp::analyse::errors::SppIterExpressionPatternMissingError final : spp::utils::errors::SemanticError {
    explicit SppIterExpressionPatternMissingError(asts::ExpressionAst const &cond, asts::TypeAst const &cond_type);
};


struct spp::analyse::errors::SppInvalidTypeAnnotationError final : spp::utils::errors::SemanticError {
    explicit SppInvalidTypeAnnotationError(asts::TypeAst const &type, asts::LocalVariableAst const &var);
};


struct spp::analyse::errors::SppMultipleSkipMultiArgumentsError final : spp::utils::errors::SemanticError {
    explicit SppMultipleSkipMultiArgumentsError(asts::LocalVariableAst const &var, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &first_arg, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &second_arg);
};


struct spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppVariableArrayDestructureArrayTypeMismatchError(asts::LocalVariableDestructureArrayAst const &var, asts::ExpressionAst const &val, asts::TypeAst const &val_type);
};


struct spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppVariableArrayDestructureArraySizeMismatchError(asts::LocalVariableDestructureArrayAst const &var, std::size_t var_size, asts::ExpressionAst const &val, std::size_t val_size);
};


struct spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppVariableTupleDestructureTupleTypeMismatchError(asts::LocalVariableDestructureTupleAst const &var, asts::ExpressionAst const &val, asts::TypeAst const &val_type);
};


struct spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppVariableTupleDestructureTupleSizeMismatchError(asts::LocalVariableDestructureTupleAst const &var, std::size_t var_size, asts::ExpressionAst const &val, std::size_t val_size);
};


struct spp::analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError final : spp::utils::errors::SemanticError {
    explicit SppVariableObjectDestructureWithBoundMultiSkipError(asts::LocalVariableDestructureObjectAst const &var, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &multi_skip);
};


struct spp::analyse::errors::SppExpressionNotBooleanError final : spp::utils::errors::SemanticError {
    explicit SppExpressionNotBooleanError(asts::Ast const &expr, asts::TypeAst const &expr_type, std::string_view what);
};


struct spp::analyse::errors::SppLoopTooManyControlFlowStatementsError final : spp::utils::errors::SemanticError {
    explicit SppLoopTooManyControlFlowStatementsError(asts::TokenAst const &tok_loop, asts::LoopControlFlowStatementAst const &stmt, std::size_t num_controls, std::size_t loop_depth);
};


struct spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError final : spp::utils::errors::SemanticError {
    explicit SppObjectInitializerMultipleAutofillArgumentsError(asts::ObjectInitializerArgumentAst const &arg1, asts::ObjectInitializerArgumentAst const &arg2);
};


struct spp::analyse::errors::SppArgumentNameInvalidError final : spp::utils::errors::SemanticError {
    explicit SppArgumentNameInvalidError(asts::Ast const &target, std::string_view target_what, asts::Ast const &source, std::string_view source_what);
};


struct spp::analyse::errors::SppArgumentMissingError final : spp::utils::errors::SemanticError {
    explicit SppArgumentMissingError(asts::Ast const &target, std::string_view target_what, asts::Ast const &source, std::string_view source_what);
};


struct spp::analyse::errors::SppEarlyReturnRequiresTryTypeError final : spp::utils::errors::SemanticError {
    explicit SppEarlyReturnRequiresTryTypeError(asts::PostfixExpressionOperatorEarlyReturnAst const &early_ret, asts::ExpressionAst const &expr, asts::TypeAst const &type);
};


struct spp::analyse::errors::SppFunctionCallAbstractFunctionError final : spp::utils::errors::SemanticError {
    explicit SppFunctionCallAbstractFunctionError(asts::PostfixExpressionOperatorFunctionCallAst const &call, asts::FunctionPrototypeAst const &proto);
};


struct spp::analyse::errors::SppFunctionCallNotImplFunctionError final : spp::utils::errors::SemanticError {
    explicit SppFunctionCallNotImplFunctionError(asts::PostfixExpressionOperatorFunctionCallAst const &call, asts::FunctionPrototypeAst const &proto);
};


struct spp::analyse::errors::SppFunctionCallTooManyArgumentsError final : spp::utils::errors::SemanticError {
    explicit SppFunctionCallTooManyArgumentsError(asts::PostfixExpressionOperatorFunctionCallAst const &call, asts::FunctionPrototypeAst const &proto);
};


struct spp::analyse::errors::SppFunctionFoldTupleElementTypeMismatchError final : spp::utils::errors::SemanticError {
    explicit SppFunctionFoldTupleElementTypeMismatchError(asts::TypeAst const &type, asts::ExpressionAst const &expr);
};


struct spp::analyse::errors::SppFunctionFoldTupleLengthMismatchError final : spp::utils::errors::SemanticError {
    explicit SppFunctionFoldTupleLengthMismatchError(asts::ExpressionAst const &first_tup, std::size_t first_length, asts::ExpressionAst const &second_tup, std::size_t second_length);
};


struct spp::analyse::errors::SppFunctionCallNoValidSignaturesError final : spp::utils::errors::SemanticError {
    explicit SppFunctionCallNoValidSignaturesError(asts::PostfixExpressionOperatorFunctionCallAst const &call, std::string_view sigs, std::string_view attempted);
};


struct spp::analyse::errors::SppFunctionCallOverloadAmbiguousError final : spp::utils::errors::SemanticError {
    explicit SppFunctionCallOverloadAmbiguousError(asts::PostfixExpressionOperatorFunctionCallAst const &call, std::string_view sigs, std::string_view attempted);
};


struct spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError final : spp::utils::errors::SemanticError {
    explicit SppMemberAccessStaticOperatorExpectedError(asts::Ast const &lhs, asts::TokenAst const &access);
};


struct spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError final : spp::utils::errors::SemanticError {
    explicit SppMemberAccessRuntimeOperatorExpectedError(asts::Ast const &lhs, asts::TokenAst const &access);
};


struct spp::analyse::errors::SppGenericTypeInvalidUsageError final : spp::utils::errors::SemanticError {
    explicit SppGenericTypeInvalidUsageError(asts::Ast const &gen_name, asts::TypeAst const &gen_val, std::string_view what);
};


struct spp::analyse::errors::SppAmbiguousMemberAccessError final : spp::utils::errors::SemanticError {
    explicit SppAmbiguousMemberAccessError(asts::Ast const &found_field_1, asts::Ast const &found_field_2, asts::Ast const &field_access);
};


struct spp::analyse::errors::SppFunctionCoroutineContainsRetExprExpressionError final : spp::utils::errors::SemanticError {
    explicit SppFunctionCoroutineContainsRetExprExpressionError(asts::TokenAst const &fun_tag, asts::TokenAst const &ret_stmt);
};
