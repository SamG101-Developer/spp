#ifndef _FWD_HPP
#define _FWD_HPP

#include <memory>
#include <variant>
#include <vector>


namespace spp::asts {
    struct AnnotationAst;
    struct AssignmentStatementAst;
    struct Ast;
    struct BooleanLiteralAst;
    struct ClassAttributeAst;
    struct ClassMemberAst;
    struct ClassImplementationAst;
    struct ClassPrototypeAst;
    struct CmpStatementAst;
    struct ConventionAst;
    struct ConventionMutAst;
    struct ConventionRefAst;
    struct ExpressionAst;

    // Functions
    struct FunctionPrototypeAst;
    struct FunctionImplementationAst;
    struct CoroutinePrototypeAst;
    struct SubroutinePrototypeAst;

    // Function Call Arguments
    struct FunctionCallArgumentAst;
    struct FunctionCallArgumentGroupAst;
    struct FunctionCallArgumentKeywordAst;
    struct FunctionCallArgumentPositionalAst;

    // Function Parameters
    struct FunctionParameterAst;
    struct FunctionParameterGroupAst;
    struct FunctionParameterSelfAst;
    struct FunctionParameterRequiredAst;
    struct FunctionParameterOptionalAst;
    struct FunctionParameterVariadicAst;

    // Generic Arguments
    struct GenericArgumentAst;
    struct GenericArgumentGroupAst;
    struct GenericArgumentCompAst;
    struct GenericArgumentCompPositionalAst;
    struct GenericArgumentCompKeywordAst;
    struct GenericArgumentTypeAst;
    struct GenericArgumentTypePositionalAst;
    struct GenericArgumentTypeKeywordAst;

    // Generic Parameters
    struct GenericParameterAst;
    struct GenericParameterGroupAst;
    struct GenericParameterCompAst;
    struct GenericParameterCompRequiredAst;
    struct GenericParameterCompOptionalAst;
    struct GenericParameterCompVariadicAst;
    struct GenericParameterTypeAst;
    struct GenericParameterTypeRequiredAst;
    struct GenericParameterTypeOptionalAst;
    struct GenericParameterTypeVariadicAst;
    struct GenericParameterTypeInlineConstraintsAst;

    struct IdentifierAst;

    // Inner Scopes
    template <typename T>
    struct InnerScopeAst;
    template <typename T>
    struct InnerScopeExpressionAst;

    struct PatternGuardAst;
    struct CasePatternVariantAst;
    struct CasePatternVariantDestructureArrayAst;
    struct CasePatternVariantDestructureObjectAst;
    struct CasePatternVariantDestructureTupleAst;
    struct CasePatternVariantDestructureSkipSingleArgumentAst;
    struct CasePatternVariantDestructureSkipMultipleArgumentsAst;
    struct CasePatternVariantDestructureAttributeBindingAst;
    struct CasePatternVariantSingleIdentifierAst;
    struct CasePatternVariantElseAst;
    struct CasePatternVariantElseCaseAst;
    struct CasePatternVariantExpressionAst;
    struct CasePatternVariantLiteralAst;

    // Primary Expressions
    struct PrimaryExpressionAst;

    struct CaseExpressionAst;
    struct CaseExpressionBranchAst;

    struct LoopExpressionAst;
    struct LoopConditionAst;
    struct LoopConditionBooleanAst;
    struct LoopConditionIterableAst;
    struct LoopControlFlowStatementAst;
    struct LoopElseStatementAst;

    struct GenExpressionAst;
    struct GenWithExpressionAst;

    struct FoldExpressionAst;

    struct LiteralAst;
    struct FloatLiteralAst;
    struct IntegerLiteralAst;
    struct ArrayLiteralAst;
    struct ArrayLiteral0Elements;
    struct ArrayLiteralNElements;
    struct StringLiteralAst;
    struct TupleLiteralAst;

    struct BinaryExpressionAst;
    struct BinaryExpressionTempAst;
    struct IsExpressionAst;
    struct IsExpressionTempAst;
    struct IterExpressionAst;
    struct IterExpressionBranchAst;
    struct IterPatternVariantAst;
    struct IterPatternVariantExceptionAst;
    struct IterPatternVariantExhaustedAst;
    struct IterPatternVariantNoValueAst;
    struct IterPatternVariantVariableAst;

    struct ClosureExpressionAst;
    struct ClosureExpressionCaptureAst;
    struct ClosureExpressionCaptureGroupAst;
    struct ClosureExpressionParameterAndCaptureGroupAst;
    using ClosureExpressionParameterAst = FunctionParameterAst;

    struct LetStatementAst;
    struct LetStatementInitializedAst;
    struct LetStatementUninitializedAst;

    struct LocalVariableAst;
    struct LocalVariableDestructureAttributeBindingAst;
    struct LocalVariableDestructureArrayAst;
    struct LocalVariableDestructureObjectAst;
    struct LocalVariableDestructureTupleAst;
    struct LocalVariableDestructureSkipSingleArgumentAst;
    struct LocalVariableDestructureSkipMultipleArgumentsAst;
    struct LocalVariableSingleIdentifierAst;
    struct LocalVariableSingleIdentifierAliasAst;

    struct ModuleImplementationAst;
    struct ModulePrototypeAst;
    using ModuleMemberAst = Ast;

    struct ObjectInitializerAst;
    struct ObjectInitializerArgumentAst;
    struct ObjectInitializerArgumentGroupAst;
    struct ObjectInitializerArgumentKeywordAst;
    struct ObjectInitializerArgumentShorthandAst;

    struct PostfixExpressionAst;
    struct PostfixExpressionOperatorAst;
    struct PostfixExpressionOperatorEarlyReturnAst;
    struct PostfixExpressionOperatorFunctionCallAst;
    struct PostfixExpressionOperatorRuntimeMemberAccessAst;
    struct PostfixExpressionOperatorStaticMemberAccessAst;
    struct PostfixExpressionOperatorKeywordNotAst;
    struct PostfixExpressionOperatorKeywordResAst;

    struct UnaryExpressionAst;
    struct UnaryExpressionOperatorAst;
    struct UnaryExpressionOperatorAsyncAst;
    struct UnaryExpressionOperatorDerefAst;

    struct ParenthesisedExpressionAst;

    struct RetStatementAst;

    struct StatementAst;
    struct TokenAst;

    struct TypeAst;
    struct TypeBinaryExpressionAst;
    struct TypeBinaryExpressionTempAst;
    struct TypePostfixExpressionAst;
    struct TypeUnaryExpressionAst;
    struct TypeIdentifierAst;

    struct TypePostfixExpressionOperatorAst;
    struct TypePostfixExpressionOperatorNestedTypeAst;
    struct TypePostfixExpressionOperatorOptionalAst;

    struct TypeUnaryExpressionOperatorAst;
    struct TypeUnaryExpressionOperatorBorrowAst;
    struct TypeUnaryExpressionOperatorNamespaceAst;

    struct TypeParenthesisedExpressionAst;
    struct TypeArrayShorthandAst;
    struct TypeTupleShorthandAst;

    struct UseStatementAst;
    struct TypeStatementAst;

    struct SupPrototypeFunctionsAst;
    struct SupPrototypeExtensionAst;
    struct SupImplementationAst;
    using SupCmpStatementAst = CmpStatementAst;
    using SupTypeStatementAst = TypeStatementAst;
    using SupMethodPrototypeAst = FunctionPrototypeAst;
    using SupMemberAst = Ast;

    using FunctionMemberAst = StatementAst;
}

#endif //_FWD_HPP
