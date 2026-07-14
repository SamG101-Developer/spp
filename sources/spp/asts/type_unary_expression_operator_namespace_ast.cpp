module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::TypeUnaryExpressionOperatorNamespaceAst::TypeUnaryExpressionOperatorNamespaceAst(
    decltype(Ns) ns,
    decltype(TokSep) &&tok_sep) :
    TypeUnaryExpressionOperatorAst(),
    Ns(std::move(ns)),
    TokSep(std::move(tok_sep)) {
}

spp::asts::TypeUnaryExpressionOperatorNamespaceAst::~TypeUnaryExpressionOperatorNamespaceAst() = default;

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::EqualsOpNamespace(
    TypeUnaryExpressionOperatorNamespaceAst const &other) const
    -> Ordering {
    // Equality based on the namespace.
    return *Ns == *other.Ns ? Ordering::equal : Ordering::less;
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::Equals(
    TypeUnaryExpressionOperatorAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsOpNamespace(*this);
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::PosStart() const
    -> std::size_t {
    // Use the namespace.
    return Ns->PosStart();
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::PosEnd() const
    -> std::size_t {
    // Use the namespace.
    return Ns->PosEnd();
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(
        AstClone(Ns), AstClone(TokSep));
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Ns);
    raw_string.append("::");
    SPP_STRING_END;
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::NsParts() const
    -> Vec<IdentifierAst*> {
    return {Ns.Get()};
}

auto spp::asts::TypeUnaryExpressionOperatorNamespaceAst::TypeParts() const
    -> Vec<TypeIdentifierAst*> {
    return {};
}

SPP_MOD_END
