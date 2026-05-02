module;
#include <spp/macros.hpp>

export module spp.asts.float_literal_ast;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
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
    SPP_GCC_VTABLE_FIX

    /**
     * The optional sign of the float literal. This can be either a plus or minus sign.
     */
    Unique<TokenAst> TokSign;

    /**
     * The integer part of the float literal. This is the part before the decimal point. It is required (.1 is not a
     * valid float literal).
     */
    Unique<TokenAst> IntVal;

    /**
     * The token that represents the decimal point in the float literal. This separates the integer part from the
     * fractional part.
     */
    Unique<TokenAst> TokDot;

    /**
     * The fractional part of the float literal. This is the part after the decimal point. It is required (1. is not a
     * valid float literal).
     */
    Unique<TokenAst> FracVal;

    /**
     * The optional type annotation of the float literal. This is used to specify the type of the float if the default
     * @c std::BigDec is not desired. This can be @c _f32 or @c _f64, for example.
     */
    Str Type;

    static auto FromSingleTok(
        decltype(TokSign) &&tok_sign,
        Unique<TokenAst> &&token,
        Str &&type)
        -> Unique<FloatLiteralAst>;

    /**
     * Construct the FloatLiteralAst with the arguments matching the members.
     * @param[in] tok_sign The optional sign of the float literal.
     * @param[in] int_val The integer part of the float literal.
     * @param[in] tok_dot The token that represents the decimal point in the float literal.
     * @param[in] frac_val The fractional part of the float literal.
     * @param[in] type The optional type annotation of the float literal.
     */
    FloatLiteralAst(
        decltype(TokSign) &&tok_sign,
        decltype(IntVal) &&int_val,
        decltype(TokDot) &&tok_dot,
        decltype(FracVal) &&frac_val,
        Str &&type);

    ~FloatLiteralAst() override;

    SPP_ATTR_NODISCARD auto EqualsFloatLiteral(FloatLiteralAst const &) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LLvmCtx *ctx) -> llvm::Value* override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

    template <typename T> requires std::floating_point<T>
    auto CppVal() const -> T;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::FloatLiteralAst)
