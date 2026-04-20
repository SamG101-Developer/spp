module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_block_name;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;


SPP_MOD_BEGIN
spp::analyse::scopes::ScopeBlockName::ScopeBlockName(
    std::string &&name) :
    name(std::move(name)) {
}


auto spp::analyse::scopes::ScopeBlockName::from_parts(
    std::string &&header,
    std::vector<asts::Ast*> const &parts,
    const std::size_t pos)
    -> ScopeBlockName {
    // Build the name string.
    auto builder = std::string();
    builder.append("<").append(header);
    for (auto const &part : parts) {
        builder.append("#").append(part->to_string());
    }
    builder.append("#").append(std::to_string(pos));
    builder.append(">");
    return ScopeBlockName(std::move(builder));
}


spp::analyse::scopes::ScopeIdentifierName::ScopeIdentifierName(
    std::shared_ptr<asts::IdentifierAst> const &name) :
    name(name) {
}


spp::analyse::scopes::ScopeTypeIdentifierName::ScopeTypeIdentifierName(
    std::shared_ptr<asts::TypeAst> const &name) :
    name(std::dynamic_pointer_cast<asts::TypeIdentifierAst>(name)) {
}


spp::analyse::scopes::ScopeTypeIdentifierName::ScopeTypeIdentifierName(
    std::shared_ptr<asts::TypeIdentifierAst> const &name) :
    name(name) {
}

SPP_MOD_END