module;
#include <spp/macros.hpp>

export module spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct IntegerLiteralAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::IntegerLiteralAst final : LiteralAst {
    std::unique_ptr<TokenAst> tok_sign;
    std::unique_ptr<TokenAst> val;
    std::string type;

    IntegerLiteralAst(
        decltype(tok_sign) &&tok_sign,
        decltype(val) &&val,
        decltype(type) &&type);
    ~IntegerLiteralAst() override;
    auto to_rust() const -> std::string override;
};
