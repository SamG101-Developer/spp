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
use(spp::asts, struct GenericArgumentCompAst);
use(spp::asts, struct GenericArgumentCompPositionalAst);
use(spp::asts, struct GenericArgumentCompKeywordAst);
use(spp::asts, struct GenericArgumentTypeAst);
use(spp::asts, struct GenericArgumentTypePositionalAst);
use(spp::asts, struct GenericArgumentTypeKeywordAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct GenericParameterGroupAst);
use(spp::asts, struct GenericParameterCompAst);
use(spp::asts, struct GenericParameterCompRequiredAst);
use(spp::asts, struct GenericParameterCompOptionalAst);
use(spp::asts, struct GenericParameterCompVariadicAst);
use(spp::asts, struct GenericParameterTypeAst);
use(spp::asts, struct GenericParameterTypeRequiredAst);
use(spp::asts, struct GenericParameterTypeOptionalAst);
use(spp::asts, struct GenericParameterTypeVariadicAst);
use(spp::asts, struct GenericParameterTypeInlineConstraintsAst);
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

  auto parse() -> Unique<ModulePrototypeAst>;
  auto parse_root() -> Unique<ModulePrototypeAst>;
  auto parse_eof() -> Unique<TokenAst>;

  auto parse_module_prototype() -> Unique<ModulePrototypeAst>;
  auto parse_module_implementation() -> Unique<ModuleImplementationAst>;
  auto parse_module_member() -> Unique<Ast>;

  auto parse_class_prototype() -> Unique<ClassPrototypeAst>;
  auto parse_class_implementation() -> Unique<ClassImplementationAst>;
  auto parse_class_member() -> Unique<Ast>;
  auto parse_class_attribute() -> Unique<ClassAttributeAst>;
  auto parse_class_attribute_default_value() -> Unique<ExpressionAst>;

  auto parse_sup_prototype_functions() -> Unique<SupPrototypeFunctionsAst>;
  auto parse_sup_prototype_extension() -> Unique<SupPrototypeExtensionAst>;
  auto parse_sup_implementation() -> Unique<SupImplementationAst>;
  auto parse_sup_member() -> Unique<Ast>;
  auto parse_sup_type_statement() -> Unique<TypeStatementAst>;
  auto parse_sup_cmp_statement() -> Unique<CmpStatementAst>;

  auto parse_function_prototype() -> Unique<FunctionPrototypeAst>;
  auto parse_subroutine_prototype() -> Unique<SubroutinePrototypeAst>;
  auto parse_coroutine_prototype() -> Unique<CoroutinePrototypeAst>;
  auto parse_function_implementation() -> Unique<FunctionImplementationAst>;
  auto parse_function_member() -> Unique<StatementAst>;
  auto parse_function_parameter_group() -> Unique<FunctionParameterGroupAst>;
  auto parse_function_parameter() -> Unique<FunctionParameterAst>;
  auto parse_function_parameter_self() -> Unique<FunctionParameterSelfAst>;
  auto parse_function_parameter_self_with_convention() -> Unique<FunctionParameterSelfAst>;
  auto parse_function_parameter_self_without_convention() -> Unique<FunctionParameterSelfAst>;
  auto parse_function_parameter_required() -> Unique<FunctionParameterRequiredAst>;
  auto parse_function_parameter_optional() -> Unique<FunctionParameterOptionalAst>;
  auto parse_function_parameter_variadic() -> Unique<FunctionParameterVariadicAst>;

  auto parse_function_call_argument_group() -> Unique<FunctionCallArgumentGroupAst>;
  auto parse_function_call_argument() -> Unique<FunctionCallArgumentAst>;
  auto parse_function_call_argument_keyword() -> Unique<FunctionCallArgumentKeywordAst>;
  auto parse_function_call_argument_positional() -> Unique<FunctionCallArgumentPositionalAst>;

  auto parse_generic_parameter_group() -> Unique<GenericParameterGroupAst>;
  auto parse_generic_parameter() -> Unique<GenericParameterAst>;
  auto parse_generic_parameter_comp() -> Unique<GenericParameterCompAst>;
  auto parse_generic_parameter_comp_required() -> Unique<GenericParameterCompRequiredAst>;
  auto parse_generic_parameter_comp_optional() -> Unique<GenericParameterCompOptionalAst>;
  auto parse_generic_parameter_comp_variadic() -> Unique<GenericParameterCompVariadicAst>;
  auto parse_generic_parameter_type() -> Unique<GenericParameterTypeAst>;
  auto parse_generic_parameter_type_required() -> Unique<GenericParameterTypeRequiredAst>;
  auto parse_generic_parameter_type_optional() -> Unique<GenericParameterTypeOptionalAst>;
  auto parse_generic_parameter_type_variadic() -> Unique<GenericParameterTypeVariadicAst>;
  auto parse_generic_parameter_type_inline_constraints() -> Unique<GenericParameterTypeInlineConstraintsAst>;

  auto parse_generic_argument_group() -> Unique<GenericArgumentGroupAst>;
  auto parse_generic_argument() -> Unique<GenericArgumentAst>;
  auto parse_generic_argument_comp() -> Unique<GenericArgumentCompAst>;
  auto parse_generic_argument_comp_positional() -> Unique<GenericArgumentCompPositionalAst>;
  auto parse_generic_argument_comp_keyword() -> Unique<GenericArgumentCompKeywordAst>;
  auto parse_generic_argument_type() -> Unique<GenericArgumentTypeAst>;
  auto parse_generic_argument_type_positional() -> Unique<GenericArgumentTypePositionalAst>;
  auto parse_generic_argument_type_keyword() -> Unique<GenericArgumentTypeKeywordAst>;

  auto parse_annotation() -> Unique<AnnotationAst>;
  auto parse_annotation_no_call() -> Unique<AnnotationAst>;
  auto parse_annotation_call() -> Unique<AnnotationAst>;

  auto parse_expression() -> Unique<ExpressionAst>;

  auto parse_binary_expression(std::uint8_t min_prec = 0) -> Unique<ExpressionAst>;

  auto parse_unary_expression() -> Unique<ExpressionAst>;
  auto parse_unary_expression_op() -> Unique<UnaryExpressionOperatorAst>;
  auto parse_unary_expression_op_async() -> Unique<UnaryExpressionOperatorAsyncAst>;

  auto parse_postfix_expression() -> Unique<ExpressionAst>;
  auto parse_postfix_expression_op() -> Unique<PostfixExpressionOperatorAst>;
  auto parse_postfix_expression_op_deref() -> Unique<PostfixExpressionOperatorDerefAst>;
  auto parse_postfix_expression_op_early_return() -> Unique<PostfixExpressionOperatorEarlyReturnAst>;
  auto parse_postfix_expression_op_function_call() -> Unique<PostfixExpressionOperatorFunctionCallAst>;
  auto parse_postfix_expression_op_runtime_member_access()
    -> Unique<PostfixExpressionOperatorRuntimeMemberAccessAst>;
  auto parse_postfix_expression_op_static_member_access()
    -> Unique<PostfixExpressionOperatorStaticMemberAccessAst>;
  auto parse_postfix_expression_op_keyword_not() -> Unique<PostfixExpressionOperatorKeywordNotAst>;
  auto parse_postfix_expression_op_keyword_await() -> Unique<PostfixExpressionOperatorKeywordAwaitAst>;

  auto parse_postfix_expression_op_keyword_res() -> Unique<PostfixExpressionOperatorKeywordResAst>;
  auto parse_postfix_expression_op_index() -> Unique<PostfixExpressionOperatorIndexAst>;
  auto parse_postfix_expression_op_slice() -> Unique<PostfixExpressionOperatorSliceAst>;
  auto parse_postfix_expression_strictly_static_access_zero() -> Unique<ExpressionAst>;
  auto parse_postfix_expression_strictly_static_access_one() -> Unique<ExpressionAst>;

  auto parse_primary_expression() -> Unique<ExpressionAst>;

  auto parse_parenthesised_expression() -> Unique<ParenthesisedExpressionAst>;

  auto parse_fold_expression() -> Unique<FoldExpressionAst>;

  auto parse_case_expression() -> Unique<CaseExpressionAst>;
  auto parse_case_expression_branch() -> Unique<CaseExpressionBranchAst>;
  auto parse_case_expression_branch_else() -> Unique<CaseExpressionBranchAst>;
  auto parse_case_expression_branch_else_case() -> Unique<CaseExpressionBranchAst>;

  auto parse_case_of_expression() -> Unique<CaseExpressionAst>;
  auto parse_case_of_expression_branch() -> Unique<CaseExpressionBranchAst>;
  auto parse_case_of_expression_branch_destructuring() -> Unique<CaseExpressionBranchAst>;
  auto parse_case_of_expression_branch_comparing() -> Unique<CaseExpressionBranchAst>;

  auto parse_case_expression_pattern_variant_destructure() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_destructure_array() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_destructure_object() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_destructure_tuple() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_destructure_skip_single_argument() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_destructure_skip_multiple_arguments()
    -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_destructure_attribute_binding() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_single_identifier() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_single_identifier_aliasable() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_single_identifier_with_convention() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_single_identifier_without_convention()
    -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_literal() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_expression() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_else() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_variant_else_case() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_nested_for_destructure_array() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_nested_for_destructure_object() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_nested_for_destructure_tuple() -> Unique<CasePatternVariantAst>;
  auto parse_case_expression_pattern_nested_for_destructure_attribute_binding() -> Unique<CasePatternVariantAst>;

  auto parse_pattern_guard() -> Unique<PatternGuardAst>;
  auto parse_boolean_comparison_op() -> Unique<TokenAst>;

  auto parse_loop_expression() -> Unique<LoopExpressionAst>;
  auto parse_loop_conditional_expression() -> Unique<LoopConditionalExpressionAst>;
  auto parse_loop_iterable_expression() -> Unique<LoopIterableExpressionAst>;
  auto parse_loop_else_statement() -> Unique<LoopElseStatementAst>;

  auto parse_gen_expression() -> Unique<GenExpressionAst>;
  auto parse_gen_expression_with_expression() -> Unique<GenExpressionAst>;
  auto parse_gen_expression_without_expression() -> Unique<GenExpressionAst>;
  auto parse_gen_unroll_expression() -> Unique<GenWithExpressionAst>;

  auto parse_inner_scope_expression(auto &&parser) -> Unique<InnerScopeExpressionAst>;
  auto parse_inner_scope(auto &&parser) -> Unique<InnerScopeAst<decltype(parser())>>;

  auto parse_statement() -> Unique<StatementAst>;
  auto parse_assignment_statement() -> Unique<AssignmentStatementAst>;
  auto parse_assignment_target() -> Unique<ExpressionAst>;
  auto parse_assignment_target_postfix_expression() -> Unique<ExpressionAst>;
  auto parse_assignment_target_postfix_expression_op() -> Unique<PostfixExpressionOperatorAst>;
  auto parse_assignment_target_primary_expression() -> Unique<ExpressionAst>;

  auto parse_ret_statement() -> Unique<RetStatementAst>;
  auto parse_defer_statement() -> Unique<DeferStatementAst>;
  auto parse_exit_statement() -> Unique<LoopControlFlowStatementAst>;
  auto parse_exit_statement_with_value() -> Unique<LoopControlFlowStatementAst>;
  auto parse_skip_statement() -> Unique<LoopControlFlowStatementAst>;
  auto parse_use_statement() -> Unique<UseStatementAst>;
  auto parse_use_var_statement() -> Unique<UseStatementVariableAst>;
  auto parse_type_statement() -> Unique<TypeStatementAst>;
  auto parse_cmp_statement() -> Unique<CmpStatementAst>;
  auto parse_let_statement() -> Unique<LetStatementAst>;
  auto parse_let_statement_initialized() -> Unique<LetStatementAst>;
  auto parse_let_statement_initialized_explicit_type() -> Unique<TypeAst>;
  auto parse_let_statement_uninitialized() -> Unique<LetStatementAst>;

  auto parse_global_use_statement() -> Unique<UseStatementAst>;
  auto parse_global_use_var_statement() -> Unique<UseStatementVariableAst>;
  auto parse_global_type_statement() -> Unique<TypeStatementAst>;
  auto parse_global_cmp_statement() -> Unique<CmpStatementAst>;

  auto parse_local_variable() -> Unique<LocalVariableAst>;
  auto parse_local_variable_destructure_array() -> Unique<LocalVariableDestructureArrayAst>;
  auto parse_local_variable_destructure_object() -> Unique<LocalVariableDestructureObjectAst>;
  auto parse_local_variable_destructure_tuple() -> Unique<LocalVariableDestructureTupleAst>;
  auto parse_local_variable_destructure_skip_single_argument()
    -> Unique<LocalVariableDestructureSkipSingleArgumentAst>;
  auto parse_local_variable_destructure_skip_multiple_arguments()
    -> Unique<LocalVariableDestructureSkipMultipleArgumentsAst>;
  auto parse_local_variable_destructure_attribute_binding()
    -> Unique<LocalVariableDestructureAttributeBindingAst>;
  auto parse_local_variable_single_identifier() -> Unique<LocalVariableSingleIdentifierAst>;
  auto parse_local_variable_single_identifier_aliasable() -> Unique<LocalVariableSingleIdentifierAst>;
  auto parse_local_variable_single_identifier_alias() -> Unique<LocalVariableSingleIdentifierAliasAst>;
  auto parse_local_variable_nested_for_destructure_array() -> Unique<LocalVariableAst>;
  auto parse_local_variable_nested_for_destructure_object() -> Unique<LocalVariableAst>;
  auto parse_local_variable_nested_for_destructure_tuple() -> Unique<LocalVariableAst>;
  auto parse_local_variable_nested_for_destructure_attribute_binding() -> Unique<LocalVariableAst>;

  auto parse_convention() -> Unique<ConventionAst>;
  auto parse_convention_ref() -> Unique<ConventionRefAst>;
  auto parse_convention_mut() -> Unique<ConventionMutAst>;

  auto parse_object_initializer() -> Unique<ObjectInitializerAst>;
  auto parse_object_initializer_argument_group() -> Unique<ObjectInitializerArgumentGroupAst>;
  auto parse_object_initializer_argument() -> Unique<ObjectInitializerArgumentAst>;
  auto parse_object_initializer_argument_keyword() -> Unique<ObjectInitializerArgumentKeywordAst>;
  auto parse_object_initializer_argument_shorthand() -> Unique<ObjectInitializerArgumentShorthandAst>;

  auto parse_closure_expression() -> Unique<ClosureExpressionAst>;

  auto parse_closure_expression_with_return_type() -> Unique<ClosureExpressionAst>;

  auto parse_closure_expression_without_return_type() -> Unique<ClosureExpressionAst>;
  auto parse_closure_expression_capture_group() -> Unique<ClosureExpressionCaptureGroupAst>;
  auto parse_closure_expression_capture() -> Unique<ClosureExpressionCaptureAst>;
  auto parse_closure_expression_parameter_and_capture_group()
    -> Unique<ClosureExpressionParameterAndCaptureGroupAst>;
  auto parse_closure_expression_parameter_group() -> Unique<ClosureExpressionParameterGroupAst>;
  auto parse_closure_expression_parameter() -> Unique<ClosureExpressionParameterAst>;

  auto parse_type_expression() -> Unique<TypeAst>;

  auto parse_binary_type_expression(std::uint8_t min_prec = 0) -> Unique<TypeAst>;

  auto parse_unary_type_expression() -> Unique<TypeAst>;
  auto parse_unary_type_expression_op() -> Unique<TypeUnaryExpressionOperatorAst>;
  auto parse_unary_type_expression_op_borrow() -> Unique<TypeUnaryExpressionOperatorBorrowAst>;
  auto parse_unary_type_expression_op_namespace() -> Unique<TypeUnaryExpressionOperatorNamespaceAst>;

  auto parse_postfix_type_expression() -> Unique<TypeAst>;
  auto parse_postfix_type_expression_op() -> Unique<TypePostfixExpressionOperatorAst>;
  auto parse_postfix_type_expression_op_nested() -> Unique<TypePostfixExpressionOperatorNestedTypeAst>;

  auto parse_type_parenthesised_expression() -> Unique<TypeAst>;
  auto parse_type_never() -> Unique<TypeAst>;

  auto parse_type_expression_simple() -> Unique<TypeAst>;
  auto parse_postfix_type_expression_simple() -> Unique<TypeAst>;
  auto parse_unary_type_expression_simple() -> Unique<TypeAst>;

  auto parse_type_identifier() -> Unique<TypeIdentifierAst>;

  auto parse_type_array() -> Unique<TypeAst>;
  auto parse_type_tuple() -> Unique<TypeAst>;
  auto parse_type_tuple_0_types() -> Unique<TypeAst>;
  auto parse_type_tuple_1_types() -> Unique<TypeAst>;
  auto parse_type_tuple_n_types() -> Unique<TypeAst>;

  auto parse_identifier() -> Unique<IdentifierAst>;
  auto parse_numeric_identifier() -> Unique<IdentifierAst>;
  auto parse_self_identifier() -> Unique<IdentifierAst>;
  auto parse_upper_identifier() -> Unique<IdentifierAst>;
  auto parse_identifier_as_expression() -> Unique<ExpressionAst>;

  auto parse_literal() -> Unique<LiteralAst>;
  auto parse_literal_char() -> Unique<CharLiteralAst>;
  auto parse_literal_string() -> Unique<StringLiteralAst>;
  auto parse_literal_float() -> Unique<FloatLiteralAst>;
  auto parse_literal_integer() -> Unique<IntegerLiteralAst>;
  auto parse_literal_boolean() -> Unique<BooleanLiteralAst>;
  auto parse_literal_tuple(std::function<Unique<ExpressionAst>()> &&elem_parser) -> Unique<TupleLiteralAst>;
  auto parse_literal_array(std::function<Unique<ExpressionAst>()> &&elem_parser) -> Unique<ArrayLiteralAst>;

  auto parse_literal_float_b10() -> Unique<FloatLiteralAst>;
  auto parse_literal_integer_b10() -> Unique<IntegerLiteralAst>;
  auto parse_literal_integer_b02() -> Unique<IntegerLiteralAst>;
  auto parse_literal_integer_b08() -> Unique<IntegerLiteralAst>;
  auto parse_literal_integer_b16() -> Unique<IntegerLiteralAst>;
  auto parse_numeric_prefix_op() -> Unique<TokenAst>;
  auto parse_float_suffix_type() -> Unique<TokenAst>;
  auto parse_integer_suffix_type() -> Unique<TokenAst>;
  auto parse_byte_prefix_type() -> Unique<TokenAst>;

  auto parse_literal_tuple_1_element(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<TupleLiteralAst>;

  auto parse_literal_tuple_n_elements(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<TupleLiteralAst>;

  auto parse_literal_array_repeated_element(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<ArrayLiteralRepeatedElementAst>;

  auto parse_literal_array_explicit_elements(
    std::function<Unique<ExpressionAst>()> &&elem_parser)
    -> Unique<ArrayLiteralExplicitElementsAst>;

  auto parse_specific_characters(
    Str &&s)
    -> Unique<TokenAst>;

  auto parse_specific_character(
    char16_t c)
    -> Unique<TokenAst>;

  auto parse_lexeme_character() -> Unique<TokenAst>;
  auto parse_lexeme_digit() -> Unique<TokenAst>;
  auto parse_lexeme_character_or_digit() -> Unique<TokenAst>;
  auto parse_lexeme_character_or_digit_or_underscore() -> Unique<TokenAst>;
  auto parse_lexeme_bin_integer() -> Unique<TokenAst>;
  auto parse_lexeme_oct_integer() -> Unique<TokenAst>;
  auto parse_lexeme_dec_integer() -> Unique<TokenAst>;
  auto parse_lexeme_hex_integer() -> Unique<TokenAst>;
  auto parse_lexeme_single_quote_char() -> Unique<TokenAst>;
  auto parse_lexeme_double_quote_string() -> Unique<TokenAst>;
  auto parse_lexeme_identifier() -> Unique<TokenAst>;
  auto parse_lexeme_upper_identifier() -> Unique<TokenAst>;

  auto parse_nothing() -> Unique<TokenAst>;
  auto parse_newline() -> Unique<TokenAst>;
  auto parse_space() -> Unique<TokenAst>;

  auto parse_token_left_curly_brace() -> Unique<TokenAst>;
  auto parse_token_right_curly_brace() -> Unique<TokenAst>;
  auto parse_token_left_parenthesis() -> Unique<TokenAst>;
  auto parse_token_right_parenthesis() -> Unique<TokenAst>;
  auto parse_token_left_square_bracket() -> Unique<TokenAst>;
  auto parse_token_right_square_bracket() -> Unique<TokenAst>;
  auto parse_token_colon() -> Unique<TokenAst>;
  auto parse_token_comma() -> Unique<TokenAst>;
  auto parse_token_assign() -> Unique<TokenAst>;
  auto parse_token_underscore() -> Unique<TokenAst>;
  auto parse_token_less_than() -> Unique<TokenAst>;
  auto parse_token_greater_than() -> Unique<TokenAst>;
  auto parse_token_add() -> Unique<TokenAst>;
  auto parse_token_sub() -> Unique<TokenAst>;
  auto parse_token_mul() -> Unique<TokenAst>;
  auto parse_token_div() -> Unique<TokenAst>;
  auto parse_token_rem() -> Unique<TokenAst>;
  auto parse_token_bit_ior() -> Unique<TokenAst>;
  auto parse_token_bit_xor() -> Unique<TokenAst>;
  auto parse_token_bit_and() -> Unique<TokenAst>;
  auto parse_token_dot() -> Unique<TokenAst>;
  auto parse_token_question_mark() -> Unique<TokenAst>;
  auto parse_token_exclamation_mark() -> Unique<TokenAst>;
  auto parse_token_deref() -> Unique<TokenAst>;
  auto parse_token_borrow() -> Unique<TokenAst>;
  auto parse_token_semicolon() -> Unique<TokenAst>;
  auto parse_token_single_quote() -> Unique<TokenAst>;
  auto parse_token_double_quote() -> Unique<TokenAst>;
  auto parse_token_dollar() -> Unique<TokenAst>;
  auto parse_token_arrow_right() -> Unique<TokenAst>;
  auto parse_token_double_dot() -> Unique<TokenAst>;
  auto parse_token_double_colon() -> Unique<TokenAst>;
  auto parse_token_equals() -> Unique<TokenAst>;
  auto parse_token_not_equals() -> Unique<TokenAst>;
  auto parse_token_less_than_equals() -> Unique<TokenAst>;
  auto parse_token_greater_than_equals() -> Unique<TokenAst>;
  auto parse_token_add_assign() -> Unique<TokenAst>;
  auto parse_token_sub_assign() -> Unique<TokenAst>;
  auto parse_token_mul_assign() -> Unique<TokenAst>;
  auto parse_token_div_assign() -> Unique<TokenAst>;
  auto parse_token_rem_assign() -> Unique<TokenAst>;
  auto parse_token_pow() -> Unique<TokenAst>;
  auto parse_token_bit_shl() -> Unique<TokenAst>;
  auto parse_token_bit_shr() -> Unique<TokenAst>;
  auto parse_token_bit_ior_assign() -> Unique<TokenAst>;
  auto parse_token_bit_xor_assign() -> Unique<TokenAst>;
  auto parse_token_bit_and_assign() -> Unique<TokenAst>;
  auto parse_token_pow_assign() -> Unique<TokenAst>;
  auto parse_token_bit_shl_assign() -> Unique<TokenAst>;
  auto parse_token_bit_shr_assign() -> Unique<TokenAst>;

  auto parse_keyword_cls() -> Unique<TokenAst>;
  auto parse_keyword_fun() -> Unique<TokenAst>;
  auto parse_keyword_cor() -> Unique<TokenAst>;
  auto parse_keyword_sup() -> Unique<TokenAst>;
  auto parse_keyword_ext() -> Unique<TokenAst>;
  auto parse_keyword_mut() -> Unique<TokenAst>;
  auto parse_keyword_use() -> Unique<TokenAst>;
  auto parse_keyword_cmp() -> Unique<TokenAst>;
  auto parse_keyword_let() -> Unique<TokenAst>;
  auto parse_keyword_type() -> Unique<TokenAst>;
  auto parse_keyword_self() -> Unique<TokenAst>;
  auto parse_keyword_case() -> Unique<TokenAst>;
  auto parse_keyword_of() -> Unique<TokenAst>;
  auto parse_keyword_loop() -> Unique<TokenAst>;
  auto parse_keyword_in() -> Unique<TokenAst>;
  auto parse_keyword_to() -> Unique<TokenAst>;
  auto parse_keyword_else() -> Unique<TokenAst>;
  auto parse_keyword_gen() -> Unique<TokenAst>;
  auto parse_keyword_with() -> Unique<TokenAst>;
  auto parse_keyword_ret() -> Unique<TokenAst>;
  auto parse_keyword_exit() -> Unique<TokenAst>;
  auto parse_keyword_skip() -> Unique<TokenAst>;
  auto parse_keyword_defer() -> Unique<TokenAst>;
  auto parse_keyword_is() -> Unique<TokenAst>;
  auto parse_keyword_as() -> Unique<TokenAst>;
  auto parse_keyword_or() -> Unique<TokenAst>;
  auto parse_keyword_and() -> Unique<TokenAst>;
  auto parse_keyword_not() -> Unique<TokenAst>;
  auto parse_keyword_async() -> Unique<TokenAst>;
  auto parse_keyword_true() -> Unique<TokenAst>;
  auto parse_keyword_false() -> Unique<TokenAst>;
  auto parse_keyword_await() -> Unique<TokenAst>;

  auto parse_keyword_res() -> Unique<TokenAst>;
  auto parse_keyword_caps() -> Unique<TokenAst>;

  auto parse_token_raw(lex::RawTokenType tok, lex::SppTokenType mapped_tok) -> Unique<TokenAst>;

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
