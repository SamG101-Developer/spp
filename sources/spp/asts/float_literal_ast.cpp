module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.float_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.strings;
import ankerl.unordered_dense;
import boost;
import llvm;

SPP_MOD_BEGIN
static const auto kFloatBounds = ankerl::unordered_dense::map<spp::Str, spp::Pair<boost::BigDec, boost::BigDec>>{
    {"f8", {boost::BigDec("-5.7344e+4"), boost::BigDec("5.7344e+4")}},
    {"f16", {boost::BigDec("-6.55e+4"), boost::BigDec("6.55e+4")}},
    {"f32", {boost::BigDec("-3.4028235e+38"), boost::BigDec("3.4028235e+38")}},
    {"f64", {boost::BigDec("-1.7976931348623157e+308"), boost::BigDec("1.7976931348623157e+308")}},
    {"f128", {boost::BigDec("-1.189731495357231765e+4932"), boost::BigDec("1.189731495357231765e+4932")}},
};

auto spp::asts::FloatLiteralAst::FromSingleTok(
    decltype(TokSign) &&tok_sign,
    Unique<TokenAst> &&token,
    Str &&type)
    -> Unique<FloatLiteralAst> {
    // Split the token data into integer and fractional parts.
    auto int_part = token->TokenData.substr(0, token->TokenData.find('.'));
    auto frac_part = token->TokenData.substr(token->TokenData.find('.') + 1);
    return MakeUnique<FloatLiteralAst>(
        std::move(tok_sign),
        MakeUnique<TokenAst>(token->PosStart(), lex::SppTokenType::LX_NUMBER, std::move(int_part)),
        MakeUnique<TokenAst>(token->PosStart() + int_part.length(), lex::SppTokenType::TK_DOT, "."),
        MakeUnique<TokenAst>(token->PosStart() + int_part.length() + 1, lex::SppTokenType::LX_NUMBER, std::move(frac_part)),
        std::move(type));
}

spp::asts::FloatLiteralAst::FloatLiteralAst(
    decltype(TokSign) &&tok_sign,
    decltype(IntVal) &&int_val,
    decltype(TokDot) &&tok_dot,
    decltype(FracVal) &&frac_val,
    Str &&type) :
    TokSign(std::move(tok_sign)),
    IntVal(std::move(int_val)),
    TokDot(std::move(tok_dot)),
    FracVal(std::move(frac_val)),
    Type(std::move(type)) {
}

spp::asts::FloatLiteralAst::~FloatLiteralAst() = default;

auto spp::asts::FloatLiteralAst::EqualsFloatLiteral(
    FloatLiteralAst const &other) const
    -> Ordering {
    // Equality based on the sign, integer part, fractional part, and type postfix.
    if (
        ((not TokSign and not other.TokSign) or (TokSign and other.TokSign and *TokSign == *other.TokSign))
        and IntVal->TokenData == other.IntVal->TokenData
        and FracVal->TokenData == other.FracVal->TokenData
        and Type == other.Type) {
        return Ordering::equal;
    }
    return Ordering::less;
}

auto spp::asts::FloatLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsFloatLiteral(*this);
}

auto spp::asts::FloatLiteralAst::PosStart() const
    -> std::size_t {
    // Use the sign token or the integer part.
    return TokSign ? TokSign->PosStart() : IntVal->PosStart();
}

auto spp::asts::FloatLiteralAst::PosEnd() const
    -> std::size_t {
    // Use teh fractional part.
    return FracVal->PosEnd();
}

auto spp::asts::FloatLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<FloatLiteralAst>(
        AstClone(TokSign), AstClone(IntVal), AstClone(TokDot), AstClone(FracVal), Type.c_str());
}

auto spp::asts::FloatLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokSign);
    SPP_STRING_APPEND(IntVal);
    SPP_STRING_APPEND(TokDot);
    SPP_STRING_APPEND(FracVal);
    raw_string.append("_").append(Type);
    SPP_STRING_END;
}

auto spp::asts::FloatLiteralAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    //
    using spp::utils::strings::NormalizeFloatString;
    using analyse::errors::SppFloatOutOfBoundsError;

    // Get the lower and upper bounds as big floats.
    Type = Type.empty() ? "f32" : Type;
    auto const &[lower, upper] = kFloatBounds.at(Type);
    auto mapped_val = NormalizeFloatString(IntVal->TokenData, FracVal->TokenData);
    if (TokSign != nullptr and TokSign->TokenType == lex::SppTokenType::TK_SUB) {
        mapped_val = boost::negate_integer(mapped_val, std::true_type());
    }

    // Check if the value is within the bounds.
    RaiseIf<SppFloatOutOfBoundsError>(
        mapped_val.compare(lower) < 0 or mapped_val.compare(upper) > 0,
        {sm->CurrentScope}, ERR_ARGS(*this, mapped_val, lower, upper, "float"));
}

auto spp::asts::FloatLiteralAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Clone and return the float literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::FloatLiteralAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the type of the float literal.
    const auto type_ast = InferType(sm, meta);
    const auto type_sym = sm->CurrentScope->GetTypeSymbol(type_ast);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);

    // Create the LLVM constant float value.
    auto const &semantics = llvm_type->getFltSemantics();
    const auto val = IntVal->TokenData + "." + FracVal->TokenData;
    const auto ap_float = llvm::APFloat(semantics, val);
    return llvm::ConstantFP::get(*ctx->Context, ap_float);
}

auto spp::asts::FloatLiteralAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    //
    using analyse::errors::SppInternalCompilerError;
    using namespace generate::common_types;

    // Map the type string literal to the correct SPP type.
    auto spp_type = Shared<TypeAst>(nullptr);
    const auto p = PosStart();
    if (Type.empty()) { spp_type = F32(p); }
    else if (Type == "f8") { spp_type = F8(p); }
    else if (Type == "f16") { spp_type = F16(p); }
    else if (Type == "f32") { spp_type = F32(p); }
    else if (Type == "f64") { spp_type = F64(p); }
    else if (Type == "f128") { spp_type = F128(p); }
    else {
        Raise<SppInternalCompilerError>(
            {sm->CurrentScope},
            ERR_ARGS(*this, "invalid float literal type"));
    }

    const auto sym = sm->CurrentScope->GetTypeSymbol(spp_type);
    return sym->FqName();
}

template <typename T> requires std::floating_point<T>
auto spp::asts::FloatLiteralAst::CppVal() const -> T {
    const auto raw_str = IntVal->ToString() + "." + FracVal->ToString();
    const auto signed_str = TokSign != nullptr ? "-" + raw_str : raw_str;
    return static_cast<T>(std::stold(signed_str));
}

template auto spp::asts::FloatLiteralAst::CppVal<std::float16_t>() const -> std::float16_t;
template auto spp::asts::FloatLiteralAst::CppVal<std::float32_t>() const -> std::float32_t;
template auto spp::asts::FloatLiteralAst::CppVal<std::float64_t>() const -> std::float64_t;
SPP_MOD_END
