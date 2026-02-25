module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_positional_ast;
import spp.asts.function_call_argument_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentPositionalAst;
    SPP_EXP_CLS struct TokenAst;
}

/**
 * The FunctionCallArgumentPositionalAst represents a positional argument in a function call. It is forces the argument
 * to be matched by an index rather than a keyword. It also support for unpacking a tuple into arguments.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentPositionalAst : FunctionCallArgumentAst {
    std::unique_ptr<TokenAst> tok_unpack;

    explicit FunctionCallArgumentPositionalAst(
        decltype(conv) &&conv,
        decltype(tok_unpack) &&tok_unpack,
        decltype(val) &&val);
    ~FunctionCallArgumentPositionalAst() override;
    auto to_rust() const -> std::string override;
};
