module;
#include <spp/macros.hpp>

module spp.asts.type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;

SPP_MOD_BEGIN
spp::asts::TypeAst::TypeAst() :
  _CachedWithoutGenerics(nullptr),
  _LookupScope(nullptr),
  _LookupSym(nullptr),
  _LookupGen(0),
  _CachedStringification("") {
}

spp::asts::TypeAst::~TypeAst() = default;

auto spp::asts::TypeAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const
  -> Shared<ExpressionAst> {
  // The type-level walk already handles "Self", nested arguments
  // and everything else, so the expression walk hands the whole
  // job to it rather than repeating any of it.
  return SubstituteGenerics(args);
}

SPP_MOD_END
