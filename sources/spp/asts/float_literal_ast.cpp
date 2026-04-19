module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import :common_types;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.utils.scope_utils;
import spp.asts.utils;
import spp.codegen.llvm_type;
import spp.lex;
import spp.utils.strings;
import llvm;
import mppp;


const auto FLOAT_TYPE_MIN_MAX = std::map<std::string, std::pair<mppp::BigDec, mppp::BigDec>>{
    {"f8", {spp::utils::strings::expand_scientific_notation("-5.7344e+4"), spp::utils::strings::expand_scientific_notation("5.7344e+4")}},
    {"f16", {spp::utils::strings::expand_scientific_notation("-6.55e+4"), spp::utils::strings::expand_scientific_notation("6.55e+4")}},
    {"f32", {spp::utils::strings::expand_scientific_notation("-3.4028235e+38"), spp::utils::strings::expand_scientific_notation("3.4028235e+38")}},
    {"f64", {spp::utils::strings::expand_scientific_notation("-1.7976931348623157e+308"), spp::utils::strings::expand_scientific_notation("1.7976931348623157e+308")}},
    {"f128", {spp::utils::strings::expand_scientific_notation("-1.189731495357231765e+4932"), spp::utils::strings::expand_scientific_notation("1.189731495357231765e+4932")}}, // check this
};


spp::asts::FloatLiteralAst::FloatLiteralAst(
    decltype(tok_sign) &&tok_sign,
    decltype(int_val) &&int_val,
    decltype(tok_dot) &&tok_dot,
    decltype(frac_val) &&frac_val,
    std::string &&type) :
    tok_sign(std::move(tok_sign)),
    int_val(std::move(int_val)),
    tok_dot(std::move(tok_dot)),
    frac_val(std::move(frac_val)),
    type(std::move(type)) {
}


spp::asts::FloatLiteralAst::~FloatLiteralAst() = default;


auto spp::asts::FloatLiteralAst::from_single_token(
    decltype(tok_sign) &&tok_sign,
    std::unique_ptr<TokenAst> &&token,
    std::string &&type)
    -> std::unique_ptr<FloatLiteralAst> {
    // Split the token data into integer and fractional parts.
    auto int_part = token->token_data.substr(0, token->token_data.find('.'));
    auto frac_part = token->token_data.substr(token->token_data.find('.') + 1);
    return std::make_unique<FloatLiteralAst>(
        std::move(tok_sign),
        std::make_unique<TokenAst>(token->pos_start(), lex::SppTokenType::LX_NUMBER, std::move(int_part)),
        std::make_unique<TokenAst>(token->pos_start() + int_part.size(), lex::SppTokenType::TK_DOT, "."),
        std::make_unique<TokenAst>(token->pos_start() + int_part.size() + 1, lex::SppTokenType::LX_NUMBER, std::move(frac_part)),
        std::move(type));
}


auto spp::asts::FloatLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_float_literal(*this);
}


auto spp::asts::FloatLiteralAst::equals_float_literal(
    FloatLiteralAst const &other) const
    -> std::strong_ordering {
    if (
        ((not tok_sign and not other.tok_sign) or (tok_sign and other.tok_sign and *tok_sign == *other.tok_sign))
        and int_val->token_data == other.int_val->token_data
        and frac_val->token_data == other.frac_val->token_data
        and type == other.type) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


template <typename T> requires std::floating_point<T>
auto spp::asts::FloatLiteralAst::cpp_value() const -> T {
    const auto raw_str = int_val->to_string() + "." + frac_val->to_string();
    const auto signed_str = tok_sign != nullptr ? "-" + raw_str : raw_str;
    return static_cast<T>(std::stold(signed_str));
}


auto spp::asts::FloatLiteralAst::pos_start() const
    -> std::size_t {
    return tok_sign ? tok_sign->pos_start() : int_val->pos_start();
}


auto spp::asts::FloatLiteralAst::pos_end() const
    -> std::size_t {
    return frac_val->pos_end();
}


auto spp::asts::FloatLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<FloatLiteralAst>(
        ast_clone(tok_sign),
        ast_clone(int_val),
        ast_clone(tok_dot),
        ast_clone(frac_val),
        type.c_str());
}


spp::asts::FloatLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sign);
    SPP_STRING_APPEND(int_val);
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(frac_val);
    raw_string.append("_").append(type);
    SPP_STRING_END;
}


auto spp::asts::FloatLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Get the lower and upper bounds as big floats.
    type = type.empty() ? "f32" : type;
    auto const &[lower, upper] = FLOAT_TYPE_MIN_MAX.at(type);
    auto mapped_val = spp::utils::strings::normalize_float_string(int_val->token_data, frac_val->token_data);
    if (tok_sign != nullptr and tok_sign->token_type == lex::SppTokenType::TK_SUB) {
        mapped_val = mapped_val.neg();
    }

    // Check if the value is within the bounds.
    raise_if<analyse::errors::SppFloatOutOfBoundsError>(
        mapped_val < lower or mapped_val > upper,
        {sm->current_scope}, ERR_ARGS(*this, mapped_val, lower, upper, "float"));
}


auto spp::asts::FloatLiteralAst::stage_9_comptime_resolution(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Clone and return the float literal as is for compile-time resolution.
    meta->cmp_result = ast_clone(this);
}


auto spp::asts::FloatLiteralAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Get the type of the float literal.
    const auto type_ast = infer_type(sm, meta);
    const auto type_sym = analyse::utils::scope_utils::get_type_symbol(*sm->current_scope, type_ast);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);

    // Create the LLVM constant float value.
    const auto &semantics = llvm_type->getFltSemantics();
    const auto val = int_val->token_data + "." + frac_val->token_data;
    const auto ap_float = llvm::APFloat(semantics, val);
    return llvm::ConstantFP::get(*ctx->context, ap_float);
}


auto spp::asts::FloatLiteralAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Todo: Use the IntergerLiteralAst implementation (it's better).
    // Map the type string literal to the correct SPP type.
    if (type.empty()) {
        return common_types::f32(pos_start());
    }
    if (type == "f8") {
        return common_types::f8(pos_start());
    }
    if (type == "f16") {
        return common_types::f16(pos_start());
    }
    if (type == "f32") {
        return common_types::f32(pos_start());
    }
    if (type == "f64") {
        return common_types::f64(pos_start());
    }
    if (type == "f128") {
        return common_types::f128(pos_start());
    }

    // This should never happen, due to parsing rules.
    raise<analyse::errors::SppInternalCompilerError>(
        {sm->current_scope},
        ERR_ARGS(*this, "invalid float literal type"));
}


template auto spp::asts::FloatLiteralAst::cpp_value<std::float16_t>() const -> std::float16_t;
template auto spp::asts::FloatLiteralAst::cpp_value<std::float32_t>() const -> std::float32_t;
template auto spp::asts::FloatLiteralAst::cpp_value<std::float64_t>() const -> std::float64_t;
