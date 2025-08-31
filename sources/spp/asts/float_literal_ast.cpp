#include <algorithm>
#include <format>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/generate/common_types.hpp>
#include <spp/asts/float_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>

#include <boost/multiprecision/cpp_dec_float.hpp>

using CppBigFloat = boost::multiprecision::cpp_dec_float_100;


const auto FLOAT_TYPE_MIN_MAX = std::map<std::string, std::pair<CppBigFloat, CppBigFloat>>{
    {"f8", {CppBigFloat("-14"), CppBigFloat("14")}},
    {"f16", {CppBigFloat("-65504"), CppBigFloat("65504")}},
    {"f32", {CppBigFloat("-3402823466385288598117041"), CppBigFloat("3402823466385288598117041")}},
    {"f64", {CppBigFloat("-1797693134862315708145274"), CppBigFloat("1797693134862315708145274")}},
    {"f128", {CppBigFloat("-11897314953572317650212638530309737165e+4932"), CppBigFloat("11897314953572317650212638530309737165e+4932")}}
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


auto spp::asts::FloatLiteralAst::pos_start() const -> std::size_t {
    return tok_sign ? tok_sign->pos_start() : int_val->pos_start();
}


auto spp::asts::FloatLiteralAst::pos_end() const -> std::size_t {
    return frac_val->pos_end();
}


auto spp::asts::FloatLiteralAst::clone() const -> std::unique_ptr<Ast> {
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
    raw_string += type;
    SPP_STRING_END;
}


auto spp::asts::FloatLiteralAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sign);
    SPP_PRINT_APPEND(int_val);
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(frac_val);
    formatted_string += type;
    SPP_PRINT_END;
}


auto spp::asts::FloatLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Get the lower and upper bounds as big floats.
    auto [lower, upper] = FLOAT_TYPE_MIN_MAX.at(type);
    const auto mapped_val = CppBigFloat((int_val->token_data + "." + frac_val->token_data).c_str());

    // Check if the value is within the bounds.
    if (mapped_val < lower or mapped_val > upper) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFloatOutOfBoundsError>().with_args(
            *this, mapped_val, lower, upper, "float").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::FloatLiteralAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *) -> std::shared_ptr<TypeAst> {
    // Map the type string literal to the correct SPP type.
    if (type == "") {
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
