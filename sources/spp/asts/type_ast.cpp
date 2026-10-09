module;
#include <spp/macros.hpp>

module spp.asts.type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_resolution;
import spp.asts.generic_argument_group_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
TypeAst::TypeAst() :
  _CachedWithoutGns(nullptr),
  _CachedStringification("") {
}

TypeAst::~TypeAst() = default;

auto TypeAst::ReadExpr(
  analyse::scopes::ExprSubst const &sub) const -> Shared<ExpressionAst> {
  return ReadExprType(sub);
}

auto TypeAst::ReadExprType(
  analyse::scopes::ExprSubst const &sub) const -> Shared<TypeAst> {
  return analyse::utils::type_resolution::ReadType(*this, sub);
}

auto TypeAst::IsAllowedInDefault() const -> bool {
  // A type written as a value - "None", a zero-sized marker -
  // is a value, so it may appear in a default.
  return true;
}

auto TypeAst::WithSourceSpanOf(
  TypeAst const &written) const -> Shared<TypeAst> {
  // A type rewritten more than once ("Self" substituted, then
  // qualified) keeps the text first written, not a rewrite's.
  auto copy = WithSourceSpanAt(written);
  if (copy->_HasSourceSpan) {
    copy->_WrittenText = written._WrittenText.empty() ? written.ToString() : written._WrittenText;
  }
  return copy;
}

auto TypeAst::WithSourceSpanAt(
  Ast const &site) const -> Shared<TypeAst> {
  // Copied rather than stamped in place: a qualified name is
  // cached on its symbol and shared by every use of it.
  auto copy = AstCloneShared(this);
  const auto start = site.PosStart();
  const auto end = site.PosEnd();
  if (start != 0 and end > start) {
    copy->_HasSourceSpan = true;
    copy->_SpanStart = start;
    copy->_SpanEnd = end;
  }
  return copy;
}

auto TypeAst::WithGns(
  Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> {
  // Clone this type and put the arguments on its right-most part.
  auto type_clone = AstClone(this);
  if (arg_group == nullptr) { arg_group = GenericArgumentGroupAst::NewEmpty(); }
  type_clone->LastTypePart()->GnArgGroup = std::move(arg_group);

  // Different arguments make a different type, so the clone keeps no written identity: the one copied from this node (a
  // cached qualified name records its symbol's) would still name the old type, and a lookup would follow it there.
  type_clone->StampTypeId(nullptr);
  type_clone->LastTypePart()->StampTypeId(nullptr);
  return type_clone;
}

SPP_MOD_END
