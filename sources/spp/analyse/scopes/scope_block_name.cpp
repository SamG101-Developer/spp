module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_block_name;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.ptr;

use_alias(spp::asts, Ast);
use_alias(spp::asts, IdentifierAst);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeIdentifierAst);

SPP_MOD_BEGIN
/// [CHECKED]
spp::analyse::scopes::ScopeBlockName::ScopeBlockName(
  Str &&name) :
  Name(std::move(name)) {
}

/// [CHECKED]
auto spp::analyse::scopes::ScopeBlockName::FromParts(
  Str &&header, Vec<Ast*> const &parts, const std::size_t pos) -> ScopeBlockName {
  // Build the name string.
  auto builder = Str();
  builder.append("<").append(header);
  for (auto const &part : parts) {
    builder.append("#").append(part->ToString());
  }
  builder.append("#").append(std::to_string(pos));
  builder.append(">");
  return ScopeBlockName(std::move(builder));
}

/// [CHECKED]
spp::analyse::scopes::ScopeIdentifierName::ScopeIdentifierName(
  Shared<IdentifierAst> const &name) :
  Name(name) {
}

/// [CHECKED]
spp::analyse::scopes::ScopeTypeIdentifierName::ScopeTypeIdentifierName(
  Shared<TypeAst> const &name) :
  Name(shared_cast<TypeIdentifierAst>(name)) {
}

/// [CHECKED]
spp::analyse::scopes::ScopeTypeIdentifierName::ScopeTypeIdentifierName(
  Shared<TypeIdentifierAst> const &name) :
  Name(name) {
}

SPP_MOD_END
