module;
#include <spp/macros.hpp>

module spp.asts.integer_literal_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;
import spp.lex.tokens;

import mppp;


const auto INTEGER_TYPE_MIN_MAX = std::map<std::string, std::pair<mppp::BigInt, mppp::BigInt>>{
    {"s8", {mppp::BigInt("-128"), mppp::BigInt("127")}},
    {"s16", {mppp::BigInt("-32768"), mppp::BigInt("32767")}},
    {"s32", {mppp::BigInt("-2147483648"), mppp::BigInt("2147483647")}},
    {"s64", {mppp::BigInt("-9223372036854775808"), mppp::BigInt("9223372036854775807")}},
    {"sz", {mppp::BigInt("-9223372036854775808"), mppp::BigInt("9223372036854775807")}},
    {"s128", {mppp::BigInt("-170141183460469231731687303715884105728"), mppp::BigInt("170141183460469231731687303715884105727")}},
    {"s256", {mppp::BigInt("-57896044618658097711785492504343953926634992332820282019728792003956564819968"), mppp::BigInt("57896044618658097711785492504343953926634992332820282019728792003956564819967")}},
    {"u8", {mppp::BigInt("0"), mppp::BigInt("255")}},
    {"u16", {mppp::BigInt("0"), mppp::BigInt("65535")}},
    {"u32", {mppp::BigInt("0"), mppp::BigInt("4294967295")}},
    {"u64", {mppp::BigInt("0"), mppp::BigInt("18446744073709551615")}},
    {"uz", {mppp::BigInt("0"), mppp::BigInt("18446744073709551615")}},
    {"u128", {mppp::BigInt("0"), mppp::BigInt("340282366841710300949128831971969468211455")}},
    {"u256", {mppp::BigInt("0"), mppp::BigInt("115792089237316195423570985008687907853269984665640564039457584007913129639935")}}
};


spp::asts::IntegerLiteralAst::IntegerLiteralAst(
    decltype(tok_sign) &&tok_sign,
    decltype(val) &&val,
    std::string &&type) :
    LiteralAst(),
    tok_sign(std::move(tok_sign)),
    val(std::move(val)),
    type(std::move(type)) {
}


spp::asts::IntegerLiteralAst::~IntegerLiteralAst() = default;


auto spp::asts::IntegerLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_integer_literal(*this);
}


auto spp::asts::IntegerLiteralAst::equals_integer_literal(
    IntegerLiteralAst const &other) const
    -> std::strong_ordering {
    if (
        ((not tok_sign and not other.tok_sign) or (tok_sign and other.tok_sign and *tok_sign == *other.tok_sign))
        and val->token_data == other.val->token_data
        and type == other.type) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::IntegerLiteralAst::pos_start() const
    -> std::size_t {
    return tok_sign ? tok_sign->pos_start() : val->pos_start();
}


auto spp::asts::IntegerLiteralAst::pos_end() const
    -> std::size_t {
    return val->pos_end();
}


auto spp::asts::IntegerLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IntegerLiteralAst>(
        ast_clone(tok_sign),
        ast_clone(val),
        type.c_str());
}


spp::asts::IntegerLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sign);
    SPP_STRING_APPEND(val);
    raw_string.append("_").append(type);
    SPP_STRING_END;
}


auto spp::asts::IntegerLiteralAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sign);
    SPP_PRINT_APPEND(val);
    formatted_string.append("_").append(type);
    SPP_PRINT_END;
}


auto spp::asts::IntegerLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Get the lower and upper bounds as big floats.
    type = type.empty() ? "s32" : type;
    auto const &[lower, upper] = INTEGER_TYPE_MIN_MAX.at(type);
    auto mapped_val = mppp::BigInt(val->token_data.c_str());
    if (tok_sign != nullptr and tok_sign->token_type == lex::SppTokenType::TK_SUB) {
        mapped_val = mapped_val.neg();
    }

    // Check if the value is within the bounds.
    if (mapped_val < lower or mapped_val > upper) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIntegerOutOfBoundsError>().with_args(
            *this, mapped_val, lower, upper, "float").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::IntegerLiteralAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // Map the type string literal to the correct SPP type.
    if (type.empty()) {
        return generate::common_types::s32(pos_start());
    }
    if (type == "s8") {
        return generate::common_types::s8(pos_start());
    }
    if (type == "s16") {
        return generate::common_types::s16(pos_start());
    }
    if (type == "s32") {
        return generate::common_types::s32(pos_start());
    }
    if (type == "s64") {
        return generate::common_types::s64(pos_start());
    }
    if (type == "s128") {
        return generate::common_types::s128(pos_start());
    }
    if (type == "s256") {
        return generate::common_types::s256(pos_start());
    }
    if (type == "sz") {
        return generate::common_types::ssize(pos_start());
    }
    if (type == "u8") {
        return generate::common_types::u8(pos_start());
    }
    if (type == "u16") {
        return generate::common_types::u16(pos_start());
    }
    if (type == "u32") {
        return generate::common_types::u32(pos_start());
    }
    if (type == "u64") {
        return generate::common_types::u64(pos_start());
    }
    if (type == "u128") {
        return generate::common_types::u128(pos_start());
    }
    if (type == "u256") {
        return generate::common_types::u256(pos_start());
    }
    if (type == "uz") {
        return generate::common_types::usize(pos_start());
    }

    // This should never happen, due to parsing rules.
    std::unreachable();
}
