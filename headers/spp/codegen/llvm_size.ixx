module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;
import spp.asts._fwd;
import spp.analyse.scopes.scope_manager;

import std;


namespace spp::codegen {
    SPP_EXP_FUN auto size_of(
        analyse::scopes::ScopeManager const &sm,
        std::shared_ptr<asts::TypeAst> const &type)
        -> std::size_t;
}
