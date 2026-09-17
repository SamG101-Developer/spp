module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
TypeUnaryExpressionOperatorNamespaceAst::TypeUnaryExpressionOperatorNamespaceAst(
  decltype(Ns) ns,
  decltype(TokSep) &&tok_sep) :
  TypeUnaryExpressionOperatorAst(),
  Ns(std::move(ns)),
  TokSep(std::move(tok_sep)) {
}

TypeUnaryExpressionOperatorNamespaceAst::~TypeUnaryExpressionOperatorNamespaceAst() = default;

auto TypeUnaryExpressionOperatorNamespaceAst::EqualsOpNamespace(
  TypeUnaryExpressionOperatorNamespaceAst const &other) const -> Ordering {
  // Equality based on the namespace.
  return *Ns == *other.Ns ? Ordering::equal : Ordering::less;
}

auto TypeUnaryExpressionOperatorNamespaceAst::Equals(
  TypeUnaryExpressionOperatorAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsOpNamespace(*this);
}

auto TypeUnaryExpressionOperatorNamespaceAst::PosStart() const -> std::size_t {
  // Use the namespace.
  return Ns->PosStart();
}

auto TypeUnaryExpressionOperatorNamespaceAst::PosEnd() const -> std::size_t {
  // Use the namespace.
  return Ns->PosEnd();
}

auto TypeUnaryExpressionOperatorNamespaceAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypeUnaryExpressionOperatorNamespaceAst>(
    AstCloneShared(Ns),
    AstClone(TokSep));
}

auto TypeUnaryExpressionOperatorNamespaceAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Ns);
  raw_string.append("::");
  SPP_STRING_END;
}

auto TypeUnaryExpressionOperatorNamespaceAst::NsParts() const -> Vec<IdentifierAst const*> {
  return {Ns.get()};
}

auto TypeUnaryExpressionOperatorNamespaceAst::NsParts() -> Vec<IdentifierAst*> {
  return {Ns.get()};
}

auto TypeUnaryExpressionOperatorNamespaceAst::TypeParts() const -> Vec<TypeIdentifierAst const*> {
  return {};
}

auto TypeUnaryExpressionOperatorNamespaceAst::TypeParts() -> Vec<TypeIdentifierAst*> {
  return {};
}

auto TypeUnaryExpressionOperatorNamespaceAst::NsPartsInto(
  Vec<IdentifierAst const*> &out) const -> void {
  out.EmplaceBack(Ns.get());
}

auto TypeUnaryExpressionOperatorNamespaceAst::TypePartsInto(
  Vec<TypeIdentifierAst const*>&) const -> void {
}

SPP_MOD_END
