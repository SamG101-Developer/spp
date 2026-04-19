module;
#include <spp/macros.hpp>

export module spp.analyse.errors:semantic_error;
import spp.abstract;
import spp.utils.errors;
import mppp;
import std;
import sys;


namespace spp::analyse::errors {
    SPP_EXP_CLS struct SemanticError;

    SPP_EXP_CLS struct SppExpressionTypeInvalidError;
    SPP_EXP_CLS struct SppTypeMismatchError;
    SPP_EXP_CLS struct SppSecondClassBorrowViolationError;
    SPP_EXP_CLS struct SppCompileTimeConstantError;
    SPP_EXP_CLS struct SppInvalidMutationError;
    SPP_EXP_CLS struct SppUninitializedMemoryUseError;
    SPP_EXP_CLS struct SppPartiallyInitializedMemoryUseError;
    SPP_EXP_CLS struct SppMoveFromBorrowedMemoryError;
    SPP_EXP_CLS struct SppMoveFromPinnedMemoryError;
    SPP_EXP_CLS struct SppInconsistentlyInitializedMemoryUseError;
    SPP_EXP_CLS struct SppInconsistentlyPinnedMemoryUseError;
    SPP_EXP_CLS struct SppMemberAccessNonIndexableError;
    SPP_EXP_CLS struct SppMemberAccessOutOfBoundsError;
    SPP_EXP_CLS struct SppCaseBranchMultipleDestructuresError;
    SPP_EXP_CLS struct SppCaseBranchElseNotLastError;
    SPP_EXP_CLS struct SppCaseBranchMissingElseError;
    SPP_EXP_CLS struct SppIdentifierDuplicateError;
    SPP_EXP_CLS struct SppRecursiveTypeError;
    SPP_EXP_CLS struct SppCoroutineInvalidReturnTypeError;
    SPP_EXP_CLS struct SppFloatOutOfBoundsError;
    SPP_EXP_CLS struct SppIntegerOutOfBoundsError;
    SPP_EXP_CLS struct SppOrderInvalidError;
    SPP_EXP_CLS struct SppExpansionOfNonTupleError;
    SPP_EXP_CLS struct SppMemoryOverlapUsageError;
    SPP_EXP_CLS struct SppMultipleSelfParametersError;
    SPP_EXP_CLS struct SppMultipleVariadicParametersError;
    SPP_EXP_CLS struct SppSelfParamInFreeFunctionError;
    SPP_EXP_CLS struct SppFunctionPrototypeConflictError;
    SPP_EXP_CLS struct SppFunctionSubroutineContainsGenExpressionError;
    SPP_EXP_CLS struct SppYieldedTypeMismatchError;
    SPP_EXP_CLS struct SppIdentifierUnknownError;
    SPP_EXP_CLS struct SppUnreachableCodeError;
    SPP_EXP_CLS struct SppInvalidTypeAnnotationError;
    SPP_EXP_CLS struct SppMultipleSkipMultiArgumentsError;
    SPP_EXP_CLS struct SppVariableArrayDestructureArrayTypeMismatchError;
    SPP_EXP_CLS struct SppVariableArrayDestructureArraySizeMismatchError;
    SPP_EXP_CLS struct SppVariableTupleDestructureTupleTypeMismatchError;
    SPP_EXP_CLS struct SppVariableTupleDestructureTupleSizeMismatchError;
    SPP_EXP_CLS struct SppVariableObjectDestructureWithBoundMultiSkipError;
    SPP_EXP_CLS struct SppExpressionNotBooleanError;
    SPP_EXP_CLS struct SppExpressionNotGeneratorError;
    SPP_EXP_CLS struct SppExpressionAmbiguousGeneratorError;
    SPP_EXP_CLS struct SppExpressionNotIndexableError;
    SPP_EXP_CLS struct SppExpressionAmbiguousIndexableError;
    SPP_EXP_CLS struct SppLoopTooManyControlFlowStatementsError;
    SPP_EXP_CLS struct SppObjectInitializerMultipleAutofillArgumentsError;
    SPP_EXP_CLS struct SppObjectInitializerInvalidArgumentError;
    SPP_EXP_CLS struct SppObjectInitializerGenericWithArgsError;
    SPP_EXP_CLS struct SppArgumentNameInvalidError;
    SPP_EXP_CLS struct SppArgumentMissingError;
    SPP_EXP_CLS struct SppEarlyReturnRequiresTryTypeError;
    SPP_EXP_CLS struct SppFunctionCallAbstractFunctionError;
    SPP_EXP_CLS struct SppFunctionCallNotImplFunctionError;
    SPP_EXP_CLS struct SppFunctionCallTooManyArgumentsError;
    SPP_EXP_CLS struct SppFunctionFoldTupleElementTypeMismatchError;
    SPP_EXP_CLS struct SppFunctionFoldTupleLengthMismatchError;
    SPP_EXP_CLS struct SppFunctionCallNoValidSignaturesError;
    SPP_EXP_CLS struct SppFunctionCallOverloadAmbiguousError;
    SPP_EXP_CLS struct SppMemberAccessStaticOperatorExpectedError;
    SPP_EXP_CLS struct SppMemberAccessRuntimeOperatorExpectedError;
    SPP_EXP_CLS struct SppGenericTypeInvalidUsageError;
    SPP_EXP_CLS struct SppAmbiguousMemberAccessError;
    SPP_EXP_CLS struct SppCoroutineContainsRetExprExpressionError;
    SPP_EXP_CLS struct SppFunctionSubroutineMissingReturnStatementError;
    SPP_EXP_CLS struct SppSuperimpositionCyclicExtensionError;
    SPP_EXP_CLS struct SppSuperimpositionDoubleExtensionError;
    SPP_EXP_CLS struct SppSuperimpositionSelfExtensionError;
    SPP_EXP_CLS struct SppSuperimpositionExtensionMethodInvalidError;
    SPP_EXP_CLS struct SppSuperimpositionExtensionNonVirtualMethodOverriddenError;
    SPP_EXP_CLS struct SppSuperimpositionOptionalGenericParameterError;
    SPP_EXP_CLS struct SppSuperimpositionUnconstrainedGenericParameterError;
    SPP_EXP_CLS struct SppSuperimpositionExtensionTypeStatementInvalidError;
    SPP_EXP_CLS struct SppSuperimpositionExtensionCmpStatementInvalidError;
    SPP_EXP_CLS struct SppAsyncTargetNotFunctionCallError;
    SPP_EXP_CLS struct SppDereferenceInvalidExpressionNonBorrowedTypeError;
    SPP_EXP_CLS struct SppInvalidExpressionNonCopyableTypeError;
    SPP_EXP_CLS struct SppGenericParameterInferredConflictInferredError;
    SPP_EXP_CLS struct SppGenericParameterNotInferredError;
    SPP_EXP_CLS struct SppGenericArgumentTooManyError;
    SPP_EXP_CLS struct SppMissingMainFunctionError;
    SPP_EXP_CLS struct SppInvalidVoidValueError;
    SPP_EXP_CLS struct SppBorrowLifetimeIncreaseError;
    SPP_EXP_CLS struct SppInvalidComptimeOperationError;
    SPP_EXP_CLS struct SppInternalCompilerError;
    SPP_EXP_CLS struct SppGenericConstraintError;
    SPP_EXP_CLS struct SppAnnotationTargetNotAnAnnotationError;
    SPP_EXP_CLS struct SppAnnotationTargetNotACmpFunctionError;
    SPP_EXP_CLS struct SppCalledAnnotationAppliedToInvalidAstError;
}


SPP_EXP_CLS struct spp::analyse::errors::SemanticError : spp::utils::errors::AbstractError {
    using AbstractError::AbstractError;
    SemanticError(SemanticError const &) = default;
    ~SemanticError() override = default;

    enum class ErrorInformationType {
        HEADER, ERROR, CONTEXT, FOOTER
    };

    std::vector<std::tuple<AbstractAst const*, ErrorInformationType, std::string, std::string>> m_error_info;

    auto add_header(std::size_t err_code, std::string &&msg) -> void;

    auto add_error(AbstractAst const *ast, std::string &&tag) -> void;

    auto add_context_for_error(AbstractAst const *ast, std::string &&tag) -> void;

    auto add_footer(std::string &&note, std::string &&help) -> void;

    SPP_ATTR_NODISCARD
    auto clone() const -> std::unique_ptr<SemanticError>;
};


// SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationInvalidApplicationError final : SemanticError {
//     explicit SppAnnotationInvalidApplicationError(AbstractAst const &annotation, AbstractAst const &ctx, std::string_view block_list);
// };
//
//
// SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationConflictError final : SemanticError {
//     explicit SppAnnotationConflictError(AbstractAst const &first_annotation, AbstractAst const &conflicting_annotation, AbstractAst const &ctx);
// };
//
//
// SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationInvalidError final : SemanticError {
//     explicit SppAnnotationInvalidError(AbstractAst const &annotation);
// };


SPP_EXP_CLS struct spp::analyse::errors::SppExpressionTypeInvalidError final : SemanticError {
    explicit SppExpressionTypeInvalidError(AbstractAst const &expr);
};


SPP_EXP_CLS struct spp::analyse::errors::SppTypeMismatchError final : SemanticError {
    explicit SppTypeMismatchError(AbstractAst const &lhs, AbstractAst const &lhs_ty, AbstractAst const &rhs, AbstractAst const &rhs_ty);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSecondClassBorrowViolationError final : SemanticError {
    explicit SppSecondClassBorrowViolationError(AbstractAst const &expr, AbstractAst const &type, std::string_view ctx);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCompileTimeConstantError final : SemanticError {
    explicit SppCompileTimeConstantError(AbstractAst const &expr);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInvalidMutationError final : SemanticError {
    explicit SppInvalidMutationError(AbstractAst const &sym, AbstractAst const &mutator, AbstractAst const &initialization_location);
};


SPP_EXP_CLS struct spp::analyse::errors::SppUninitializedMemoryUseError final : SemanticError {
    explicit SppUninitializedMemoryUseError(AbstractAst const &ast, AbstractAst const &init_location, AbstractAst const &move_location);
};


SPP_EXP_CLS struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : SemanticError {
    explicit SppPartiallyInitializedMemoryUseError(AbstractAst const &ast, AbstractAst const &init_location, AbstractAst const &partial_move_location);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : SemanticError {
    explicit SppMoveFromBorrowedMemoryError(AbstractAst const &ast, AbstractAst const &move_location, AbstractAst const &borrow_location);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMoveFromPinnedMemoryError final : SemanticError {
    explicit SppMoveFromPinnedMemoryError(AbstractAst const &ast, AbstractAst const &init_location, AbstractAst const &move_location, AbstractAst const &pin_location);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : SemanticError {
    explicit SppInconsistentlyInitializedMemoryUseError(AbstractAst const &ast, AbstractAst const &branch_1, AbstractAst const &branch_2, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyPinnedMemoryUseError final : SemanticError {
    explicit SppInconsistentlyPinnedMemoryUseError(AbstractAst const &ast, AbstractAst const &branch_1, AbstractAst const &branch_2);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessNonIndexableError final : SemanticError {
    explicit SppMemberAccessNonIndexableError(AbstractAst const &lhs, AbstractAst const &lhs_type, AbstractAst const &access_op);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessOutOfBoundsError final : SemanticError {
    explicit SppMemberAccessOutOfBoundsError(AbstractAst const &lhs, AbstractAst const &lhs_type, AbstractAst const &access_op);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchMultipleDestructuresError final : SemanticError {
    explicit SppCaseBranchMultipleDestructuresError(AbstractAst const &first_pattern, AbstractAst const &second_pattern);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchElseNotLastError final : SemanticError {
    explicit SppCaseBranchElseNotLastError(AbstractAst const &non_last_else_branch, AbstractAst const &last_branch);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchMissingElseError final : SemanticError {
    explicit SppCaseBranchMissingElseError(AbstractAst const &case_expr, AbstractAst const &last_branch);
};


SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierDuplicateError final : SemanticError {
    explicit SppIdentifierDuplicateError(AbstractAst const &first_identifier, AbstractAst const &duplicate_identifier, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppRecursiveTypeError final : SemanticError {
    explicit SppRecursiveTypeError(AbstractAst const &type, AbstractAst const &recursion);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCoroutineInvalidReturnTypeError final : SemanticError {
    explicit SppCoroutineInvalidReturnTypeError(AbstractAst const &proto, AbstractAst const &return_type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFloatOutOfBoundsError final : SemanticError {
    explicit SppFloatOutOfBoundsError(AbstractAst const &literal, mppp::BigDec const &value, mppp::BigDec const &lower, mppp::BigDec const &upper, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppIntegerOutOfBoundsError final : SemanticError {
    explicit SppIntegerOutOfBoundsError(AbstractAst const &literal, mppp::BigInt const &value, mppp::BigInt const &lower, mppp::BigInt const &upper, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppOrderInvalidError final : SemanticError {
    explicit SppOrderInvalidError(std::string_view first_what, AbstractAst const &first, std::string_view second_what, AbstractAst const &second);
};


SPP_EXP_CLS struct spp::analyse::errors::SppExpansionOfNonTupleError final : SemanticError {
    explicit SppExpansionOfNonTupleError(AbstractAst const &ast, AbstractAst const &type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMemoryOverlapUsageError final : SemanticError {
    explicit SppMemoryOverlapUsageError(AbstractAst const &ast, AbstractAst const &overlap_ast);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMultipleSelfParametersError final : SemanticError {
    explicit SppMultipleSelfParametersError(AbstractAst const &first_self, AbstractAst const &second_self);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMultipleVariadicParametersError final : SemanticError {
    explicit SppMultipleVariadicParametersError(AbstractAst const &first_variadic, AbstractAst const &second_variadic);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSelfParamInFreeFunctionError final : SemanticError {
    explicit SppSelfParamInFreeFunctionError(AbstractAst const &function_proto, AbstractAst const &self_param);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionPrototypeConflictError final : SemanticError {
    explicit SppFunctionPrototypeConflictError(AbstractAst const &first_proto, AbstractAst const &second_proto);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError final : SemanticError {
    explicit SppFunctionSubroutineContainsGenExpressionError(AbstractAst const &fun_tag, AbstractAst const &gen_expr);
};


SPP_EXP_CLS struct spp::analyse::errors::SppYieldedTypeMismatchError final : SemanticError {
    explicit SppYieldedTypeMismatchError(AbstractAst const &lhs, AbstractAst const &lhs_ty, AbstractAst const &rhs, AbstractAst const &rhs_ty);
};


SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierUnknownError final : SemanticError {
    explicit SppIdentifierUnknownError(AbstractAst const &name, std::string_view what, std::optional<std::string> const &closest = {});
};


SPP_EXP_CLS struct spp::analyse::errors::SppUnreachableCodeError final : SemanticError {
    explicit SppUnreachableCodeError(AbstractAst const &member, AbstractAst const &next_member);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInvalidTypeAnnotationError final : SemanticError {
    explicit SppInvalidTypeAnnotationError(AbstractAst const &type, AbstractAst const &var);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMultipleSkipMultiArgumentsError final : SemanticError {
    explicit SppMultipleSkipMultiArgumentsError(AbstractAst const &var, AbstractAst const &first_arg, AbstractAst const &second_arg);
};


SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError final : SemanticError {
    explicit SppVariableArrayDestructureArrayTypeMismatchError(AbstractAst const &var, AbstractAst const &val, AbstractAst const &val_type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError final : SemanticError {
    explicit SppVariableArrayDestructureArraySizeMismatchError(AbstractAst const &var, std::size_t var_size, AbstractAst const &val, std::size_t val_size);
};


SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError final : SemanticError {
    explicit SppVariableTupleDestructureTupleTypeMismatchError(AbstractAst const &var, AbstractAst const &val, AbstractAst const &val_type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError final : SemanticError {
    explicit SppVariableTupleDestructureTupleSizeMismatchError(AbstractAst const &var, std::size_t var_size, AbstractAst const &val, std::size_t val_size);
};


SPP_EXP_CLS struct spp::analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError final : SemanticError {
    explicit SppVariableObjectDestructureWithBoundMultiSkipError(AbstractAst const &var, AbstractAst const &multi_skip);
};


SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotBooleanError final : SemanticError {
    explicit SppExpressionNotBooleanError(AbstractAst const &expr, AbstractAst const &expr_type, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotGeneratorError final : SemanticError {
    explicit SppExpressionNotGeneratorError(AbstractAst const &expr, AbstractAst const &expr_type, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousGeneratorError final : SemanticError {
    explicit SppExpressionAmbiguousGeneratorError(AbstractAst const &expr, AbstractAst const &expr_type, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotIndexableError final : SemanticError {
    explicit SppExpressionNotIndexableError(AbstractAst const &expr, AbstractAst const &expr_type, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousIndexableError final : SemanticError {
    explicit SppExpressionAmbiguousIndexableError(AbstractAst const &expr, AbstractAst const &expr_type, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppLoopTooManyControlFlowStatementsError final : SemanticError {
    explicit SppLoopTooManyControlFlowStatementsError(AbstractAst const &tok_loop, AbstractAst const &stmt, std::size_t num_controls, std::size_t loop_depth);
};


SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError final : SemanticError {
    explicit SppObjectInitializerMultipleAutofillArgumentsError(AbstractAst const &arg1, AbstractAst const &arg2);
};


SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerInvalidArgumentError final : SemanticError {
    explicit SppObjectInitializerInvalidArgumentError(AbstractAst const &arg);
};


SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerGenericWithArgsError final : SemanticError {
    explicit SppObjectInitializerGenericWithArgsError(AbstractAst const &type, AbstractAst const &arg);
};


SPP_EXP_CLS struct spp::analyse::errors::SppArgumentNameInvalidError final : SemanticError {
    explicit SppArgumentNameInvalidError(AbstractAst const &target, std::string_view target_what, AbstractAst const &source, std::string_view source_what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppArgumentMissingError final : SemanticError {
    explicit SppArgumentMissingError(AbstractAst const &target, std::string_view target_what, AbstractAst const &source, std::string_view source_what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppEarlyReturnRequiresTryTypeError final : SemanticError {
    explicit SppEarlyReturnRequiresTryTypeError(AbstractAst const &expr, AbstractAst const &type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallAbstractFunctionError final : SemanticError {
    explicit SppFunctionCallAbstractFunctionError(AbstractAst const &proto, AbstractAst const &call);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallNotImplFunctionError final : SemanticError {
    explicit SppFunctionCallNotImplFunctionError(AbstractAst const &proto, AbstractAst const &call);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallTooManyArgumentsError final : SemanticError {
    explicit SppFunctionCallTooManyArgumentsError(AbstractAst const &proto, AbstractAst const &call);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionFoldTupleElementTypeMismatchError final : SemanticError {
    explicit SppFunctionFoldTupleElementTypeMismatchError(AbstractAst const &type, AbstractAst const &expr);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionFoldTupleLengthMismatchError final : SemanticError {
    explicit SppFunctionFoldTupleLengthMismatchError(AbstractAst const &first_tup, std::size_t first_length, AbstractAst const &second_tup, std::size_t second_length);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallNoValidSignaturesError final : SemanticError {
    explicit SppFunctionCallNoValidSignaturesError(AbstractAst const &call, std::string_view sigs, std::string_view attempted);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallOverloadAmbiguousError final : SemanticError {
    explicit SppFunctionCallOverloadAmbiguousError(AbstractAst const &call, std::string_view sigs, std::string_view attempted);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError final : SemanticError {
    explicit SppMemberAccessStaticOperatorExpectedError(AbstractAst const &lhs, AbstractAst const &access);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError final : SemanticError {
    explicit SppMemberAccessRuntimeOperatorExpectedError(AbstractAst const &lhs, AbstractAst const &access);
};


SPP_EXP_CLS struct spp::analyse::errors::SppGenericTypeInvalidUsageError final : SemanticError {
    explicit SppGenericTypeInvalidUsageError(AbstractAst const &gen_name, AbstractAst const &gen_val, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppAmbiguousMemberAccessError final : SemanticError {
    explicit SppAmbiguousMemberAccessError(AbstractAst const &found_field_1, AbstractAst const &found_field_2, AbstractAst const &field_access);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCoroutineContainsRetExprExpressionError final : SemanticError {
    explicit SppCoroutineContainsRetExprExpressionError(AbstractAst const &fun_tag, AbstractAst const &ret_stmt);
};


SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError final : SemanticError {
    explicit SppFunctionSubroutineMissingReturnStatementError(AbstractAst const &final_member, AbstractAst const &return_type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionCyclicExtensionError final : SemanticError {
    explicit SppSuperimpositionCyclicExtensionError(AbstractAst const &first_extension, AbstractAst const &second_extension);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionDoubleExtensionError final : SemanticError {
    explicit SppSuperimpositionDoubleExtensionError(AbstractAst const &first_extension, AbstractAst const &second_extension);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionSelfExtensionError final : SemanticError {
    explicit SppSuperimpositionSelfExtensionError(AbstractAst const &first_extension, AbstractAst const &second_extension);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionMethodInvalidError(AbstractAst const &new_method, AbstractAst const &super_class);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError final : SemanticError {
    explicit SppSuperimpositionExtensionNonVirtualMethodOverriddenError(AbstractAst const &new_method, AbstractAst const &base_method, AbstractAst const &super_class);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError final : SemanticError {
    explicit SppSuperimpositionOptionalGenericParameterError(AbstractAst const &param);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError final : SemanticError {
    explicit SppSuperimpositionUnconstrainedGenericParameterError(AbstractAst const &param);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionTypeStatementInvalidError(AbstractAst const &stmt, AbstractAst const &super_class);
};


SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionCmpStatementInvalidError(AbstractAst const &stmt, AbstractAst const &super_class);
};


SPP_EXP_CLS struct spp::analyse::errors::SppAsyncTargetNotFunctionCallError final : SemanticError {
    explicit SppAsyncTargetNotFunctionCallError(AbstractAst const &async_op, AbstractAst const &rhs);
};


SPP_EXP_CLS struct spp::analyse::errors::SppDereferenceInvalidExpressionNonBorrowedTypeError final : SemanticError {
    explicit SppDereferenceInvalidExpressionNonBorrowedTypeError(AbstractAst const &tok_deref, AbstractAst const &expr, AbstractAst const &type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInvalidExpressionNonCopyableTypeError final : SemanticError {
    explicit SppInvalidExpressionNonCopyableTypeError(AbstractAst const &expr, AbstractAst const &type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterInferredConflictInferredError final : SemanticError {
    explicit SppGenericParameterInferredConflictInferredError(AbstractAst const &param, AbstractAst const &first_infer, AbstractAst const &second_infer);
};


SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterNotInferredError final : SemanticError {
    explicit SppGenericParameterNotInferredError(AbstractAst const &param, AbstractAst const &ctx);
};


SPP_EXP_CLS struct spp::analyse::errors::SppGenericArgumentTooManyError final : SemanticError {
    explicit SppGenericArgumentTooManyError(AbstractAst const &param, AbstractAst const &owner, AbstractAst const &arg);
};


SPP_EXP_CLS struct spp::analyse::errors::SppMissingMainFunctionError final : SemanticError {
    explicit SppMissingMainFunctionError(AbstractAst const &mod);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInvalidVoidValueError final : SemanticError {
    explicit SppInvalidVoidValueError(AbstractAst const &expr, std::string_view what);
};


SPP_EXP_CLS struct spp::analyse::errors::SppBorrowLifetimeIncreaseError final : SemanticError {
    explicit SppBorrowLifetimeIncreaseError(AbstractAst const& extension_ast, AbstractAst const& lhs_init_definition, AbstractAst const& rhs_borrow_definition);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInvalidComptimeOperationError final : SemanticError { // Todo: Check other comptime error: merge?
    explicit SppInvalidComptimeOperationError(AbstractAst const& ast);
};


SPP_EXP_CLS struct spp::analyse::errors::SppInternalCompilerError final : SemanticError {
    explicit SppInternalCompilerError(AbstractAst const &ast, std::string_view message);
};


SPP_EXP_CLS struct spp::analyse::errors::SppGenericConstraintError final : SemanticError {
    explicit SppGenericConstraintError(AbstractAst const &constraint, AbstractAst const &concrete_type);
};


SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotAnAnnotationError final : SemanticError {
    explicit SppAnnotationTargetNotAnAnnotationError(AbstractAst const &call_site, AbstractAst const &target_definition);
};


SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotACmpFunctionError final : SemanticError {
    explicit SppAnnotationTargetNotACmpFunctionError(AbstractAst const &annotation_marker, AbstractAst const &non_function_ast);
};


SPP_EXP_CLS struct spp::analyse::errors::SppCalledAnnotationAppliedToInvalidAstError final : SemanticError {
    explicit SppCalledAnnotationAppliedToInvalidAstError(AbstractAst const &invalid_ast, AbstractAst const &annotation_call, AbstractAst const &annotation_definition);
};
