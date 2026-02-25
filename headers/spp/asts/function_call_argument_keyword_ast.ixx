module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The FunctionCallArgumentKeywordAst represents a keyword argument in a function call. It is forces the argument
 * to be matched by a keyword rather than an index.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentKeywordAst final : FunctionCallArgumentAst {
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<TokenAst> tok_assign;

    explicit FunctionCallArgumentKeywordAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(conv) &&conv,
        decltype(val) &&val);
    ~FunctionCallArgumentKeywordAst() override;
    auto to_rust() const -> std::string override;
};
