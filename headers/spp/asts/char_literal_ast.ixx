module;
#include <spp/macros.hpp>

export module spp.asts.char_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CharLiteralAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::CharLiteralAst final : LiteralAst {
    std::unique_ptr<TokenAst> val;

    explicit CharLiteralAst(
        decltype(val) &&val);
    ~CharLiteralAst() override;
    auto to_rust() const -> std::string override;
};
