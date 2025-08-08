#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_nested_type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


spp::asts::TypePostfixExpressionOperatorNestedTypeAst::TypePostfixExpressionOperatorNestedTypeAst(
    decltype(tok_sep) &&tok_sep,
    decltype(name) &&name):
    TypePostfixExpressionOperatorAst(),
    tok_sep(std::move(tok_sep)),
    name(std::move(name)) {
}


spp::asts::TypePostfixExpressionOperatorNestedTypeAst::~TypePostfixExpressionOperatorNestedTypeAst() = default;


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::pos_start() const -> std::size_t {
    return tok_sep->pos_start();
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


spp::asts::TypePostfixExpressionOperatorNestedTypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sep);
    SPP_STRING_APPEND(name);
    SPP_STRING_END;
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sep);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_END;
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::ns_parts() const -> std::vector<IdentifierAst const*> {
    return {};
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::type_parts() const -> std::vector<TypeIdentifierAst const*> {
    return {name.get()};
}
