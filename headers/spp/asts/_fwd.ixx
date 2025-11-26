module;
#include <spp/macros.hpp>

export module spp.asts._fwd;


/// @cond
export namespace spp::asts {
    SPP_EXP struct AnnotationAst;
    SPP_EXP struct AssignmentStatementAst;
    SPP_EXP struct Ast;
    SPP_EXP struct BooleanLiteralAst;
    SPP_EXP struct ClassAttributeAst;
    SPP_EXP struct ClassMemberAst;
    SPP_EXP struct ClassImplementationAst;
    SPP_EXP struct ClassPrototypeAst;
    SPP_EXP struct CmpStatementAst;
    SPP_EXP struct ConventionAst;
    SPP_EXP struct ConventionMovAst;
    SPP_EXP struct ConventionMutAst;
    SPP_EXP struct ConventionRefAst;
    SPP_EXP struct ExpressionAst;

    // Functions
    SPP_EXP struct FunctionPrototypeAst;
    SPP_EXP struct FunctionImplementationAst;
    SPP_EXP struct CoroutinePrototypeAst;
    SPP_EXP struct SubroutinePrototypeAst;

    // Function Call Arguments
    SPP_EXP struct FunctionCallArgumentAst;
    SPP_EXP struct FunctionCallArgumentGroupAst;
    SPP_EXP struct FunctionCallArgumentKeywordAst;
    SPP_EXP struct FunctionCallArgumentPositionalAst;

    // Function Parameters
    SPP_EXP struct FunctionParameterAst;
    SPP_EXP struct FunctionParameterGroupAst;
    SPP_EXP struct FunctionParameterSelfAst;
    SPP_EXP struct FunctionParameterRequiredAst;
    SPP_EXP struct FunctionParameterOptionalAst;
    SPP_EXP struct FunctionParameterVariadicAst;

    // Generic Arguments
    SPP_EXP struct GenericArgumentAst;
    SPP_EXP struct GenericArgumentGroupAst;
    SPP_EXP struct GenericArgumentCompAst;
    SPP_EXP struct GenericArgumentCompPositionalAst;
    SPP_EXP struct GenericArgumentCompKeywordAst;
    SPP_EXP struct GenericArgumentTypeAst;
    SPP_EXP struct GenericArgumentTypePositionalAst;
    SPP_EXP struct GenericArgumentTypeKeywordAst;

    // Generic Parameters
    SPP_EXP struct GenericParameterAst;
    SPP_EXP struct GenericParameterGroupAst;
    SPP_EXP struct GenericParameterCompAst;
    SPP_EXP struct GenericParameterCompRequiredAst;
    SPP_EXP struct GenericParameterCompOptionalAst;
    SPP_EXP struct GenericParameterCompVariadicAst;
    SPP_EXP struct GenericParameterTypeAst;
    SPP_EXP struct GenericParameterTypeRequiredAst;
    SPP_EXP struct GenericParameterTypeOptionalAst;
    SPP_EXP struct GenericParameterTypeVariadicAst;
    SPP_EXP struct GenericParameterTypeInlineConstraintsAst;

    SPP_EXP struct IdentifierAst;

    // Inner Scopes
    template <typename T>
    SPP_EXP struct InnerScopeAst;
    template <typename T>
    SPP_EXP struct InnerScopeExpressionAst;

    SPP_EXP struct PatternGuardAst;
    SPP_EXP struct CasePatternVariantAst;
    SPP_EXP struct CasePatternVariantDestructureArrayAst;
    SPP_EXP struct CasePatternVariantDestructureObjectAst;
    SPP_EXP struct CasePatternVariantDestructureTupleAst;
    SPP_EXP struct CasePatternVariantDestructureSkipSingleArgumentAst;
    SPP_EXP struct CasePatternVariantDestructureSkipMultipleArgumentsAst;
    SPP_EXP struct CasePatternVariantDestructureAttributeBindingAst;
    SPP_EXP struct CasePatternVariantSingleIdentifierAst;
    SPP_EXP struct CasePatternVariantElseAst;
    SPP_EXP struct CasePatternVariantElseCaseAst;
    SPP_EXP struct CasePatternVariantExpressionAst;
    SPP_EXP struct CasePatternVariantLiteralAst;

    // Primary Expressions
    SPP_EXP struct PrimaryExpressionAst;

    SPP_EXP struct CaseExpressionAst;
    SPP_EXP struct CaseExpressionBranchAst;

    SPP_EXP struct LoopExpressionAst;
    SPP_EXP struct LoopConditionAst;
    SPP_EXP struct LoopConditionBooleanAst;
    SPP_EXP struct LoopConditionIterableAst;
    SPP_EXP struct LoopControlFlowStatementAst;
    SPP_EXP struct LoopElseStatementAst;

    SPP_EXP struct GenExpressionAst;
    SPP_EXP struct GenWithExpressionAst;

    SPP_EXP struct FoldExpressionAst;

    SPP_EXP struct LiteralAst;
    SPP_EXP struct FloatLiteralAst;
    SPP_EXP struct IntegerLiteralAst;
    SPP_EXP struct ArrayLiteralAst;
    SPP_EXP struct ArrayLiteralRepeatedElementAst;
    SPP_EXP struct ArrayLiteralExplicitElementsAst;
    SPP_EXP struct StringLiteralAst;
    SPP_EXP struct TupleLiteralAst;

    SPP_EXP struct BinaryExpressionAst;
    SPP_EXP struct BinaryExpressionTempAst;
    SPP_EXP struct IsExpressionAst;
    SPP_EXP struct IsExpressionTempAst;
    SPP_EXP struct IterExpressionAst;
    SPP_EXP struct IterExpressionBranchAst;
    SPP_EXP struct IterPatternVariantAst;
    SPP_EXP struct IterPatternVariantElseAst;
    SPP_EXP struct IterPatternVariantExceptionAst;
    SPP_EXP struct IterPatternVariantExhaustedAst;
    SPP_EXP struct IterPatternVariantNoValueAst;
    SPP_EXP struct IterPatternVariantVariableAst;

    SPP_EXP struct ClosureExpressionAst;
    SPP_EXP struct ClosureExpressionCaptureAst;
    SPP_EXP struct ClosureExpressionCaptureGroupAst;
    SPP_EXP struct ClosureExpressionParameterAndCaptureGroupAst;
    using ClosureExpressionParameterAst = FunctionParameterAst;
    using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst;

    SPP_EXP struct LetStatementAst;
    SPP_EXP struct LetStatementInitializedAst;
    SPP_EXP struct LetStatementUninitializedAst;

    SPP_EXP struct LocalVariableAst;
    SPP_EXP struct LocalVariableDestructureAttributeBindingAst;
    SPP_EXP struct LocalVariableDestructureArrayAst;
    SPP_EXP struct LocalVariableDestructureObjectAst;
    SPP_EXP struct LocalVariableDestructureTupleAst;
    SPP_EXP struct LocalVariableDestructureSkipSingleArgumentAst;
    SPP_EXP struct LocalVariableDestructureSkipMultipleArgumentsAst;
    SPP_EXP struct LocalVariableSingleIdentifierAst;
    SPP_EXP struct LocalVariableSingleIdentifierAliasAst;

    SPP_EXP struct ModuleImplementationAst;
    SPP_EXP struct ModulePrototypeAst;
    SPP_EXP struct ModuleMemberAst;

    SPP_EXP struct ObjectInitializerAst;
    SPP_EXP struct ObjectInitializerArgumentAst;
    SPP_EXP struct ObjectInitializerArgumentGroupAst;
    SPP_EXP struct ObjectInitializerArgumentKeywordAst;
    SPP_EXP struct ObjectInitializerArgumentShorthandAst;

    SPP_EXP struct PostfixExpressionAst;
    SPP_EXP struct PostfixExpressionOperatorAst;
    SPP_EXP struct PostfixExpressionOperatorEarlyReturnAst;
    SPP_EXP struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP struct PostfixExpressionOperatorIndexAst;
    SPP_EXP struct PostfixExpressionOperatorKeywordNotAst;
    SPP_EXP struct PostfixExpressionOperatorKeywordResAst;
    SPP_EXP struct PostfixExpressionOperatorRuntimeMemberAccessAst;
    SPP_EXP struct PostfixExpressionOperatorStaticMemberAccessAst;

    SPP_EXP struct UnaryExpressionAst;
    SPP_EXP struct UnaryExpressionOperatorAst;
    SPP_EXP struct UnaryExpressionOperatorAsyncAst;
    SPP_EXP struct UnaryExpressionOperatorDerefAst;

    SPP_EXP struct ParenthesisedExpressionAst;

    SPP_EXP struct RetStatementAst;

    SPP_EXP struct StatementAst;
    SPP_EXP struct TokenAst;

    SPP_EXP struct TypeAst;
    SPP_EXP struct TypeBinaryExpressionAst;
    SPP_EXP struct TypeBinaryExpressionTempAst;
    SPP_EXP struct TypePostfixExpressionAst;
    SPP_EXP struct TypeUnaryExpressionAst;
    SPP_EXP struct TypeIdentifierAst;

    SPP_EXP struct TypePostfixExpressionOperatorAst;
    SPP_EXP struct TypePostfixExpressionOperatorNestedTypeAst;

    SPP_EXP struct TypeUnaryExpressionOperatorAst;
    SPP_EXP struct TypeUnaryExpressionOperatorBorrowAst;
    SPP_EXP struct TypeUnaryExpressionOperatorNamespaceAst;

    SPP_EXP struct TypeParenthesisedExpressionAst;
    SPP_EXP struct TypeArrayShorthandAst;
    SPP_EXP struct TypeTupleShorthandAst;

    SPP_EXP struct UseStatementAst;
    SPP_EXP struct TypeStatementAst;

    SPP_EXP struct SupPrototypeFunctionsAst;
    SPP_EXP struct SupPrototypeExtensionAst;
    SPP_EXP struct SupImplementationAst;
    SPP_EXP struct SupMemberAst;

    using FunctionMemberAst = StatementAst;
}
/// @endcond
