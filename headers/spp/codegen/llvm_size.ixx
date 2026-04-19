module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_size;
import spp.analyse.scopes;
import spp.asts;
import std;


namespace spp::codegen {
    SPP_EXP_FUN auto size_of(
        analyse::scopes::ScopeManager const &sm,
        std::shared_ptr<asts::TypeAst> const &type)
        -> std::size_t;
}
