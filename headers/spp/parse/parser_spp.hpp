#pragma once
#include <spp/pch.hpp>
#include <spp/parse/parser_base.hpp>


/// @cond
namespace spp::parse {
    class ParserSpp;
}
/// @endcond


class spp::parse::ParserSpp final : public ParserBase {
public:
    using ParserBase::ParserBase;

public:
    auto parse() -> std::unique_ptr<asts::ModulePrototypeAst>;
    auto parse_root() -> std::unique_ptr<asts::ModulePrototypeAst>;
    auto parse_eof() -> std::unique_ptr<asts::TokenAst>;

    auto parse_module_prototype() -> std::unique_ptr<asts::ModulePrototypeAst>;
    auto parse_module_implementation() -> std::unique_ptr<asts::ModuleImplementationAst>;
    auto parse_module_member() -> std::unique_ptr<asts::ModuleMemberAst>;

    auto parse_class_prototype() -> std::unique_ptr<asts::ClassPrototypeAst>;
    auto parse_class_implementation() -> std::unique_ptr<asts::ClassImplementationAst>;
    auto parse_class_member() -> std::unique_ptr<asts::ClassMemberAst>;
    auto parse_class_attribute() -> std::unique_ptr<asts::ClassAttributeAst>;
    auto parse_class_attribute_default_value() -> std::unique_ptr<asts::ExpressionAst>;

    auto parse_sup_prototype_functions() -> std::unique_ptr<asts::SupPrototypeFunctionsAst>;
    auto parse_sup_prototype_extension() -> std::unique_ptr<asts::SupPrototypeExtensionAst>;
    auto parse_sup_implementation() -> std::unique_ptr<asts::SupImplementationAst>;
    auto parse_sup_member() -> std::unique_ptr<asts::SupMemberAst>;
    auto parse_sup_type_statement() -> std::unique_ptr<asts::TypeStatementAst>;
    auto parse_sup_cmp_statement() -> std::unique_ptr<asts::CmpStatementAst>;

    auto parse_function_prototype() -> std::unique_ptr<asts::FunctionPrototypeAst>;
    auto parse_subroutine_prototype() -> std::unique_ptr<asts::SubroutinePrototypeAst>;
    auto parse_coroutine_prototype() -> std::unique_ptr<asts::CoroutinePrototypeAst>;
    auto parse_function_implementation() -> std::unique_ptr<asts::FunctionImplementationAst>;
    auto parse_function_member() -> std::unique_ptr<asts::FunctionMemberAst>;
    auto parse_function_parameter_group() -> std::unique_ptr<asts::FunctionParameterGroupAst>;
    auto parse_function_parameter() -> std::unique_ptr<asts::FunctionParameterAst>;
    auto parse_function_parameter_self() -> std::unique_ptr<asts::FunctionParameterSelfAst>;
    auto parse_function_parameter_self_with_convention() -> std::unique_ptr<asts::FunctionParameterSelfAst>;
    auto parse_function_parameter_self_without_convention() -> std::unique_ptr<asts::FunctionParameterSelfAst>;
    auto parse_function_parameter_required() -> std::unique_ptr<asts::FunctionParameterRequiredAst>;
    auto parse_function_parameter_optional() -> std::unique_ptr<asts::FunctionParameterOptionalAst>;
    auto parse_function_parameter_variadic() -> std::unique_ptr<asts::FunctionParameterVariadicAst>;

    auto parse_function_call_argument_group() -> std::unique_ptr<asts::FunctionCallArgumentGroupAst>;
    auto parse_function_call_argument() -> std::unique_ptr<asts::FunctionCallArgumentAst>;
    auto parse_function_call_argument_keyword() -> std::unique_ptr<asts::FunctionCallArgumentKeywordAst>;
    auto parse_function_call_argument_positional() -> std::unique_ptr<asts::FunctionCallArgumentPositionalAst>;

    auto parse_generic_parameter_group() -> std::unique_ptr<asts::GenericParameterGroupAst>;
    auto parse_generic_parameter() -> std::unique_ptr<asts::GenericParameterAst>;
    auto parse_generic_parameter_comp() -> std::unique_ptr<asts::GenericParameterCompAst>;
    auto parse_generic_parameter_comp_required() -> std::unique_ptr<asts::GenericParameterCompRequiredAst>;
    auto parse_generic_parameter_comp_optional() -> std::unique_ptr<asts::GenericParameterCompOptionalAst>;
    auto parse_generic_parameter_comp_variadic() -> std::unique_ptr<asts::GenericParameterCompVariadicAst>;
    auto parse_generic_parameter_type() -> std::unique_ptr<asts::GenericParameterTypeAst>;
    auto parse_generic_parameter_type_required() -> std::unique_ptr<asts::GenericParameterTypeRequiredAst>;
    auto parse_generic_parameter_type_optional() -> std::unique_ptr<asts::GenericParameterTypeOptionalAst>;
    auto parse_generic_parameter_type_variadic() -> std::unique_ptr<asts::GenericParameterTypeVariadicAst>;
    auto parse_generic_parameter_type_inline_constraints() -> std::unique_ptr<asts::GenericParameterTypeInlineConstraintsAst>;

    auto parse_generic_argument_group() -> std::unique_ptr<asts::GenericArgumentGroupAst>;
    auto parse_generic_argument() -> std::unique_ptr<asts::GenericArgumentAst>;
    auto parse_generic_argument_comp() -> std::unique_ptr<asts::GenericArgumentCompAst>;
    auto parse_generic_argument_comp_positional() -> std::unique_ptr<asts::GenericArgumentCompPositionalAst>;
    auto parse_generic_argument_comp_keyword() -> std::unique_ptr<asts::GenericArgumentCompKeywordAst>;
    auto parse_generic_argument_type() -> std::unique_ptr<asts::GenericArgumentTypeAst>;
    auto parse_generic_argument_type_positional() -> std::unique_ptr<asts::GenericArgumentTypePositionalAst>;
    auto parse_generic_argument_type_keyword() -> std::unique_ptr<asts::GenericArgumentTypeKeywordAst>;

    auto parse_annotation() -> std::unique_ptr<asts::AnnotationAst>;

    auto parse_expression() -> std::unique_ptr<asts::ExpressionAst>;

    auto parse_binary_expression_precedence_level_n_rhs(std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::ExpressionAst>()> &&rhs_parser) -> std::unique_ptr<asts::BinaryExpressionTempAst>;
    auto parse_binary_expression_precedence_level_n_rhs_for_is(std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::CasePatternVariantAst>()> &&rhs_parser) -> std::unique_ptr<asts::IsExpressionTempAst>;
    auto parse_binary_expression_precedence_level_n(std::function<std::unique_ptr<asts::ExpressionAst>()> &&lhs_parser, std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::ExpressionAst>()> &&rhs_parser) -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_n_for_is(std::function<std::unique_ptr<asts::ExpressionAst>()> &&lhs_parser, std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::CasePatternVariantAst>()> &&rhs_parser) -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_0() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_1() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_2() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_3() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_4() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_5() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_6() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_7() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_8() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_9() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_precedence_level_10() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_binary_expression_op_precedence_level_0() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_1() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_2() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_3() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_4() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_5() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_6() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_7() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_8() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_9() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_expression_op_precedence_level_10() -> std::unique_ptr<asts::TokenAst>;

    auto parse_unary_expression() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_unary_expression_op() -> std::unique_ptr<asts::UnaryExpressionOperatorAst>;
    auto parse_unary_expression_op_async() -> std::unique_ptr<asts::UnaryExpressionOperatorAsyncAst>;
    auto parse_unary_expression_op_deref() -> std::unique_ptr<asts::UnaryExpressionOperatorDerefAst>;

    auto parse_postfix_expression() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_postfix_expression_op() -> std::unique_ptr<asts::PostfixExpressionOperatorAst>;
    auto parse_postfix_expression_op_early_return() -> std::unique_ptr<asts::PostfixExpressionOperatorEarlyReturnAst>;
    auto parse_postfix_expression_op_function_call() -> std::unique_ptr<asts::PostfixExpressionOperatorFunctionCallAst>;
    auto parse_postfix_expression_op_runtime_member_access() -> std::unique_ptr<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>;
    auto parse_postfix_expression_op_static_member_access() -> std::unique_ptr<asts::PostfixExpressionOperatorStaticMemberAccessAst>;
    auto parse_postfix_expression_op_keyword_not() -> std::unique_ptr<asts::PostfixExpressionOperatorKeywordNotAst>;
    auto parse_postfix_expression_op_keyword_res() -> std::unique_ptr<asts::PostfixExpressionOperatorKeywordResAst>;

    auto parse_primary_expression() -> std::unique_ptr<asts::ExpressionAst>;

    auto parse_parenthesised_expression() -> std::unique_ptr<asts::ParenthesisedExpressionAst>;

    auto parse_fold_expression() -> std::unique_ptr<asts::FoldExpressionAst>;

    auto parse_case_expression() -> std::unique_ptr<asts::CaseExpressionAst>;
    auto parse_case_expression_branch() -> std::unique_ptr<asts::CaseExpressionBranchAst>;
    auto parse_case_expression_branch_else() -> std::unique_ptr<asts::CaseExpressionBranchAst>;
    auto parse_case_expression_branch_else_case() -> std::unique_ptr<asts::CaseExpressionBranchAst>;

    auto parse_case_of_expression() -> std::unique_ptr<asts::CaseExpressionAst>;
    auto parse_case_of_expression_branch() -> std::unique_ptr<asts::CaseExpressionBranchAst>;
    auto parse_case_of_expression_branch_destructuring() -> std::unique_ptr<asts::CaseExpressionBranchAst>;
    auto parse_case_of_expression_branch_comparing() -> std::unique_ptr<asts::CaseExpressionBranchAst>;

    auto parse_case_expression_pattern_variant_destructure() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_array() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_object() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_tuple() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_skip_single_argument() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_skip_multiple_arguments() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_attribute_binding() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_single_identifier() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_literal() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_expression() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_else() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_else_case() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_array() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_object() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_tuple() -> std::unique_ptr<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_attribute_binding() -> std::unique_ptr<asts::CasePatternVariantAst>;

    auto parse_pattern_guard() -> std::unique_ptr<asts::PatternGuardAst>;
    auto parse_boolean_comparison_op() -> std::unique_ptr<asts::TokenAst>;

    auto parse_loop_expression() -> std::unique_ptr<asts::LoopExpressionAst>;
    auto parse_loop_condition() -> std::unique_ptr<asts::LoopConditionAst>;
    auto parse_loop_condition_boolean() -> std::unique_ptr<asts::LoopConditionBooleanAst>;
    auto parse_loop_condition_iterable() -> std::unique_ptr<asts::LoopConditionIterableAst>;
    auto parse_loop_else_statement() -> std::unique_ptr<asts::LoopElseStatementAst>;

    auto parse_iter_of_expression() -> std::unique_ptr<asts::IterExpressionAst>;
    auto parse_iter_of_expression_branch() -> std::unique_ptr<asts::IterExpressionBranchAst>;
    auto parse_iter_expression_pattern_variant() -> std::unique_ptr<asts::IterPatternVariantAst>;
    auto parse_iter_expression_pattern_variant_variable() -> std::unique_ptr<asts::IterPatternVariantVariableAst>;
    auto parse_iter_expression_pattern_variant_no_value() -> std::unique_ptr<asts::IterPatternVariantNoValueAst>;
    auto parse_iter_expression_pattern_variant_exhausted() -> std::unique_ptr<asts::IterPatternVariantExhaustedAst>;
    auto parse_iter_expression_pattern_variant_exception() -> std::unique_ptr<asts::IterPatternVariantExceptionAst>;
    auto parse_iter_expression_pattern_variant_else() -> std::unique_ptr<asts::IterPatternVariantElseAst>;

    auto parse_gen_expression() -> std::unique_ptr<asts::GenExpressionAst>;
    auto parse_gen_expression_with_expression() -> std::unique_ptr<asts::GenExpressionAst>;
    auto parse_gen_expression_without_expression() -> std::unique_ptr<asts::GenExpressionAst>;
    auto parse_gen_unroll_expression() -> std::unique_ptr<asts::GenWithExpressionAst>;

    auto parse_inner_scope_expression(auto &&parser) -> std::unique_ptr<asts::InnerScopeExpressionAst<decltype(parser())>>;
    auto parse_inner_scope(auto &&parser) -> std::unique_ptr<asts::InnerScopeAst<decltype(parser())>>;

    auto parse_statement() -> std::unique_ptr<asts::StatementAst>;
    auto parse_assignment_statement() -> std::unique_ptr<asts::AssignmentStatementAst>;
    auto parse_ret_statement() -> std::unique_ptr<asts::RetStatementAst>;
    auto parse_exit_statement() -> std::unique_ptr<asts::LoopControlFlowStatementAst>;
    auto parse_exit_statement_with_value() -> std::unique_ptr<asts::LoopControlFlowStatementAst>;
    auto parse_skip_statement() -> std::unique_ptr<asts::LoopControlFlowStatementAst>;
    auto parse_use_statement() -> std::unique_ptr<asts::UseStatementAst>;
    auto parse_type_statement() -> std::unique_ptr<asts::TypeStatementAst>;
    auto parse_cmp_statement() -> std::unique_ptr<asts::CmpStatementAst>;
    auto parse_let_statement() -> std::unique_ptr<asts::LetStatementAst>;
    auto parse_let_statement_initialized() -> std::unique_ptr<asts::LetStatementAst>;
    auto parse_let_statement_initialized_explicit_type() -> std::unique_ptr<asts::TypeAst>;
    auto parse_let_statement_uninitialized() -> std::unique_ptr<asts::LetStatementAst>;

    auto parse_global_use_statement() -> std::unique_ptr<asts::UseStatementAst>;
    auto parse_global_type_statement() -> std::unique_ptr<asts::TypeStatementAst>;
    auto parse_global_cmp_statement() -> std::unique_ptr<asts::CmpStatementAst>;

    auto parse_local_variable() -> std::unique_ptr<asts::LocalVariableAst>;
    auto parse_local_variable_destructure_array() -> std::unique_ptr<asts::LocalVariableDestructureArrayAst>;
    auto parse_local_variable_destructure_object() -> std::unique_ptr<asts::LocalVariableDestructureObjectAst>;
    auto parse_local_variable_destructure_tuple() -> std::unique_ptr<asts::LocalVariableDestructureTupleAst>;
    auto parse_local_variable_destructure_skip_single_argument() -> std::unique_ptr<asts::LocalVariableDestructureSkipSingleArgumentAst>;
    auto parse_local_variable_destructure_skip_multiple_arguments() -> std::unique_ptr<asts::LocalVariableDestructureSkipMultipleArgumentsAst>;
    auto parse_local_variable_destructure_attribute_binding() -> std::unique_ptr<asts::LocalVariableDestructureAttributeBindingAst>;
    auto parse_local_variable_single_identifier() -> std::unique_ptr<asts::LocalVariableSingleIdentifierAst>;
    auto parse_local_variable_single_identifier_alias() -> std::unique_ptr<asts::LocalVariableSingleIdentifierAliasAst>;
    auto parse_local_variable_nested_for_destructure_array() -> std::unique_ptr<asts::LocalVariableAst>;
    auto parse_local_variable_nested_for_destructure_object() -> std::unique_ptr<asts::LocalVariableAst>;
    auto parse_local_variable_nested_for_destructure_tuple() -> std::unique_ptr<asts::LocalVariableAst>;
    auto parse_local_variable_nested_for_destructure_attribute_binding() -> std::unique_ptr<asts::LocalVariableAst>;

    auto parse_convention() -> std::unique_ptr<asts::ConventionAst>;
    auto parse_convention_ref() -> std::unique_ptr<asts::ConventionRefAst>;
    auto parse_convention_mut() -> std::unique_ptr<asts::ConventionMutAst>;

    auto parse_object_initializer() -> std::unique_ptr<asts::ObjectInitializerAst>;
    auto parse_object_initializer_argument_group() -> std::unique_ptr<asts::ObjectInitializerArgumentGroupAst>;
    auto parse_object_initializer_argument() -> std::unique_ptr<asts::ObjectInitializerArgumentAst>;
    auto parse_object_initializer_argument_keyword() -> std::unique_ptr<asts::ObjectInitializerArgumentKeywordAst>;
    auto parse_object_initializer_argument_shorthand() -> std::unique_ptr<asts::ObjectInitializerArgumentShorthandAst>;

    auto parse_closure_expression() -> std::unique_ptr<asts::ClosureExpressionAst>;
    auto parse_closure_expression_capture_group() -> std::unique_ptr<asts::ClosureExpressionCaptureGroupAst>;
    auto parse_closure_expression_capture() -> std::unique_ptr<asts::ClosureExpressionCaptureAst>;
    auto parse_closure_expression_parameter_and_capture_group() -> std::unique_ptr<asts::ClosureExpressionParameterAndCaptureGroupAst>;
    auto parse_closure_expression_parameter_group() -> std::unique_ptr<asts::ClosureExpressionParameterGroupAst>;
    auto parse_closure_expression_parameter() -> std::unique_ptr<asts::ClosureExpressionParameterAst>;

    auto parse_type_expression() -> std::unique_ptr<asts::TypeAst>;

    auto parse_binary_type_expression_precedence_level_n_rhs(std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::TypeAst>()> &&rhs_parser) -> std::unique_ptr<asts::TypeBinaryExpressionTempAst>;
    auto parse_binary_type_expression_precedence_level_n(std::function<std::unique_ptr<asts::TypeAst>()> &&lhs_parser, std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::TypeAst>()> &&rhs_parser) -> std::unique_ptr<asts::TypeAst>;
    auto parse_binary_type_expression_precedence_level_1() -> std::unique_ptr<asts::TypeAst>;
    auto parse_binary_type_expression_precedence_level_2() -> std::unique_ptr<asts::TypeAst>;
    auto parse_binary_type_expression_op_precedence_level_1() -> std::unique_ptr<asts::TokenAst>;
    auto parse_binary_type_expression_op_precedence_level_2() -> std::unique_ptr<asts::TokenAst>;

    auto parse_unary_type_expression() -> std::unique_ptr<asts::TypeAst>;
    auto parse_unary_type_expression_op() -> std::unique_ptr<asts::TypeUnaryExpressionOperatorAst>;
    auto parse_unary_type_expression_op_borrow() -> std::unique_ptr<asts::TypeUnaryExpressionOperatorBorrowAst>;
    auto parse_unary_type_expression_op_namespace() -> std::unique_ptr<asts::TypeUnaryExpressionOperatorNamespaceAst>;

    auto parse_postfix_type_expression() -> std::unique_ptr<asts::TypeAst>;
    auto parse_postfix_type_expression_op() -> std::unique_ptr<asts::TypePostfixExpressionOperatorAst>;
    auto parse_postfix_type_expression_op_optional() -> std::unique_ptr<asts::TypePostfixExpressionOperatorOptionalAst>;
    auto parse_postfix_type_expression_op_nested() -> std::unique_ptr<asts::TypePostfixExpressionOperatorNestedTypeAst>;

    auto parse_type_parenthesised_expression() -> std::unique_ptr<asts::TypeAst>;
    auto parse_type_never() -> std::unique_ptr<asts::TypeAst>;

    auto parse_type_expression_simple() -> std::unique_ptr<asts::TypeAst>;
    auto parse_postfix_type_expression_simple() -> std::unique_ptr<asts::TypeAst>;
    auto parse_unary_type_expression_simple() -> std::unique_ptr<asts::TypeAst>;

    auto parse_type_identifier() -> std::unique_ptr<asts::TypeIdentifierAst>;

    auto parse_type_array() -> std::unique_ptr<asts::TypeAst>;
    auto parse_type_tuple() -> std::unique_ptr<asts::TypeAst>;
    auto parse_type_tuple_0_types() -> std::unique_ptr<asts::TypeAst>;
    auto parse_type_tuple_1_types() -> std::unique_ptr<asts::TypeAst>;
    auto parse_type_tuple_n_types() -> std::unique_ptr<asts::TypeAst>;

    auto parse_identifier() -> std::unique_ptr<asts::IdentifierAst>;
    auto parse_numeric_identifier() -> std::unique_ptr<asts::IdentifierAst>;
    auto parse_self_identifier() -> std::unique_ptr<asts::IdentifierAst>;
    auto parse_upper_identifier() -> std::unique_ptr<asts::IdentifierAst>;

    auto parse_literal() -> std::unique_ptr<asts::LiteralAst>;
    auto parse_literal_string() -> std::unique_ptr<asts::StringLiteralAst>;
    auto parse_literal_float() -> std::unique_ptr<asts::FloatLiteralAst>;
    auto parse_literal_integer() -> std::unique_ptr<asts::IntegerLiteralAst>;
    auto parse_literal_boolean() -> std::unique_ptr<asts::BooleanLiteralAst>;
    auto parse_literal_tuple(std::function<std::unique_ptr<asts::ExpressionAst>()> &&elem_parser) -> std::unique_ptr<asts::TupleLiteralAst>;
    auto parse_literal_array(std::function<std::unique_ptr<asts::ExpressionAst>()> &&elem_parser) -> std::unique_ptr<asts::ArrayLiteralAst>;

    auto parse_literal_float_b10() -> std::unique_ptr<asts::FloatLiteralAst>;
    auto parse_literal_integer_b10() -> std::unique_ptr<asts::IntegerLiteralAst>;
    auto parse_literal_integer_b02() -> std::unique_ptr<asts::IntegerLiteralAst>;
    auto parse_literal_integer_b08() -> std::unique_ptr<asts::IntegerLiteralAst>;
    auto parse_literal_integer_b16() -> std::unique_ptr<asts::IntegerLiteralAst>;
    auto parse_numeric_prefix_op() -> std::unique_ptr<asts::TokenAst>;
    auto parse_float_suffix_type() -> std::unique_ptr<asts::TokenAst>;
    auto parse_integer_suffix_type() -> std::unique_ptr<asts::TokenAst>;

    auto parse_literal_tuple_1_element(std::function<std::unique_ptr<asts::ExpressionAst>()> &&elem_parser) -> std::unique_ptr<asts::TupleLiteralAst>;
    auto parse_literal_tuple_n_elements(std::function<std::unique_ptr<asts::ExpressionAst>()> &&elem_parser) -> std::unique_ptr<asts::TupleLiteralAst>;

    auto parse_literal_array_repeated_element(std::function<std::unique_ptr<asts::ExpressionAst>()> &&elem_parser) -> std::unique_ptr<asts::ArrayLiteralRepeatedElementAst>;
    auto parse_literal_array_explicit_elements(std::function<std::unique_ptr<asts::ExpressionAst>()> &&elem_parser) -> std::unique_ptr<asts::ArrayLiteralExplicitElementsAst>;

    auto parse_cmp_value() -> std::unique_ptr<asts::ExpressionAst>;
    auto parse_cmp_object_initializer() -> std::unique_ptr<asts::ObjectInitializerAst>;
    auto parse_cmp_object_initializer_argument_group() -> std::unique_ptr<asts::ObjectInitializerArgumentGroupAst>;
    auto parse_cmp_object_initializer_argument() -> std::unique_ptr<asts::ObjectInitializerArgumentAst>;
    auto parse_cmp_object_initializer_argument_keyword() -> std::unique_ptr<asts::ObjectInitializerArgumentKeywordAst>;

    auto parse_specific_characters(std::string &&s) -> std::unique_ptr<asts::TokenAst>;
    auto parse_specific_character(char16_t c) -> std::unique_ptr<asts::TokenAst>;

    auto parse_lexeme_character() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_digit() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_character_or_digit() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_character_or_digit_or_underscore() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_bin_integer() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_oct_integer() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_dec_integer() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_hex_integer() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_double_quote_string() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_identifier() -> std::unique_ptr<asts::TokenAst>;
    auto parse_lexeme_upper_identifier() -> std::unique_ptr<asts::TokenAst>;

    auto parse_nothing() -> std::unique_ptr<asts::TokenAst>;
    auto parse_newline() -> std::unique_ptr<asts::TokenAst>;
    auto parse_space() -> std::unique_ptr<asts::TokenAst>;

    auto parse_token_left_curly_brace() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_right_curly_brace() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_left_parenthesis() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_right_parenthesis() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_left_square_bracket() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_right_square_bracket() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_colon() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_comma() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_at() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_underscore() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_less_than() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_greater_than() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_add() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_sub() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_mul() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_div() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_rem() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_ior() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_xor() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_and() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_dot() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_question_mark() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_exclamation_mark() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_deref() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_borrow() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_vertical_bar() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_semicolon() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_double_quote() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_dollar() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_arrow_right() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_double_dot() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_double_colon() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_double_exclamation_mark() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_equals() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_not_equals() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_less_than_equals() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_greater_than_equals() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_add_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_sub_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_mul_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_div_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_rem_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_pow() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_shl() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_shr() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_ior_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_xor_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_and_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_pow_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_shl_assign() -> std::unique_ptr<asts::TokenAst>;
    auto parse_token_bit_shr_assign() -> std::unique_ptr<asts::TokenAst>;

    auto parse_keyword_cls() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_fun() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_cor() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_sup() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_ext() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_mut() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_use() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_cmp() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_let() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_type() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_self() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_case() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_iter() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_of() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_loop() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_in() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_else() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_gen() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_with() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_ret() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_exit() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_skip() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_is() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_as() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_or() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_and() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_not() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_async() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_true() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_false() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_res() -> std::unique_ptr<asts::TokenAst>;
    auto parse_keyword_caps() -> std::unique_ptr<asts::TokenAst>;

    auto parse_token_raw(lex::RawTokenType tok, lex::SppTokenType mapped_tok) -> std::unique_ptr<asts::TokenAst>;

private:
    auto m_store_error(std::size_t pos, std::string &&err_str) const -> bool;
};
