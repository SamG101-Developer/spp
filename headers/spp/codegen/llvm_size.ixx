module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::codegen {
    SPP_EXP_FUN auto SizeOf(
        analyse::scopes::ScopeManager const &sm,
        asts::TypeAst const *type)
        -> std::size_t;
}
