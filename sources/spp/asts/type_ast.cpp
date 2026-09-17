module;
#include <spp/macros.hpp>

module spp.asts.type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.asts.generic_argument_group_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
TypeAst::TypeAst() :
  _CachedWithoutGenerics(nullptr),
  _LookupScope(nullptr),
  _LookupSym(nullptr),
  _LookupGen(0),
  _CachedStringification("") {
}

TypeAst::~TypeAst() = default;

auto TypeAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const -> Shared<ExpressionAst> {
  // The type-level walk already handles "Self", nested arguments
  // and everything else, so the expression walk hands the whole
  // job to it rather than repeating any of it.
  return SubstituteGenerics(args);
}

auto TypeAst::IsAllowedInDefault() const -> bool {
  // A type written as a value - "None", a zero-sized marker -
  // is a value, so it may appear in a default.
  return true;
}

auto TypeAst::WithSourceSpanOf(
  TypeAst const &written) const -> Shared<TypeAst> {
  // Copied rather than stamped in place: a qualified name is
  // cached on its symbol and shared by every use of it.
  auto copy = AstCloneShared(this);
  const auto start = written.PosStart();
  const auto end = written.PosEnd();
  if (start != 0 and end > start) {
    copy->_HasSourceSpan = true;
    copy->_SpanStart = start;
    copy->_SpanEnd = end;

    // A type rewritten more than once ("Self" substituted, then
    // qualified) keeps the text first written, not a rewrite's.
    copy->_WrittenText = written._WrittenText.empty() ? written.ToString() : written._WrittenText;
  }
  return copy;
}

auto TypeAst::WithGenerics(
  Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> {
  // Clone this type and put the arguments on its right-most part.
  auto type_clone = AstClone(this);
  if (arg_group == nullptr) { arg_group = GenericArgumentGroupAst::NewEmpty(); }
  type_clone->LastTypePart()->GnArgGroup = std::move(arg_group);

  // Different arguments make a different type, so the clone keeps no stamp: the one copied from this node (a cached
  // qualified name is stamped with its symbol) would still name the old type, and a lookup would follow it there.
  type_clone->SetStamp(nullptr);
  type_clone->LastTypePart()->SetStamp(nullptr);
  return type_clone;
}

SPP_MOD_END
