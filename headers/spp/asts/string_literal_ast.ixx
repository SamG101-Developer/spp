module;
#include <spp/macros.hpp>

export module spp.asts.string_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct StringLiteralAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::StringLiteralAst final : LiteralAst {
    std::unique_ptr<TokenAst> val;

    explicit StringLiteralAst(
        decltype(val) &&val);
    ~StringLiteralAst() override;
    auto to_rust() const -> std::string override;
};
