module;
#include <spp/macros.hpp>

export module spp.parse.parser_spp;
import spp.lex.tokens;
import spp.parse.parser_base;
import spp.utils.types;
import std;

use(spp::asts, struct AnnotationAst);
use(spp::asts, struct AssignmentStatementAst);
use(spp::asts, struct Ast);
use(spp::asts, struct BooleanLiteralAst);
use(spp::asts, struct ClassAttributeAst);
use(spp::asts, struct ClassMemberAst);
use(spp::asts, struct ClassImplementationAst);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct CmpStatementAst);
use(spp::asts, struct ConventionAst);
use(spp::asts, struct ConventionMutAst);
use(spp::asts, struct ConventionRefAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct FunctionImplementationAst);
use(spp::asts, struct CoroutinePrototypeAst);
use(spp::asts, struct SubroutinePrototypeAst);
use(spp::asts, struct FunctionCallArgumentAst);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionCallArgumentKeywordAst);
use(spp::asts, struct FunctionCallArgumentPositionalAst);
use(spp::asts, struct FunctionParameterAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct FunctionParameterSelfAst);
use(spp::asts, struct FunctionParameterRequiredAst);
use(spp::asts, struct FunctionParameterOptionalAst);
use(spp::asts, struct FunctionParameterVariadicAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct GenericParameterTypeConstraintsAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, template <typename T> struct InnerScopeAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct PatternGuardAst);
use(spp::asts, struct CasePatternVariantAst);
use(spp::asts, struct CasePatternVariantDestructureArrayAst);
use(spp::asts, struct CasePatternVariantDestructureObjectAst);
use(spp::asts, struct CasePatternVariantDestructureTupleAst);
use(spp::asts, struct CasePatternVariantDestructureSkipSingleArgumentAst);
use(spp::asts, struct CasePatternVariantDestructureSkipMultipleArgumentsAst);
use(spp::asts, struct CasePatternVariantDestructureAttributeBindingAst);
use(spp::asts, struct CasePatternVariantSingleIdentifierAst);
use(spp::asts, struct CasePatternVariantElseAst);
use(spp::asts, struct CasePatternVariantElseCaseAst);
use(spp::asts, struct CasePatternVariantExpressionAst);
use(spp::asts, struct CasePatternVariantLiteralAst);
use(spp::asts, struct CharLiteralAst);
use(spp::asts, struct PrimaryExpressionAst);
use(spp::asts, struct CaseExpressionAst);
use(spp::asts, struct CaseExpressionBranchAst);
use(spp::asts, struct LoopConditionalExpressionAst);
use(spp::asts, struct LoopExpressionAst);
use(spp::asts, struct LoopIterableExpressionAst);
use(spp::asts, struct LoopControlFlowStatementAst);
use(spp::asts, struct LoopElseStatementAst);
use(spp::asts, struct GenExpressionAst);
use(spp::asts, struct GenWithExpressionAst);
use(spp::asts, struct FoldExpressionAst);
use(spp::asts, struct LiteralAst);
use(spp::asts, struct FloatLiteralAst);
use(spp::asts, struct IntegerLiteralAst);
use(spp::asts, struct ArrayLiteralAst);
use(spp::asts, struct ArrayLiteralRepeatedElementAst);
use(spp::asts, struct ArrayLiteralExplicitElementsAst);
use(spp::asts, struct StringLiteralAst);
use(spp::asts, struct TupleLiteralAst);
use(spp::asts, struct BinaryExpressionAst);
use(spp::asts, struct IsExpressionAst);
use(spp::asts, struct ClosureExpressionAst);
use(spp::asts, struct ClosureExpressionCaptureAst);
use(spp::asts, struct ClosureExpressionCaptureGroupAst);
use(spp::asts, struct ClosureExpressionParameterAndCaptureGroupAst);
use(spp::asts, using ClosureExpressionParameterAst = FunctionParameterAst);
use(spp::asts, using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst);
use(spp::asts, struct LetStatementAst);
use(spp::asts, struct LetStatementInitializedAst);
use(spp::asts, struct LetStatementUninitializedAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct LocalVariableDestructureAttributeBindingAst);
use(spp::asts, struct LocalVariableDestructureArrayAst);
use(spp::asts, struct LocalVariableDestructureObjectAst);
use(spp::asts, struct LocalVariableDestructureTupleAst);
use(spp::asts, struct LocalVariableDestructureSkipSingleArgumentAst);
use(spp::asts, struct LocalVariableDestructureSkipMultipleArgumentsAst);
use(spp::asts, struct LocalVariableSingleIdentifierAst);
use(spp::asts, struct LocalVariableSingleIdentifierAliasAst);
use(spp::asts, struct ModuleImplementationAst);
use(spp::asts, struct ModulePrototypeAst);
use(spp::asts, struct ModuleMemberAst);
use(spp::asts, struct ObjectInitializerAst);
use(spp::asts, struct ObjectInitializerArgumentAst);
use(spp::asts, struct ObjectInitializerArgumentGroupAst);
use(spp::asts, struct ObjectInitializerArgumentKeywordAst);
use(spp::asts, struct ObjectInitializerArgumentShorthandAst);
use(spp::asts, struct PostfixExpressionAst);
use(spp::asts, struct PostfixExpressionOperatorAst);
use(spp::asts, struct PostfixExpressionOperatorEarlyReturnAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);
use(spp::asts, struct PostfixExpressionOperatorIndexAst);
use(spp::asts, struct PostfixExpressionOperatorKeywordNotAst);
use(spp::asts, struct PostfixExpressionOperatorKeywordAwaitAst);
use(spp::asts, struct PostfixExpressionOperatorKeywordResAst);
use(spp::asts, struct PostfixExpressionOperatorRuntimeMemberAccessAst);
use(spp::asts, struct PostfixExpressionOperatorSliceAst);
use(spp::asts, struct PostfixExpressionOperatorStaticMemberAccessAst);
use(spp::asts, struct UnaryExpressionAst);
use(spp::asts, struct UnaryExpressionOperatorAst);
use(spp::asts, struct UnaryExpressionOperatorAsyncAst);
use(spp::asts, struct PostfixExpressionOperatorDerefAst);
use(spp::asts, struct ParenthesisedExpressionAst);
use(spp::asts, struct DeferStatementAst);
use(spp::asts, struct RetStatementAst);
use(spp::asts, struct StatementAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeBinaryExpressionAst);
use(spp::asts, struct TypePostfixExpressionAst);
use(spp::asts, struct TypeUnaryExpressionAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypePostfixExpressionOperatorAst);
use(spp::asts, struct TypePostfixExpressionOperatorNestedTypeAst);
use(spp::asts, struct TypeUnaryExpressionOperatorAst);
use(spp::asts, struct TypeUnaryExpressionOperatorBorrowAst);
use(spp::asts, struct TypeUnaryExpressionOperatorNamespaceAst);
use(spp::asts, struct TypeParenthesisedExpressionAst);
use(spp::asts, struct TypeArrayShorthandAst);
use(spp::asts, struct TypeTupleShorthandAst);
use(spp::asts, struct UseStatementAst);
use(spp::asts, struct UseStatementVariableAst);
use(spp::asts, struct TypeStatementAst);
use(spp::asts, struct SupPrototypeFunctionsAst);
use(spp::asts, struct SupPrototypeExtensionAst);
use(spp::asts, struct SupImplementationAst);
use(spp::asts, struct SupMemberAst);
use(spp::asts, using FunctionMemberAst = StatementAst);

use(spp::parse, class ParserSpp);

SPP_EXP_CLS class spp::parse::ParserSpp final : public ParserBase {
public:
  using ParserBase::ParserBase;
  ~ParserSpp() override = default;

  auto Parse() -> Unique<ModulePrototypeAst>;
  auto ParseRoot() -> Unique<ModulePrototypeAst>;
  auto ParseEof() -> Unique<TokenAst>;

  auto ParseModulePrototype() -> Unique<ModulePrototypeAst>;
  auto ParseModuleImplementation() -> Unique<ModuleImplementationAst>;
  auto ParseModuleMember() -> Unique<Ast>;

  auto ParseClassPrototype() -> Unique<ClassPrototypeAst>;
  auto ParseClassImplementation() -> Unique<ClassImplementationAst>;
  auto ParseClassMember() -> Unique<Ast>;
  auto ParseClassAttribute() -> Unique<ClassAttributeAst>;
  auto ParseClassAttributeDefaultValue() -> Unique<ExpressionAst>;

  auto ParseSupPrototypeFunctions() -> Unique<SupPrototypeFunctionsAst>;
  auto ParseSupPrototypeExtension() -> Unique<SupPrototypeExtensionAst>;
  auto ParseSupImplementation() -> Unique<SupImplementationAst>;
  auto ParseSupMember() -> Unique<Ast>;
  auto ParseSupTypeStatement() -> Unique<TypeStatementAst>;
  auto ParseSupCmpStatement() -> Unique<CmpStatementAst>;

  auto ParseFunctionPrototype() -> Unique<FunctionPrototypeAst>;
  auto ParseSubroutinePrototype() -> Unique<SubroutinePrototypeAst>;
  auto ParseCoroutinePrototype() -> Unique<CoroutinePrototypeAst>;
  auto ParseFunctionImplementation() -> Unique<FunctionImplementationAst>;
  auto ParseFunctionMember() -> Unique<StatementAst>;
  auto ParseFunctionParameterGroup() -> Unique<FunctionParameterGroupAst>;
  auto ParseFunctionParameter() -> Unique<FunctionParameterAst>;
  auto ParseFunctionParameterSelf() -> Unique<FunctionParameterSelfAst>;
  auto ParseFunctionParameterSelfWithConvention() -> Unique<FunctionParameterSelfAst>;
  auto ParseFunctionParameterSelfWithoutConvention() -> Unique<FunctionParameterSelfAst>;
  auto ParseFunctionParameterRequired() -> Unique<FunctionParameterRequiredAst>;
  auto ParseFunctionParameterOptional() -> Unique<FunctionParameterOptionalAst>;
  auto ParseFunctionParameterVariadic() -> Unique<FunctionParameterVariadicAst>;

  auto ParseFunctionCallArgumentGroup() -> Unique<FunctionCallArgumentGroupAst>;
  auto ParseFunctionCallArgument() -> Unique<FunctionCallArgumentAst>;
  auto ParseFunctionCallArgumentKeyword() -> Unique<FunctionCallArgumentKeywordAst>;
  auto ParseFunctionCallArgumentPositional() -> Unique<FunctionCallArgumentPositionalAst>;

  auto ParseGenericParameterGroup() -> Unique<GenericParameterGroupAst>;
  auto ParseGenericParameter() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterComp() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterCompRequired() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterCompOptional() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterCompVariadic() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterType() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterTypeRequired() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterTypeOptional() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterTypeVariadic() -> Unique<GenericParameterAst>;
  auto ParseGenericParameterTypeConstraints() -> Unique<GenericParameterTypeConstraintsAst>;

  auto ParseGenericArgumentGroup() -> Unique<GenericArgumentGroupAst>;
  auto ParseGenericArgument() -> Unique<GenericArgumentAst>;
  auto ParseGenericArgumentComp() -> Unique<GenericArgumentAst>;
  auto ParseGenericArgumentCompPositional() -> Unique<GenericArgumentAst>;
  auto ParseGenericArgumentCompKeyword() -> Unique<GenericArgumentAst>;
  auto ParseGenericArgumentType() -> Unique<GenericArgumentAst>;
  auto ParseGenericArgumentTypePositional() -> Unique<GenericArgumentAst>;
  auto ParseGenericArgumentTypeKeyword() -> Unique<GenericArgumentAst>;

  auto ParseAnnotation() -> Unique<AnnotationAst>;
  auto ParseAnnotationNoCall() -> Unique<AnnotationAst>;
  auto ParseAnnotationCall() -> Unique<AnnotationAst>;

  auto ParseExpression() -> Unique<ExpressionAst>;

  auto ParseBinaryExpression(std::uint8_t min_prec = 0) -> Unique<ExpressionAst>;

  auto ParseUnaryExpression() -> Unique<ExpressionAst>;
  auto ParseUnaryExpressionOp() -> Unique<UnaryExpressionOperatorAst>;
  auto ParseUnaryExpressionOpAsync() -> Unique<UnaryExpressionOperatorAsyncAst>;

  auto ParsePostfixExpression() -> Unique<ExpressionAst>;
  auto ParsePostfixExpressionOp() -> Unique<PostfixExpressionOperatorAst>;
  auto ParsePostfixExpressionOpDeref() -> Unique<PostfixExpressionOperatorDerefAst>;
  auto ParsePostfixExpressionOpEarlyReturn() -> Unique<PostfixExpressionOperatorEarlyReturnAst>;
  auto ParsePostfixExpressionOpFunctionCall() -> Unique<PostfixExpressionOperatorFunctionCallAst>;
  auto ParsePostfixExpressionOpRuntimeMemberAccess()
    -> Unique<PostfixExpressionOperatorRuntimeMemberAccessAst>;
  auto ParsePostfixExpressionOpStaticMemberAccess()
    -> Unique<PostfixExpressionOperatorStaticMemberAccessAst>;
  auto ParsePostfixExpressionOpKeywordNot() -> Unique<PostfixExpressionOperatorKeywordNotAst>;
  auto ParsePostfixExpressionOpKeywordAwait() -> Unique<PostfixExpressionOperatorKeywordAwaitAst>;

  auto ParsePostfixExpressionOpKeywordRes() -> Unique<PostfixExpressionOperatorKeywordResAst>;
  auto ParsePostfixExpressionOpIndex() -> Unique<PostfixExpressionOperatorIndexAst>;
  auto ParsePostfixExpressionOpSlice() -> Unique<PostfixExpressionOperatorSliceAst>;
  auto ParsePostfixExpressionStrictlyStaticAccessZero() -> Unique<ExpressionAst>;
  auto ParsePostfixExpressionStrictlyStaticAccessOne() -> Unique<ExpressionAst>;

  auto ParsePrimaryExpression() -> Unique<ExpressionAst>;

  auto ParseParenthesisedExpression() -> Unique<ParenthesisedExpressionAst>;

  auto ParseFoldExpression() -> Unique<FoldExpressionAst>;

  auto ParseCaseExpression() -> Unique<CaseExpressionAst>;
  auto ParseCaseExpressionBranch() -> Unique<CaseExpressionBranchAst>;
  auto ParseCaseExpressionBranchElse() -> Unique<CaseExpressionBranchAst>;
  auto ParseCaseExpressionBranchElseCase() -> Unique<CaseExpressionBranchAst>;

  auto ParseCaseOfExpression() -> Unique<CaseExpressionAst>;
  auto ParseCaseOfExpressionBranch() -> Unique<CaseExpressionBranchAst>;
  auto ParseCaseOfExpressionBranchDestructuring() -> Unique<CaseExpressionBranchAst>;
  auto ParseCaseOfExpressionBranchComparing() -> Unique<CaseExpressionBranchAst>;

  auto ParseCaseExpressionPatternVariantDestructure() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantDestructureArray() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantDestructureObject() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantDestructureTuple() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantDestructureSkipSingleArgument() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantDestructureSkipMultipleArguments()
    -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantDestructureAttributeBinding() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantSingleIdentifier() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantSingleIdentifierAliasable() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantSingleIdentifierWithConvention() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantSingleIdentifierWithoutConvention()
    -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantLiteral() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantExpression() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantElse() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternVariantElseCase() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternNestedForDestructureArray() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternNestedForDestructureObject() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternNestedForDestructureTuple() -> Unique<CasePatternVariantAst>;
  auto ParseCaseExpressionPatternNestedForDestructureAttributeBinding() -> Unique<CasePatternVariantAst>;

  auto ParsePatternGuard() -> Unique<PatternGuardAst>;
  auto ParseBooleanComparisonOp() -> Unique<TokenAst>;

  auto ParseLoopExpression() -> Unique<LoopExpressionAst>;
  auto ParseLoopConditionalExpression() -> Unique<LoopConditionalExpressionAst>;
  auto ParseLoopIterableExpression() -> Unique<LoopIterableExpressionAst>;
  auto ParseLoopElseStatement() -> Unique<LoopElseStatementAst>;

  auto ParseGenExpression() -> Unique<GenExpressionAst>;
  auto ParseGenExpressionWithExpression() -> Unique<GenExpressionAst>;
  auto ParseGenExpressionWithoutExpression() -> Unique<GenExpressionAst>;
  auto ParseGenUnrollExpression() -> Unique<GenWithExpressionAst>;

  auto ParseInnerScopeExpression(auto &&parser) -> Unique<InnerScopeExpressionAst>;
  auto ParseInnerScope(auto &&parser) -> Unique<InnerScopeAst<decltype(parser())>>;

  auto ParseStatement() -> Unique<StatementAst>;
  auto ParseAssignmentStatement() -> Unique<AssignmentStatementAst>;
  auto ParseAssignmentTarget() -> Unique<ExpressionAst>;
  auto ParseAssignmentTargetPostfixExpression() -> Unique<ExpressionAst>;
  auto ParseAssignmentTargetPostfixExpressionOp() -> Unique<PostfixExpressionOperatorAst>;
  auto ParseAssignmentTargetPrimaryExpression() -> Unique<ExpressionAst>;

  auto ParseRetStatement() -> Unique<RetStatementAst>;
  auto ParseDeferStatement() -> Unique<DeferStatementAst>;
  auto ParseExitStatement() -> Unique<LoopControlFlowStatementAst>;
  auto ParseExitStatementWithValue() -> Unique<LoopControlFlowStatementAst>;
  auto ParseSkipStatement() -> Unique<LoopControlFlowStatementAst>;
  auto ParseUseStatement() -> Unique<UseStatementAst>;
  auto ParseUseVarStatement() -> Unique<UseStatementVariableAst>;
  auto ParseTypeStatement() -> Unique<TypeStatementAst>;
  auto ParseCmpStatement() -> Unique<CmpStatementAst>;
  auto ParseLetStatement() -> Unique<LetStatementAst>;
  auto ParseLetStatementInitialized() -> Unique<LetStatementAst>;
  auto ParseLetStatementInitializedExplicitType() -> Unique<TypeAst>;
  auto ParseLetStatementUninitialized() -> Unique<LetStatementAst>;

  auto ParseGlobalUseStatement() -> Unique<UseStatementAst>;
  auto ParseGlobalUseVarStatement() -> Unique<UseStatementVariableAst>;
  auto ParseGlobalTypeStatement() -> Unique<TypeStatementAst>;
  auto ParseGlobalCmpStatement() -> Unique<CmpStatementAst>;

  auto ParseLocalVariable() -> Unique<LocalVariableAst>;
  auto ParseLocalVariableDestructureArray() -> Unique<LocalVariableDestructureArrayAst>;
  auto ParseLocalVariableDestructureObject() -> Unique<LocalVariableDestructureObjectAst>;
  auto ParseLocalVariableDestructureTuple() -> Unique<LocalVariableDestructureTupleAst>;
  auto ParseLocalVariableDestructureSkipSingleArgument()
    -> Unique<LocalVariableDestructureSkipSingleArgumentAst>;
  auto ParseLocalVariableDestructureSkipMultipleArguments()
    -> Unique<LocalVariableDestructureSkipMultipleArgumentsAst>;
  auto ParseLocalVariableDestructureAttributeBinding()
    -> Unique<LocalVariableDestructureAttributeBindingAst>;
  auto ParseLocalVariableSingleIdentifier() -> Unique<LocalVariableSingleIdentifierAst>;
  auto ParseLocalVariableSingleIdentifierAliasable() -> Unique<LocalVariableSingleIdentifierAst>;
  auto ParseLocalVariableSingleIdentifierAlias() -> Unique<LocalVariableSingleIdentifierAliasAst>;
  auto ParseLocalVariableNestedForDestructureArray() -> Unique<LocalVariableAst>;
  auto ParseLocalVariableNestedForDestructureObject() -> Unique<LocalVariableAst>;
  auto ParseLocalVariableNestedForDestructureTuple() -> Unique<LocalVariableAst>;
  auto ParseLocalVariableNestedForDestructureAttributeBinding() -> Unique<LocalVariableAst>;

  auto ParseConvention() -> Unique<ConventionAst>;
  auto ParseConventionRef() -> Unique<ConventionRefAst>;
  auto ParseConventionMut() -> Unique<ConventionMutAst>;

  auto ParseObjectInitializer() -> Unique<ObjectInitializerAst>;
  auto ParseObjectInitializerArgumentGroup() -> Unique<ObjectInitializerArgumentGroupAst>;
  auto ParseObjectInitializerArgument() -> Unique<ObjectInitializerArgumentAst>;
  auto ParseObjectInitializerArgumentKeyword() -> Unique<ObjectInitializerArgumentKeywordAst>;
  auto ParseObjectInitializerArgumentShorthand() -> Unique<ObjectInitializerArgumentShorthandAst>;

  auto ParseClosureExpression() -> Unique<ClosureExpressionAst>;

  auto ParseClosureExpressionWithReturnType() -> Unique<ClosureExpressionAst>;

  auto ParseClosureExpressionWithoutReturnType() -> Unique<ClosureExpressionAst>;
  auto ParseClosureExpressionCaptureGroup() -> Unique<ClosureExpressionCaptureGroupAst>;
  auto ParseClosureExpressionCapture() -> Unique<ClosureExpressionCaptureAst>;
  auto ParseClosureExpressionParameterAndCaptureGroup()
    -> Unique<ClosureExpressionParameterAndCaptureGroupAst>;
  auto ParseClosureExpressionParameterGroup() -> Unique<ClosureExpressionParameterGroupAst>;
  auto ParseClosureExpressionParameter() -> Unique<ClosureExpressionParameterAst>;

  auto ParseTypeExpression() -> Unique<TypeAst>;

  auto ParseBinaryTypeExpression(std::uint8_t min_prec = 0) -> Unique<TypeAst>;

  auto ParseUnaryTypeExpression() -> Unique<TypeAst>;
  auto ParseUnaryTypeExpressionOp() -> Unique<TypeUnaryExpressionOperatorAst>;
  auto ParseUnaryTypeExpressionOpBorrow() -> Unique<TypeUnaryExpressionOperatorBorrowAst>;
  auto ParseUnaryTypeExpressionOpNamespace() -> Unique<TypeUnaryExpressionOperatorNamespaceAst>;

  auto ParsePostfixTypeExpression() -> Unique<TypeAst>;
  auto ParsePostfixTypeExpressionOp() -> Unique<TypePostfixExpressionOperatorAst>;
  auto ParsePostfixTypeExpressionOpNested() -> Unique<TypePostfixExpressionOperatorNestedTypeAst>;

  auto ParseTypeParenthesisedExpression() -> Unique<TypeAst>;
  auto ParseTypeNever() -> Unique<TypeAst>;

  auto ParseTypeExpressionSimple() -> Unique<TypeAst>;
  auto ParsePostfixTypeExpressionSimple() -> Unique<TypeAst>;
  auto ParseUnaryTypeExpressionSimple() -> Unique<TypeAst>;

  auto ParseTypeIdentifier() -> Unique<TypeIdentifierAst>;

  auto ParseTypeArray() -> Unique<TypeAst>;
  auto ParseTypeTuple() -> Unique<TypeAst>;
  auto ParseTypeTuple0Types() -> Unique<TypeAst>;
  auto ParseTypeTuple1Types() -> Unique<TypeAst>;
  auto ParseTypeTupleNTypes() -> Unique<TypeAst>;

  auto ParseIdentifier() -> Unique<IdentifierAst>;
  auto ParseNumericIdentifier() -> Unique<IdentifierAst>;
  auto ParseSelfIdentifier() -> Unique<IdentifierAst>;
  auto ParseUpperIdentifier() -> Unique<IdentifierAst>;
  auto ParseIdentifierAsExpression() -> Unique<ExpressionAst>;

  auto ParseLiteral() -> Unique<LiteralAst>;
  auto ParseLiteralChar() -> Unique<CharLiteralAst>;
  auto ParseLiteralString() -> Unique<StringLiteralAst>;
  auto ParseLiteralFloat() -> Unique<FloatLiteralAst>;
  auto ParseLiteralInteger() -> Unique<IntegerLiteralAst>;
  auto ParseLiteralBoolean() -> Unique<BooleanLiteralAst>;
  auto ParseLiteralTuple(std::function<Unique<ExpressionAst>()> &&elem_parser) -> Unique<TupleLiteralAst>;
  auto ParseLiteralArray(std::function<Unique<ExpressionAst>()> &&elem_parser) -> Unique<ArrayLiteralAst>;

  auto ParseLiteralFloatB10() -> Unique<FloatLiteralAst>;
  auto ParseLiteralIntegerB10() -> Unique<IntegerLiteralAst>;
  auto ParseLiteralIntegerB02() -> Unique<IntegerLiteralAst>;
  auto ParseLiteralIntegerB08() -> Unique<IntegerLiteralAst>;
  auto ParseLiteralIntegerB16() -> Unique<IntegerLiteralAst>;
  auto ParseNumericPrefixOp() -> Unique<TokenAst>;
  auto ParseFloatSuffixType() -> Unique<TokenAst>;
  auto ParseIntegerSuffixType() -> Unique<TokenAst>;
  auto ParseBytePrefixType() -> Unique<TokenAst>;

  auto ParseLiteralTuple1Element(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<TupleLiteralAst>;

  auto ParseLiteralTupleNElements(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<TupleLiteralAst>;

  auto ParseLiteralArrayRepeatedElement(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<ArrayLiteralRepeatedElementAst>;

  auto ParseLiteralArrayExplicitElements(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<ArrayLiteralExplicitElementsAst>;

  auto ParseSpecificCharacters(
    Str &&s)
    -> Unique<TokenAst>;

  auto ParseSpecificCharacter(
    char16_t c)
    -> Unique<TokenAst>;

  auto ParseLexemeCharacter() -> Unique<TokenAst>;
  auto ParseLexemeDigit() -> Unique<TokenAst>;
  auto ParseLexemeCharacterOrDigit() -> Unique<TokenAst>;
  auto ParseLexemeCharacterOrDigitOrUnderscore() -> Unique<TokenAst>;
  auto ParseLexemeBinInteger() -> Unique<TokenAst>;
  auto ParseLexemeOctInteger() -> Unique<TokenAst>;
  auto ParseLexemeDecInteger() -> Unique<TokenAst>;
  auto ParseLexemeHexInteger() -> Unique<TokenAst>;
  auto ParseLexemeSingleQuoteChar() -> Unique<TokenAst>;
  auto ParseLexemeDoubleQuoteString() -> Unique<TokenAst>;
  auto ParseLexemeIdentifier() -> Unique<TokenAst>;
  auto ParseLexemeUpperIdentifier() -> Unique<TokenAst>;

  auto ParseNothing() -> Unique<TokenAst>;
  auto ParseNewline() -> Unique<TokenAst>;
  auto ParseSpace() -> Unique<TokenAst>;

  auto ParseTokenLeftCurlyBrace() -> Unique<TokenAst>;
  auto ParseTokenRightCurlyBrace() -> Unique<TokenAst>;
  auto ParseTokenLeftParenthesis() -> Unique<TokenAst>;
  auto ParseTokenRightParenthesis() -> Unique<TokenAst>;
  auto ParseTokenLeftSquareBracket() -> Unique<TokenAst>;
  auto ParseTokenRightSquareBracket() -> Unique<TokenAst>;
  auto ParseTokenColon() -> Unique<TokenAst>;
  auto ParseTokenComma() -> Unique<TokenAst>;
  auto ParseTokenAssign() -> Unique<TokenAst>;
  auto ParseTokenUnderscore() -> Unique<TokenAst>;
  auto ParseTokenLessThan() -> Unique<TokenAst>;
  auto ParseTokenGreaterThan() -> Unique<TokenAst>;
  auto ParseTokenAdd() -> Unique<TokenAst>;
  auto ParseTokenSub() -> Unique<TokenAst>;
  auto ParseTokenMul() -> Unique<TokenAst>;
  auto ParseTokenDiv() -> Unique<TokenAst>;
  auto ParseTokenRem() -> Unique<TokenAst>;
  auto ParseTokenBitIor() -> Unique<TokenAst>;
  auto ParseTokenBitXor() -> Unique<TokenAst>;
  auto ParseTokenBitAnd() -> Unique<TokenAst>;
  auto ParseTokenDot() -> Unique<TokenAst>;
  auto ParseTokenQuestionMark() -> Unique<TokenAst>;
  auto ParseTokenExclamationMark() -> Unique<TokenAst>;
  auto ParseTokenDeref() -> Unique<TokenAst>;
  auto ParseTokenBorrow() -> Unique<TokenAst>;
  auto ParseTokenSemicolon() -> Unique<TokenAst>;
  auto ParseTokenSingleQuote() -> Unique<TokenAst>;
  auto ParseTokenDoubleQuote() -> Unique<TokenAst>;
  auto ParseTokenDollar() -> Unique<TokenAst>;
  auto ParseTokenArrowRight() -> Unique<TokenAst>;
  auto ParseTokenDoubleDot() -> Unique<TokenAst>;
  auto ParseTokenDoubleColon() -> Unique<TokenAst>;
  auto ParseTokenEquals() -> Unique<TokenAst>;
  auto ParseTokenNotEquals() -> Unique<TokenAst>;
  auto ParseTokenLessThanEquals() -> Unique<TokenAst>;
  auto ParseTokenGreaterThanEquals() -> Unique<TokenAst>;
  auto ParseTokenAddAssign() -> Unique<TokenAst>;
  auto ParseTokenSubAssign() -> Unique<TokenAst>;
  auto ParseTokenMulAssign() -> Unique<TokenAst>;
  auto ParseTokenDivAssign() -> Unique<TokenAst>;
  auto ParseTokenRemAssign() -> Unique<TokenAst>;
  auto ParseTokenPow() -> Unique<TokenAst>;
  auto ParseTokenBitShl() -> Unique<TokenAst>;
  auto ParseTokenBitShr() -> Unique<TokenAst>;
  auto ParseTokenBitIorAssign() -> Unique<TokenAst>;
  auto ParseTokenBitXorAssign() -> Unique<TokenAst>;
  auto ParseTokenBitAndAssign() -> Unique<TokenAst>;
  auto ParseTokenPowAssign() -> Unique<TokenAst>;
  auto ParseTokenBitShlAssign() -> Unique<TokenAst>;
  auto ParseTokenBitShrAssign() -> Unique<TokenAst>;

  auto ParseKeywordCls() -> Unique<TokenAst>;
  auto ParseKeywordFun() -> Unique<TokenAst>;
  auto ParseKeywordCor() -> Unique<TokenAst>;
  auto ParseKeywordSup() -> Unique<TokenAst>;
  auto ParseKeywordExt() -> Unique<TokenAst>;
  auto ParseKeywordMut() -> Unique<TokenAst>;
  auto ParseKeywordUse() -> Unique<TokenAst>;
  auto ParseKeywordCmp() -> Unique<TokenAst>;
  auto ParseKeywordLet() -> Unique<TokenAst>;
  auto ParseKeywordType() -> Unique<TokenAst>;
  auto ParseKeywordSelf() -> Unique<TokenAst>;
  auto ParseKeywordCase() -> Unique<TokenAst>;
  auto ParseKeywordOf() -> Unique<TokenAst>;
  auto ParseKeywordLoop() -> Unique<TokenAst>;
  auto ParseKeywordIn() -> Unique<TokenAst>;
  auto ParseKeywordTo() -> Unique<TokenAst>;
  auto ParseKeywordElse() -> Unique<TokenAst>;
  auto ParseKeywordGen() -> Unique<TokenAst>;
  auto ParseKeywordWith() -> Unique<TokenAst>;
  auto ParseKeywordRet() -> Unique<TokenAst>;
  auto ParseKeywordExit() -> Unique<TokenAst>;
  auto ParseKeywordSkip() -> Unique<TokenAst>;
  auto ParseKeywordDefer() -> Unique<TokenAst>;
  auto ParseKeywordIs() -> Unique<TokenAst>;
  auto ParseKeywordAs() -> Unique<TokenAst>;
  auto ParseKeywordOr() -> Unique<TokenAst>;
  auto ParseKeywordAnd() -> Unique<TokenAst>;
  auto ParseKeywordNot() -> Unique<TokenAst>;
  auto ParseKeywordAsync() -> Unique<TokenAst>;
  auto ParseKeywordTrue() -> Unique<TokenAst>;
  auto ParseKeywordFalse() -> Unique<TokenAst>;
  auto ParseKeywordAwait() -> Unique<TokenAst>;

  auto ParseKeywordRes() -> Unique<TokenAst>;
  auto ParseKeywordCaps() -> Unique<TokenAst>;

  auto ParseTokenRaw(lex::RawTokenType tok, lex::SppTokenType mapped_tok) -> Unique<TokenAst>;

private:
  /// Store error information about a discovered failure in
  /// the parsing.
  auto _StoreError(std::size_t pos, Str &&err_str) const -> bool;

  /// Check if a line feed sits between the token just parsed
  /// and the next one. Tokens usually skip line feeds freely,
  /// so the postfix operators that open with a bracket use
  /// this to stay on the line of they apply to. A "(" or "["
  /// starting a line begins a statement of its own (tuple or
  /// array), and reading it as a call or index would swallow
  /// the statement into the one above it. "something\n[1]"
  /// should be identifier then array, not an index operation etc.
  auto _LineFeedAhead() const -> bool;
};
