#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_postfix_expression_operator_optional_ast.hpp>


spp::asts::TypePostfixExpressionOperatorOptionalAst::TypePostfixExpressionOperatorOptionalAst(
    decltype(tok_qst) &&tok_qst) :
    TypePostfixExpressionOperatorAst(),
    tok_qst(std::move(tok_qst)) {
}


spp::asts::TypePostfixExpressionOperatorOptionalAst::~TypePostfixExpressionOperatorOptionalAst() = default;


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::pos_start() const -> std::size_t {
    return tok_qst->pos_start();
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::pos_end() const -> std::size_t {
    return tok_qst->pos_end();
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypePostfixExpressionOperatorOptionalAst>(
        ast_clone(tok_qst));
}


spp::asts::TypePostfixExpressionOperatorOptionalAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_qst);
    SPP_STRING_END;
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_qst);
    SPP_PRINT_END;
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::equals(
    const TypePostfixExpressionOperatorAst &other) const
    -> bool {
    return other.equals_optional(*this);
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::equals_optional(
    TypePostfixExpressionOperatorOptionalAst const &) const
    -> bool {
    return true;
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> {
    return {};
}


auto spp::asts::TypePostfixExpressionOperatorOptionalAst::type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    return {};
}
