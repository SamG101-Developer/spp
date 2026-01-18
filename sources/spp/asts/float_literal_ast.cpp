module;
#include <spp/macros.hpp>

module spp.asts.float_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import llvm;
import mppp;


// const auto FLOAT_TYPE_MIN_MAX = std::map<std::string, std::pair<mppp::BigDec, mppp::BigDec>>{
//     {"f8", {mppp::BigDec("-5.7344e+4"), mppp::BigDec("5.7344e+4")}},
//     {"f16", {mppp::BigDec("-6.55e+4"), mppp::BigDec("6.55e+4")}},
//     {"f32", {mppp::BigDec("-3.4028235e+38"), mppp::BigDec("3.4028235e+38")}},
//     {"f64", {mppp::BigDec("-1.7976931348623157e+308"), mppp::BigDec("1.7976931348623157e+308")}},
//     {"f128", {mppp::BigDec("-1.189731495357231765e+4932"), mppp::BigDec("1.189731495357231765e+4932")}}, // check this
// };


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
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // Get the lower and upper bounds as big floats.
    type = type.empty() ? "f32" : type;
    // auto const &[lower, upper] = FLOAT_TYPE_MIN_MAX.at(type);
    // auto mapped_val = mppp::BigDec((int_val->token_data + "." + frac_val->token_data).c_str());
    // if (tok_sign != nullptr and tok_sign->token_type == lex::SppTokenType::TK_SUB) {
    //     mapped_val = mapped_val.neg();
    // }

    // Check if the value is within the bounds.
    // if (mapped_val < lower or mapped_val > upper) {
    //     analyse::errors::SemanticErrorBuilder<analyse::errors::SppFloatOutOfBoundsError>().with_args(
    //         *this, mapped_val, lower, upper, "float").with_scopes({sm->current_scope}).raise();
    // }
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
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the type of the float literal.
    const auto type_ast = infer_type(sm, meta);
    const auto type_sym = sm->current_scope->get_type_symbol(type_ast);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);

    // Create the LLVM constant float value.
    const auto &semantics = llvm_type->getFltSemantics();
    const auto val = int_val->token_data + "." + frac_val->token_data;
    const auto ap_float = llvm::APFloat(semantics, val);
    return llvm::ConstantFP::get(*ctx->context, ap_float);
}


auto spp::asts::FloatLiteralAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Map the type string literal to the correct SPP type.
    if (type.empty()) {
        return generate::common_types::f32(pos_start());
    }
    if (type == "f8") {
        return generate::common_types::f8(pos_start());
    }
    if (type == "f16") {
        return generate::common_types::f16(pos_start());
    }
    if (type == "f32") {
        return generate::common_types::f32(pos_start());
    }
    if (type == "f64") {
        return generate::common_types::f64(pos_start());
    }
    if (type == "f128") {
        return generate::common_types::f128(pos_start());
    }

    // This should never happen, due to parsing rules.
    std::unreachable();
}
