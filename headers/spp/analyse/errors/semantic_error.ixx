module;
#include <spp/macros.hpp>

export module spp.analyse.errors.semantic_error;
import spp.utils.errors;
import spp.utils.types;
import std;
import numex.big_dec;
import numex.big_int;

use(spp::asts, struct Ast);
use(spp::analyse::errors, struct SemanticError);
use(spp::analyse::errors, struct SppInvalidPrimaryExpressionError);
use(spp::analyse::errors, struct SppTypeMismatchError);
use(spp::analyse::errors, struct SppSecondClassBorrowViolationError);
use(spp::analyse::errors, struct SppCompileTimeConstantError);
use(spp::analyse::errors, struct SppInvalidMutationError);
use(spp::analyse::errors, struct SppUninitializedMemoryUseError);
use(spp::analyse::errors, struct SppPartiallyInitializedMemoryUseError);
use(spp::analyse::errors, struct SppMoveFromBorrowedMemoryError);
use(spp::analyse::errors, struct SppInconsistentlyInitializedMemoryUseError);
use(spp::analyse::errors, struct SppInconsistentlyEscapingBorrows);
use(spp::analyse::errors, struct SppMemberAccessNonIndexableError);
use(spp::analyse::errors, struct SppMemberAccessOutOfBoundsError);
use(spp::analyse::errors, struct SppCaseBranchElseNotLastError);
use(spp::analyse::errors, struct SppCaseBranchMissingElseError);
use(spp::analyse::errors, struct SppIdentifierDuplicateError);
use(spp::analyse::errors, struct SppRecursiveTypeError);
use(spp::analyse::errors, struct SppFloatOutOfBoundsError);
use(spp::analyse::errors, struct SppIntegerOutOfBoundsError);
use(spp::analyse::errors, struct SppOrderInvalidError);
use(spp::analyse::errors, struct SppExpansionOfNonTupleError);
use(spp::analyse::errors, struct SppMemoryOverlapUsageError);
use(spp::analyse::errors, struct SppMultipleSelfParametersError);
use(spp::analyse::errors, struct SppMultipleVariadicParametersError);
use(spp::analyse::errors, struct SppFunctionPrototypeConflictError);
use(spp::analyse::errors, struct SppFunctionSubroutineContainsGenExpressionError);
use(spp::analyse::errors, struct SppYieldedTypeMismatchError);
use(spp::analyse::errors, struct SppIdentifierUnknownError);
use(spp::analyse::errors, struct SppSelfIdentifierInvalidContextError);
use(spp::analyse::errors, struct SppUnreachableCodeError);
use(spp::analyse::errors, struct SppInvalidLocalVariableTypeAnnotationError);
use(spp::analyse::errors, struct SppMultipleRestPatternsError);
use(spp::analyse::errors, struct SppVariableArrayDestructureArrayTypeMismatchError);
use(spp::analyse::errors, struct SppVariableArrayDestructureArraySizeMismatchError);
use(spp::analyse::errors, struct SppVariableTupleDestructureTupleTypeMismatchError);
use(spp::analyse::errors, struct SppVariableTupleDestructureTupleSizeMismatchError);
use(spp::analyse::errors, struct SppVariableObjectDestructureWithBoundRestPatternError);
use(spp::analyse::errors, struct SppDestructureSkipsOwnedPartError);
use(spp::analyse::errors, struct SppPartialMoveOfDestructibleValueError);
use(spp::analyse::errors, struct SppExpressionNotBooleanError);
use(spp::analyse::errors, struct SppExpressionNotGeneratorError);
use(spp::analyse::errors, struct SppExpressionNotTryError);
use(spp::analyse::errors, struct SppExpressionAmbiguousGeneratorError);
use(spp::analyse::errors, struct SppExpressionAmbiguousTryError);
use(spp::analyse::errors, struct SppLoopTooManyControlFlowStatementsError);
use(spp::analyse::errors, struct SppObjectInitializerMultipleAutofillArgumentsError);
use(spp::analyse::errors, struct SppObjectInitializerInvalidArgumentError);
use(spp::analyse::errors, struct SppObjectInitializerVariantError);
use(spp::analyse::errors, struct SppObjectInitializerGeneratorError);
use(spp::analyse::errors, struct SppAbstractTypeUseError);
use(spp::analyse::errors, struct SppArgumentNameInvalidError);
use(spp::analyse::errors, struct SppArgumentMissingError);
use(spp::analyse::errors, struct SppFunctionCallAbstractFunctionError);
use(spp::analyse::errors, struct SppFunctionCallTooManyArgumentsError);
use(spp::analyse::errors, struct SppFunctionCallNoValidSignaturesError);
use(spp::analyse::errors, struct SppFunctionCallOverloadAmbiguousError);
use(spp::analyse::errors, struct SppMemberAccessStaticOperatorExpectedError);
use(spp::analyse::errors, struct SppMemberAccessRuntimeOperatorExpectedError);
use(spp::analyse::errors, struct SppGenericTypeInvalidUsageError);
use(spp::analyse::errors, struct SppAmbiguousMemberAccessError);
use(spp::analyse::errors, struct SppCoroutineContainsReturnStatementError);
use(spp::analyse::errors, struct SppFunctionSubroutineMissingReturnStatementError);
use(spp::analyse::errors, struct SppSuperimpositionCyclicExtensionError);
use(spp::analyse::errors, struct SppTypeAliasCyclicError);
use(spp::analyse::errors, struct SppDivisionByZeroError);
use(spp::analyse::errors, struct SppShiftAmountOutOfBoundsError);
use(spp::analyse::errors, struct SppSuperimpositionDoubleExtensionError);
use(spp::analyse::errors, struct SppSuperimpositionSelfExtensionError);
use(spp::analyse::errors, struct SppSuperimpositionExtensionMethodInvalidError);
use(spp::analyse::errors, struct SppSuperimpositionExtensionNonVirtualMethodOverriddenError);
use(spp::analyse::errors, struct SppSuperimpositionOptionalGenericParameterError);
use(spp::analyse::errors, struct SppSuperimpositionUnconstrainedGenericParameterError);
use(spp::analyse::errors, struct SppSuperimpositionExtensionTypeStatementInvalidError);
use(spp::analyse::errors, struct SppSuperimpositionExtensionCmpStatementInvalidError);
use(spp::analyse::errors, struct SppAsyncTargetNotFunctionCallError);
use(spp::analyse::errors, struct SppAwaitTargetNotFutureError);
use(spp::analyse::errors, struct SppInvalidDefaultValueError);
use(spp::analyse::errors, struct SppDereferenceNonBorrowedTypeError);
use(spp::analyse::errors, struct SppNonCopyableTypeError);
use(spp::analyse::errors, struct SppGenericParameterConflictError);
use(spp::analyse::errors, struct SppGenericParameterNotInferredError);
use(spp::analyse::errors, struct SppGenericArgumentTooManyError);
use(spp::analyse::errors, struct SppMissingMainFunctionError);
use(spp::analyse::errors, struct SppInvalidVoidValueError);
use(spp::analyse::errors, struct SppBorrowLifetimeIncreaseError);
use(spp::analyse::errors, struct SppInvalidComptimeOperationError);
use(spp::analyse::errors, struct SppInternalCompilerError);
use(spp::analyse::errors, struct SppGenericConstraintError);
use(spp::analyse::errors, struct SppAnnotationTargetNotAnAnnotationError);
use(spp::analyse::errors, struct SppAnnotationTargetNotACmpFunctionError);
use(spp::analyse::errors, struct SppCalledAnnotationAppliedToInvalidAstError);
use(spp::analyse::errors, struct SppGenOnceFinishesWithoutYieldingError);
use(spp::analyse::errors, struct SppUnitTestInvalidSignatureError);
use(spp::analyse::errors, struct SppUnitTestNotCallableError);
use(spp::analyse::errors, struct SppFfiGenericParameterError);
use(spp::analyse::errors, struct SppEmptyBodyRequiredError);
use(spp::analyse::errors, struct SppInvalidBinaryFoldExpressionError);
use(spp::analyse::errors, struct SppAccessViolationError);
use(spp::analyse::errors, struct SppFunctionOverloadVisibilityMismatchError);
use(spp::analyse::errors, struct SppMovingEscapingBorrowedMemoryError);
use(spp::analyse::errors, struct SppMovingComptimeConstantMemoryError);
use(spp::analyse::errors, struct SppHigherOrderGenericsNotSupportedError);
use(spp::analyse::errors, struct SppGeneratedCodeError);
use(spp::analyse::errors, struct SppCharLiteralOutOfBoundsError);
use(spp::analyse::errors, struct SppLinearValueNotConsumedError);
use(spp::analyse::errors, struct SppDiscardedValueError);
use(spp::analyse::errors, struct SppLinearValueSkippedInDestructureError);
use(spp::analyse::errors, struct SppDeferTerminatesError);
use(spp::analyse::errors, struct SppDeferInCompileTimeFunctionError);
use(spp::analyse::errors, struct SppDeferConsumesMovedValueError);
use(spp::analyse::errors, struct SppFeatureNotYetSupportedError);

use(spp::analyse::errors, enum class NotYetSupportedFeature);
use(spp::analyse::errors, enum class ErrorInformationKind);
use(spp::analyse::errors, struct ErrorInformation);

/// A feature that can be syntactically used, but hasn't got
/// the semantic analysis in place for yet. Easy swap-out for
/// once the analysis is ready.
SPP_EXP_CLS enum class spp::analyse::errors::NotYetSupportedFeature {
  NestedTypeBeforeSupScopes,
  CoroutineClosure,
};

/// The type of the piece of text being rendered as part of
/// the error message. The classical 4 are the header, footer,
/// context and error. There is also the lesser-used "wrapped",
/// for when one error catches and wraps an error.
SPP_EXP_CLS enum class spp::analyse::errors::ErrorInformationKind {
  HEADER, ERROR, CONTEXT, FOOTER,
  WRAPPED
};

/// The error information contains all associated information
/// per-part of an error. It is creates for each piece of
/// text appended to an error call. Can have a nullptr ast
/// for the header or footer.
SPP_EXP_CLS struct spp::analyse::errors::ErrorInformation {
  asts::Ast const *Ast;
  ErrorInformationKind Kind;
  Str Tag;
  Str Msg;
};

/// The base semantic error type forms the basis for all the
/// raised exceptions within the S++ compilation pipeline.
/// All the information appending methods are provided for
/// easy setup.
SPP_EXP_CLS struct spp::analyse::errors::SemanticError :
  utils::errors::AbstractError {
  using AbstractError::AbstractError;
  SPP_ATTR_COLD SemanticError(SemanticError const &) = default;

  ~SemanticError() override = default;

  /// The list of error information that will be stacked when
  /// displaying the error.
  Vec<ErrorInformation> ErrorInfo;

  /// Add the header to the error message. This provides a
  /// title and error code.
  auto AddHeader(std::size_t err_code, Str &&msg) -> void;

  /// Add error information from an ast with a tag. This ast
  /// will get "^^^" underlined based on start/end positions,
  /// and the tag appended after "<-". Transform to linked asts
  /// for better reporting.
  auto AddErr(Ast const *ast, Str &&tag) -> void;

  /// A slightly modified error append. Usually, the asts have
  /// some transformation they can go through depending on what
  /// variant they are. In this case, no transformation at all.
  auto AddErrExact(Ast const *ast, Str &&tag) -> void;

  /// Add context to an error, for example, pointing to another
  /// ast that provides more information about the error.
  auto AddCtxForErr(Ast const *ast, Str &&tag) -> void;

  /// Add the footer, containing the final summary note, and a
  /// help line.
  auto AddFooter(Str &&note, Str &&help) -> void;

  /// Wrap an error that has been caught, re-displaying it within
  /// this error.
  auto AddWrapped(Str &&msg) -> void;

  /// Simple clone copying over all the fields. Todo: Do we still
  /// need this method? Is it being called anywhere?
  SPP_ATTR_NODISCARD auto Clone() const -> Unique<SemanticError>;
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidPrimaryExpressionError final : SemanticError {
  explicit SppInvalidPrimaryExpressionError(Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppTypeMismatchError final : SemanticError {
  explicit SppTypeMismatchError(Ast const &lhs, Ast const &lhs_ty, Ast const &rhs,
    Ast const &rhs_ty);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSecondClassBorrowViolationError final : SemanticError {
  explicit SppSecondClassBorrowViolationError(Ast const &expr, Ast const &type, StrView ctx);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCompileTimeConstantError final : SemanticError {
  explicit SppCompileTimeConstantError(Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidMutationError final : SemanticError {
  explicit SppInvalidMutationError(Ast const &sym, Ast const &mutator,
    Ast const &initialization_location, StrView extra);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUninitializedMemoryUseError final : SemanticError {
  explicit SppUninitializedMemoryUseError(Ast const &ast, Ast const &init_location,
    Ast const &move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppPartiallyInitializedMemoryUseError final : SemanticError {
  explicit SppPartiallyInitializedMemoryUseError(Ast const &ast, Ast const &init_location,
    Ast const &partial_move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMoveFromBorrowedMemoryError final : SemanticError {
  explicit SppMoveFromBorrowedMemoryError(Ast const &ast, Ast const &move_location,
    Ast const &borrow_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyInitializedMemoryUseError final : SemanticError {
  explicit SppInconsistentlyInitializedMemoryUseError(Ast const &ast, Ast const &branch_1,
    Ast const &branch_2, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInconsistentlyEscapingBorrows final : SemanticError {
  explicit SppInconsistentlyEscapingBorrows(Ast const &ast, Ast const &branch_1,
    Ast const &branch_2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessNonIndexableError final : SemanticError {
  explicit SppMemberAccessNonIndexableError(Ast const &lhs, Ast const &lhs_type,
    Ast const &access_op);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessOutOfBoundsError final : SemanticError {
  explicit SppMemberAccessOutOfBoundsError(Ast const &lhs, Ast const &lhs_type, std::size_t n,
    Ast const &access_op);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchElseNotLastError final : SemanticError {
  explicit SppCaseBranchElseNotLastError(Ast const &non_last_else_branch, Ast const &last_branch);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCaseBranchMissingElseError final : SemanticError {
  explicit SppCaseBranchMissingElseError(Ast const &case_expr, Ast const &last_branch);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierDuplicateError final : SemanticError {
  explicit SppIdentifierDuplicateError(Ast const &first_identifier, Ast const &duplicate_identifier,
    StrView what);

  /// A name the prelude already imports: only the author's own
  /// identifier is shown, as the prelude's is not one they wrote.
  explicit SppIdentifierDuplicateError(Ast const &duplicate_identifier, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppRecursiveTypeError final : SemanticError {
  explicit SppRecursiveTypeError(Ast const &type, Ast const &recursion);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFloatOutOfBoundsError final : SemanticError {
  explicit SppFloatOutOfBoundsError(Ast const &literal, numex::BigDec const &value,
    numex::BigDec const &lower, numex::BigDec const &upper, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDivisionByZeroError final : SemanticError {
  explicit SppDivisionByZeroError(Ast const &operation, Ast const &divisor);
};

SPP_EXP_CLS struct spp::analyse::errors::SppShiftAmountOutOfBoundsError final : SemanticError {
  explicit SppShiftAmountOutOfBoundsError(Ast const &operation, Ast const &amount, StrView type,
    std::size_t width);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIntegerOutOfBoundsError final : SemanticError {
  explicit SppIntegerOutOfBoundsError(Ast const &literal, numex::BigInt const &value,
    numex::BigInt const &lower, numex::BigInt const &upper, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppOrderInvalidError final : SemanticError {
  explicit SppOrderInvalidError(StrView first_what, Ast const &first, StrView second_what,
    Ast const &second);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpansionOfNonTupleError final : SemanticError {
  explicit SppExpansionOfNonTupleError(Ast const &unpack, Ast const &ast, Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemoryOverlapUsageError final : SemanticError {
  explicit SppMemoryOverlapUsageError(Ast const &ast, Ast const &overlap_ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleSelfParametersError final : SemanticError {
  explicit SppMultipleSelfParametersError(Ast const &first_self,
    Ast const &second_self);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleVariadicParametersError final : SemanticError {
  explicit SppMultipleVariadicParametersError(Ast const &first_variadic, Ast const &second_variadic);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionPrototypeConflictError final : SemanticError {
  explicit SppFunctionPrototypeConflictError(Ast const &first_proto, Ast const &second_proto);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineContainsGenExpressionError final : SemanticError {
  explicit SppFunctionSubroutineContainsGenExpressionError(Ast const &fun_tag, Ast const &gen_expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppYieldedTypeMismatchError final : SemanticError {
  explicit SppYieldedTypeMismatchError(Ast const &lhs, Ast const &lhs_ty, Ast const &rhs,
    Ast const &rhs_ty);
};

SPP_EXP_CLS struct spp::analyse::errors::SppIdentifierUnknownError final : SemanticError {
  explicit SppIdentifierUnknownError(Ast const &name, StrView what, std::optional<Str> const &closest = {});
};

SPP_EXP_CLS struct spp::analyse::errors::SppSelfIdentifierInvalidContextError final : SemanticError {
  explicit SppSelfIdentifierInvalidContextError(Ast const &self);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUnreachableCodeError final : SemanticError {
  explicit SppUnreachableCodeError(Ast const &member, Ast const &next_member);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidLocalVariableTypeAnnotationError final : SemanticError {
  explicit SppInvalidLocalVariableTypeAnnotationError(Ast const &type, Ast const &var);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMultipleRestPatternsError final : SemanticError {
  explicit SppMultipleRestPatternsError(Ast const &var, Ast const &pattern_1, Ast const &pattern_2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArrayTypeMismatchError final : SemanticError {
  explicit SppVariableArrayDestructureArrayTypeMismatchError(Ast const &var, Ast const &val,
    Ast const &val_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableArrayDestructureArraySizeMismatchError final : SemanticError {
  explicit SppVariableArrayDestructureArraySizeMismatchError(Ast const &var, std::size_t var_size,
    Ast const &val, std::size_t val_size);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError final : SemanticError {
  explicit SppVariableTupleDestructureTupleTypeMismatchError(Ast const &var, Ast const &val,
    Ast const &val_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError final : SemanticError {
  explicit SppVariableTupleDestructureTupleSizeMismatchError(Ast const &var, std::size_t var_size,
    Ast const &val, std::size_t val_size);
};

SPP_EXP_CLS struct spp::analyse::errors::SppVariableObjectDestructureWithBoundRestPatternError final : SemanticError {
  explicit SppVariableObjectDestructureWithBoundRestPatternError(Ast const &var, Ast const &rest_pattern);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDestructureSkipsOwnedPartError final : SemanticError {
  explicit SppDestructureSkipsOwnedPartError(
    Ast const &destructure, Ast const &value, StrView part);
};

SPP_EXP_CLS struct spp::analyse::errors::SppPartialMoveOfDestructibleValueError final : SemanticError {
  explicit SppPartialMoveOfDestructibleValueError(
    Ast const &exit_point, Ast const &move, Ast const &destructor, StrView type_name);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotBooleanError final : SemanticError {
  explicit SppExpressionNotBooleanError(Ast const &expr, Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotGeneratorError final : SemanticError {
  explicit SppExpressionNotGeneratorError(Ast const &expr, Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionNotTryError final : SemanticError {
  explicit SppExpressionNotTryError(Ast const &expr, Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousGeneratorError final : SemanticError {
  explicit SppExpressionAmbiguousGeneratorError(Ast const &expr, Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppExpressionAmbiguousTryError final : SemanticError {
  explicit SppExpressionAmbiguousTryError(Ast const &expr, Ast const &expr_type, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLoopTooManyControlFlowStatementsError final : SemanticError {
  explicit SppLoopTooManyControlFlowStatementsError(Ast const &tok_loop, Ast const &stmt,
    std::size_t num_controls, std::size_t loop_depth);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerMultipleAutofillArgumentsError final : SemanticError {
  explicit SppObjectInitializerMultipleAutofillArgumentsError(Ast const &arg1, Ast const &arg2);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerInvalidArgumentError final : SemanticError {
  explicit SppObjectInitializerInvalidArgumentError(Ast const &arg);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerVariantError final : SemanticError {
  explicit SppObjectInitializerVariantError(Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppObjectInitializerGeneratorError final : SemanticError {
  explicit SppObjectInitializerGeneratorError(Ast const &type, Ast const &generator_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAbstractTypeUseError final : SemanticError {
  explicit SppAbstractTypeUseError(Ast const &type, Ast const &unimplemented);
};

SPP_EXP_CLS struct spp::analyse::errors::SppArgumentNameInvalidError final : SemanticError {
  explicit SppArgumentNameInvalidError(Ast const &target, StrView target_what, Ast const &source,
    StrView source_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppArgumentMissingError final : SemanticError {
  explicit SppArgumentMissingError(Ast const &target, StrView target_what, Ast const &source,
    StrView source_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallAbstractFunctionError final : SemanticError {
  explicit SppFunctionCallAbstractFunctionError(Ast const &proto, Ast const &call);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallTooManyArgumentsError final : SemanticError {
  explicit SppFunctionCallTooManyArgumentsError(Ast const &proto, std::size_t proto_proto_count,
    Ast const &call, std::size_t call_arg_count);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallNoValidSignaturesError final : SemanticError {
  explicit SppFunctionCallNoValidSignaturesError(Ast const &call, StrView sigs, StrView attempted);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionCallOverloadAmbiguousError final : SemanticError {
  explicit SppFunctionCallOverloadAmbiguousError(Ast const &call, StrView sigs, StrView attempted);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessStaticOperatorExpectedError final : SemanticError {
  explicit SppMemberAccessStaticOperatorExpectedError(Ast const &lhs, Ast const &access, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMemberAccessRuntimeOperatorExpectedError final : SemanticError {
  explicit SppMemberAccessRuntimeOperatorExpectedError(Ast const &lhs, Ast const &access, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericTypeInvalidUsageError final : SemanticError {
  explicit SppGenericTypeInvalidUsageError(Ast const &gen_name, Ast const &gen_val, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAmbiguousMemberAccessError final : SemanticError {
  explicit SppAmbiguousMemberAccessError(Ast const &found_field_1, Ast const &found_field_2,
    Ast const &field_access);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCoroutineContainsReturnStatementError final : SemanticError {
  explicit SppCoroutineContainsReturnStatementError(Ast const &fun_tag, Ast const &ret_stmt);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionSubroutineMissingReturnStatementError final : SemanticError {
  explicit SppFunctionSubroutineMissingReturnStatementError(Ast const &final_member,
    Ast const &return_type_definition, Ast const &return_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionCyclicExtensionError final : SemanticError {
  explicit SppSuperimpositionCyclicExtensionError(Ast const &first_extension,
    Ast const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppTypeAliasCyclicError final : SemanticError {
  explicit SppTypeAliasCyclicError(Ast const &first_alias,
    Ast const &cyclic_alias);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionDoubleExtensionError final : SemanticError {
  explicit SppSuperimpositionDoubleExtensionError(Ast const &first_extension,
    Ast const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionSelfExtensionError final : SemanticError {
  explicit SppSuperimpositionSelfExtensionError(Ast const &first_extension,
    Ast const &second_extension);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionMethodInvalidError final : SemanticError {
  explicit SppSuperimpositionExtensionMethodInvalidError(Ast const &new_method,
    Ast const &super_class);
};

SPP_EXP_CLS struct
  spp::analyse::errors::SppSuperimpositionExtensionNonVirtualMethodOverriddenError final : SemanticError {
  explicit SppSuperimpositionExtensionNonVirtualMethodOverriddenError(Ast const &new_method,
    Ast const &base_method, Ast const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionOptionalGenericParameterError final : SemanticError {
  explicit SppSuperimpositionOptionalGenericParameterError(Ast const &param);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError final : SemanticError {
  explicit SppSuperimpositionUnconstrainedGenericParameterError(Ast const &param);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionTypeStatementInvalidError final : SemanticError {
  explicit SppSuperimpositionExtensionTypeStatementInvalidError(Ast const &stmt,
    Ast const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppSuperimpositionExtensionCmpStatementInvalidError final : SemanticError {
  explicit SppSuperimpositionExtensionCmpStatementInvalidError(Ast const &stmt,
    Ast const &super_class);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAsyncTargetNotFunctionCallError final : SemanticError {
  explicit SppAsyncTargetNotFunctionCallError(Ast const &async_op, Ast const &rhs);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAwaitTargetNotFutureError final : SemanticError {
  explicit SppAwaitTargetNotFutureError(Ast const &await_op, Ast const &lhs, Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidDefaultValueError final : SemanticError {
  explicit SppInvalidDefaultValueError(Ast const &default_val, StrView owner, StrView use_site);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDereferenceNonBorrowedTypeError final : SemanticError {
  explicit SppDereferenceNonBorrowedTypeError(Ast const &tok_deref, Ast const &expr,
    Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppNonCopyableTypeError final : SemanticError {
  explicit SppNonCopyableTypeError(Ast const &ctx, Ast const &expr, Ast const &type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterConflictError final : SemanticError {
  explicit SppGenericParameterConflictError(Ast const &param, Ast const &first_infer,
    Ast const &second_infer);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericParameterNotInferredError final : SemanticError {
  explicit SppGenericParameterNotInferredError(Ast const &param, Ast const &ctx);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericArgumentTooManyError final : SemanticError {
  explicit SppGenericArgumentTooManyError(Ast const &param, Ast const &owner,
    Ast const &arg);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMissingMainFunctionError final : SemanticError {
  explicit SppMissingMainFunctionError();
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidVoidValueError final : SemanticError {
  explicit SppInvalidVoidValueError(Ast const &expr, StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppBorrowLifetimeIncreaseError final : SemanticError {
  explicit SppBorrowLifetimeIncreaseError(Ast const &extension_ast, Ast const &lhs_init_definition,
    Ast const &rhs_borrow_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidComptimeOperationError final : SemanticError {
  // Todo: Check other comptime error: merge?
  explicit SppInvalidComptimeOperationError(Ast const &ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInternalCompilerError final : SemanticError {
  explicit SppInternalCompilerError(Ast const &ast, StrView message);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenericConstraintError final : SemanticError {
  explicit SppGenericConstraintError(Ast const &constraint, Ast const &concrete_type);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotAnAnnotationError final : SemanticError {
  explicit SppAnnotationTargetNotAnAnnotationError(Ast const &call_site, Ast const &target_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAnnotationTargetNotACmpFunctionError final : SemanticError {
  explicit SppAnnotationTargetNotACmpFunctionError(Ast const &annotation_marker,
    Ast const &non_function_ast);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCalledAnnotationAppliedToInvalidAstError final : SemanticError {
  explicit SppCalledAnnotationAppliedToInvalidAstError(Ast const &invalid_ast, Ast const &annotation_call,
    Ast const &annotation_definition);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUnitTestInvalidSignatureError final : SemanticError {
  explicit SppUnitTestInvalidSignatureError(Ast const &annotation, Ast const &fun_name,
    StrView requirement);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFfiGenericParameterError final : SemanticError {
  explicit SppFfiGenericParameterError(
    Ast const &annotation, Ast const &generic_parameter, StrView symbol);
};

SPP_EXP_CLS struct spp::analyse::errors::SppEmptyBodyRequiredError final : SemanticError {
  explicit SppEmptyBodyRequiredError(
    Ast const &annotation, Ast const &member, StrView what, StrView reason);
};

SPP_EXP_CLS struct spp::analyse::errors::SppUnitTestNotCallableError final : SemanticError {
  explicit SppUnitTestNotCallableError(Ast const &call_site, Ast const &annotation);
};

SPP_EXP_CLS struct spp::analyse::errors::SppInvalidBinaryFoldExpressionError final : SemanticError {
  explicit SppInvalidBinaryFoldExpressionError(Ast const &expr, Ast const &tup_type,
    std::size_t tup_num_elems);
};

SPP_EXP_CLS struct spp::analyse::errors::SppAccessViolationError final : SemanticError {
  explicit SppAccessViolationError(Ast const &access_site, Ast const &symbol_definition, StrView visibility,
    StrView what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFunctionOverloadVisibilityMismatchError final : SemanticError {
  explicit SppFunctionOverloadVisibilityMismatchError(Ast const &first_annotation,
    Ast const &conflicting_overload, Ast const &conflicting_annotation);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMovingEscapingBorrowedMemoryError final : SemanticError {
  explicit SppMovingEscapingBorrowedMemoryError(Ast const &container, Ast const &where_moved);
};

SPP_EXP_CLS struct spp::analyse::errors::SppMovingComptimeConstantMemoryError final : SemanticError {
  explicit SppMovingComptimeConstantMemoryError(Ast const &ast, Ast const &move_location);
};

SPP_EXP_CLS struct spp::analyse::errors::SppHigherOrderGenericsNotSupportedError final : SemanticError {
  explicit SppHigherOrderGenericsNotSupportedError(Ast const &ast, Ast const &generic_arg_group);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGeneratedCodeError final : SemanticError {
  explicit SppGeneratedCodeError(Ast const &ast, Str &&wrapped_error);
};

SPP_EXP_CLS struct spp::analyse::errors::SppCharLiteralOutOfBoundsError final : SemanticError {
  explicit SppCharLiteralOutOfBoundsError(Ast const &literal, std::uint32_t code_point);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLinearValueNotConsumedError final : SemanticError {
  explicit SppLinearValueNotConsumedError(Ast const &symbol_definition, Ast const &exit_point,
    StrView symbol_name, StrView type_name, StrView exit_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDiscardedValueError final : SemanticError {
  explicit SppDiscardedValueError(Ast const &expr, StrView type_name);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDeferTerminatesError final : SemanticError {
  explicit SppDeferTerminatesError(Ast const &tok_defer, Ast const &expr);
};

SPP_EXP_CLS struct spp::analyse::errors::SppFeatureNotYetSupportedError final : SemanticError {
  explicit SppFeatureNotYetSupportedError(NotYetSupportedFeature feature, Ast const &context, Ast const &site);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDeferConsumesMovedValueError final : SemanticError {
  explicit SppDeferConsumesMovedValueError(Ast const &deferred, Ast const &consumed_at,
    StrView symbol_name, StrView exit_what);
};

SPP_EXP_CLS struct spp::analyse::errors::SppDeferInCompileTimeFunctionError final : SemanticError {
  explicit SppDeferInCompileTimeFunctionError(Ast const &tok_defer);
};

SPP_EXP_CLS struct spp::analyse::errors::SppLinearValueSkippedInDestructureError final : SemanticError {
  explicit SppLinearValueSkippedInDestructureError(Ast const &skip, Ast const &destructure,
    StrView attr_name, StrView type_name);
};

SPP_EXP_CLS struct spp::analyse::errors::SppGenOnceFinishesWithoutYieldingError final : SemanticError {
  explicit SppGenOnceFinishesWithoutYieldingError(Ast const &ret_stmt);
};
