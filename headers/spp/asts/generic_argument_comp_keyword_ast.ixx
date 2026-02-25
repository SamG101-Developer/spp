module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericArgumentCompKeywordAst represents a keyword argument in a generic argument context. It is forces the argument
 * to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompKeywordAst final : GenericArgumentCompAst {
    std::unique_ptr<TypeAst> name;

    GenericArgumentCompKeywordAst(
        decltype(name) name,
        decltype(val) &&val);
    ~GenericArgumentCompKeywordAst() override;
    auto to_rust() const -> std::string override;
};
