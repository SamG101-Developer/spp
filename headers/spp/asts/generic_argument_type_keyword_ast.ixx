module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}

/**
 * The GenericArgumentTypeKeywordAst represents a keyword argument in a generic argument context. It is forces the
 * argument to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeKeywordAst final : GenericArgumentTypeAst {
    std::unique_ptr<TypeAst> name;
    std::unique_ptr<TokenAst> tok_assign;

    GenericArgumentTypeKeywordAst(
        decltype(name) name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) val);
    ~GenericArgumentTypeKeywordAst() override;
    auto to_rust() const -> std::string override;
};
