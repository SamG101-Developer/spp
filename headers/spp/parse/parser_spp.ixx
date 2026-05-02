module;
#include <spp/macros.hpp>

export module spp.parse.parser_spp;
import spp.lex.tokens;
import spp.parse.parser_base;
import spp.utils.types;
import std;

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
    SPP_EXP_CLS struct ConventionMutAst;
    SPP_EXP_CLS struct ConventionRefAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct FunctionImplementationAst;
    SPP_EXP_CLS struct CoroutinePrototypeAst;
    SPP_EXP_CLS struct SubroutinePrototypeAst;
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
    SPP_EXP_CLS struct FunctionCallArgumentPositionalAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionParameterSelfAst;
    SPP_EXP_CLS struct FunctionParameterRequiredAst;
    SPP_EXP_CLS struct FunctionParameterOptionalAst;
    SPP_EXP_CLS struct FunctionParameterVariadicAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
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
    SPP_EXP_CLS template <typename T>
    struct InnerScopeAst;
    SPP_EXP_CLS struct InnerScopeExpressionAst;
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
    SPP_EXP_CLS struct CharLiteralAst;
    SPP_EXP_CLS struct PrimaryExpressionAst;
    SPP_EXP_CLS struct CaseExpressionAst;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct LoopConditionalExpressionAst;
    SPP_EXP_CLS struct LoopExpressionAst;
    SPP_EXP_CLS struct LoopIterableExpressionAst;
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
    SPP_EXP_CLS struct IsExpressionAst;
    SPP_EXP_CLS struct ClosureExpressionAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureAst;
    SPP_EXP_CLS struct ClosureExpressionCaptureGroupAst;
    SPP_EXP_CLS struct ClosureExpressionParameterAndCaptureGroupAst;
    SPP_EXP_CLS using ClosureExpressionParameterAst = FunctionParameterAst;
    SPP_EXP_CLS using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst;
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
    SPP_EXP_CLS struct PostfixExpressionOperatorDerefAst;
    SPP_EXP_CLS struct ParenthesisedExpressionAst;
    SPP_EXP_CLS struct RetStatementAst;
    SPP_EXP_CLS struct StatementAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeBinaryExpressionAst;
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
    SPP_EXP_CLS struct UseStatementVariableAst;
    SPP_EXP_CLS struct TypeStatementAst;
    SPP_EXP_CLS struct SupPrototypeFunctionsAst;
    SPP_EXP_CLS struct SupPrototypeExtensionAst;
    SPP_EXP_CLS struct SupImplementationAst;
    SPP_EXP_CLS struct SupMemberAst;
    SPP_EXP_CLS using FunctionMemberAst = StatementAst;
}

namespace spp::parse {
    SPP_EXP_CLS class ParserSpp;
}

SPP_EXP_CLS class spp::parse::ParserSpp final : public ParserBase {
public:
    using ParserBase::ParserBase;
    ~ParserSpp() override = default;

public:
    auto parse() -> Unique<asts::ModulePrototypeAst>;
    auto parse_root() -> Unique<asts::ModulePrototypeAst>;
    auto parse_eof() -> Unique<asts::TokenAst>;

    auto parse_module_prototype() -> Unique<asts::ModulePrototypeAst>;
    auto parse_module_implementation() -> Unique<asts::ModuleImplementationAst>;
    auto parse_module_member() -> Unique<asts::Ast>;

    auto parse_class_prototype() -> Unique<asts::ClassPrototypeAst>;
    auto parse_class_implementation() -> Unique<asts::ClassImplementationAst>;
    auto parse_class_member() -> Unique<asts::Ast>;
    auto parse_class_attribute() -> Unique<asts::ClassAttributeAst>;
    auto parse_class_attribute_default_value() -> Unique<asts::ExpressionAst>;

    auto parse_sup_prototype_functions() -> Unique<asts::SupPrototypeFunctionsAst>;
    auto parse_sup_prototype_extension() -> Unique<asts::SupPrototypeExtensionAst>;
    auto parse_sup_implementation() -> Unique<asts::SupImplementationAst>;
    auto parse_sup_member() -> Unique<asts::Ast>;
    auto parse_sup_type_statement() -> Unique<asts::TypeStatementAst>;
    auto parse_sup_cmp_statement() -> Unique<asts::CmpStatementAst>;

    auto parse_function_prototype() -> Unique<asts::FunctionPrototypeAst>;
    auto parse_subroutine_prototype() -> Unique<asts::SubroutinePrototypeAst>;
    auto parse_coroutine_prototype() -> Unique<asts::CoroutinePrototypeAst>;
    auto parse_function_implementation() -> Unique<asts::FunctionImplementationAst>;
    auto parse_function_member() -> Unique<asts::StatementAst>;
    auto parse_function_parameter_group() -> Unique<asts::FunctionParameterGroupAst>;
    auto parse_function_parameter() -> Unique<asts::FunctionParameterAst>;
    auto parse_function_parameter_self() -> Unique<asts::FunctionParameterSelfAst>;
    auto parse_function_parameter_self_with_convention() -> Unique<asts::FunctionParameterSelfAst>;
    auto parse_function_parameter_self_without_convention() -> Unique<asts::FunctionParameterSelfAst>;
    auto parse_function_parameter_required() -> Unique<asts::FunctionParameterRequiredAst>;
    auto parse_function_parameter_optional() -> Unique<asts::FunctionParameterOptionalAst>;
    auto parse_function_parameter_variadic() -> Unique<asts::FunctionParameterVariadicAst>;

    auto parse_function_call_argument_group() -> Unique<asts::FunctionCallArgumentGroupAst>;
    auto parse_function_call_argument() -> Unique<asts::FunctionCallArgumentAst>;
    auto parse_function_call_argument_keyword() -> Unique<asts::FunctionCallArgumentKeywordAst>;
    auto parse_function_call_argument_positional() -> Unique<asts::FunctionCallArgumentPositionalAst>;

    auto parse_generic_parameter_group() -> Unique<asts::GenericParameterGroupAst>;
    auto parse_generic_parameter() -> Unique<asts::GenericParameterAst>;
    auto parse_generic_parameter_comp() -> Unique<asts::GenericParameterCompAst>;
    auto parse_generic_parameter_comp_required() -> Unique<asts::GenericParameterCompRequiredAst>;
    auto parse_generic_parameter_comp_optional() -> Unique<asts::GenericParameterCompOptionalAst>;
    auto parse_generic_parameter_comp_variadic() -> Unique<asts::GenericParameterCompVariadicAst>;
    auto parse_generic_parameter_type() -> Unique<asts::GenericParameterTypeAst>;
    auto parse_generic_parameter_type_required() -> Unique<asts::GenericParameterTypeRequiredAst>;
    auto parse_generic_parameter_type_optional() -> Unique<asts::GenericParameterTypeOptionalAst>;
    auto parse_generic_parameter_type_variadic() -> Unique<asts::GenericParameterTypeVariadicAst>;
    auto parse_generic_parameter_type_inline_constraints() -> Unique<asts::GenericParameterTypeInlineConstraintsAst>;

    auto parse_generic_argument_group() -> Unique<asts::GenericArgumentGroupAst>;
    auto parse_generic_argument() -> Unique<asts::GenericArgumentAst>;
    auto parse_generic_argument_comp() -> Unique<asts::GenericArgumentCompAst>;
    auto parse_generic_argument_comp_positional() -> Unique<asts::GenericArgumentCompPositionalAst>;
    auto parse_generic_argument_comp_keyword() -> Unique<asts::GenericArgumentCompKeywordAst>;
    auto parse_generic_argument_type() -> Unique<asts::GenericArgumentTypeAst>;
    auto parse_generic_argument_type_positional() -> Unique<asts::GenericArgumentTypePositionalAst>;
    auto parse_generic_argument_type_keyword() -> Unique<asts::GenericArgumentTypeKeywordAst>;

    auto parse_annotation() -> Unique<asts::AnnotationAst>;
    auto parse_annotation_no_call() -> Unique<asts::AnnotationAst>;
    auto parse_annotation_call() -> Unique<asts::AnnotationAst>;

    auto parse_expression() -> Unique<asts::ExpressionAst>;

    auto parse_binary_expression(std::uint8_t min_prec = 0) -> Unique<asts::ExpressionAst>;

    auto parse_unary_expression() -> Unique<asts::ExpressionAst>;
    auto parse_unary_expression_op() -> Unique<asts::UnaryExpressionOperatorAst>;
    auto parse_unary_expression_op_async() -> Unique<asts::UnaryExpressionOperatorAsyncAst>;

    auto parse_postfix_expression() -> Unique<asts::ExpressionAst>;
    auto parse_postfix_expression_op() -> Unique<asts::PostfixExpressionOperatorAst>;
    auto parse_postfix_expression_op_deref() -> Unique<asts::PostfixExpressionOperatorDerefAst>;
    auto parse_postfix_expression_op_early_return() -> Unique<asts::PostfixExpressionOperatorEarlyReturnAst>;
    auto parse_postfix_expression_op_function_call() -> Unique<asts::PostfixExpressionOperatorFunctionCallAst>;
    auto parse_postfix_expression_op_runtime_member_access() -> Unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>;
    auto parse_postfix_expression_op_static_member_access() -> Unique<asts::PostfixExpressionOperatorStaticMemberAccessAst>;
    auto parse_postfix_expression_op_keyword_not() -> Unique<asts::PostfixExpressionOperatorKeywordNotAst>;
    auto parse_postfix_expression_op_keyword_res() -> Unique<asts::PostfixExpressionOperatorKeywordResAst>;
    auto parse_postfix_expression_op_index() -> Unique<asts::PostfixExpressionOperatorIndexAst>;
    auto parse_postfix_expression_strictly_static_access_zero() -> Unique<asts::ExpressionAst>;
    auto parse_postfix_expression_strictly_static_access_one() -> Unique<asts::ExpressionAst>;

    auto parse_primary_expression() -> Unique<asts::ExpressionAst>;

    auto parse_parenthesised_expression() -> Unique<asts::ParenthesisedExpressionAst>;

    auto parse_fold_expression() -> Unique<asts::FoldExpressionAst>;

    auto parse_case_expression() -> Unique<asts::CaseExpressionAst>;
    auto parse_case_expression_branch() -> Unique<asts::CaseExpressionBranchAst>;
    auto parse_case_expression_branch_else() -> Unique<asts::CaseExpressionBranchAst>;
    auto parse_case_expression_branch_else_case() -> Unique<asts::CaseExpressionBranchAst>;

    auto parse_case_of_expression() -> Unique<asts::CaseExpressionAst>;
    auto parse_case_of_expression_branch() -> Unique<asts::CaseExpressionBranchAst>;
    auto parse_case_of_expression_branch_destructuring() -> Unique<asts::CaseExpressionBranchAst>;
    auto parse_case_of_expression_branch_comparing() -> Unique<asts::CaseExpressionBranchAst>;

    auto parse_case_expression_pattern_variant_destructure() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_array() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_object() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_tuple() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_skip_single_argument() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_skip_multiple_arguments() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_destructure_attribute_binding() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_single_identifier() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_single_identifier_with_convention() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_single_identifier_without_convention() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_literal() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_expression() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_else() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_variant_else_case() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_array() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_object() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_tuple() -> Unique<asts::CasePatternVariantAst>;
    auto parse_case_expression_pattern_nested_for_destructure_attribute_binding() -> Unique<asts::CasePatternVariantAst>;

    auto parse_pattern_guard() -> Unique<asts::PatternGuardAst>;
    auto parse_boolean_comparison_op() -> Unique<asts::TokenAst>;

    auto parse_loop_expression() -> Unique<asts::LoopExpressionAst>;
    auto parse_loop_conditional_expression() -> Unique<asts::LoopConditionalExpressionAst>;
    auto parse_loop_iterable_expression() -> Unique<asts::LoopIterableExpressionAst>;
    auto parse_loop_else_statement() -> Unique<asts::LoopElseStatementAst>;

    auto parse_gen_expression() -> Unique<asts::GenExpressionAst>;
    auto parse_gen_expression_with_expression() -> Unique<asts::GenExpressionAst>;
    auto parse_gen_expression_without_expression() -> Unique<asts::GenExpressionAst>;
    auto parse_gen_unroll_expression() -> Unique<asts::GenWithExpressionAst>;

    auto parse_inner_scope_expression(auto &&parser) -> Unique<asts::InnerScopeExpressionAst>;
    auto parse_inner_scope(auto &&parser) -> Unique<asts::InnerScopeAst<decltype(parser())>>;

    auto parse_statement() -> Unique<asts::StatementAst>;
    auto parse_assignment_statement() -> Unique<asts::AssignmentStatementAst>;
    auto parse_assignment_target() -> Unique<asts::ExpressionAst>;
    auto parse_assignment_target_postfix_expression() -> Unique<asts::ExpressionAst>;
    auto parse_assignment_target_postfix_expression_op() -> Unique<asts::PostfixExpressionOperatorAst>;
    auto parse_assignment_target_primary_expression() -> Unique<asts::ExpressionAst>;

    auto parse_ret_statement() -> Unique<asts::RetStatementAst>;
    auto parse_exit_statement() -> Unique<asts::LoopControlFlowStatementAst>;
    auto parse_exit_statement_with_value() -> Unique<asts::LoopControlFlowStatementAst>;
    auto parse_skip_statement() -> Unique<asts::LoopControlFlowStatementAst>;
    auto parse_use_statement() -> Unique<asts::UseStatementAst>;
    auto parse_use_var_statement() -> Unique<asts::UseStatementVariableAst>;
    auto parse_type_statement() -> Unique<asts::TypeStatementAst>;
    auto parse_cmp_statement() -> Unique<asts::CmpStatementAst>;
    auto parse_let_statement() -> Unique<asts::LetStatementAst>;
    auto parse_let_statement_initialized() -> Unique<asts::LetStatementAst>;
    auto parse_let_statement_initialized_explicit_type() -> Unique<asts::TypeAst>;
    auto parse_let_statement_uninitialized() -> Unique<asts::LetStatementAst>;

    auto parse_global_use_statement() -> Unique<asts::UseStatementAst>;
    auto parse_global_use_var_statement() -> Unique<asts::UseStatementVariableAst>;
    auto parse_global_type_statement() -> Unique<asts::TypeStatementAst>;
    auto parse_global_cmp_statement() -> Unique<asts::CmpStatementAst>;

    auto parse_local_variable() -> Unique<asts::LocalVariableAst>;
    auto parse_local_variable_destructure_array() -> Unique<asts::LocalVariableDestructureArrayAst>;
    auto parse_local_variable_destructure_object() -> Unique<asts::LocalVariableDestructureObjectAst>;
    auto parse_local_variable_destructure_tuple() -> Unique<asts::LocalVariableDestructureTupleAst>;
    auto parse_local_variable_destructure_skip_single_argument() -> Unique<asts::LocalVariableDestructureSkipSingleArgumentAst>;
    auto parse_local_variable_destructure_skip_multiple_arguments() -> Unique<asts::LocalVariableDestructureSkipMultipleArgumentsAst>;
    auto parse_local_variable_destructure_attribute_binding() -> Unique<asts::LocalVariableDestructureAttributeBindingAst>;
    auto parse_local_variable_single_identifier() -> Unique<asts::LocalVariableSingleIdentifierAst>;
    auto parse_local_variable_single_identifier_alias() -> Unique<asts::LocalVariableSingleIdentifierAliasAst>;
    auto parse_local_variable_nested_for_destructure_array() -> Unique<asts::LocalVariableAst>;
    auto parse_local_variable_nested_for_destructure_object() -> Unique<asts::LocalVariableAst>;
    auto parse_local_variable_nested_for_destructure_tuple() -> Unique<asts::LocalVariableAst>;
    auto parse_local_variable_nested_for_destructure_attribute_binding() -> Unique<asts::LocalVariableAst>;

    auto parse_convention() -> Unique<asts::ConventionAst>;
    auto parse_convention_ref() -> Unique<asts::ConventionRefAst>;
    auto parse_convention_mut() -> Unique<asts::ConventionMutAst>;

    auto parse_object_initializer() -> Unique<asts::ObjectInitializerAst>;
    auto parse_object_initializer_argument_group() -> Unique<asts::ObjectInitializerArgumentGroupAst>;
    auto parse_object_initializer_argument() -> Unique<asts::ObjectInitializerArgumentAst>;
    auto parse_object_initializer_argument_keyword() -> Unique<asts::ObjectInitializerArgumentKeywordAst>;
    auto parse_object_initializer_argument_shorthand() -> Unique<asts::ObjectInitializerArgumentShorthandAst>;

    auto parse_closure_expression() -> Unique<asts::ClosureExpressionAst>;
    auto parse_closure_expression_capture_group() -> Unique<asts::ClosureExpressionCaptureGroupAst>;
    auto parse_closure_expression_capture() -> Unique<asts::ClosureExpressionCaptureAst>;
    auto parse_closure_expression_parameter_and_capture_group() -> Unique<asts::ClosureExpressionParameterAndCaptureGroupAst>;
    auto parse_closure_expression_parameter_group() -> Unique<asts::ClosureExpressionParameterGroupAst>;
    auto parse_closure_expression_parameter() -> Unique<asts::ClosureExpressionParameterAst>;

    auto parse_type_expression() -> Unique<asts::TypeAst>;

    auto parse_binary_type_expression(std::uint8_t min_prec = 0) -> Unique<asts::TypeAst>;

    auto parse_unary_type_expression() -> Unique<asts::TypeAst>;
    auto parse_unary_type_expression_op() -> Unique<asts::TypeUnaryExpressionOperatorAst>;
    auto parse_unary_type_expression_op_borrow() -> Unique<asts::TypeUnaryExpressionOperatorBorrowAst>;
    auto parse_unary_type_expression_op_namespace() -> Unique<asts::TypeUnaryExpressionOperatorNamespaceAst>;

    auto parse_postfix_type_expression() -> Unique<asts::TypeAst>;
    auto parse_postfix_type_expression_op() -> Unique<asts::TypePostfixExpressionOperatorAst>;
    auto parse_postfix_type_expression_op_nested() -> Unique<asts::TypePostfixExpressionOperatorNestedTypeAst>;

    auto parse_type_parenthesised_expression() -> Unique<asts::TypeAst>;
    auto parse_type_never() -> Unique<asts::TypeAst>;

    auto parse_type_expression_simple() -> Unique<asts::TypeAst>;
    auto parse_postfix_type_expression_simple() -> Unique<asts::TypeAst>;
    auto parse_unary_type_expression_simple() -> Unique<asts::TypeAst>;

    auto parse_type_identifier() -> Unique<asts::TypeIdentifierAst>;

    auto parse_type_array() -> Unique<asts::TypeAst>;
    auto parse_type_tuple() -> Unique<asts::TypeAst>;
    auto parse_type_tuple_0_types() -> Unique<asts::TypeAst>;
    auto parse_type_tuple_1_types() -> Unique<asts::TypeAst>;
    auto parse_type_tuple_n_types() -> Unique<asts::TypeAst>;

    auto parse_identifier() -> Unique<asts::IdentifierAst>;
    auto parse_numeric_identifier() -> Unique<asts::IdentifierAst>;
    auto parse_self_identifier() -> Unique<asts::IdentifierAst>;
    auto parse_upper_identifier() -> Unique<asts::IdentifierAst>;
    auto parse_identifier_as_expression() -> Unique<asts::ExpressionAst>;

    auto parse_literal() -> Unique<asts::LiteralAst>;
    auto parse_literal_char() -> Unique<asts::CharLiteralAst>;
    auto parse_literal_string() -> Unique<asts::StringLiteralAst>;
    auto parse_literal_float() -> Unique<asts::FloatLiteralAst>;
    auto parse_literal_integer() -> Unique<asts::IntegerLiteralAst>;
    auto parse_literal_boolean() -> Unique<asts::BooleanLiteralAst>;
    auto parse_literal_tuple(std::function<Unique<asts::ExpressionAst>()> &&elem_parser) -> Unique<asts::TupleLiteralAst>;
    auto parse_literal_array(std::function<Unique<asts::ExpressionAst>()> &&elem_parser) -> Unique<asts::ArrayLiteralAst>;

    auto parse_literal_float_b10() -> Unique<asts::FloatLiteralAst>;
    auto parse_literal_integer_b10() -> Unique<asts::IntegerLiteralAst>;
    auto parse_literal_integer_b02() -> Unique<asts::IntegerLiteralAst>;
    auto parse_literal_integer_b08() -> Unique<asts::IntegerLiteralAst>;
    auto parse_literal_integer_b16() -> Unique<asts::IntegerLiteralAst>;
    auto parse_numeric_prefix_op() -> Unique<asts::TokenAst>;
    auto parse_float_suffix_type() -> Unique<asts::TokenAst>;
    auto parse_integer_suffix_type() -> Unique<asts::TokenAst>;

    auto parse_literal_tuple_1_element(std::function<Unique<asts::ExpressionAst>()> &&elem_parser) -> Unique<asts::TupleLiteralAst>;
    auto parse_literal_tuple_n_elements(std::function<Unique<asts::ExpressionAst>()> &&elem_parser) -> Unique<asts::TupleLiteralAst>;

    auto parse_literal_array_repeated_element(std::function<Unique<asts::ExpressionAst>()> &&elem_parser) -> Unique<asts::ArrayLiteralRepeatedElementAst>;
    auto parse_literal_array_explicit_elements(std::function<Unique<asts::ExpressionAst>()> &&elem_parser) -> Unique<asts::ArrayLiteralExplicitElementsAst>;

    auto parse_specific_characters(Str &&s) -> Unique<asts::TokenAst>;
    auto parse_specific_character(char16_t c) -> Unique<asts::TokenAst>;

    auto parse_lexeme_character() -> Unique<asts::TokenAst>;
    auto parse_lexeme_digit() -> Unique<asts::TokenAst>;
    auto parse_lexeme_character_or_digit() -> Unique<asts::TokenAst>;
    auto parse_lexeme_character_or_digit_or_underscore() -> Unique<asts::TokenAst>;
    auto parse_lexeme_bin_integer() -> Unique<asts::TokenAst>;
    auto parse_lexeme_oct_integer() -> Unique<asts::TokenAst>;
    auto parse_lexeme_dec_integer() -> Unique<asts::TokenAst>;
    auto parse_lexeme_hex_integer() -> Unique<asts::TokenAst>;
    auto parse_lexeme_single_quote_char() -> Unique<asts::TokenAst>;
    auto parse_lexeme_double_quote_string() -> Unique<asts::TokenAst>;
    auto parse_lexeme_identifier() -> Unique<asts::TokenAst>;
    auto parse_lexeme_upper_identifier() -> Unique<asts::TokenAst>;

    auto parse_nothing() -> Unique<asts::TokenAst>;
    auto parse_newline() -> Unique<asts::TokenAst>;
    auto parse_space() -> Unique<asts::TokenAst>;

    auto parse_token_left_curly_brace() -> Unique<asts::TokenAst>;
    auto parse_token_right_curly_brace() -> Unique<asts::TokenAst>;
    auto parse_token_left_parenthesis() -> Unique<asts::TokenAst>;
    auto parse_token_right_parenthesis() -> Unique<asts::TokenAst>;
    auto parse_token_left_square_bracket() -> Unique<asts::TokenAst>;
    auto parse_token_right_square_bracket() -> Unique<asts::TokenAst>;
    auto parse_token_colon() -> Unique<asts::TokenAst>;
    auto parse_token_comma() -> Unique<asts::TokenAst>;
    auto parse_token_assign() -> Unique<asts::TokenAst>;
    auto parse_token_at() -> Unique<asts::TokenAst>;
    auto parse_token_underscore() -> Unique<asts::TokenAst>;
    auto parse_token_less_than() -> Unique<asts::TokenAst>;
    auto parse_token_greater_than() -> Unique<asts::TokenAst>;
    auto parse_token_add() -> Unique<asts::TokenAst>;
    auto parse_token_sub() -> Unique<asts::TokenAst>;
    auto parse_token_mul() -> Unique<asts::TokenAst>;
    auto parse_token_div() -> Unique<asts::TokenAst>;
    auto parse_token_rem() -> Unique<asts::TokenAst>;
    auto parse_token_bit_ior() -> Unique<asts::TokenAst>;
    auto parse_token_bit_xor() -> Unique<asts::TokenAst>;
    auto parse_token_bit_and() -> Unique<asts::TokenAst>;
    auto parse_token_dot() -> Unique<asts::TokenAst>;
    auto parse_token_question_mark() -> Unique<asts::TokenAst>;
    auto parse_token_exclamation_mark() -> Unique<asts::TokenAst>;
    auto parse_token_deref() -> Unique<asts::TokenAst>;
    auto parse_token_borrow() -> Unique<asts::TokenAst>;
    auto parse_token_vertical_bar() -> Unique<asts::TokenAst>;
    auto parse_token_semicolon() -> Unique<asts::TokenAst>;
    auto parse_token_single_quote() -> Unique<asts::TokenAst>;
    auto parse_token_double_quote() -> Unique<asts::TokenAst>;
    auto parse_token_dollar() -> Unique<asts::TokenAst>;
    auto parse_token_arrow_right() -> Unique<asts::TokenAst>;
    auto parse_token_double_dot() -> Unique<asts::TokenAst>;
    auto parse_token_double_colon() -> Unique<asts::TokenAst>;
    auto parse_token_equals() -> Unique<asts::TokenAst>;
    auto parse_token_not_equals() -> Unique<asts::TokenAst>;
    auto parse_token_less_than_equals() -> Unique<asts::TokenAst>;
    auto parse_token_greater_than_equals() -> Unique<asts::TokenAst>;
    auto parse_token_add_assign() -> Unique<asts::TokenAst>;
    auto parse_token_sub_assign() -> Unique<asts::TokenAst>;
    auto parse_token_mul_assign() -> Unique<asts::TokenAst>;
    auto parse_token_div_assign() -> Unique<asts::TokenAst>;
    auto parse_token_rem_assign() -> Unique<asts::TokenAst>;
    auto parse_token_pow() -> Unique<asts::TokenAst>;
    auto parse_token_bit_shl() -> Unique<asts::TokenAst>;
    auto parse_token_bit_shr() -> Unique<asts::TokenAst>;
    auto parse_token_bit_ior_assign() -> Unique<asts::TokenAst>;
    auto parse_token_bit_xor_assign() -> Unique<asts::TokenAst>;
    auto parse_token_bit_and_assign() -> Unique<asts::TokenAst>;
    auto parse_token_pow_assign() -> Unique<asts::TokenAst>;
    auto parse_token_bit_shl_assign() -> Unique<asts::TokenAst>;
    auto parse_token_bit_shr_assign() -> Unique<asts::TokenAst>;

    auto parse_keyword_cls() -> Unique<asts::TokenAst>;
    auto parse_keyword_fun() -> Unique<asts::TokenAst>;
    auto parse_keyword_cor() -> Unique<asts::TokenAst>;
    auto parse_keyword_sup() -> Unique<asts::TokenAst>;
    auto parse_keyword_ext() -> Unique<asts::TokenAst>;
    auto parse_keyword_mut() -> Unique<asts::TokenAst>;
    auto parse_keyword_use() -> Unique<asts::TokenAst>;
    auto parse_keyword_cmp() -> Unique<asts::TokenAst>;
    auto parse_keyword_let() -> Unique<asts::TokenAst>;
    auto parse_keyword_type() -> Unique<asts::TokenAst>;
    auto parse_keyword_self() -> Unique<asts::TokenAst>;
    auto parse_keyword_case() -> Unique<asts::TokenAst>;
    auto parse_keyword_of() -> Unique<asts::TokenAst>;
    auto parse_keyword_loop() -> Unique<asts::TokenAst>;
    auto parse_keyword_in() -> Unique<asts::TokenAst>;
    auto parse_keyword_else() -> Unique<asts::TokenAst>;
    auto parse_keyword_gen() -> Unique<asts::TokenAst>;
    auto parse_keyword_with() -> Unique<asts::TokenAst>;
    auto parse_keyword_ret() -> Unique<asts::TokenAst>;
    auto parse_keyword_exit() -> Unique<asts::TokenAst>;
    auto parse_keyword_skip() -> Unique<asts::TokenAst>;
    auto parse_keyword_is() -> Unique<asts::TokenAst>;
    auto parse_keyword_as() -> Unique<asts::TokenAst>;
    auto parse_keyword_or() -> Unique<asts::TokenAst>;
    auto parse_keyword_and() -> Unique<asts::TokenAst>;
    auto parse_keyword_not() -> Unique<asts::TokenAst>;
    auto parse_keyword_async() -> Unique<asts::TokenAst>;
    auto parse_keyword_true() -> Unique<asts::TokenAst>;
    auto parse_keyword_false() -> Unique<asts::TokenAst>;
    auto parse_keyword_res() -> Unique<asts::TokenAst>;
    auto parse_keyword_caps() -> Unique<asts::TokenAst>;

    auto parse_token_raw(lex::RawTokenType tok, lex::SppTokenType mapped_tok) -> Unique<asts::TokenAst>;

private:
    auto m_store_error(std::size_t pos, Str &&err_str) const -> bool;
};
