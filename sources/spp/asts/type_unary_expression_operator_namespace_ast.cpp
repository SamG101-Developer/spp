#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::TypeUnaryExpressionOperatorNamespaceAst::TypeUnaryExpressionOperatorNamespaceAst(
    decltype(ns) &&ns,
    decltype(tok_sep) &&tok_sep):
    TypeUnaryExpressionOperatorAst(),
    ns(std::move(ns)),
    tok_sep(std::move(tok_sep)) {
}


spp::asts::TypeUnaryExpressionOperatorNamespaceAst::~TypeUnaryExpressionOperatorNamespaceAst() = default;


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::pos_start() const -> std::size_t {
    return ns->pos_start();
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::pos_end() const -> std::size_t {
    return ns->pos_end();
}


spp::asts::TypeUnaryExpressionOperatorNamespaceAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(ns);
    raw_string.append("::");
    SPP_STRING_END;
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(ns);
    formatted_string.append("::");
    SPP_PRINT_END;
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::ns_parts() const -> std::vector<IdentifierAst const *> {
    return {ns.get()};
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::type_parts() const -> std::vector<TypeIdentifierAst const*> {
    return {};
}
