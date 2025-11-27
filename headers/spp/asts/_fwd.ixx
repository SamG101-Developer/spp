module;
#include <spp/macros.hpp>

export module spp.asts._fwd;


/// @cond
namespace spp::asts {
    SPP_EXP_CLS struct AnnotationAst;
    SPP_EXP_CLS struct AssignmentStatementAst;
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct BooleanLiteralAst;
    SPP_EXP_CLS struct ClassAttributeAst;
    SPP_EXP_CLS struct ClassMemberAst;
    SPP_EXP_CLS struct ClassImplementationAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct CmpStatementAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct ConventionMovAst;
    SPP_EXP_CLS struct ConventionMutAst;
    SPP_EXP_CLS struct ConventionRefAst;
    SPP_EXP_CLS struct ExpressionAst;

    // Functions
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct FunctionImplementationAst;
    SPP_EXP_CLS struct CoroutinePrototypeAst;
    SPP_EXP_CLS struct SubroutinePrototypeAst;

    // Function Call Arguments
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
    SPP_EXP_CLS struct FunctionCallArgumentPositionalAst;

    // Function Parameters
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionParameterSelfAst;
    SPP_EXP_CLS struct FunctionParameterRequiredAst;
    SPP_EXP_CLS struct FunctionParameterOptionalAst;
    SPP_EXP_CLS struct FunctionParameterVariadicAst;

    // Generic Arguments
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;

    // Generic Parameters
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct GenericParameterCompRequiredAst;
    SPP_EXP_CLS struct GenericParameterCompOptionalAst;
    SPP_EXP_CLS struct GenericParameterCompVariadicAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct GenericParameterTypeRequiredAst;
    SPP_EXP_CLS struct GenericParameterTypeOptionalAst;
    SPP_EXP_CLS struct GenericParameterTypeVariadicAst;
    SPP_EXP_CLS struct GenericParameterTypeInlineConstraintsAst;

    SPP_EXP_CLS struct IdentifierAst;

    // Inner Scopes
    SPP_EXP_CLS template <typename T>
    struct InnerScopeAst;
    SPP_EXP_CLS template <typename T>
    struct InnerScopeExpressionAst;

    SPP_EXP_CLS struct PatternGuardAst;
    SPP_EXP_CLS struct CasePatternVariantAst;
    SPP_EXP_CLS struct CasePatternVariantDestructureArrayAst;
    SPP_EXP_CLS struct CasePatternVariantDestructureObjectAst;
    SPP_EXP_CLS struct CasePatternVariantDestructureTupleAst;
    SPP_EXP_CLS struct CasePatternVariantDestructureSkipSingleArgumentAst;
    SPP_EXP_CLS struct CasePatternVariantDestructureSkipMultipleArgumentsAst;
    SPP_EXP_CLS struct CasePatternVariantDestructureAttributeBindingAst;
    SPP_EXP_CLS struct CasePatternVariantSingleIdentifierAst;
    SPP_EXP_CLS struct CasePatternVariantElseAst;
    SPP_EXP_CLS struct CasePatternVariantElseCaseAst;
    SPP_EXP_CLS struct CasePatternVariantExpressionAst;
    SPP_EXP_CLS struct CasePatternVariantLiteralAst;

    // Primary Expressions
    SPP_EXP_CLS struct PrimaryExpressionAst;

    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CaseExpressionBranchAst;

    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS struct LoopConditionAst;
    SPP_EXP_CLS struct LoopConditionBooleanAst;
    SPP_EXP_CLS struct LoopConditionIterableAst;
    SPP_EXP_CLS struct LoopControlFlowStatementAst;
    SPP_EXP_CLS struct LoopElseStatementAst;

    SPP_EXP_CLS struct GenExpressionAst;
    SPP_EXP_CLS struct GenWithExpressionAst;

    SPP_EXP_CLS struct FoldExpressionAst;

    SPP_EXP_CLS struct LiteralAst;
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct ArrayLiteralAst;
    SPP_EXP_CLS struct ArrayLiteralRepeatedElementAst;
    SPP_EXP_CLS struct ArrayLiteralExplicitElementsAst;
    SPP_EXP_CLS struct StringLiteralAst;
    SPP_EXP_CLS struct TupleLiteralAst;

    SPP_EXP_CLS struct BinaryExpressionAst;
    SPP_EXP_CLS struct BinaryExpressionTempAst;
    SPP_EXP_CLS struct IsExpressionAst;
    SPP_EXP_CLS struct IsExpressionTempAst;
    SPP_EXP_CLS struct IterExpressionAst;
    SPP_EXP_CLS struct IterExpressionBranchAst;
    SPP_EXP_CLS struct IterPatternVariantAst;
    SPP_EXP_CLS struct IterPatternVariantElseAst;
    SPP_EXP_CLS struct IterPatternVariantExceptionAst;
    SPP_EXP_CLS struct IterPatternVariantExhaustedAst;
    SPP_EXP_CLS struct IterPatternVariantNoValueAst;
    SPP_EXP_CLS struct IterPatternVariantVariableAst;

    SPP_EXP_CLS struct ClosureExpressionAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
    using ClosureExpressionParameterAst = FunctionParameterAst;
    using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst;

    SPP_EXP_CLS struct LetStatementAst;
    SPP_EXP_CLS struct LetStatementInitializedAst;
    SPP_EXP_CLS struct LetStatementUninitializedAst;

    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct LocalVariableDestructureAttributeBindingAst;
    SPP_EXP_CLS struct LocalVariableDestructureArrayAst;
    SPP_EXP_CLS struct LocalVariableDestructureObjectAst;
    SPP_EXP_CLS struct LocalVariableDestructureTupleAst;
    SPP_EXP_CLS struct LocalVariableDestructureSkipSingleArgumentAst;
    SPP_EXP_CLS struct LocalVariableDestructureSkipMultipleArgumentsAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAliasAst;

    SPP_EXP_CLS struct ModuleImplementationAst;
    SPP_EXP_CLS struct ModulePrototypeAst;
    SPP_EXP_CLS struct ModuleMemberAst;

    SPP_EXP_CLS struct ObjectInitializerAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentGroupAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentKeywordAst;
    SPP_EXP_CLS struct ObjectInitializerArgumentShorthandAst;

    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorEarlyReturnAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorIndexAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorKeywordNotAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorKeywordResAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorRuntimeMemberAccessAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorStaticMemberAccessAst;

    SPP_EXP_CLS struct UnaryExpressionAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAsyncAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorDerefAst;

    SPP_EXP_CLS struct ParenthesisedExpressionAst;

    SPP_EXP_CLS struct RetStatementAst;

    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;

    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeBinaryExpressionAst;
    SPP_EXP_CLS struct TypeBinaryExpressionTempAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
    SPP_EXP_CLS struct TypeIdentifierAst;

    SPP_EXP_CLS struct TypePostfixExpressionOperatorAst;
    SPP_EXP_CLS struct TypePostfixExpressionOperatorNestedTypeAst;

    SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
    SPP_EXP_CLS struct TypeUnaryExpressionOperatorNamespaceAst;

    SPP_EXP_CLS struct TypeParenthesisedExpressionAst;
    SPP_EXP_CLS struct TypeArrayShorthandAst;
    SPP_EXP_CLS struct TypeTupleShorthandAst;

    SPP_EXP_CLS struct UseStatementAst;
    SPP_EXP_CLS struct TypeStatementAst;

    SPP_EXP_CLS struct SupPrototypeFunctionsAst;
    SPP_EXP_CLS struct SupPrototypeExtensionAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupMemberAst;

    using FunctionMemberAst = StatementAst;
}
/// @endcond
