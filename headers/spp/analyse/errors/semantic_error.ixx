module;
#include <spp/macros.hpp>

export module spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.asts._fwd;

import boost;
import std;


/// @cond
namespace spp::analyse::errors {
    SPP_EXP struct SemanticError;

    SPP_EXP struct SppAnnotationInvalidApplicationError;
    SPP_EXP struct SppAnnotationConflictError;
    SPP_EXP struct SppAnnotationInvalidError;
    SPP_EXP struct SppExpressionTypeInvalidError;
    SPP_EXP struct SppTypeMismatchError;
    SPP_EXP struct SppSecondClassBorrowViolationError;
    SPP_EXP struct SppCompileTimeConstantError;
    SPP_EXP struct SppInvalidMutationError;
    SPP_EXP struct SppUninitializedMemoryUseError;
    SPP_EXP struct SppPartiallyInitializedMemoryUseError;
    SPP_EXP struct SppMoveFromBorrowedMemoryError;
    SPP_EXP struct SppMoveFromPinnedMemoryError;
    SPP_EXP struct SppMoveFromPinLinkedMemoryError;
    SPP_EXP struct SppInconsistentlyInitializedMemoryUseError;
    SPP_EXP struct SppInconsistentlyPinnedMemoryUseError;
    SPP_EXP struct SppAssignmentTargetError;
    SPP_EXP struct SppMemberAccessNonIndexableError;
    SPP_EXP struct SppMemberAccessOutOfBoundsError;
    SPP_EXP struct SppCaseBranchMultipleDestructuresError;
    SPP_EXP struct SppCaseBranchElseNotLastError;
    SPP_EXP struct SppCaseBranchMissingElseError;
    SPP_EXP struct SppIdentifierDuplicateError;
    SPP_EXP struct SppRecursiveTypeError;
    SPP_EXP struct SppCoroutineInvalidReturnTypeError;
    SPP_EXP struct SppFloatOutOfBoundsError;
    SPP_EXP struct SppIntegerOutOfBoundsError;
    SPP_EXP struct SppOrderInvalidError;
    SPP_EXP struct SppExpansionOfNonTupleError;
    SPP_EXP struct SppMemoryOverlapUsageError;
    SPP_EXP struct SppMultipleSelfParametersError;
    SPP_EXP struct SppMultipleVariadicParametersError;
    SPP_EXP struct SppSelfParamInFreeFunctionError;
    SPP_EXP struct SppFunctionPrototypeConflictError;
    SPP_EXP struct SppFunctionSubroutineContainsGenExpressionError;
    SPP_EXP struct SppYieldedTypeMismatchError;
    SPP_EXP struct SppIdentifierUnknownError;
    SPP_EXP struct SppUnreachableCodeError;
    SPP_EXP struct SppIterExpressionPatternTypeDuplicateError;
    SPP_EXP struct SppIterExpressionPatternIncompatibleError;
    SPP_EXP struct SppIterExpressionPatternMissingError;
    SPP_EXP struct SppInvalidTypeAnnotationError;
    SPP_EXP struct SppMultipleSkipMultiArgumentsError;
    SPP_EXP struct SppVariableArrayDestructureArrayTypeMismatchError;
    SPP_EXP struct SppVariableArrayDestructureArraySizeMismatchError;
    SPP_EXP struct SppVariableTupleDestructureTupleTypeMismatchError;
    SPP_EXP struct SppVariableTupleDestructureTupleSizeMismatchError;
    SPP_EXP struct SppVariableObjectDestructureWithBoundMultiSkipError;
    SPP_EXP struct SppExpressionNotBooleanError;
    SPP_EXP struct SppExpressionNotGeneratorError;
    SPP_EXP struct SppExpressionAmbiguousGeneratorError;
    SPP_EXP struct SppExpressionNotIndexableError;
    SPP_EXP struct SppExpressionAmbiguousIndexableError;
    SPP_EXP struct SppLoopTooManyControlFlowStatementsError;
    SPP_EXP struct SppObjectInitializerMultipleAutofillArgumentsError;
    SPP_EXP struct SppObjectInitializerInvalidArgumentError;
    SPP_EXP struct SppObjectInitializerGenericWithArgsError;
    SPP_EXP struct SppArgumentNameInvalidError;
    SPP_EXP struct SppArgumentMissingError;
    SPP_EXP struct SppEarlyReturnRequiresTryTypeError;
    SPP_EXP struct SppFunctionCallAbstractFunctionError;
    SPP_EXP struct SppFunctionCallNotImplFunctionError;
    SPP_EXP struct SppFunctionCallTooManyArgumentsError;
    SPP_EXP struct SppFunctionFoldTupleElementTypeMismatchError;
    SPP_EXP struct SppFunctionFoldTupleLengthMismatchError;
    SPP_EXP struct SppFunctionCallNoValidSignaturesError;
    SPP_EXP struct SppFunctionCallOverloadAmbiguousError;
    SPP_EXP struct SppMemberAccessStaticOperatorExpectedError;
    SPP_EXP struct SppMemberAccessRuntimeOperatorExpectedError;
    SPP_EXP struct SppGenericTypeInvalidUsageError;
    SPP_EXP struct SppAmbiguousMemberAccessError;
    SPP_EXP struct SppCoroutineContainsRetExprExpressionError;
    SPP_EXP struct SppFunctionSubroutineMissingReturnStatementError;
    SPP_EXP struct SppSuperimpositionCyclicExtensionError;
    SPP_EXP struct SppSuperimpositionDoubleExtensionError;
    SPP_EXP struct SppSuperimpositionSelfExtensionError;
    SPP_EXP struct SppSuperimpositionExtensionMethodInvalidError;
    SPP_EXP struct SppSuperimpositionExtensionNonVirtualMethodOverriddenError;
    SPP_EXP struct SppSuperimpositionOptionalGenericParameterError;
    SPP_EXP struct SppSuperimpositionUnconstrainedGenericParameterError;
    SPP_EXP struct SppSuperimpositionExtensionTypeStatementInvalidError;
    SPP_EXP struct SppSuperimpositionExtensionCmpStatementInvalidError;
    SPP_EXP struct SppAsyncTargetNotFunctionCallError;
    SPP_EXP struct SppDereferenceInvalidExpressionNonBorrowedTypeError;
    SPP_EXP struct SppInvalidExpressionNonCopyableTypeError;
    SPP_EXP struct SppGenericParameterInferredConflictInferredError;
    SPP_EXP struct SppGenericParameterNotInferredError;
    SPP_EXP struct SppGenericArgumentTooManyError;
    SPP_EXP struct SppMissingMainFunctionError;
}

/// @endcond


SPP_EXP struct spp::analyse::errors::SemanticError : spp::utils::errors::AbstractError {
    using AbstractError::AbstractError;
    SemanticError(SemanticError const &) = default;
    ~SemanticError() override = default;

    enum class ErrorInformationType {
        HEADER, ERROR, CONTEXT, FOOTER
    };

    std::vector<std::tuple<asts::Ast const*, ErrorInformationType, std::string, std::string>> m_error_info;

    auto add_header(std::size_t err_code, std::string &&msg) -> void;

    auto add_error(asts::Ast const *ast, std::string &&tag) -> void;

    auto add_context_for_error(asts::Ast const *ast, std::string &&tag) -> void;

    auto add_footer(std::string &&note, std::string &&help) -> void;

    SPP_ATTR_NODISCARD
    auto clone() const -> std::unique_ptr<SemanticError>;
};


SPP_EXP struct spp::analyse::errors::SppAnnotationInvalidApplicationError final : SemanticError {
    explicit SppAnnotationInvalidApplicationError(asts::AnnotationAst const &annotation, asts::Ast const &ctx, std::string &&block_list);
};


SPP_EXP struct spp::analyse::errors::SppAnnotationConflictError final : SemanticError {
    explicit SppAnnotationConflictError(asts::AnnotationAst const &first_annotation, asts::AnnotationAst const &conflicting_annotation, asts::Ast const &ctx);
};


SPP_EXP struct spp::analyse::errors::SppAnnotationInvalidError final : SemanticError {
    explicit SppAnnotationInvalidError(asts::AnnotationAst const &annotation);
};


SPP_EXP struct spp::analyse::errors::SppExpressionTypeInvalidError final : SemanticError {
    explicit SppExpressionTypeInvalidError(asts::Ast const &expr);
};


SPP_EXP struct spp::analyse::errors::SppTypeMismatchError final : SemanticError {
    explicit SppTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty);
};


SPP_EXP struct spp::analyse::errors::SppSecondClassBorrowViolationError final : SemanticError {
    explicit SppSecondClassBorrowViolationError(asts::Ast const &expr, asts::Ast const &conv, std::string_view ctx);
};


SPP_EXP struct spp::analyse::errors::SppCompileTimeConstantError final : SemanticError {
    explicit SppCompileTimeConstantError(asts::ExpressionAst const &expr);
};


SPP_EXP struct spp::analyse::errors::SppInvalidMutationError final : SemanticError {
    explicit SppInvalidMutationError(asts::Ast const &sym, asts::Ast const &mutator, asts::Ast const &initialization_location);
};


SPP_EXP struct spp::analyse::errors::SppUninitializedMemoryUseError final : SemanticError {
    explicit SppUninitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location);
};


SPP_EXP struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : SemanticError {
    explicit SppPartiallyInitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &partial_move_location);
};


SPP_EXP struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : SemanticError {
    explicit SppMoveFromBorrowedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &move_location, asts::Ast const &borrow_location);
};


SPP_EXP struct spp::analyse::errors::SppMoveFromPinnedMemoryError final : SemanticError {
    explicit SppMoveFromPinnedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location, asts::Ast const &pin_location);
};


SPP_EXP struct spp::analyse::errors::SppMoveFromPinLinkedMemoryError final : SemanticError {
    explicit SppMoveFromPinLinkedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location, asts::Ast const &pin_location);
};


SPP_EXP struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : SemanticError {
    explicit SppInconsistentlyInitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &branch_1, asts::Ast const &branch_2, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError final : SemanticError {
    explicit SppInconsistentlyPinnedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &branch_1, asts::Ast const &branch_2);
};


SPP_EXP struct spp::analyse::errors::SppAssignmentTargetError final : SemanticError {
    explicit SppAssignmentTargetError(asts::ExpressionAst const &lhs);
};


SPP_EXP struct spp::analyse::errors::SppMemberAccessNonIndexableError final : SemanticError {
    explicit SppMemberAccessNonIndexableError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, asts::Ast const &access_op);
};


SPP_EXP struct spp::analyse::errors::SppMemberAccessOutOfBoundsError final : SemanticError {
    explicit SppMemberAccessOutOfBoundsError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, asts::Ast const &access_op);
};


SPP_EXP struct spp::analyse::errors::SppCaseBranchMultipleDestructuresError final : SemanticError {
    explicit SppCaseBranchMultipleDestructuresError(asts::CasePatternVariantAst const &first_pattern, asts::CasePatternVariantAst const &second_pattern);
};


SPP_EXP struct spp::analyse::errors::SppCaseBranchElseNotLastError final : SemanticError {
    explicit SppCaseBranchElseNotLastError(asts::CaseExpressionBranchAst const &non_last_else_branch, asts::CaseExpressionBranchAst const &last_branch);
};


SPP_EXP struct spp::analyse::errors::SppCaseBranchMissingElseError final : SemanticError {
    explicit SppCaseBranchMissingElseError(asts::CaseExpressionAst const &case_expr, asts::CaseExpressionBranchAst const &last_branch);
};


SPP_EXP struct spp::analyse::errors::SppIdentifierDuplicateError final : SemanticError {
    explicit SppIdentifierDuplicateError(asts::Ast const &first_identifier, asts::Ast const &duplicate_identifier, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppRecursiveTypeError final : SemanticError {
    explicit SppRecursiveTypeError(asts::ClassPrototypeAst const &type, asts::TypeAst const &recursion);
};


SPP_EXP struct spp::analyse::errors::SppCoroutineInvalidReturnTypeError final : SemanticError {
    explicit SppCoroutineInvalidReturnTypeError(asts::CoroutinePrototypeAst const &proto, asts::TypeAst const &return_type);
};


SPP_EXP struct spp::analyse::errors::SppFloatOutOfBoundsError final : SemanticError {
    explicit SppFloatOutOfBoundsError(asts::LiteralAst const &literal, boost::multiprecision::cpp_dec_float_100 const &value, boost::multiprecision::cpp_dec_float_100 const &lower, boost::multiprecision::cpp_dec_float_100 const &upper, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppIntegerOutOfBoundsError final : SemanticError {
    explicit SppIntegerOutOfBoundsError(asts::LiteralAst const &literal, boost::multiprecision::cpp_int const &value, boost::multiprecision::cpp_int const &lower, boost::multiprecision::cpp_int const &upper, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppOrderInvalidError final : SemanticError {
    explicit SppOrderInvalidError(std::string_view first_what, asts::Ast const &first, std::string_view second_what, asts::Ast const &second);
};


SPP_EXP struct spp::analyse::errors::SppExpansionOfNonTupleError final : SemanticError {
    explicit SppExpansionOfNonTupleError(asts::Ast const &ast, asts::TypeAst const &type);
};


SPP_EXP struct spp::analyse::errors::SppMemoryOverlapUsageError final : SemanticError {
    explicit SppMemoryOverlapUsageError(asts::Ast const &ast, asts::Ast const &overlap_ast);
};


SPP_EXP struct spp::analyse::errors::SppMultipleSelfParametersError final : SemanticError {
    explicit SppMultipleSelfParametersError(asts::FunctionParameterSelfAst const &first_self, asts::FunctionParameterSelfAst const &second_self);
};


SPP_EXP struct spp::analyse::errors::SppMultipleVariadicParametersError final : SemanticError {
    explicit SppMultipleVariadicParametersError(asts::FunctionParameterVariadicAst const &first_variadic, asts::FunctionParameterVariadicAst const &second_variadic);
};


SPP_EXP struct spp::analyse::errors::SppSelfParamInFreeFunctionError final : SemanticError {
    explicit SppSelfParamInFreeFunctionError(asts::FunctionPrototypeAst const &function_proto, asts::FunctionParameterSelfAst const &self_param);
};


SPP_EXP struct spp::analyse::errors::SppFunctionPrototypeConflictError final : SemanticError {
    explicit SppFunctionPrototypeConflictError(asts::FunctionPrototypeAst const &first_proto, asts::FunctionPrototypeAst const &second_proto);
};


SPP_EXP struct spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError final : SemanticError {
    explicit SppFunctionSubroutineContainsGenExpressionError(asts::TokenAst const &fun_tag, asts::TokenAst const &gen_expr);
};


SPP_EXP struct spp::analyse::errors::SppYieldedTypeMismatchError final : SemanticError {
    explicit SppYieldedTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty, bool is_optional, bool is_fallible, asts::TypeAst const &error_type);
};


SPP_EXP struct spp::analyse::errors::SppIdentifierUnknownError final : SemanticError {
    explicit SppIdentifierUnknownError(asts::Ast const &name, std::string_view what, std::optional<std::string> const &closest);
};


SPP_EXP struct spp::analyse::errors::SppUnreachableCodeError final : SemanticError {
    explicit SppUnreachableCodeError(asts::Ast const &member, asts::Ast const &next_member);
};


SPP_EXP struct spp::analyse::errors::SppIterExpressionPatternTypeDuplicateError final : SemanticError {
    explicit SppIterExpressionPatternTypeDuplicateError(asts::IterPatternVariantAst const &first_branch, asts::IterPatternVariantAst const &second_branch);
};


SPP_EXP struct spp::analyse::errors::SppIterExpressionPatternIncompatibleError final : SemanticError {
    explicit SppIterExpressionPatternIncompatibleError(asts::ExpressionAst const &cond, asts::TypeAst const &cond_type, asts::IterPatternVariantAst const &pattern, asts::TypeAst const &required_generator_type);
};


SPP_EXP struct spp::analyse::errors::SppIterExpressionPatternMissingError final : SemanticError {
    explicit SppIterExpressionPatternMissingError(asts::ExpressionAst const &cond, asts::TypeAst const &cond_type);
};


SPP_EXP struct spp::analyse::errors::SppInvalidTypeAnnotationError final : SemanticError {
    explicit SppInvalidTypeAnnotationError(asts::TypeAst const &type, asts::LocalVariableAst const &var);
};


SPP_EXP struct spp::analyse::errors::SppMultipleSkipMultiArgumentsError final : SemanticError {
    explicit SppMultipleSkipMultiArgumentsError(asts::LocalVariableAst const &var, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &first_arg, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &second_arg);
};


SPP_EXP struct spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError final : SemanticError {
    explicit SppVariableArrayDestructureArrayTypeMismatchError(asts::LocalVariableDestructureArrayAst const &var, asts::ExpressionAst const &val, asts::TypeAst const &val_type);
};


SPP_EXP struct spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError final : SemanticError {
    explicit SppVariableArrayDestructureArraySizeMismatchError(asts::LocalVariableDestructureArrayAst const &var, std::size_t var_size, asts::ExpressionAst const &val, std::size_t val_size);
};


SPP_EXP struct spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError final : SemanticError {
    explicit SppVariableTupleDestructureTupleTypeMismatchError(asts::LocalVariableDestructureTupleAst const &var, asts::ExpressionAst const &val, asts::TypeAst const &val_type);
};


SPP_EXP struct spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError final : SemanticError {
    explicit SppVariableTupleDestructureTupleSizeMismatchError(asts::LocalVariableDestructureTupleAst const &var, std::size_t var_size, asts::ExpressionAst const &val, std::size_t val_size);
};


SPP_EXP struct spp::analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError final : SemanticError {
    explicit SppVariableObjectDestructureWithBoundMultiSkipError(asts::LocalVariableDestructureObjectAst const &var, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &multi_skip);
};


SPP_EXP struct spp::analyse::errors::SppExpressionNotBooleanError final : SemanticError {
    explicit SppExpressionNotBooleanError(asts::Ast const &expr, asts::TypeAst const &expr_type, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppExpressionNotGeneratorError final : SemanticError {
    explicit SppExpressionNotGeneratorError(asts::Ast const &expr, asts::TypeAst const &expr_type, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppExpressionAmbiguousGeneratorError final : SemanticError {
    explicit SppExpressionAmbiguousGeneratorError(asts::Ast const &expr, asts::TypeAst const &expr_type, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppExpressionNotIndexableError final : SemanticError {
    explicit SppExpressionNotIndexableError(asts::Ast const &expr, asts::TypeAst const &expr_type, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppExpressionAmbiguousIndexableError final : SemanticError {
    explicit SppExpressionAmbiguousIndexableError(asts::Ast const &expr, asts::TypeAst const &expr_type, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppLoopTooManyControlFlowStatementsError final : SemanticError {
    explicit SppLoopTooManyControlFlowStatementsError(asts::TokenAst const &tok_loop, asts::LoopControlFlowStatementAst const &stmt, std::size_t num_controls, std::size_t loop_depth);
};


SPP_EXP struct spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError final : SemanticError {
    explicit SppObjectInitializerMultipleAutofillArgumentsError(asts::ObjectInitializerArgumentAst const &arg1, asts::ObjectInitializerArgumentAst const &arg2);
};


SPP_EXP struct spp::analyse::errors::SppObjectInitializerInvalidArgumentError final : SemanticError {
    explicit SppObjectInitializerInvalidArgumentError(asts::ObjectInitializerArgumentAst const &arg);
};


SPP_EXP struct spp::analyse::errors::SppObjectInitializerGenericWithArgsError final : SemanticError {
    explicit SppObjectInitializerGenericWithArgsError(asts::TypeAst const &type, asts::ObjectInitializerArgumentAst const &arg);
};


SPP_EXP struct spp::analyse::errors::SppArgumentNameInvalidError final : SemanticError {
    explicit SppArgumentNameInvalidError(asts::Ast const &target, std::string_view target_what, asts::Ast const &source, std::string_view source_what);
};


SPP_EXP struct spp::analyse::errors::SppArgumentMissingError final : SemanticError {
    explicit SppArgumentMissingError(asts::Ast const &target, std::string_view target_what, asts::Ast const &source, std::string_view source_what);
};


SPP_EXP struct spp::analyse::errors::SppEarlyReturnRequiresTryTypeError final : SemanticError {
    explicit SppEarlyReturnRequiresTryTypeError(asts::ExpressionAst const &expr, asts::TypeAst const &type);
};


SPP_EXP struct spp::analyse::errors::SppFunctionCallAbstractFunctionError final : SemanticError {
    explicit SppFunctionCallAbstractFunctionError(asts::FunctionPrototypeAst const &proto, asts::PostfixExpressionOperatorFunctionCallAst const &call);
};


SPP_EXP struct spp::analyse::errors::SppFunctionCallNotImplFunctionError final : SemanticError {
    explicit SppFunctionCallNotImplFunctionError(asts::FunctionPrototypeAst const &proto, asts::PostfixExpressionOperatorFunctionCallAst const &call);
};


SPP_EXP struct spp::analyse::errors::SppFunctionCallTooManyArgumentsError final : SemanticError {
    explicit SppFunctionCallTooManyArgumentsError(asts::FunctionPrototypeAst const &proto, asts::PostfixExpressionOperatorFunctionCallAst const &call);
};


SPP_EXP struct spp::analyse::errors::SppFunctionFoldTupleElementTypeMismatchError final : SemanticError {
    explicit SppFunctionFoldTupleElementTypeMismatchError(asts::TypeAst const &type, asts::ExpressionAst const &expr);
};


SPP_EXP struct spp::analyse::errors::SppFunctionFoldTupleLengthMismatchError final : SemanticError {
    explicit SppFunctionFoldTupleLengthMismatchError(asts::ExpressionAst const &first_tup, std::size_t first_length, asts::ExpressionAst const &second_tup, std::size_t second_length);
};


SPP_EXP struct spp::analyse::errors::SppFunctionCallNoValidSignaturesError final : SemanticError {
    explicit SppFunctionCallNoValidSignaturesError(asts::PostfixExpressionOperatorFunctionCallAst const &call, std::string_view sigs, std::string_view attempted);
};


SPP_EXP struct spp::analyse::errors::SppFunctionCallOverloadAmbiguousError final : SemanticError {
    explicit SppFunctionCallOverloadAmbiguousError(asts::PostfixExpressionOperatorFunctionCallAst const &call, std::string_view sigs, std::string_view attempted);
};


SPP_EXP struct spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError final : SemanticError {
    explicit SppMemberAccessStaticOperatorExpectedError(asts::Ast const &lhs, asts::TokenAst const &access);
};


SPP_EXP struct spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError final : SemanticError {
    explicit SppMemberAccessRuntimeOperatorExpectedError(asts::Ast const &lhs, asts::TokenAst const &access);
};


SPP_EXP struct spp::analyse::errors::SppGenericTypeInvalidUsageError final : SemanticError {
    explicit SppGenericTypeInvalidUsageError(asts::Ast const &gen_name, asts::TypeAst const &gen_val, std::string_view what);
};


SPP_EXP struct spp::analyse::errors::SppAmbiguousMemberAccessError final : SemanticError {
    explicit SppAmbiguousMemberAccessError(asts::Ast const &found_field_1, asts::Ast const &found_field_2, asts::Ast const &field_access);
};


SPP_EXP struct spp::analyse::errors::SppCoroutineContainsRetExprExpressionError final : SemanticError {
    explicit SppCoroutineContainsRetExprExpressionError(asts::TokenAst const &fun_tag, asts::TokenAst const &ret_stmt);
};


SPP_EXP struct spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError final : SemanticError {
    explicit SppFunctionSubroutineMissingReturnStatementError(asts::Ast const &final_member, asts::TypeAst const &return_type);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionCyclicExtensionError final : SemanticError {
    explicit SppSuperimpositionCyclicExtensionError(asts::TypeAst const &first_extension, asts::TypeAst const &second_extension);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionDoubleExtensionError final : SemanticError {
    explicit SppSuperimpositionDoubleExtensionError(asts::TypeAst const &first_extension, asts::TypeAst const &second_extension);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionSelfExtensionError final : SemanticError {
    explicit SppSuperimpositionSelfExtensionError(asts::TypeAst const &first_extension, asts::TypeAst const &second_extension);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionMethodInvalidError(asts::IdentifierAst const &new_method, asts::TypeAst const &super_class);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError final : SemanticError {
    explicit SppSuperimpositionExtensionNonVirtualMethodOverriddenError(asts::IdentifierAst const &new_method, asts::IdentifierAst const &base_method, asts::TypeAst const &super_class);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError final : SemanticError {
    explicit SppSuperimpositionOptionalGenericParameterError(asts::GenericParameterAst const &param);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError final : SemanticError {
    explicit SppSuperimpositionUnconstrainedGenericParameterError(asts::GenericParameterAst const &param);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionTypeStatementInvalidError(asts::TypeStatementAst const &stmt, asts::TypeAst const &super_class);
};


SPP_EXP struct spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionCmpStatementInvalidError(asts::CmpStatementAst const &stmt, asts::TypeAst const &super_class);
};


SPP_EXP struct spp::analyse::errors::SppAsyncTargetNotFunctionCallError final : SemanticError {
    explicit SppAsyncTargetNotFunctionCallError(asts::TokenAst const &async_op, asts::Ast const &rhs);
};


SPP_EXP struct spp::analyse::errors::SppDereferenceInvalidExpressionNonBorrowedTypeError final : SemanticError {
    explicit SppDereferenceInvalidExpressionNonBorrowedTypeError(asts::TokenAst const &tok_deref, asts::ExpressionAst const &expr, asts::TypeAst const &type);
};


SPP_EXP struct spp::analyse::errors::SppInvalidExpressionNonCopyableTypeError final : SemanticError {
    explicit SppInvalidExpressionNonCopyableTypeError(asts::ExpressionAst const &expr, asts::TypeAst const &type);
};


SPP_EXP struct spp::analyse::errors::SppGenericParameterInferredConflictInferredError final : SemanticError {
    explicit SppGenericParameterInferredConflictInferredError(asts::Ast const &param, asts::Ast const &first_infer, asts::Ast const &second_infer);
};


SPP_EXP struct spp::analyse::errors::SppGenericParameterNotInferredError final : SemanticError {
    explicit SppGenericParameterNotInferredError(asts::Ast const &param, asts::Ast const &ctx);
};


SPP_EXP struct spp::analyse::errors::SppGenericArgumentTooManyError final : SemanticError {
    explicit SppGenericArgumentTooManyError(asts::Ast const &param, asts::Ast const &owner, asts::GenericArgumentAst const &arg);
};


SPP_EXP struct spp::analyse::errors::SppMissingMainFunctionError final : SemanticError {
    explicit SppMissingMainFunctionError(asts::ModulePrototypeAst const &mod);
};
