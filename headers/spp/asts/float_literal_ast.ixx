module;
#include <spp/macros.hpp>

export module spp.asts.float_literal_ast;
import spp.asts.literal_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * The FloatLiteralAst represents a floating-point literal. It supports a prefix sign, an integer part, a decimal point,
 * and a fractional part. The type of the float can be specific with a postfix type annotation, such as @c _f32 or
 * @c _f64. No postfix defaults the type to @c std::BigDec.
 */
SPP_EXP_CLS struct spp::asts::FloatLiteralAst final : LiteralAst {
    std::unique_ptr<TokenAst> tok_sign;
    std::unique_ptr<TokenAst> int_val;
    std::unique_ptr<TokenAst> tok_dot;
    std::unique_ptr<TokenAst> frac_val;
    std::string type;

    explicit FloatLiteralAst(
        decltype(tok_sign) &&tok_sign,
        decltype(int_val) &&int_val,
        decltype(tok_dot) &&tok_dot,
        decltype(frac_val) &&frac_val,
        std::string &&type);
    ~FloatLiteralAst() override;
    auto to_rust() const -> std::string override;
};
