#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <boost/multiprecision/cpp_int.hpp>

using CppBigInt = boost::multiprecision::cpp_int;


const auto INTEGER_TYPE_MIN_MAX = std::map<std::string, std::pair<CppBigInt, CppBigInt>>{
    {"s8", {CppBigInt("-128"), CppBigInt("127")}},
    {"s16", {CppBigInt("-32768"), CppBigInt("32767")}},
    {"s32", {CppBigInt("-2147483648"), CppBigInt("2147483647")}},
    {"s64", {CppBigInt("-9223372036854775808"), CppBigInt("9223372036854775807")}},
    {"sz", {CppBigInt("-9223372036854775808"), CppBigInt("9223372036854775807")}},
    {"s128", {CppBigInt("-170141183460469231731687303715884105728"), CppBigInt("170141183460469231731687303715884105727")}},
    {"s256", {CppBigInt("-57896044618658097711785492504343953926634992332820282019728792003956564819968"), CppBigInt("57896044618658097711785492504343953926634992332820282019728792003956564819967")}},
    {"u8", {CppBigInt("0"), CppBigInt("255")}},
    {"u16", {CppBigInt("0"), CppBigInt("65535")}},
    {"u32", {CppBigInt("0"), CppBigInt("4294967295")}},
    {"u64", {CppBigInt("0"), CppBigInt("18446744073709551615")}},
    {"uz", {CppBigInt("0"), CppBigInt("18446744073709551615")}},
    {"u128", {CppBigInt("0"), CppBigInt("340282366841710300949128831971969468211455")}},
    {"u256", {CppBigInt("0"), CppBigInt("115792089237316195423570985008687907853269984665640564039457584007913129639935")}}
};


spp::asts::IntegerLiteralAst::IntegerLiteralAst(
    decltype(sign) &&sign,
    decltype(val) &&val,
    std::string &&type) :
    LiteralAst(),
    sign(std::move(sign)),
    val(std::move(val)),
    type(std::move(type)) {
}


spp::asts::IntegerLiteralAst::~IntegerLiteralAst() = default;


auto spp::asts::IntegerLiteralAst::pos_start() const -> std::size_t {
    return sign ? sign->pos_start() : val->pos_start();
}


auto spp::asts::IntegerLiteralAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


auto spp::asts::IntegerLiteralAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<IntegerLiteralAst>(
        ast_clone(sign),
        ast_clone(val),
        type.c_str());
}


spp::asts::IntegerLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(sign);
    SPP_STRING_APPEND(val);
    raw_string.append(type);
    SPP_STRING_END;
}


auto spp::asts::IntegerLiteralAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(sign);
    SPP_PRINT_APPEND(val);
    formatted_string.append(type);
    SPP_PRINT_END;
}


auto spp::asts::IntegerLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_integer_literal(*this);
}


auto spp::asts::IntegerLiteralAst::equals_integer_literal(
    IntegerLiteralAst const &other) const
    -> std::strong_ordering {
    if (
        ((sign == nullptr and other.sign == nullptr) or (sign != nullptr and other.sign != nullptr and *sign == *other.sign))
        and val->token_data == other.val->token_data
        and type == other.type) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::IntegerLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Get the lower and upper bounds as big floats.
    auto const &[lower, upper] = INTEGER_TYPE_MIN_MAX.at(type);
    const auto mapped_val = CppBigInt(val->token_data.c_str());

    // Check if the value is within the bounds.
    if (mapped_val < lower or mapped_val > upper) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIntegerOutOfBoundsError>().with_args(
            *this, mapped_val, lower, upper, "float").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::IntegerLiteralAst::infer_type(
    ScopeManager *,
    mixins::CompilerMetaData *) -> std::shared_ptr<TypeAst> {
    // Map the type string literal to the correct SPP type.
    if (type == "") {
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
