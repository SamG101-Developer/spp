#pragma once
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>


namespace spp::codegen {
    auto size_of(
        analyse::scopes::ScopeManager const &sm,
        std::shared_ptr<asts::TypeAst> const &type)
        -> std::size_t;
}
