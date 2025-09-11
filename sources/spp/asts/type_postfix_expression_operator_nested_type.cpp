#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_nested_type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


spp::asts::TypePostfixExpressionOperatorNestedTypeAst::TypePostfixExpressionOperatorNestedTypeAst(
    decltype(tok_sep) &&tok_sep,
    decltype(name) &&name) :
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


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypePostfixExpressionOperatorNestedTypeAst>(
        ast_clone(tok_sep),
        ast_clone(name));
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


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::equals(
    const TypePostfixExpressionOperatorAst &other) const
    -> std::weak_ordering {
    return other.equals_nested_type(*this);
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::equals_nested_type(
    TypePostfixExpressionOperatorNestedTypeAst const &other) const
    -> std::weak_ordering {
    // Compare the members for equality.
    return *name <=> *other.name;
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::ns_parts(
    ) const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    return {};
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::ns_parts(
    ) -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {};
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::type_parts(
    ) const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    return {name};
}


auto spp::asts::TypePostfixExpressionOperatorNestedTypeAst::type_parts(
    ) -> std::vector<std::shared_ptr<TypeIdentifierAst>> {
    return {name};
}
