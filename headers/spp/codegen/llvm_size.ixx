module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
}

namespace analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}


namespace spp::codegen {
    SPP_EXP_FUN auto size_of(
        analyse::scopes::ScopeManager const &sm,
        std::shared_ptr<asts::TypeAst> const &type)
        -> std::size_t;
}
