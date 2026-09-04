module;
#include <spp/macros.hpp>

export module spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.utils.types;
import std;
import numex.big_dec;
import numex.big_int;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
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
  SPP_EXP_CLS struct SppExpressionNotTryError;
  SPP_EXP_CLS struct SppExpressionAmbiguousGeneratorError;
  SPP_EXP_CLS struct SppExpressionAmbiguousTryError;
  SPP_EXP_CLS struct SppLoopTooManyControlFlowStatementsError;
  SPP_EXP_CLS struct SppObjectInitializerMultipleAutofillArgumentsError;
  SPP_EXP_CLS struct SppObjectInitializerInvalidArgumentError;
  SPP_EXP_CLS struct SppObjectInitializerVariantError;
  SPP_EXP_CLS struct SppObjectInitializerGeneratorError;
  SPP_EXP_CLS struct SppAbstractTypeUseError;
  SPP_EXP_CLS struct SppArgumentNameInvalidError;
  SPP_EXP_CLS struct SppArgumentMissingError;
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
  SPP_EXP_CLS struct SppTypeAliasCyclicError;
  SPP_EXP_CLS struct SppDivisionByZeroError;
  SPP_EXP_CLS struct SppShiftAmountOutOfBoundsError;
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
  SPP_EXP_CLS struct SppGenericParameterConflictError;
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
  SPP_EXP_CLS struct SppUnitTestInvalidSignatureError;
  SPP_EXP_CLS struct SppUnitTestNotCallableError;
  SPP_EXP_CLS struct SppFfiGenericParameterError;
  SPP_EXP_CLS struct SppInvalidBinaryFoldExpressionError;
  SPP_EXP_CLS struct SppAccessViolationError;
  SPP_EXP_CLS struct SppFunctionOverloadVisibilityMismatchError;
  SPP_EXP_CLS struct SppMovingEscapingBorrowedMemoryError;
  SPP_EXP_CLS struct SppMovingComptimeConstantMemoryError;
  SPP_EXP_CLS struct SppHigherOrderGenericsNotSupportedError;
  SPP_EXP_CLS struct SppGeneratedCodeError;
  SPP_EXP_CLS struct SppCharLiteralOutOfBoundsError;
  SPP_EXP_CLS struct SppLinearValueNotConsumedError;
  SPP_EXP_CLS struct SppDiscardedValueError;
  SPP_EXP_CLS struct SppLinearValueSkippedInDestructureError;
  SPP_EXP_CLS struct SppDeferTerminatesError;
  SPP_EXP_CLS struct SppDeferInCompileTimeFunctionError;
  SPP_EXP_CLS struct SppDeferConsumesMovedValueError;
  SPP_EXP_CLS struct SppFeatureNotYetSupportedError;

  /**
   * A feature the language means to have and does not have yet. Each one carries its own explanation of why it does
   * not work today, so that reaching it reads as "not yet" rather than as a mistake in the code that reached it.
   */
  SPP_EXP_CLS enum class NotYetSupportedFeature {
    NestedTypeBeforeSupScopes,
  };

  SPP_EXP_CLS enum class ErrorInformationKind {
    HEADER, ERROR, CONTEXT, FOOTER,
    WRAPPED
  };

  SPP_EXP_CLS struct ErrorInformation {
    asts::Ast const *Ast;
    ErrorInformationKind Kind;
    Str Tag;
    Str Msg;
  };
}

SPP_EXP_CLS struct spp::analyse::errors::SemanticError : spp::utils::errors::AbstractError {
  using AbstractError::AbstractError;
  SPP_ATTR_COLD SemanticError(SemanticError const &) = default;

  ~SemanticError() override = default;

  Vec<ErrorInformation> ErrorInfo;

  auto AddHeaders(std::size_t err_code, Str &&msg) -> void;

  auto AddErr(asts::Ast const *ast, Str &&tag) -> void;

  /**
   * As @c AddErr , but marks the ast exactly as given rather than narrowing a call expression to its argument group.
   * Use it when the error is about the expression as a whole - what its value is, or that nothing takes it - rather
   * than about the call within it, where narrowing would point at the arguments and read as though they were at fault.
   * @param ast The ast to mark.
   * @param tag The message to attach to it.
   */
  auto AddErrExact(asts::Ast const *ast, Str &&tag) -> void;

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
  explicit SppTypeMismatchError(asts::Ast const &lhs, asts::Ast const &lhs_ty, asts::Ast const &rhs,
    asts::Ast const &rhs_ty);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSecondClassBorrowViolationError final : SemanticError {
  explicit SppSecondClassBorrowViolationError(asts::Ast const &expr, asts::Ast const &type, StrView ctx);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCompileTimeConstantError final : SemanticError {
  explicit SppCompileTimeConstantError(asts::Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidMutationError final : SemanticError {
  explicit SppInvalidMutationError(asts::Ast const &sym, asts::Ast const &mutator,
    asts::Ast const &initialization_location, StrView extra);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUninitializedMemoryUseError final : SemanticError {
  explicit SppUninitializedMemoryUseError(asts::Ast const &ast, asts::Ast const &init_location,
    asts::Ast const &move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : SemanticError {
  explicit SppPartiallyInitializedMemoryUseError(asts::Ast const &ast, asts::Ast const &init_location,
    asts::Ast const &partial_move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : SemanticError {
  explicit SppMoveFromBorrowedMemoryError(asts::Ast const &ast, asts::Ast const &move_location,
    asts::Ast const &borrow_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : SemanticError {
  explicit SppInconsistentlyInitializedMemoryUseError(asts::Ast const &ast, asts::Ast const &branch_1,
    asts::Ast const &branch_2, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyEscapingBorrows final : SemanticError {
  explicit SppInconsistentlyEscapingBorrows(asts::Ast const &ast, asts::Ast const &branch_1,
    asts::Ast const &branch_2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessNonIndexableError final : SemanticError {
  explicit SppMemberAccessNonIndexableError(asts::Ast const &lhs, asts::Ast const &lhs_type,
    asts::Ast const &access_op);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessOutOfBoundsError final : SemanticError {
  explicit SppMemberAccessOutOfBoundsError(asts::Ast const &lhs, asts::Ast const &lhs_type, std::size_t n,
    asts::Ast const &access_op);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchElseNotLastError final : SemanticError {
  explicit SppCaseBranchElseNotLastError(asts::Ast const &non_last_else_branch, asts::Ast const &last_branch);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchMissingElseError final : SemanticError {
  explicit SppCaseBranchMissingElseError(asts::Ast const &case_expr, asts::Ast const &last_branch);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierDuplicateError final : SemanticError {
  explicit SppIdentifierDuplicateError(asts::Ast const &first_identifier, asts::Ast const &duplicate_identifier,
    StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppRecursiveTypeError final : SemanticError {
  explicit SppRecursiveTypeError(asts::Ast const &type, asts::Ast const &recursion);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFloatOutOfBoundsError final : SemanticError {
  explicit SppFloatOutOfBoundsError(asts::Ast const &literal, numex::BigDec const &value,
    numex::BigDec const &lower, numex::BigDec const &upper, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDivisionByZeroError final : SemanticError {
  explicit SppDivisionByZeroError(asts::Ast const &operation, asts::Ast const &divisor);
};

SPP_EXP_CLS struct spp::analyse::errors::SppShiftAmountOutOfBoundsError final : SemanticError {
  explicit SppShiftAmountOutOfBoundsError(asts::Ast const &operation, asts::Ast const &amount, StrView type,
    std::size_t width);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIntegerOutOfBoundsError final : SemanticError {
  explicit SppIntegerOutOfBoundsError(asts::Ast const &literal, numex::BigInt const &value,
    numex::BigInt const &lower, numex::BigInt const &upper, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppOrderInvalidError final : SemanticError {
  explicit SppOrderInvalidError(StrView first_what, asts::Ast const &first, StrView second_what,
    asts::Ast const &second);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpansionOfNonTupleError final : SemanticError {
  explicit SppExpansionOfNonTupleError(asts::Ast const &unpack, asts::Ast const &ast, asts::Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemoryOverlapUsageError final : SemanticError {
  explicit SppMemoryOverlapUsageError(asts::Ast const &ast, asts::Ast const &overlap_ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleSelfParametersError final : SemanticError {
  explicit SppMultipleSelfParametersError(asts::Ast const &first_self,
    asts::Ast const &second_self);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleVariadicParametersError final : SemanticError {
  explicit SppMultipleVariadicParametersError(asts::Ast const &first_variadic, asts::Ast const &second_variadic);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionPrototypeConflictError final : SemanticError {
  explicit SppFunctionPrototypeConflictError(asts::Ast const &first_proto, asts::Ast const &second_proto);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError final : SemanticError {
  explicit SppFunctionSubroutineContainsGenExpressionError(asts::Ast const &fun_tag, asts::Ast const &gen_expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppYieldedTypeMismatchError final : SemanticError {
  explicit SppYieldedTypeMismatchError(asts::Ast const &lhs, asts::Ast const &lhs_ty, asts::Ast const &rhs,
    asts::Ast const &rhs_ty);
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
  explicit SppInvalidLocalVariableTypeAnnotationError(asts::Ast const &type, asts::Ast const &var);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleRestPatternsError final : SemanticError {
  explicit SppMultipleRestPatternsError(asts::Ast const &var, asts::Ast const &pattern_1, asts::Ast const &pattern_2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError final : SemanticError {
  explicit SppVariableArrayDestructureArrayTypeMismatchError(asts::Ast const &var, asts::Ast const &val,
    asts::Ast const &val_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError final : SemanticError {
  explicit SppVariableArrayDestructureArraySizeMismatchError(asts::Ast const &var, std::size_t var_size,
    asts::Ast const &val, std::size_t val_size);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError final : SemanticError {
  explicit SppVariableTupleDestructureTupleTypeMismatchError(asts::Ast const &var, asts::Ast const &val,
    asts::Ast const &val_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError final : SemanticError {
  explicit SppVariableTupleDestructureTupleSizeMismatchError(asts::Ast const &var, std::size_t var_size,
    asts::Ast const &val, std::size_t val_size);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableObjectDestructureWithBoundRestPatternError final : SemanticError {
  explicit SppVariableObjectDestructureWithBoundRestPatternError(asts::Ast const &var, asts::Ast const &rest_pattern);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotBooleanError final : SemanticError {
  explicit SppExpressionNotBooleanError(asts::Ast const &expr, asts::Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotGeneratorError final : SemanticError {
  explicit SppExpressionNotGeneratorError(asts::Ast const &expr, asts::Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotTryError final : SemanticError {
  explicit SppExpressionNotTryError(asts::Ast const &expr, asts::Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousGeneratorError final : SemanticError {
  explicit SppExpressionAmbiguousGeneratorError(asts::Ast const &expr, asts::Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousTryError final : SemanticError {
  explicit SppExpressionAmbiguousTryError(asts::Ast const &expr, asts::Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLoopTooManyControlFlowStatementsError final : SemanticError {
  explicit SppLoopTooManyControlFlowStatementsError(asts::Ast const &tok_loop, asts::Ast const &stmt,
    std::size_t num_controls, std::size_t loop_depth);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError final : SemanticError {
  explicit SppObjectInitializerMultipleAutofillArgumentsError(asts::Ast const &arg1, asts::Ast const &arg2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerInvalidArgumentError final : SemanticError {
  explicit SppObjectInitializerInvalidArgumentError(asts::Ast const &arg);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerVariantError final : SemanticError {
  explicit SppObjectInitializerVariantError(asts::Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerGeneratorError final : SemanticError {
  explicit SppObjectInitializerGeneratorError(asts::Ast const &type, asts::Ast const &generator_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAbstractTypeUseError final : SemanticError {
  explicit SppAbstractTypeUseError(asts::Ast const &type, asts::Ast const &unimplemented);
};

SPP_EXP_CLS struct spp::analyse::errors::SppArgumentNameInvalidError final : SemanticError {
  explicit SppArgumentNameInvalidError(asts::Ast const &target, StrView target_what, asts::Ast const &source,
    StrView source_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppArgumentMissingError final : SemanticError {
  explicit SppArgumentMissingError(asts::Ast const &target, StrView target_what, asts::Ast const &source,
    StrView source_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallAbstractFunctionError final : SemanticError {
  explicit SppFunctionCallAbstractFunctionError(asts::Ast const &proto, asts::Ast const &call);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallTooManyArgumentsError final : SemanticError {
  explicit SppFunctionCallTooManyArgumentsError(asts::Ast const &proto, std::size_t proto_proto_count,
    asts::Ast const &call, std::size_t call_arg_count);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallNoValidSignaturesError final : SemanticError {
  explicit SppFunctionCallNoValidSignaturesError(asts::Ast const &call, StrView sigs, StrView attempted);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallOverloadAmbiguousError final : SemanticError {
  explicit SppFunctionCallOverloadAmbiguousError(asts::Ast const &call, StrView sigs, StrView attempted);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError final : SemanticError {
  explicit SppMemberAccessStaticOperatorExpectedError(asts::Ast const &lhs, asts::Ast const &access, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError final : SemanticError {
  explicit SppMemberAccessRuntimeOperatorExpectedError(asts::Ast const &lhs, asts::Ast const &access);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericTypeInvalidUsageError final : SemanticError {
  explicit SppGenericTypeInvalidUsageError(asts::Ast const &gen_name, asts::Ast const &gen_val, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAmbiguousMemberAccessError final : SemanticError {
  explicit SppAmbiguousMemberAccessError(asts::Ast const &found_field_1, asts::Ast const &found_field_2,
    asts::Ast const &field_access);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCoroutineContainsReturnStatementError final : SemanticError {
  explicit SppCoroutineContainsReturnStatementError(asts::Ast const &fun_tag, asts::Ast const &ret_stmt);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError final : SemanticError {
  explicit SppFunctionSubroutineMissingReturnStatementError(asts::Ast const &final_member,
    asts::Ast const &return_type_definition, asts::Ast const &return_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionCyclicExtensionError final : SemanticError {
  explicit SppSuperimpositionCyclicExtensionError(asts::Ast const &first_extension,
    asts::Ast const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppTypeAliasCyclicError final : SemanticError {
  explicit SppTypeAliasCyclicError(asts::Ast const &first_alias,
    asts::Ast const &cyclic_alias);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionDoubleExtensionError final : SemanticError {
  explicit SppSuperimpositionDoubleExtensionError(asts::Ast const &first_extension,
    asts::Ast const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionSelfExtensionError final : SemanticError {
  explicit SppSuperimpositionSelfExtensionError(asts::Ast const &first_extension,
    asts::Ast const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError final : SemanticError {
  explicit SppSuperimpositionExtensionMethodInvalidError(asts::Ast const &new_method,
    asts::Ast const &super_class);
};

SPP_EXP_CLS struct
  spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError final : SemanticError {
  explicit SppSuperimpositionExtensionNonVirtualMethodOverriddenError(asts::Ast const &new_method,
    asts::Ast const &base_method, asts::Ast const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError final : SemanticError {
  explicit SppSuperimpositionOptionalGenericParameterError(asts::Ast const &param);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError final : SemanticError {
  explicit SppSuperimpositionUnconstrainedGenericParameterError(asts::Ast const &param);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError final : SemanticError {
  explicit SppSuperimpositionExtensionTypeStatementInvalidError(asts::Ast const &stmt,
    asts::Ast const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError final : SemanticError {
  explicit SppSuperimpositionExtensionCmpStatementInvalidError(asts::Ast const &stmt,
    asts::Ast const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAsyncTargetNotFunctionCallError final : SemanticError {
  explicit SppAsyncTargetNotFunctionCallError(asts::Ast const &async_op, asts::Ast const &rhs);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDereferenceNonBorrowedTypeError final : SemanticError {
  explicit SppDereferenceNonBorrowedTypeError(asts::Ast const &tok_deref, asts::Ast const &expr,
    asts::Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppNonCopyableTypeError final : SemanticError {
  explicit SppNonCopyableTypeError(asts::Ast const &ctx, asts::Ast const &expr, asts::Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterConflictError final : SemanticError {
  explicit SppGenericParameterConflictError(asts::Ast const &param, asts::Ast const &first_infer,
    asts::Ast const &second_infer);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterNotInferredError final : SemanticError {
  explicit SppGenericParameterNotInferredError(asts::Ast const &param, asts::Ast const &ctx);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericArgumentTooManyError final : SemanticError {
  explicit SppGenericArgumentTooManyError(asts::Ast const &param, asts::Ast const &owner,
    asts::Ast const &arg);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMissingMainFunctionError final : SemanticError {
  explicit SppMissingMainFunctionError();
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidVoidValueError final : SemanticError {
  explicit SppInvalidVoidValueError(asts::Ast const &expr, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppBorrowLifetimeIncreaseError final : SemanticError {
  explicit SppBorrowLifetimeIncreaseError(asts::Ast const &extension_ast, asts::Ast const &lhs_init_definition,
    asts::Ast const &rhs_borrow_definition);
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
  explicit SppAnnotationTargetNotAnAnnotationError(asts::Ast const &call_site, asts::Ast const &target_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotACmpFunctionError final : SemanticError {
  explicit SppAnnotationTargetNotACmpFunctionError(asts::Ast const &annotation_marker,
    asts::Ast const &non_function_ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCalledAnnotationAppliedToInvalidAstError final : SemanticError {
  explicit SppCalledAnnotationAppliedToInvalidAstError(asts::Ast const &invalid_ast, asts::Ast const &annotation_call,
    asts::Ast const &annotation_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUnitTestInvalidSignatureError final : SemanticError {
  explicit SppUnitTestInvalidSignatureError(asts::Ast const &annotation, asts::Ast const &fun_name,
    StrView requirement);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFfiGenericParameterError final : SemanticError {
  explicit SppFfiGenericParameterError(
    asts::Ast const &annotation, asts::Ast const &generic_parameter, StrView symbol);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUnitTestNotCallableError final : SemanticError {
  explicit SppUnitTestNotCallableError(asts::Ast const &call_site, asts::Ast const &annotation);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidBinaryFoldExpressionError final : SemanticError {
  explicit SppInvalidBinaryFoldExpressionError(asts::Ast const &expr, asts::Ast const &tup_type,
    std::size_t tup_num_elems);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAccessViolationError final : SemanticError {
  explicit SppAccessViolationError(asts::Ast const &access_site, asts::Ast const &symbol_definition, StrView visibility,
    StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionOverloadVisibilityMismatchError final : SemanticError {
  explicit SppFunctionOverloadVisibilityMismatchError(asts::Ast const &first_annotation,
    asts::Ast const &conflicting_overload, asts::Ast const &conflicting_annotation);
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

SPP_EXP_CLS struct spp::analyse::errors::SppCharLiteralOutOfBoundsError final : SemanticError {
  explicit SppCharLiteralOutOfBoundsError(asts::Ast const &literal, std::uint32_t code_point);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLinearValueNotConsumedError final : SemanticError {
  explicit SppLinearValueNotConsumedError(asts::Ast const &symbol_definition, asts::Ast const &exit_point,
    StrView symbol_name, StrView type_name, StrView exit_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDiscardedValueError final : SemanticError {
  explicit SppDiscardedValueError(asts::Ast const &expr, StrView type_name);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDeferTerminatesError final : SemanticError {
  explicit SppDeferTerminatesError(asts::Ast const &tok_defer, asts::Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFeatureNotYetSupportedError final : SemanticError {
  /**
   * @param feature Which unsupported feature was reached; selects the explanation.
   * @param context The ast to point at for context - typically what the feature was used on.
   * @param site The ast to point at as the error - typically where it was written.
   */
  explicit SppFeatureNotYetSupportedError(
    NotYetSupportedFeature feature, asts::Ast const &context, asts::Ast const &site);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDeferConsumesMovedValueError final : SemanticError {
  explicit SppDeferConsumesMovedValueError(asts::Ast const &deferred, asts::Ast const &consumed_at,
    StrView symbol_name, StrView exit_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDeferInCompileTimeFunctionError final : SemanticError {
  explicit SppDeferInCompileTimeFunctionError(asts::Ast const &tok_defer);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLinearValueSkippedInDestructureError final : SemanticError {
  explicit SppLinearValueSkippedInDestructureError(asts::Ast const &skip, asts::Ast const &destructure,
    StrView attr_name, StrView type_name);
};
