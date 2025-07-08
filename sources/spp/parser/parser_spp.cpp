#include <numeric>
#include <spp/parse/parser_spp.hpp>

#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/ast.hpp>
#include <spp/asts/binary_expression_ast.hpp>
#include <spp/asts/binary_expression_temp_ast.hpp>
#include <spp/asts/boolean_literal_ast.hpp>
#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/case_pattern_variant_destructure_attribute_binding_ast.hpp>
#include <spp/asts/case_pattern_variant_destructure_array_ast.hpp>
#include <spp/asts/case_pattern_variant_destructure_object_ast.hpp>
#include <spp/asts/case_pattern_variant_destructure_tuple_ast.hpp>
#include <spp/asts/case_pattern_variant_destructure_skip_single_argument_ast.hpp>
#include <spp/asts/case_pattern_variant_destructure_skip_multiple_arguments_ast.hpp>
#include <spp/asts/case_pattern_variant_else_ast.hpp>
#include <spp/asts/case_pattern_variant_else_case_ast.hpp>
#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/case_pattern_variant_literal_ast.hpp>
#include <spp/asts/case_pattern_variant_single_identifier_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/coroutine_prototype_ast.hpp>
#include <spp/asts/closure_expression_ast.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/float_literal_ast.hpp>
#include <spp/asts/fold_expression_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_call_argument_ast.hpp>
#include <spp/asts/function_call_argument_keyword_ast.hpp>
#include <spp/asts/function_call_argument_positional_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_required_ast.hpp>
#include <spp/asts/function_parameter_optional_ast.hpp>
#include <spp/asts/function_parameter_variadic_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/gen_expression_ast.hpp>
#include <spp/asts/gen_with_expression_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_comp_ast.hpp>
#include <spp/asts/generic_argument_comp_positional_ast.hpp>
#include <spp/asts/generic_argument_comp_keyword_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_comp_ast.hpp>
#include <spp/asts/generic_parameter_comp_required_ast.hpp>
#include <spp/asts/generic_parameter_comp_optional_ast.hpp>
#include <spp/asts/generic_parameter_comp_variadic_ast.hpp>
#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/generic_parameter_type_required_ast.hpp>
#include <spp/asts/generic_parameter_type_optional_ast.hpp>
#include <spp/asts/generic_parameter_type_variadic_ast.hpp>
#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/is_expression_ast.hpp>
#include <spp/asts/is_expression_temp_ast.hpp>
#include <spp/asts/iter_expression_ast.hpp>
#include <spp/asts/iter_expression_branch_ast.hpp>
#include <spp/asts/iter_pattern_variant_ast.hpp>
#include <spp/asts/iter_pattern_variant_exception_ast.hpp>
#include <spp/asts/iter_pattern_variant_exhausted_ast.hpp>
#include <spp/asts/iter_pattern_variant_no_value_ast.hpp>
#include <spp/asts/iter_pattern_variant_variable_ast.hpp>
#include <spp/asts/let_statement_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/let_statement_uninitialized_ast.hpp>
#include <spp/asts/literal_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/loop_condition_ast.hpp>
#include <spp/asts/loop_condition_boolean_ast.hpp>
#include <spp/asts/loop_condition_iterable_ast.hpp>
#include <spp/asts/loop_control_flow_statement_ast.hpp>
#include <spp/asts/loop_else_statement_ast.hpp>
#include <spp/asts/loop_expression_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/parenthesised_expression.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/postfix_expression_operator_early_return_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_keyword_not_ast.hpp>
#include <spp/asts/postfix_expression_operator_keyword_res_ast.hpp>
#include <spp/asts/ret_statement_ast.hpp>
#include <spp/asts/string_literal_ast.hpp>
#include <spp/asts/subroutine_prototype_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>
#include <spp/asts/unary_expression_ast.hpp>
#include <spp/asts/unary_expression_operator_ast.hpp>
#include <spp/asts/unary_expression_operator_async_ast.hpp>
#include <spp/asts/use_statement_ast.hpp>

#include <spp/parse/parser_errors.hpp>

#include "spp/asts/case_pattern_variant_ast.hpp"


auto spp::parse::ParserSpp::parse() -> std::unique_ptr<asts::ModulePrototypeAst> {
    return parse_root();
}


auto spp::parse::ParserSpp::parse_root() -> std::unique_ptr<asts::ModulePrototypeAst> {
    PARSE_ONCE(p1, parse_module_prototype)
    PARSE_ONCE(_a, parse_eof);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_eof() -> std::unique_ptr<asts::TokenAst> {
    auto p1 = parse_token_raw(lex::RawTokenType::SP_EOF, lex::SppTokenType::SP_NO_TOK);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_module_prototype() -> std::unique_ptr<asts::ModulePrototypeAst> {
    PARSE_ONCE(p1, parse_module_implementation);
    return CREATE_AST(asts::ModulePrototypeAst, p1);
}


auto spp::parse::ParserSpp::parse_module_implementation() -> std::unique_ptr<asts::ModuleImplementationAst> {
    PARSE_ZERO_OR_MORE(p1, parse_module_member, parse_newline);
    return CREATE_AST(asts::ModuleImplementationAst, p1);
}


auto spp::parse::ParserSpp::parse_module_member() -> std::unique_ptr<asts::ModuleMemberAst> {
    PARSE_ALTERNATE(
        p1, asts::ModuleMemberAst, parse_function_prototype, parse_class_prototype, parse_sup_prototype_functions,
        parse_sup_prototype_extension, parse_global_use_statement, parse_global_type_statement,
        parse_global_cmp_statement);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_class_prototype() -> std::unique_ptr<asts::ClassPrototypeAst> {
    PARSE_ZERO_OR_MORE(p1, parse_annotation, parse_newline);
    PARSE_ONCE(p2, parse_keyword_cls);
    PARSE_ONCE(p3, parse_upper_identifier);
    PARSE_OPTIONAL(p4, parse_generic_parameter_group);
    PARSE_ONCE(p5, parse_class_implementation);
    return CREATE_AST(asts::ClassPrototypeAst, p1, p2, p3, p4, p5);
}


auto spp::parse::ParserSpp::parse_class_implementation() -> std::unique_ptr<asts::ClassImplementationAst> {
    PARSE_ONCE(p1, [this] { return parse_inner_scope([this] { return parse_class_member(); }); });
    CREATE_AST_WITH_BASE(p2, asts::ClassImplementationAst, p1);
    return FORWARD_AST(p2);
}


auto spp::parse::ParserSpp::parse_class_member() -> std::unique_ptr<asts::ClassMemberAst> {
    PARSE_ALTERNATE(p1, asts::ClassMemberAst, parse_class_attribute);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_class_attribute() -> std::unique_ptr<asts::ClassAttributeAst> {
    PARSE_ZERO_OR_MORE(p1, parse_annotation, parse_newline);
    PARSE_ONCE(p2, parse_identifier);
    PARSE_ONCE(p3, parse_token_colon);
    PARSE_ONCE(p4, parse_type_expression);
    PARSE_OPTIONAL(p5, parse_class_attribute_default_value);
    return CREATE_AST(asts::ClassAttributeAst, p1, p2, p3, p4, p5);
}


auto spp::parse::ParserSpp::parse_class_attribute_default_value() -> std::unique_ptr<asts::ExpressionAst> {
    PARSE_ONCE(p1, parse_token_assign);
    PARSE_ONCE(p2, parse_expression);
    return FORWARD_AST(p2);
}


auto spp::parse::ParserSpp::parse_sup_prototype_functions() -> std::unique_ptr<asts::SupPrototypeFunctionsAst> {
    PARSE_ONCE(p1, parse_keyword_sup);
    PARSE_OPTIONAL(p2, parse_generic_parameter_group);
    PARSE_ONCE(p3, parse_type_expression);
    PARSE_ONCE(p4, parse_sup_implementation);
    return CREATE_AST(asts::SupPrototypeFunctionsAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_sup_prototype_extension() -> std::unique_ptr<asts::SupPrototypeExtensionAst> {
    PARSE_ONCE(p1, parse_keyword_sup);
    PARSE_OPTIONAL(p2, parse_generic_parameter_group);
    PARSE_ONCE(p3, parse_type_expression);
    PARSE_ONCE(p4, parse_token_assign);
    PARSE_ONCE(p5, parse_type_expression);
    PARSE_ONCE(p6, parse_sup_implementation);
    return CREATE_AST(asts::SupPrototypeExtensionAst, p1, p2, p3, p4, p5, p6);
}


auto spp::parse::ParserSpp::parse_sup_implementation() -> std::unique_ptr<asts::SupImplementationAst> {
    PARSE_ONCE(p1, [this] { return parse_inner_scope([this] { return parse_sup_member(); }); });
    CREATE_AST_WITH_BASE(p2, asts::SupImplementationAst, p1);
    return FORWARD_AST(p2);
}


auto spp::parse::ParserSpp::parse_sup_member() -> std::unique_ptr<asts::SupMemberAst> {
    PARSE_ALTERNATE(
        p1, asts::SupMemberAst, parse_sup_method_prototype, parse_sup_type_statement, parse_sup_cmp_statement);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_sup_method_prototype() -> std::unique_ptr<asts::SupMethodPrototypeAst> {
    PARSE_ONCE(p1, parse_function_prototype);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_sup_type_statement() -> std::unique_ptr<asts::SupTypeStatementAst> {
    PARSE_ZERO_OR_MORE(p1, parse_annotation, parse_newline);
    PARSE_ONCE(p2, parse_type_statement);
    p2->annotations = std::move(p1);
    return FORWARD_AST(p2);
}


auto spp::parse::ParserSpp::parse_sup_cmp_statement() -> std::unique_ptr<asts::SupCmpStatementAst> {
    PARSE_ZERO_OR_MORE(p1, parse_annotation, parse_newline);
    PARSE_ONCE(p2, parse_cmp_statement);
    p2->annotations = std::move(p1);
    return FORWARD_AST(p2);
}


auto spp::parse::ParserSpp::parse_function_prototype() -> std::unique_ptr<asts::FunctionPrototypeAst> {
    PARSE_ALTERNATE(p1, asts::FunctionPrototypeAst, parse_subroutine_prototype, parse_coroutine_prototype);
}


auto spp::parse::ParserSpp::parse_subroutine_prototype() -> std::unique_ptr<asts::SubroutinePrototypeAst> {
    PARSE_ZERO_OR_MORE(p1, parse_annotation, parse_newline);
    PARSE_ONCE(p2, parse_keyword_fun);
    PARSE_ONCE(p3, parse_identifier);
    PARSE_OPTIONAL(p4, parse_generic_parameter_group);
    PARSE_ONCE(p5, parse_function_parameter_group);
    PARSE_ONCE(p6, parse_token_arrow_right);
    PARSE_ONCE(p7, parse_type_expression);
    PARSE_ONCE(p8, parse_function_implementation);
    return CREATE_AST(asts::SubroutinePrototypeAst, p1, p2, p3, p4, p5, p6, p7, p8);
}


auto spp::parse::ParserSpp::parse_coroutine_prototype() -> std::unique_ptr<asts::CoroutinePrototypeAst> {
    PARSE_ZERO_OR_MORE(p1, parse_annotation, parse_newline);
    PARSE_ONCE(p2, parse_keyword_cor);
    PARSE_ONCE(p3, parse_identifier);
    PARSE_OPTIONAL(p4, parse_generic_parameter_group);
    PARSE_ONCE(p5, parse_function_parameter_group);
    PARSE_ONCE(p6, parse_token_arrow_right);
    PARSE_ONCE(p7, parse_type_expression);
    PARSE_ONCE(p8, parse_function_implementation);
    return CREATE_AST(asts::CoroutinePrototypeAst, p1, p2, p3, p4, p5, p6, p7, p8);
}


auto spp::parse::ParserSpp::parse_function_implementation() -> std::unique_ptr<asts::FunctionImplementationAst> {
    PARSE_ONCE(p1, [this] { return parse_inner_scope([this] { return parse_function_member(); }); });
    CREATE_AST_WITH_BASE(p2, asts::FunctionImplementationAst, p1);
    return FORWARD_AST(p2);
}


auto spp::parse::ParserSpp::parse_function_member() -> std::unique_ptr<asts::FunctionMemberAst> {
    PARSE_ONCE(p1, parse_statement);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_function_parameter_group() -> std::unique_ptr<asts::FunctionParameterGroupAst> {
    PARSE_ONCE(p1, parse_token_left_parenthesis);
    PARSE_ZERO_OR_MORE(p2, parse_function_parameter, parse_token_comma);
    PARSE_ONCE(p3, parse_token_right_parenthesis);
    return CREATE_AST(asts::FunctionParameterGroupAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_function_parameter() -> std::unique_ptr<asts::FunctionParameterAst> {
    PARSE_ALTERNATE(
        p1, asts::FunctionParameterAst, parse_function_parameter_self, parse_function_parameter_required,
        parse_function_parameter_optional, parse_function_parameter_variadic);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_function_parameter_self() -> std::unique_ptr<asts::FunctionParameterSelfAst> {
    PARSE_ONCE(p1, parse_keyword_mut);
    PARSE_ONCE(p2, parse_convention);
    PARSE_ONCE(p3, parse_self_identifier);
    return CREATE_AST(asts::FunctionParameterSelfAst, p2, CREATE_AST(asts::LocalVariableSingleIdentifierAst, p1, p3, nullptr));
}


auto spp::parse::ParserSpp::parse_function_parameter_required() -> std::unique_ptr<asts::FunctionParameterRequiredAst> {
    PARSE_ONCE(p1, parse_local_variable);
    PARSE_ONCE(p2, parse_token_colon);
    PARSE_ONCE(p3, parse_type_expression);
    return CREATE_AST(asts::FunctionParameterRequiredAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_function_parameter_optional() -> std::unique_ptr<asts::FunctionParameterOptionalAst> {
    PARSE_ONCE(p1, parse_local_variable);
    PARSE_ONCE(p2, parse_token_colon);
    PARSE_ONCE(p3, parse_type_expression);
    PARSE_ONCE(p4, parse_token_assign);
    PARSE_ONCE(p5, parse_expression);
    return CREATE_AST(asts::FunctionParameterOptionalAst, p1, p2, p3, p4, p5);
}


auto spp::parse::ParserSpp::parse_function_parameter_variadic() -> std::unique_ptr<asts::FunctionParameterVariadicAst> {
    PARSE_ONCE(p1, parse_token_double_dot);
    PARSE_ONCE(p2, parse_local_variable);
    PARSE_ONCE(p3, parse_token_colon);
    PARSE_ONCE(p4, parse_type_expression);
    return CREATE_AST(asts::FunctionParameterVariadicAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_function_call_argument_group() -> std::unique_ptr<asts::FunctionCallArgumentGroupAst> {
    PARSE_ONCE(p1, parse_token_left_parenthesis);
    PARSE_ZERO_OR_MORE(p2, parse_function_call_argument, parse_token_comma);
    PARSE_ONCE(p3, parse_token_right_parenthesis);
    return CREATE_AST(asts::FunctionCallArgumentGroupAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_function_call_argument() -> std::unique_ptr<asts::FunctionCallArgumentAst> {
    PARSE_ALTERNATE(
        p1, asts::FunctionCallArgumentAst, parse_function_call_argument_keyword,
        parse_function_call_argument_positional);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_function_call_argument_positional() -> std::unique_ptr<asts::FunctionCallArgumentPositionalAst> {
    PARSE_OPTIONAL(p1, parse_convention);
    PARSE_OPTIONAL(p2, parse_token_double_dot)
    PARSE_ONCE(p3, parse_expression);
    return CREATE_AST(asts::FunctionCallArgumentPositionalAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_function_call_argument_keyword() -> std::unique_ptr<asts::FunctionCallArgumentKeywordAst> {
    PARSE_ONCE(p1, parse_identifier);
    PARSE_ONCE(p2, parse_token_assign);
    PARSE_OPTIONAL(p3, parse_convention);
    PARSE_ONCE(p4, parse_expression);
    return CREATE_AST(asts::FunctionCallArgumentKeywordAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_generic_parameter_group() -> std::unique_ptr<asts::GenericParameterGroupAst> {
    PARSE_ONCE(p1, parse_token_left_square_bracket);
    PARSE_ZERO_OR_MORE(p2, parse_generic_parameter, parse_token_comma);
    PARSE_ONCE(p3, parse_token_right_square_bracket);
    return CREATE_AST(asts::GenericParameterGroupAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_generic_parameter() -> std::unique_ptr<asts::GenericParameterAst> {
    PARSE_ALTERNATE(
        p1, asts::GenericParameterAst, parse_generic_parameter_comp, parse_generic_parameter_type);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_generic_parameter_comp() -> std::unique_ptr<asts::GenericParameterCompAst> {
    PARSE_ALTERNATE(
        p1, asts::GenericParameterCompAst, parse_generic_parameter_comp_required, parse_generic_parameter_comp_optional,
        parse_generic_parameter_comp_variadic);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_generic_parameter_comp_required() -> std::unique_ptr<asts::GenericParameterCompRequiredAst> {
    PARSE_ONCE(p1, parse_keyword_cmp);
    PARSE_ONCE(p2, parse_identifier);
    PARSE_ONCE(p3, parse_token_colon);
    PARSE_ONCE(p4, parse_type_expression);
    return CREATE_AST(asts::GenericParameterCompRequiredAst, p1, asts::TypeIdentifierAst::from_identifier(*p2), p3, p4);
}


auto spp::parse::ParserSpp::parse_generic_parameter_comp_optional() -> std::unique_ptr<asts::GenericParameterCompOptionalAst> {
    PARSE_ONCE(p1, parse_keyword_cmp);
    PARSE_ONCE(p2, parse_identifier);
    PARSE_ONCE(p3, parse_token_colon);
    PARSE_ONCE(p4, parse_type_expression);
    PARSE_ONCE(p5, parse_token_assign);
    PARSE_ONCE(p6, parse_cmp_value);
    return CREATE_AST(asts::GenericParameterCompOptionalAst, p1, asts::TypeIdentifierAst::from_identifier(*p2), p3, p4, p5, p6);
}


auto spp::parse::ParserSpp::parse_generic_parameter_comp_variadic() -> std::unique_ptr<asts::GenericParameterCompVariadicAst> {
    PARSE_ONCE(p1, parse_keyword_cmp);
    PARSE_ONCE(p2, parse_token_double_dot);
    PARSE_ONCE(p3, parse_identifier);
    PARSE_ONCE(p4, parse_token_colon);
    PARSE_ONCE(p5, parse_type_expression);
    return CREATE_AST(asts::GenericParameterCompVariadicAst, p1, p2, asts::TypeIdentifierAst::from_identifier(*p3), p4, p5);
}


auto spp::parse::ParserSpp::parse_generic_parameter_type() -> std::unique_ptr<asts::GenericParameterTypeAst> {
    PARSE_ALTERNATE(
        p1, asts::GenericParameterTypeAst, parse_generic_parameter_type_required, parse_generic_parameter_type_optional,
        parse_generic_parameter_type_variadic);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_generic_parameter_type_required() -> std::unique_ptr<asts::GenericParameterTypeRequiredAst> {
    PARSE_ONCE(p1, parse_type_identifier);
    PARSE_ONCE(p2, parse_generic_parameter_type_inline_constraints);
    return CREATE_AST(asts::GenericParameterTypeRequiredAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_generic_parameter_type_optional() -> std::unique_ptr<asts::GenericParameterTypeOptionalAst> {
    PARSE_ONCE(p1, parse_type_identifier);
    PARSE_ONCE(p2, parse_generic_parameter_type_inline_constraints);
    PARSE_ONCE(p3, parse_token_assign);
    PARSE_ONCE(p4, parse_type_expression);
    return CREATE_AST(asts::GenericParameterTypeOptionalAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_generic_parameter_type_variadic() -> std::unique_ptr<asts::GenericParameterTypeVariadicAst> {
    PARSE_ONCE(p1, parse_token_double_dot);
    PARSE_ONCE(p2, parse_type_identifier);
    PARSE_ONCE(p3, parse_generic_parameter_type_inline_constraints);
    return CREATE_AST(asts::GenericParameterTypeVariadicAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_generic_parameter_type_inline_constraints() -> std::unique_ptr<asts::GenericParameterTypeInlineConstraintsAst> {
    PARSE_ONCE(p1, parse_token_colon);
    PARSE_ONE_OR_MORE(p2, parse_type_expression, parse_token_comma);
    return CREATE_AST(asts::GenericParameterTypeInlineConstraintsAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_generic_argument_group() -> std::unique_ptr<asts::GenericArgumentGroupAst> {
    PARSE_ONCE(p1, parse_token_left_square_bracket);
    PARSE_ZERO_OR_MORE(p2, parse_generic_argument, parse_token_comma);
    PARSE_ONCE(p3, parse_token_right_square_bracket);
    return CREATE_AST(asts::GenericArgumentGroupAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_generic_argument() -> std::unique_ptr<asts::GenericArgumentAst> {
    PARSE_ALTERNATE(
        p1, asts::GenericArgumentAst, parse_generic_argument_comp, parse_generic_argument_type);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_generic_argument_comp() -> std::unique_ptr<asts::GenericArgumentCompAst> {
    PARSE_ALTERNATE(
        p1, asts::GenericArgumentCompAst, parse_generic_argument_comp_positional, parse_generic_argument_comp_keyword);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_generic_argument_comp_positional() -> std::unique_ptr<asts::GenericArgumentCompPositionalAst> {
    PARSE_ONCE(p1, parse_cmp_value);
    return CREATE_AST(asts::GenericArgumentCompPositionalAst, p1);
}


auto spp::parse::ParserSpp::parse_generic_argument_comp_keyword() -> std::unique_ptr<asts::GenericArgumentCompKeywordAst> {
    PARSE_ONCE(p1, parse_identifier);
    PARSE_ONCE(p2, parse_token_assign);
    PARSE_ONCE(p3, parse_cmp_value);
    return CREATE_AST(asts::GenericArgumentCompKeywordAst, asts::TypeIdentifierAst::from_identifier(*p1), p2, p3);
}


auto spp::parse::ParserSpp::parse_generic_argument_type() -> std::unique_ptr<asts::GenericArgumentTypeAst> {
    PARSE_ALTERNATE(
        p1, asts::GenericArgumentTypeAst, parse_generic_argument_type_positional, parse_generic_argument_type_keyword);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_generic_argument_type_positional() -> std::unique_ptr<asts::GenericArgumentTypePositionalAst> {
    PARSE_ONCE(p1, parse_type_expression);
    return CREATE_AST(asts::GenericArgumentTypePositionalAst, p1);
}


auto spp::parse::ParserSpp::parse_generic_argument_type_keyword() -> std::unique_ptr<asts::GenericArgumentTypeKeywordAst> {
    PARSE_ONCE(p1, parse_upper_identifier);
    PARSE_ONCE(p2, parse_token_assign);
    PARSE_ONCE(p3, parse_type_expression);
    return CREATE_AST(asts::GenericArgumentTypeKeywordAst, asts::TypeIdentifierAst::from_identifier(*p1), p2, p3);
}


auto spp::parse::ParserSpp::parse_annotation() -> std::unique_ptr<asts::AnnotationAst> {
    PARSE_ONCE(p1, parse_token_at);
    PARSE_ONCE(p2, parse_identifier);
    PARSE_OPTIONAL(p3, parse_generic_argument_group);
    return CREATE_AST(asts::AnnotationAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_expression() -> std::unique_ptr<asts::ExpressionAst> {
    PARSE_ONCE(p1, parse_binary_expression_precedence_level_1);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_n_rhs(std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::ExpressionAst>()> &&rhs_parser) -> std::unique_ptr<asts::BinaryExpressionTempAst> {
    PARSE_ONCE(p1, op_parser);
    PARSE_ONCE(p2, rhs_parser);
    return CREATE_AST(asts::BinaryExpressionTempAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_n_rhs_for_is(std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::CasePatternVariantAst>()> &&rhs_parser) -> std::unique_ptr<asts::IsExpressionTempAst> {
    PARSE_ONCE(p1, op_parser);
    PARSE_ONCE(p2, rhs_parser);
    return CREATE_AST(asts::IsExpressionTempAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_n(std::function<std::unique_ptr<asts::ExpressionAst>()> &&lhs_parser, std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::ExpressionAst>()> &&rhs_parser) -> std::unique_ptr<asts::ExpressionAst> {
    auto op_rhs_parser = [this, op_parser, rhs_parser] mutable { return parse_binary_expression_precedence_level_n_rhs(std::move(op_parser), std::move(rhs_parser)); };
    PARSE_ONCE(p1, lhs_parser);
    PARSE_OPTIONAL(p2, op_rhs_parser);
    if (p2 == nullptr) { return p1; }
    return CREATE_AST(asts::BinaryExpressionAst, p1, p2->op, p2->rhs);
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_n_for_is(std::function<std::unique_ptr<asts::ExpressionAst>()> &&lhs_parser, std::function<std::unique_ptr<asts::TokenAst>()> &&op_parser, std::function<std::unique_ptr<asts::CasePatternVariantAst>()> &&rhs_parser) -> std::unique_ptr<asts::ExpressionAst> {
    auto op_rhs_parser = [this, op_parser, rhs_parser] mutable { return parse_binary_expression_precedence_level_n_rhs_for_is(std::move(op_parser), std::move(rhs_parser)); };
    PARSE_ONCE(p1, lhs_parser);
    PARSE_OPTIONAL(p2, op_rhs_parser);
    if (p2 == nullptr) { return p1; }
    return CREATE_AST(asts::IsExpressionAst, p1, p2->op, p2->rhs);
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_1() -> std::unique_ptr<asts::ExpressionAst> {
    return parse_binary_expression_precedence_level_n(
        [this] { return parse_binary_expression_precedence_level_2(); },
        [this] { return parse_binary_expression_op_precedence_level_1(); },
        [this] { return parse_binary_expression_precedence_level_1(); });
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_2() -> std::unique_ptr<asts::ExpressionAst> {
    return parse_binary_expression_precedence_level_n(
        [this] { return parse_binary_expression_precedence_level_3(); },
        [this] { return parse_binary_expression_op_precedence_level_2(); },
        [this] { return parse_binary_expression_precedence_level_2(); });
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_3() -> std::unique_ptr<asts::ExpressionAst> {
    return parse_binary_expression_precedence_level_n_for_is(
        [this] { return parse_binary_expression_precedence_level_4(); },
        [this] { return parse_binary_expression_op_precedence_level_3(); },
        [this] { return parse_case_expression_pattern_variant_destructure(); });
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_4() -> std::unique_ptr<asts::ExpressionAst> {
    return parse_binary_expression_precedence_level_n(
        [this] { return parse_binary_expression_precedence_level_5(); },
        [this] { return parse_binary_expression_op_precedence_level_4(); },
        [this] { return parse_binary_expression_precedence_level_4(); });
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_5() -> std::unique_ptr<asts::ExpressionAst> {
    return parse_binary_expression_precedence_level_n(
        [this] { return parse_binary_expression_precedence_level_6(); },
        [this] { return parse_binary_expression_op_precedence_level_5(); },
        [this] { return parse_binary_expression_precedence_level_5(); });
}


auto spp::parse::ParserSpp::parse_binary_expression_precedence_level_6() -> std::unique_ptr<asts::ExpressionAst> {
    return parse_binary_expression_precedence_level_n(
        [this] { return parse_unary_expression(); },
        [this] { return parse_binary_expression_op_precedence_level_6(); },
        [this] { return parse_binary_expression_precedence_level_6(); });
}


auto spp::parse::ParserSpp::parse_binary_expression_op_precedence_level_1() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ONCE(p1, parse_keyword_or);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_binary_expression_op_precedence_level_2() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ONCE(p1, parse_keyword_and);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_binary_expression_op_precedence_level_3() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ONCE(p1, parse_keyword_is);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_binary_expression_op_precedence_level_4() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ALTERNATE(
        p1, asts::TokenAst, parse_token_equals, parse_token_not_equals, parse_token_less_than, parse_token_greater_than,
        parse_token_less_than_equals, parse_token_greater_than_equals);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_binary_expression_op_precedence_level_5() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ALTERNATE(
        p1, asts::TokenAst, parse_token_plus, parse_token_minus, parse_token_plus_assign, parse_token_minus_assign);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_binary_expression_op_precedence_level_6() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ALTERNATE(
        p1, asts::TokenAst, parse_token_multiply, parse_token_divide, parse_token_remainder, parse_token_exponentiation,
        parse_token_multiply_assign, parse_token_divide_assign, parse_token_remainder_assign,
        parse_token_exponentiation_assign);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_unary_expression() -> std::unique_ptr<asts::ExpressionAst> {
    PARSE_ZERO_OR_MORE(p1, parse_unary_op, parse_nothing);
    PARSE_ONCE(p2, parse_postfix_expression);
    return std::accumulate(
        p1.rbegin(), p1.rend(), std::move(p2),
        [](std::unique_ptr<asts::ExpressionAst> acc, std::unique_ptr<asts::TokenAst> &x) {
            return CREATE_AST(asts::UnaryExpressionAst, x, std::move(acc));
        });
}


auto spp::parse::ParserSpp::parse_unary_op() -> std::unique_ptr<asts::UnaryExpressionOperatorAst> {
    PARSE_ALTERNATE(
        p1, asts::UnaryExpressionOperatorAst, parse_unary_op_async);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_unary_op_async() -> std::unique_ptr<asts::UnaryExpressionOperatorAsyncAst> {
    PARSE_ONCE(p1, parse_keyword_async);
    return CREATE_AST(asts::UnaryExpressionOperatorAsyncAst, p1);
}


auto spp::parse::ParserSpp::parse_postfix_expression() -> std::unique_ptr<asts::ExpressionAst> {
    PARSE_ONCE(p1, parse_primary_expression);
    PARSE_ZERO_OR_MORE(p2, parse_postfix_expression_op, parse_nothing);
    return std::accumulate(
        p2.begin(), p2.end(), std::move(p1),
        [](std::unique_ptr<asts::ExpressionAst> acc, std::unique_ptr<asts::TokenAst> &x) {
            return CREATE_AST(asts::PostfixExpressionAst, std::move(acc), x);
        });
}


auto spp::parse::ParserSpp::parse_postfix_expression_op() -> std::unique_ptr<asts::PostfixExpressionOperatorAst> {
    PARSE_ALTERNATE(
        p1, asts::PostfixExpressionOperatorAst, parse_postfix_expression_op_early_return,
        parse_postfix_expression_op_function_call, parse_postfix_expression_op_runtime_member_access,
        parse_postfix_expression_op_static_member_access, parse_postfix_expression_op_keyword_not,
        parse_postfix_expression_op_keyword_res);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_postfix_expression_op_early_return() -> std::unique_ptr<asts::PostfixExpressionOperatorEarlyReturnAst> {
    PARSE_ONCE(p1, parse_token_question_mark);
    return CREATE_AST(asts::PostfixExpressionOperatorEarlyReturnAst, p1);
}


auto spp::parse::ParserSpp::parse_postfix_expression_op_function_call() -> std::unique_ptr<asts::PostfixExpressionOperatorFunctionCallAst> {
    PARSE_OPTIONAL(p1, parse_generic_argument_group);
    PARSE_ONCE(p2, parse_function_call_argument_group);
    PARSE_ONCE(p3, parse_fold_expression);
    return CREATE_AST(asts::PostfixExpressionOperatorFunctionCallAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_postfix_expression_op_runtime_member_access() -> std::unique_ptr<asts::PostfixExpressionOperatorRuntimeMemberAccessAst> {
    PARSE_ONCE(p1, parse_token_dot);
    PARSE_ALTERNATE(p2, asts::IdentifierAst, parse_identifier, parse_numeric_identifier);
    return CREATE_AST(asts::PostfixExpressionOperatorRuntimeMemberAccessAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_postfix_expression_op_static_member_access() -> std::unique_ptr<asts::PostfixExpressionOperatorStaticMemberAccessAst> {
    PARSE_ONCE(p1, parse_token_double_colon);
    PARSE_ONCE(p2, parse_identifier);
    return CREATE_AST(asts::PostfixExpressionOperatorStaticMemberAccessAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_postfix_expression_op_keyword_not() -> std::unique_ptr<asts::PostfixExpressionOperatorKeywordNotAst> {
    PARSE_ONCE(p1, parse_token_dot);
    PARSE_ONCE(p2, parse_keyword_not);
    return CREATE_AST(asts::PostfixExpressionOperatorKeywordNotAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_postfix_expression_op_keyword_res() -> std::unique_ptr<asts::PostfixExpressionOperatorKeywordResAst> {
    PARSE_ONCE(p1, parse_token_dot);
    PARSE_ONCE(p2, parse_keyword_res);
    PARSE_ONCE(p3, parse_function_call_argument_group)
    return CREATE_AST(asts::PostfixExpressionOperatorKeywordResAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_primary_expression() -> std::unique_ptr<asts::ExpressionAst> {
    PARSE_ALTERNATE(
        p1, asts::PrimaryExpressionAst, parse_closure_expression, parse_literal, parse_object_initializer,
        parse_parenthesised_expression, parse_case_of_expression, parse_case_expression, parse_loop_expression,
        parse_iter_of_expression, parse_gen_expression, parse_gen_unroll_expression, parse_type_expression,
        parse_self_identifier, parse_identifier,
        [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); }, parse_fold_expression);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_parenthesised_expression() -> std::unique_ptr<asts::ParenthesisedExpressionAst> {
    PARSE_ONCE(p1, parse_token_left_parenthesis);
    PARSE_ONCE(p2, parse_expression);
    PARSE_ONCE(p3, parse_token_right_parenthesis);
    return CREATE_AST(asts::ParenthesisedExpressionAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_fold_expression() -> std::unique_ptr<asts::FoldExpressionAst> {
    PARSE_ONCE(p1, parse_token_double_dot);
    return CREATE_AST(asts::FoldExpressionAst, p1);
}


auto spp::parse::ParserSpp::parse_case_expression() -> std::unique_ptr<asts::CaseExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_case);
    PARSE_ONCE(p2, parse_expression);
    PARSE_ONCE(p3, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })

    PARSE_ZERO_OR_MORE(p4, parse_case_expression_branch, parse_nothing);
    return CREATE_AST_CUSTOM(asts::CaseExpressionAst, new_non_pattern_match, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_case_expression_branch() -> std::unique_ptr<asts::CaseExpressionBranchAst> {
    PARSE_ALTERNATE(
        p1, asts::CaseExpressionBranchAst, parse_case_expression_branch_else_case, parse_case_expression_branch_else)
    return p1;
}


auto spp::parse::ParserSpp::parse_case_expression_branch_else() -> std::unique_ptr<asts::CaseExpressionBranchAst> {
    PARSE_ONCE(p1, parse_case_expression_pattern_variant_else);
    PARSE_ONCE(p2, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })
    return CREATE_AST(asts::CaseExpressionBranchAst, nullptr, std::vector{std::move(p1)}, nullptr, p2);
}


auto spp::parse::ParserSpp::parse_case_expression_branch_else_case() -> std::unique_ptr<asts::CaseExpressionBranchAst> {
    PARSE_ONCE(p1, parse_case_expression_pattern_variant_else_case);
    auto else_pattern = std::unique_ptr<asts::CasePatternVariantAst>(CREATE_AST(asts::CasePatternVariantElseAst, nullptr).release());
    auto else_case_body = CREATE_AST(asts::InnerScopeExpressionAst<std::unique_ptr<asts::StatementAst>>, nullptr, std::vector{std::unique_ptr<asts::StatementAst>(dynamic_cast<asts::CasePatternVariantElseCaseAst*>(p1.release())->case_expr.release())}, nullptr);
    return CREATE_AST(asts::CaseExpressionBranchAst, nullptr, std::vector{std::move(else_pattern)}, nullptr, else_case_body);
}


auto spp::parse::ParserSpp::parse_case_of_expression() -> std::unique_ptr<asts::CaseExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_case);
    PARSE_ONCE(p2, parse_expression);
    PARSE_ONCE(p3, parse_keyword_of);
    PARSE_ONE_OR_MORE(p4, parse_case_of_expression_branch, parse_newline);
    return CREATE_AST(asts::CaseExpressionAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_case_of_expression_branch() -> std::unique_ptr<asts::CaseExpressionBranchAst> {
    PARSE_ALTERNATE(
        p1, asts::CaseExpressionBranchAst, parse_case_of_expression_branch_destructuring,
        parse_case_of_expression_branch_comparing, parse_case_expression_branch_else_case,
        parse_case_expression_branch_else);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_case_of_expression_branch_destructuring() -> std::unique_ptr<asts::CaseExpressionBranchAst> {
    PARSE_ONCE(p1, parse_keyword_is);
    PARSE_ONE_OR_MORE(p2, parse_case_expression_pattern_variant_destructure, parse_token_comma);
    PARSE_OPTIONAL(p3, parse_pattern_guard)
    PARSE_ONCE(p4, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })
    return CREATE_AST(asts::CaseExpressionBranchAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_case_of_expression_branch_comparing() -> std::unique_ptr<asts::CaseExpressionBranchAst> {
    PARSE_ONCE(p1, parse_boolean_comparison_op)
    PARSE_ONE_OR_MORE(p2, parse_case_expression_pattern_variant_expression, parse_token_comma);
    PARSE_ONCE(p3, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })
    return CREATE_AST(asts::CaseExpressionBranchAst, p1, p2, nullptr, p3);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::CasePatternVariantAst, parse_case_expression_pattern_variant_destructure_array,
        parse_case_expression_pattern_variant_destructure_object,
        parse_case_expression_pattern_variant_destructure_tuple);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure_array() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_token_left_square_bracket);
    PARSE_ZERO_OR_MORE(p2, parse_case_expression_pattern_nested_for_destructure_array, parse_token_comma);
    PARSE_ONCE(p3, parse_token_right_square_bracket);
    return CREATE_AST(asts::CasePatternVariantDestructureArrayAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure_object() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_type_expression_simple)
    PARSE_ONCE(p2, parse_token_left_parenthesis);
    PARSE_ZERO_OR_MORE(p3, parse_case_expression_pattern_nested_for_destructure_object, parse_token_comma);
    PARSE_ONCE(p4, parse_token_right_parenthesis);
    return CREATE_AST(asts::CasePatternVariantDestructureObjectAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure_tuple() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_token_left_parenthesis);
    PARSE_ZERO_OR_MORE(p2, parse_case_expression_pattern_nested_for_destructure_tuple, parse_token_comma);
    PARSE_ONCE(p3, parse_token_right_parenthesis);
    return CREATE_AST(asts::CasePatternVariantDestructureTupleAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure_skip_single_argument() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_token_underscore);
    return CREATE_AST(asts::CasePatternVariantDestructureSkipSingleArgumentAst, p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure_skip_multiple_arguments() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_token_double_dot);
    PARSE_OPTIONAL(p2, parse_case_expression_pattern_variant_single_identifier);
    return CREATE_AST(asts::CasePatternVariantDestructureSkipMultipleArgumentsAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_destructure_attribute_binding() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_identifier);
    PARSE_ONCE(p2, parse_token_assign);
    PARSE_ONCE(p3, parse_case_expression_pattern_nested_for_destructure_attribute_binding);
    return CREATE_AST(asts::CasePatternVariantDestructureAttributeBindingAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_single_identifier() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_OPTIONAL(p1, parse_keyword_mut);
    PARSE_ONCE(p2, parse_identifier);
    PARSE_OPTIONAL(p3, parse_local_variable_single_identifier_alias);
    return CREATE_AST(asts::CasePatternVariantSingleIdentifierAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_literal() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::LiteralAst, parse_literal_float, parse_literal_integer, parse_literal_string, parse_literal_boolean);
    return CREATE_AST(asts::CasePatternVariantLiteralAst, p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_expression() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_expression);
    return CREATE_AST(asts::CasePatternVariantExpressionAst, p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_else() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_keyword_else);
    return CREATE_AST(asts::CasePatternVariantElseAst, p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_variant_else_case() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ONCE(p1, parse_keyword_else);
    PARSE_ONCE(p2, parse_case_expression);
    return CREATE_AST(asts::CasePatternVariantElseCaseAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_nested_for_destructure_array() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::CasePatternVariantAst, parse_case_expression_pattern_variant_destructure_skip_single_argument,
        parse_case_expression_pattern_variant_destructure_skip_multiple_arguments,
        parse_case_expression_pattern_variant_destructure_array,
        parse_case_expression_pattern_variant_destructure_tuple,
        parse_case_expression_pattern_variant_destructure_object,
        parse_case_expression_pattern_variant_single_identifier,
        parse_case_expression_pattern_variant_literal);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_nested_for_destructure_object() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::CasePatternVariantAst, parse_case_expression_pattern_variant_destructure_skip_single_argument,
        parse_case_expression_pattern_variant_destructure_skip_multiple_arguments,
        parse_case_expression_pattern_variant_destructure_array,
        parse_case_expression_pattern_variant_destructure_tuple,
        parse_case_expression_pattern_variant_destructure_object,
        parse_case_expression_pattern_variant_single_identifier,
        parse_case_expression_pattern_variant_literal);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_nested_for_destructure_tuple() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::CasePatternVariantAst, parse_case_expression_pattern_variant_destructure_skip_multiple_arguments,
        parse_case_expression_pattern_variant_destructure_attribute_binding,
        parse_case_expression_pattern_variant_literal);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_case_expression_pattern_nested_for_destructure_attribute_binding() -> std::unique_ptr<asts::CasePatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::CasePatternVariantAst, parse_case_expression_pattern_variant_destructure_array,
        parse_case_expression_pattern_variant_destructure_object,
        parse_case_expression_pattern_variant_destructure_tuple,
        parse_case_expression_pattern_variant_literal);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_pattern_guard() -> std::unique_ptr<asts::PatternGuardAst> {
    PARSE_ONCE(p1, parse_keyword_and);
    PARSE_ONCE(p2, parse_expression);
    return CREATE_AST(asts::PatternGuardAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_boolean_comparison_op() -> std::unique_ptr<asts::TokenAst> {
    PARSE_ALTERNATE(
        p1, asts::TokenAst, parse_token_equals, parse_token_not_equals, parse_token_less_than, parse_token_greater_than,
        parse_token_less_than_equals, parse_token_greater_than_equals);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_loop_expression() -> std::unique_ptr<asts::LoopExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_loop);
    PARSE_ONCE(p2, parse_loop_condition);
    PARSE_ONCE(p3, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })
    PARSE_OPTIONAL(p4, parse_loop_else_statement);
    return CREATE_AST(asts::LoopExpressionAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_loop_condition() -> std::unique_ptr<asts::LoopConditionAst> {
    PARSE_ALTERNATE(
        p1, asts::LoopConditionAst, parse_loop_condition_boolean, parse_loop_condition_iterable);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_loop_condition_boolean() -> std::unique_ptr<asts::LoopConditionBooleanAst> {
    PARSE_ONCE(p1, parse_expression);
    return CREATE_AST(asts::LoopConditionBooleanAst, p1);
}


auto spp::parse::ParserSpp::parse_loop_condition_iterable() -> std::unique_ptr<asts::LoopConditionIterableAst> {
    PARSE_ONCE(p1, parse_local_variable);
    PARSE_OPTIONAL(p2, parse_keyword_in);
    PARSE_OPTIONAL(p3, parse_expression);
    return CREATE_AST(asts::LoopConditionIterableAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_loop_else_statement() -> std::unique_ptr<asts::LoopElseStatementAst> {
    PARSE_ONCE(p1, parse_keyword_else);
    PARSE_ONCE(p2, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })
    return CREATE_AST(asts::LoopElseStatementAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_iter_of_expression() -> std::unique_ptr<asts::IterExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_iter);
    PARSE_ONCE(p2, parse_expression);
    PARSE_ONCE(p3, parse_keyword_of);
    PARSE_ONE_OR_MORE(p4, parse_iter_of_expression_branch, parse_newline);
    return CREATE_AST(asts::IterExpressionAst, p1, p2, p3, p4);
}


auto spp::parse::ParserSpp::parse_iter_of_expression_branch() -> std::unique_ptr<asts::IterExpressionBranchAst> {
    PARSE_ONCE(p1, parse_iter_expression_pattern_variant)
    PARSE_ONCE(p2, [this] { return parse_inner_scope_expression([this] { return parse_statement(); }); })
    PARSE_OPTIONAL(p3, parse_pattern_guard);
    return CREATE_AST(asts::IterExpressionBranchAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_iter_expression_pattern_variant() -> std::unique_ptr<asts::IterPatternVariantAst> {
    PARSE_ALTERNATE(
        p1, asts::IterPatternVariantAst, parse_iter_expression_pattern_variant_no_value,
        parse_iter_expression_pattern_variant_exhausted, parse_iter_expression_pattern_variant_exception,
        parse_iter_expression_pattern_variant_variable);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_iter_expression_pattern_variant_variable() -> std::unique_ptr<asts::IterPatternVariantVariableAst> {
    PARSE_ONCE(p1, parse_local_variable);
    return CREATE_AST(asts::IterPatternVariantVariableAst, p1);
}


auto spp::parse::ParserSpp::parse_iter_expression_pattern_variant_exhausted() -> std::unique_ptr<asts::IterPatternVariantExhaustedAst> {
    PARSE_ONCE(p1, parse_token_double_exclamation_mark);
    return CREATE_AST(asts::IterPatternVariantExhaustedAst, p1);
}


auto spp::parse::ParserSpp::parse_iter_expression_pattern_variant_no_value() -> std::unique_ptr<asts::IterPatternVariantNoValueAst> {
    PARSE_ONCE(p1, parse_token_underscore);
    return CREATE_AST(asts::IterPatternVariantNoValueAst, p1);
}


auto spp::parse::ParserSpp::parse_iter_expression_pattern_variant_exception() -> std::unique_ptr<asts::IterPatternVariantExceptionAst> {
    PARSE_ONCE(p1, parse_token_exclamation_mark);
    PARSE_ONCE(p2, parse_local_variable);
    return CREATE_AST(asts::IterPatternVariantExceptionAst, p1, p2);
}


auto spp::parse::ParserSpp::parse_gen_expression() -> std::unique_ptr<asts::GenExpressionAst> {
    PARSE_ALTERNATE(
        p1, asts::GenExpressionAst, parse_gen_expression_with_expression, parse_gen_expression_without_expression);
    return FORWARD_AST(p1);
}


auto spp::parse::ParserSpp::parse_gen_expression_with_expression() -> std::unique_ptr<asts::GenExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_gen);
    PARSE_OPTIONAL(p2, parse_convention);
    PARSE_ONCE(p3, parse_expression);
    return CREATE_AST(asts::GenExpressionAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_gen_expression_without_expression() -> std::unique_ptr<asts::GenExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_gen);
    return CREATE_AST(asts::GenExpressionAst, p1, nullptr, nullptr);
}


auto spp::parse::ParserSpp::parse_gen_unroll_expression() -> std::unique_ptr<asts::GenWithExpressionAst> {
    PARSE_ONCE(p1, parse_keyword_gen);
    PARSE_ONCE(p2, parse_keyword_with);
    PARSE_ONCE(p3, parse_expression);
    return CREATE_AST(asts::GenWithExpressionAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_inner_scope(auto &&parser) -> std::unique_ptr<asts::InnerScopeAst<decltype(parser())>> {
    PARSE_ONCE(p1, parse_token_left_curly_brace);
    PARSE_ZERO_OR_MORE(p2, std::forward<decltype(parser)>(parser), parse_newline);
    PARSE_ONCE(p3, parse_token_right_curly_brace);
    return CREATE_AST(asts::InnerScopeAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_inner_scope_expression(auto &&parser) -> std::unique_ptr<asts::InnerScopeExpressionAst<decltype(parser())>> {
    PARSE_ONCE(p1, parse_token_left_curly_brace);
    PARSE_ZERO_OR_MORE(p2, std::forward<decltype(parser)>(parser), parse_newline);
    PARSE_ONCE(p3, parse_token_right_curly_brace);
    return CREATE_AST(asts::InnerScopeExpressionAst, p1, p2, p3);
}


auto spp::parse::ParserSpp::parse_statement() -> std::unique_ptr<asts::StatementAst> {
    PARSE_ALTERNATE(
        p1, asts::StatementAst, parse_use_statement, parse_type_statement, parse_let_statement, parse_ret_statement,
        parse_exit_statement, parse_skip_statement, parse_assignment_statement, parse_expression);
    return FORWARD_AST(p1);
}

