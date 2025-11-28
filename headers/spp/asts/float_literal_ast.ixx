module;
#include <spp/macros.hpp>

export module spp.asts.float_literal_ast;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;

import llvm;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FloatLiteralAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The FloatLiteralAst represents a floating-point literal. It supports a prefix sign, an integer part, a decimal point,
 * and a fractional part. The type of the float can be specific with a postfix type annotation, such as @c _f32 or
 * @c _f64. No postfix defaults the type to @c std::BigDec.
 */
SPP_EXP_CLS struct spp::asts::FloatLiteralAst final : LiteralAst {
    /**
     * The optional sign of the float literal. This can be either a plus or minus sign.
     */
    std::unique_ptr<TokenAst> tok_sign;

    /**
     * The integer part of the float literal. This is the part before the decimal point. It is required (.1 is not a
     * valid float literal).
     */
    std::unique_ptr<TokenAst> int_val;

    /**
     * The token that represents the decimal point in the float literal. This separates the integer part from the
     * fractional part.
     */
    std::unique_ptr<TokenAst> tok_dot;

    /**
     * The fractional part of the float literal. This is the part after the decimal point. It is required (1. is not a
     * valid float literal).
     */
    std::unique_ptr<TokenAst> frac_val;

    /**
     * The optional type annotation of the float literal. This is used to specify the type of the float if the default
     * @c std::BigDec is not desired. This can be @c _f32 or @c _f64, for example.
     */
    std::string type;

    /**
     * Construct the FloatLiteralAst with the arguments matching the members.
     * @param[in] tok_sign The optional sign of the float literal.
     * @param[in] int_val The integer part of the float literal.
     * @param[in] tok_dot The token that represents the decimal point in the float literal.
     * @param[in] frac_val The fractional part of the float literal.
     * @param[in] type The optional type annotation of the float literal.
     */
    FloatLiteralAst(
        decltype(tok_sign) &&tok_sign,
        decltype(int_val) &&int_val,
        decltype(tok_dot) &&tok_dot,
        decltype(frac_val) &&frac_val,
        std::string &&type);

    ~FloatLiteralAst() override;

protected:
    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    SPP_ATTR_NODISCARD auto equals_float_literal(FloatLiteralAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_10_code_gen_2(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
