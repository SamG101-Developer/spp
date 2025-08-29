#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::TypeUnaryExpressionOperatorNamespaceAst::TypeUnaryExpressionOperatorNamespaceAst(
    decltype(ns) ns,
    decltype(tok_sep) &&tok_sep) :
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


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<TypeUnaryExpressionOperatorNamespaceAst>(
        ast_clone(ns),
        ast_clone(tok_sep));
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


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::equals(
    TypeUnaryExpressionOperatorAst const &other) const
    -> bool {
    // Double dispatch to the appropriate equals method.
    return other.equals_op_namespace(*this);
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::equals_op_namespace(
    TypeUnaryExpressionOperatorNamespaceAst const &other) const
    -> bool {
    // Check if the namespace identifiers are the same.
    return *ns == *other.ns;
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::ns_parts(
    ) const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    return {ns};
}


auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::type_parts(
    ) const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    return {};
}
