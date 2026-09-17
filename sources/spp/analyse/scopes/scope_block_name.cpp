module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_block_name;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.ptr;

SPP_MOD_BEGIN
ScopeBlockName::ScopeBlockName(
  Str &&name) :
  Name(std::move(name)) {
}

auto ScopeBlockName::FromParts(
  Str &&header, Vec<Ast*> const &parts, const std::size_t pos)
  -> ScopeBlockName {
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

ScopeIdentifierName::ScopeIdentifierName(
  Shared<IdentifierAst> const &name) :
  Name(name) {
}

ScopeTypeIdentifierName::ScopeTypeIdentifierName(
  Shared<TypeAst> const &name) :
  Name(dynamic_shared_cast<TypeIdentifierAst>(name)) {
}

ScopeTypeIdentifierName::ScopeTypeIdentifierName(
  Shared<TypeIdentifierAst> const &name) :
  Name(name) {
}

SPP_MOD_END
