module;
#include <spp/macros.hpp>

export module spp.asts.tuple_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TupleLiteralAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::TupleLiteralAst final : LiteralAst {
    std::unique_ptr<TokenAst> tok_l;
    std::vector<std::unique_ptr<ExpressionAst>> elems;
    std::unique_ptr<TokenAst> tok_r;

    explicit TupleLiteralAst(
        decltype(tok_l) &&tok_l,
        decltype(elems) &&elements,
        decltype(tok_r) &&tok_r);
    ~TupleLiteralAst() override;
    auto to_rust() const -> std::string override;
};
