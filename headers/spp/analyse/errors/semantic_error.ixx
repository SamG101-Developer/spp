module;
#include <spp/macros.hpp>

export module spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.utils.types;
import boost;
import std;
import sys;

namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct CoroutinePrototypeAst;
    SPP_EXP_CLS struct LiteralAst;
    SPP_EXP_CLS struct FunctionParameterSelfAst;
    SPP_EXP_CLS struct FunctionParameterVariadicAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LocalVariableDestructureSkipMultipleArgumentsAst;
    SPP_EXP_CLS struct LocalVariableDestructureArrayAst;
    SPP_EXP_CLS struct LocalVariableDestructureTupleAst;
    SPP_EXP_CLS struct LocalVariableDestructureObjectAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct TypeStatementAst;
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct LoopControlFlowStatementAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct ModulePrototypeAst;
}

namespace spp::analyse::errors {
    SPP_EXP_CLS struct SemanticError;

    SPP_EXP_CLS struct SppInvalidPrimaryExpressionError;
    SPP_EXP_CLS struct SppTypeMismatchError;
    SPP_EXP_CLS struct SppSecondClassBorrowViolationError;
    SPP_EXP_CLS struct SppCompileTimeConstantError;
    SPP_EXP_CLS struct SppInvalidMutationError;
    SPP_EXP_CLS struct SppUninitializedMemoryUseError;
    SPP_EXP_CLS struct SppPartiallyInitializedMemoryUseError;
    SPP_EXP_CLS struct SppMoveFromBorrowedMemoryError;
    SPP_EXP_CLS struct SppInconsistentlyInitializedMemoryUseError;
    SPP_EXP_CLS struct SppInconsistentlyEscapingBorrows;
    SPP_EXP_CLS struct SppMemberAccessNonIndexableError;
    SPP_EXP_CLS struct SppMemberAccessOutOfBoundsError;
    SPP_EXP_CLS struct SppCaseBranchElseNotLastError;
    SPP_EXP_CLS struct SppCaseBranchMissingElseError;
    SPP_EXP_CLS struct SppIdentifierDuplicateError;
    SPP_EXP_CLS struct SppRecursiveTypeError;
    SPP_EXP_CLS struct SppFloatOutOfBoundsError;
    SPP_EXP_CLS struct SppIntegerOutOfBoundsError;
    SPP_EXP_CLS struct SppOrderInvalidError;
    SPP_EXP_CLS struct SppExpansionOfNonTupleError;
    SPP_EXP_CLS struct SppMemoryOverlapUsageError;
    SPP_EXP_CLS struct SppMultipleSelfParametersError;
    SPP_EXP_CLS struct SppMultipleVariadicParametersError;
    SPP_EXP_CLS struct SppFunctionPrototypeConflictError;
    SPP_EXP_CLS struct SppFunctionSubroutineContainsGenExpressionError;
    SPP_EXP_CLS struct SppYieldedTypeMismatchError;
    SPP_EXP_CLS struct SppIdentifierUnknownError;
    SPP_EXP_CLS struct SppSelfIdentifierInvalidContextError;
    SPP_EXP_CLS struct SppUnreachableCodeError;
    SPP_EXP_CLS struct SppInvalidLocalVariableTypeAnnotationError;
    SPP_EXP_CLS struct SppMultipleRestPatternsError;
    SPP_EXP_CLS struct SppVariableArrayDestructureArrayTypeMismatchError;
    SPP_EXP_CLS struct SppVariableArrayDestructureArraySizeMismatchError;
    SPP_EXP_CLS struct SppVariableTupleDestructureTupleTypeMismatchError;
    SPP_EXP_CLS struct SppVariableTupleDestructureTupleSizeMismatchError;
    SPP_EXP_CLS struct SppVariableObjectDestructureWithBoundRestPatternError;
    SPP_EXP_CLS struct SppExpressionNotBooleanError;
    SPP_EXP_CLS struct SppExpressionNotGeneratorError;
    SPP_EXP_CLS struct SppExpressionAmbiguousGeneratorError;
    SPP_EXP_CLS struct SppExpressionAmbiguousIndexableError;
    SPP_EXP_CLS struct SppLoopTooManyControlFlowStatementsError;
    SPP_EXP_CLS struct SppObjectInitializerMultipleAutofillArgumentsError;
    SPP_EXP_CLS struct SppObjectInitializerInvalidArgumentError;
    SPP_EXP_CLS struct SppObjectInitializerVariantError;
    SPP_EXP_CLS struct SppAbstractTypeUseError;
    SPP_EXP_CLS struct SppArgumentNameInvalidError;
    SPP_EXP_CLS struct SppArgumentMissingError;
    SPP_EXP_CLS struct SppEarlyReturnRequiresTryTypeError;
    SPP_EXP_CLS struct SppFunctionCallAbstractFunctionError;
    SPP_EXP_CLS struct SppFunctionCallTooManyArgumentsError;
    SPP_EXP_CLS struct SppFunctionCallNoValidSignaturesError;
    SPP_EXP_CLS struct SppFunctionCallOverloadAmbiguousError;
    SPP_EXP_CLS struct SppMemberAccessStaticOperatorExpectedError;
    SPP_EXP_CLS struct SppMemberAccessRuntimeOperatorExpectedError;
    SPP_EXP_CLS struct SppGenericTypeInvalidUsageError;
    SPP_EXP_CLS struct SppAmbiguousMemberAccessError;
    SPP_EXP_CLS struct SppCoroutineContainsReturnStatementError;
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
    SPP_EXP_CLS struct SppDereferenceNonBorrowedTypeError;
    SPP_EXP_CLS struct SppNonCopyableTypeError;
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
    SPP_EXP_CLS struct SppInvalidBinaryFoldExpressionError;
    SPP_EXP_CLS struct SppAccessViolationError;
    SPP_EXP_CLS struct SppFunctionOverloadVisibilityMismatchError;
    SPP_EXP_CLS struct SppMovingEscapingBorrowedMemoryError;
    SPP_EXP_CLS struct SppMovingComptimeConstantMemoryError;
    SPP_EXP_CLS struct SppHigherOrderGenericsNotSupportedError;
    SPP_EXP_CLS struct SppGeneratedCodeError;
}

SPP_EXP_CLS struct SPP_ATTR_COLD spp::analyse::errors::SemanticError : spp::utils::errors::AbstractError {
    using AbstractError::AbstractError;
    SemanticError(SemanticError const &) = default;
    ~SemanticError() override = default;

    enum class ErrorInformationType {
        HEADER, ERROR, CONTEXT, FOOTER,
        WRAPPED
    };

    Vec<std::tuple<asts::Ast const*, ErrorInformationType, Str, Str>> ErrorInfo;

    auto AddHeaders(std::size_t err_code, Str &&msg) -> void;

    auto AddErr(asts::Ast const *ast, Str &&tag) -> void;

    auto AddCtxForErr(asts::Ast const *ast, Str &&tag) -> void;

    auto AddFooter(Str &&note, Str &&help) -> void;

    auto AddWrapped(Str &&msg) -> void;

    SPP_ATTR_NODISCARD
    auto Clone() const -> Unique<SemanticError>;
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidPrimaryExpressionError final : SemanticError {
    explicit SppInvalidPrimaryExpressionError(asts::Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppTypeMismatchError final : SemanticError {
    explicit SppTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSecondClassBorrowViolationError final : SemanticError {
    explicit SppSecondClassBorrowViolationError(asts::Ast const &expr, asts::Ast const &type, StrView ctx);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCompileTimeConstantError final : SemanticError {
    explicit SppCompileTimeConstantError(asts::Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidMutationError final : SemanticError {
    explicit SppInvalidMutationError(asts::Ast const &sym, asts::Ast const &mutator, asts::Ast const &initialization_location, StrView extra);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUninitializedMemoryUseError final : SemanticError {
    explicit SppUninitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : SemanticError {
    explicit SppPartiallyInitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &init_location, asts::Ast const &partial_move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : SemanticError {
    explicit SppMoveFromBorrowedMemoryError(asts::ExpressionAst const &ast, asts::Ast const &move_location, asts::Ast const &borrow_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : SemanticError {
    explicit SppInconsistentlyInitializedMemoryUseError(asts::ExpressionAst const &ast, asts::Ast const &branch_1, asts::Ast const &branch_2, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyEscapingBorrows final : SemanticError {
    explicit SppInconsistentlyEscapingBorrows(asts::ExpressionAst const &ast, asts::Ast const &branch_1, asts::Ast const &branch_2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessNonIndexableError final : SemanticError {
    explicit SppMemberAccessNonIndexableError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, asts::Ast const &access_op);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessOutOfBoundsError final : SemanticError {
    explicit SppMemberAccessOutOfBoundsError(asts::ExpressionAst const &lhs, asts::TypeAst const &lhs_type, std::size_t n, asts::Ast const &access_op);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchElseNotLastError final : SemanticError {
    explicit SppCaseBranchElseNotLastError(asts::CaseExpressionBranchAst const &non_last_else_branch, asts::CaseExpressionBranchAst const &last_branch);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchMissingElseError final : SemanticError {
    explicit SppCaseBranchMissingElseError(asts::CaseExpressionAst const &case_expr, asts::CaseExpressionBranchAst const &last_branch);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierDuplicateError final : SemanticError {
    explicit SppIdentifierDuplicateError(asts::Ast const &first_identifier, asts::Ast const &duplicate_identifier, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppRecursiveTypeError final : SemanticError {
    explicit SppRecursiveTypeError(asts::ClassPrototypeAst const &type, asts::TypeAst const &recursion);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFloatOutOfBoundsError final : SemanticError {
    explicit SppFloatOutOfBoundsError(asts::LiteralAst const &literal, boost::BigDec const &value, boost::BigDec const &lower, boost::BigDec const &upper, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIntegerOutOfBoundsError final : SemanticError {
    explicit SppIntegerOutOfBoundsError(asts::LiteralAst const &literal, boost::BigInt const &value, boost::BigInt const &lower, boost::BigInt const &upper, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppOrderInvalidError final : SemanticError {
    explicit SppOrderInvalidError(StrView first_what, asts::Ast const &first, StrView second_what, asts::Ast const &second);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpansionOfNonTupleError final : SemanticError {
    explicit SppExpansionOfNonTupleError(asts::TokenAst const &unpack, asts::Ast const &ast, asts::TypeAst const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemoryOverlapUsageError final : SemanticError {
    explicit SppMemoryOverlapUsageError(asts::Ast const &ast, asts::Ast const &overlap_ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleSelfParametersError final : SemanticError {
    explicit SppMultipleSelfParametersError(asts::FunctionParameterSelfAst const &first_self, asts::FunctionParameterSelfAst const &second_self);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleVariadicParametersError final : SemanticError {
    explicit SppMultipleVariadicParametersError(asts::FunctionParameterVariadicAst const &first_variadic, asts::FunctionParameterVariadicAst const &second_variadic);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionPrototypeConflictError final : SemanticError {
    explicit SppFunctionPrototypeConflictError(asts::FunctionPrototypeAst const &first_proto, asts::FunctionPrototypeAst const &second_proto);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError final : SemanticError {
    explicit SppFunctionSubroutineContainsGenExpressionError(asts::TokenAst const &fun_tag, asts::TokenAst const &gen_expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppYieldedTypeMismatchError final : SemanticError {
    explicit SppYieldedTypeMismatchError(asts::Ast const &lhs, asts::TypeAst const &lhs_ty, asts::Ast const &rhs, asts::TypeAst const &rhs_ty);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierUnknownError final : SemanticError {
    explicit SppIdentifierUnknownError(asts::Ast const &name, StrView what, std::optional<Str> const &closest = {});
};

SPP_EXP_CLS struct spp::analyse::errors::SppSelfIdentifierInvalidContextError final : SemanticError {
    explicit SppSelfIdentifierInvalidContextError(asts::Ast const &self);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUnreachableCodeError final : SemanticError {
    explicit SppUnreachableCodeError(asts::Ast const &member, asts::Ast const &next_member);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidLocalVariableTypeAnnotationError final : SemanticError {
    explicit SppInvalidLocalVariableTypeAnnotationError(asts::TypeAst const &type, asts::LocalVariableAst const &var);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleRestPatternsError final : SemanticError {
    explicit SppMultipleRestPatternsError(asts::LocalVariableAst const &var, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &pattern_1, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &pattern_2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError final : SemanticError {
    explicit SppVariableArrayDestructureArrayTypeMismatchError(asts::LocalVariableDestructureArrayAst const &var, asts::ExpressionAst const &val, asts::TypeAst const &val_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError final : SemanticError {
    explicit SppVariableArrayDestructureArraySizeMismatchError(asts::LocalVariableDestructureArrayAst const &var, std::size_t var_size, asts::ExpressionAst const &val, std::size_t val_size);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError final : SemanticError {
    explicit SppVariableTupleDestructureTupleTypeMismatchError(asts::LocalVariableDestructureTupleAst const &var, asts::ExpressionAst const &val, asts::TypeAst const &val_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError final : SemanticError {
    explicit SppVariableTupleDestructureTupleSizeMismatchError(asts::LocalVariableDestructureTupleAst const &var, std::size_t var_size, asts::ExpressionAst const &val, std::size_t val_size);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableObjectDestructureWithBoundRestPatternError final : SemanticError {
    explicit SppVariableObjectDestructureWithBoundRestPatternError(asts::LocalVariableDestructureObjectAst const &var, asts::LocalVariableDestructureSkipMultipleArgumentsAst const &rest_pattern);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotBooleanError final : SemanticError {
    explicit SppExpressionNotBooleanError(asts::Ast const &expr, asts::TypeAst const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotGeneratorError final : SemanticError {
    explicit SppExpressionNotGeneratorError(asts::Ast const &expr, asts::TypeAst const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousGeneratorError final : SemanticError {
    explicit SppExpressionAmbiguousGeneratorError(asts::Ast const &expr, asts::TypeAst const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousIndexableError final : SemanticError {
    explicit SppExpressionAmbiguousIndexableError(asts::Ast const &expr, asts::TypeAst const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLoopTooManyControlFlowStatementsError final : SemanticError {
    explicit SppLoopTooManyControlFlowStatementsError(asts::TokenAst const &tok_loop, asts::LoopControlFlowStatementAst const &stmt, std::size_t num_controls, std::size_t loop_depth);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError final : SemanticError {
    explicit SppObjectInitializerMultipleAutofillArgumentsError(asts::ObjectInitializerArgumentAst const &arg1, asts::ObjectInitializerArgumentAst const &arg2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerInvalidArgumentError final : SemanticError {
    explicit SppObjectInitializerInvalidArgumentError(asts::ObjectInitializerArgumentAst const &arg);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerVariantError final : SemanticError {
    explicit SppObjectInitializerVariantError(asts::TypeAst const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAbstractTypeUseError final : SemanticError {
    explicit SppAbstractTypeUseError(asts::TypeAst const &type, asts::FunctionPrototypeAst const &unimplemented);
};

SPP_EXP_CLS struct spp::analyse::errors::SppArgumentNameInvalidError final : SemanticError {
    explicit SppArgumentNameInvalidError(asts::Ast const &target, StrView target_what, asts::Ast const &source, StrView source_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppArgumentMissingError final : SemanticError {
    explicit SppArgumentMissingError(asts::Ast const &target, StrView target_what, asts::Ast const &source, StrView source_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppEarlyReturnRequiresTryTypeError final : SemanticError {
    explicit SppEarlyReturnRequiresTryTypeError(asts::ExpressionAst const &expr, asts::TypeAst const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallAbstractFunctionError final : SemanticError {
    explicit SppFunctionCallAbstractFunctionError(asts::FunctionPrototypeAst const &proto, asts::PostfixExpressionOperatorFunctionCallAst const &call);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallTooManyArgumentsError final : SemanticError {
    explicit SppFunctionCallTooManyArgumentsError(asts::FunctionPrototypeAst const &proto, asts::PostfixExpressionOperatorFunctionCallAst const &call);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallNoValidSignaturesError final : SemanticError {
    explicit SppFunctionCallNoValidSignaturesError(asts::PostfixExpressionOperatorFunctionCallAst const &call, StrView sigs, StrView attempted);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallOverloadAmbiguousError final : SemanticError {
    explicit SppFunctionCallOverloadAmbiguousError(asts::PostfixExpressionOperatorFunctionCallAst const &call, StrView sigs, StrView attempted);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError final : SemanticError {
    explicit SppMemberAccessStaticOperatorExpectedError(asts::Ast const &lhs, asts::TokenAst const &access, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError final : SemanticError {
    explicit SppMemberAccessRuntimeOperatorExpectedError(asts::Ast const &lhs, asts::TokenAst const &access);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericTypeInvalidUsageError final : SemanticError {
    explicit SppGenericTypeInvalidUsageError(asts::Ast const &gen_name, asts::TypeAst const &gen_val, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAmbiguousMemberAccessError final : SemanticError {
    explicit SppAmbiguousMemberAccessError(asts::Ast const &found_field_1, asts::Ast const &found_field_2, asts::Ast const &field_access);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCoroutineContainsReturnStatementError final : SemanticError {
    explicit SppCoroutineContainsReturnStatementError(asts::TokenAst const &fun_tag, asts::TokenAst const &ret_stmt);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError final : SemanticError {
    explicit SppFunctionSubroutineMissingReturnStatementError(asts::Ast const &final_member, asts::Ast const &return_type_definition, asts::TypeAst const &return_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionCyclicExtensionError final : SemanticError {
    explicit SppSuperimpositionCyclicExtensionError(asts::TypeAst const &first_extension, asts::TypeAst const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionDoubleExtensionError final : SemanticError {
    explicit SppSuperimpositionDoubleExtensionError(asts::TypeAst const &first_extension, asts::TypeAst const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionSelfExtensionError final : SemanticError {
    explicit SppSuperimpositionSelfExtensionError(asts::TypeAst const &first_extension, asts::TypeAst const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionMethodInvalidError(asts::IdentifierAst const &new_method, asts::TypeAst const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError final : SemanticError {
    explicit SppSuperimpositionExtensionNonVirtualMethodOverriddenError(asts::IdentifierAst const &new_method, asts::IdentifierAst const &base_method, asts::TypeAst const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError final : SemanticError {
    explicit SppSuperimpositionOptionalGenericParameterError(asts::GenericParameterAst const &param);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError final : SemanticError {
    explicit SppSuperimpositionUnconstrainedGenericParameterError(asts::GenericParameterAst const &param);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionTypeStatementInvalidError(asts::TypeStatementAst const &stmt, asts::TypeAst const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError final : SemanticError {
    explicit SppSuperimpositionExtensionCmpStatementInvalidError(asts::CmpStatementAst const &stmt, asts::TypeAst const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAsyncTargetNotFunctionCallError final : SemanticError {
    explicit SppAsyncTargetNotFunctionCallError(asts::TokenAst const &async_op, asts::Ast const &rhs);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDereferenceNonBorrowedTypeError final : SemanticError {
    explicit SppDereferenceNonBorrowedTypeError(asts::TokenAst const &tok_deref, asts::ExpressionAst const &expr, asts::TypeAst const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppNonCopyableTypeError final : SemanticError {
    explicit SppNonCopyableTypeError(asts::Ast const &ctx, asts::ExpressionAst const &expr, asts::TypeAst const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterInferredConflictInferredError final : SemanticError {
    explicit SppGenericParameterInferredConflictInferredError(asts::Ast const &param, asts::Ast const &first_infer, asts::Ast const &second_infer);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterNotInferredError final : SemanticError {
    explicit SppGenericParameterNotInferredError(asts::Ast const &param, asts::Ast const &ctx);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericArgumentTooManyError final : SemanticError {
    explicit SppGenericArgumentTooManyError(asts::Ast const &param, asts::Ast const &owner, asts::GenericArgumentAst const &arg);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMissingMainFunctionError final : SemanticError {
    explicit SppMissingMainFunctionError();
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidVoidValueError final : SemanticError {
    explicit SppInvalidVoidValueError(asts::ExpressionAst const &expr, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppBorrowLifetimeIncreaseError final : SemanticError {
    explicit SppBorrowLifetimeIncreaseError(asts::Ast const &extension_ast, asts::Ast const &lhs_init_definition, asts::Ast const &rhs_borrow_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidComptimeOperationError final : SemanticError {
    // Todo: Check other comptime error: merge?
    explicit SppInvalidComptimeOperationError(asts::Ast const &ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInternalCompilerError final : SemanticError {
    explicit SppInternalCompilerError(asts::Ast const &ast, StrView message);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericConstraintError final : SemanticError {
    explicit SppGenericConstraintError(asts::Ast const &constraint, asts::Ast const &concrete_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotAnAnnotationError final : SemanticError {
    explicit SppAnnotationTargetNotAnAnnotationError(asts::AnnotationAst const &call_site, asts::FunctionPrototypeAst const &target_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotACmpFunctionError final : SemanticError {
    explicit SppAnnotationTargetNotACmpFunctionError(asts::AnnotationAst const &annotation_marker, asts::Ast const &non_function_ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCalledAnnotationAppliedToInvalidAstError final : SemanticError {
    explicit SppCalledAnnotationAppliedToInvalidAstError(asts::Ast const &invalid_ast, asts::Ast const &annotation_call, asts::Ast const &annotation_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidBinaryFoldExpressionError final : SemanticError {
    explicit SppInvalidBinaryFoldExpressionError(asts::Ast const &expr, asts::Ast const &tup_type, std::size_t tup_num_elems);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAccessViolationError final : SemanticError {
    explicit SppAccessViolationError(asts::Ast const &access_site, asts::Ast const &symbol_definition, StrView visibility, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionOverloadVisibilityMismatchError final : SemanticError {
    explicit SppFunctionOverloadVisibilityMismatchError(asts::AnnotationAst const &first_annotation, asts::FunctionPrototypeAst const &conflicting_overload);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMovingEscapingBorrowedMemoryError final : SemanticError {
    explicit SppMovingEscapingBorrowedMemoryError(asts::Ast const &container, asts::Ast const &where_moved);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMovingComptimeConstantMemoryError final : SemanticError {
    explicit SppMovingComptimeConstantMemoryError(asts::Ast const &ast, asts::Ast const &move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppHigherOrderGenericsNotSupportedError final : SemanticError {
    explicit SppHigherOrderGenericsNotSupportedError(asts::Ast const &ast, asts::Ast const &generic_arg_group);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGeneratedCodeError final : SemanticError {
    explicit SppGeneratedCodeError(asts::Ast const &ast, Str &&wrapped_error);
};
