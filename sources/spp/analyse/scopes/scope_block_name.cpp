module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_block_name;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.ptr;

SPP_MOD_BEGIN
spp::analyse::scopes::ScopeBlockName::ScopeBlockName(
    Str &&name) :
    Name(std::move(name)) {
}

auto spp::analyse::scopes::ScopeBlockName::FromParts(
    Str &&header,
    Vec<asts::Ast*> const &parts,
    const std::size_t pos)
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

spp::analyse::scopes::ScopeIdentifierName::ScopeIdentifierName(
    Unique<asts::IdentifierAst> &&name) :
    Name(std::move(name)) {
}

spp::analyse::scopes::ScopeTypeIdentifierName::ScopeTypeIdentifierName(
    asts::TypeAst *name) :
    Name(dynamic_cast<asts::TypeIdentifierAst*>(name)) {
}

spp::analyse::scopes::ScopeTypeIdentifierName::ScopeTypeIdentifierName(
    asts::TypeIdentifierAst *name) :
    Name(name) {
}

SPP_MOD_END
