module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.integer_literal_ast;
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
import spp.utils.strings;
import ankerl.unordered_dense;
import llvm;
import mppp;

SPP_MOD_BEGIN
const auto INTEGER_TYPE_MIN_MAX = ankerl::unordered_dense::map<spp::Str, spp::Pair<mppp::BigInt, mppp::BigInt>>{
    {"s8", {
        mppp::BigInt("-128"),
        mppp::BigInt("127")}},
    {"s16", {
        mppp::BigInt("-32768"),
        mppp::BigInt("32767")}},
    {"s32", {
        mppp::BigInt("-2147483648"),
        mppp::BigInt("2147483647")}},
    {"s64", {
        mppp::BigInt("-9223372036854775808"),
        mppp::BigInt("9223372036854775807")}},
    {"sz", {
        mppp::BigInt("-9223372036854775808"),
        mppp::BigInt("9223372036854775807")}},
    {"s128", {
        mppp::BigInt("-170141183460469231731687303715884105728"),
        mppp::BigInt("170141183460469231731687303715884105727")}},
    {"s256", {
        mppp::BigInt("-57896044618658097711785492504343953926634992332820282019728792003956564819968"),
        mppp::BigInt("57896044618658097711785492504343953926634992332820282019728792003956564819967")}},
    {"u8", {
        mppp::BigInt("0"),
        mppp::BigInt("255")}},
    {"u16", {
        mppp::BigInt("0"),
        mppp::BigInt("65535")}},
    {"u32", {
        mppp::BigInt("0"),
        mppp::BigInt("4294967295")}},
    {"u64", {
        mppp::BigInt("0"),
        mppp::BigInt("18446744073709551615")}},
    {"uz", {
        mppp::BigInt("0"),
        mppp::BigInt("18446744073709551615")}},
    {"u128", {
        mppp::BigInt("0"),
        mppp::BigInt("340282366841710300949128831971969468211455")}},
    {"u256", {
        mppp::BigInt("0"),
        mppp::BigInt("115792089237316195423570985008687907853269984665640564039457584007913129639935")}}
};

spp::asts::IntegerLiteralAst::IntegerLiteralAst(
    decltype(TokSign) &&tok_sign,
    decltype(Val) &&val,
    Str &&type) :
    TokSign(std::move(tok_sign)),
    Val(std::move(val)),
    Type(std::move(type)) {}

spp::asts::IntegerLiteralAst::~IntegerLiteralAst() = default;

auto spp::asts::IntegerLiteralAst::EqualsIntegerLiteral(
    IntegerLiteralAst const &other) const
    -> Ordering {
    if (
        ((not TokSign and not other.TokSign) or (TokSign and other.TokSign and *TokSign == *other.TokSign))
        and Val->TokenData == other.Val->TokenData
        and Type == other.Type) {
        return Ordering::equal;
    }
    return Ordering::less;
}

auto spp::asts::IntegerLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    return other.EqualsIntegerLiteral(*this);
}

auto spp::asts::IntegerLiteralAst::PosStart() const
    -> std::size_t {
    return TokSign ? TokSign->PosStart() : Val->PosStart();
}

auto spp::asts::IntegerLiteralAst::PosEnd() const
    -> std::size_t {
    return Val->PosEnd();
}

auto spp::asts::IntegerLiteralAst::Clone() const
    -> Unique<Ast> {
    return MakeUnique<IntegerLiteralAst>(
        AstClone(TokSign),
        AstClone(Val),
        Type.c_str());
}

auto spp::asts::IntegerLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokSign);
    SPP_STRING_APPEND(Val);
    raw_string.append("_").append(Type);
    SPP_STRING_END;
}

auto spp::asts::IntegerLiteralAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    //
    using spp::utils::strings::NormaliseIntegerString;
    using analyse::errors::SppIntegerOutOfBoundsError;

    // Get the lower and upper bounds as big ints.
    Type = Type.empty() ? "s32" : Type;
    auto const &[lower, upper] = INTEGER_TYPE_MIN_MAX.at(Type);
    auto mapped_val = NormaliseIntegerString(Val->TokenData);
    if (TokSign != nullptr and TokSign->TokenType == lex::SppTokenType::TK_SUB) {
        mapped_val = mapped_val.neg();
    }

    // Check if the value is within the bounds.
    RaiseIf<SppIntegerOutOfBoundsError>(
        mapped_val < lower or mapped_val > upper,
        {sm->CurrentScope}, ERR_ARGS(*this, mapped_val, lower, upper, "int"));
}

auto spp::asts::IntegerLiteralAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Clone and return the float literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::IntegerLiteralAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the type of the integer literal.
    const auto type_ast = InferType(sm, meta);
    const auto type_sym = sm->CurrentScope->GetTypeSymbol(type_ast);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);

    // Create the LLVM constant integer value.
    const auto bit_width = llvm_type->getIntegerBitWidth();
    const auto ap_int = llvm::APInt(bit_width, Val->TokenData, 10);
    return llvm::ConstantInt::get(*ctx->Context, ap_int);
}

auto spp::asts::IntegerLiteralAst::InferType(
    ScopeManager *sm,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    //
    using namespace generate::common_types;
    using analyse::errors::SppInternalCompilerError;

    // Map the type string literal to the correct SPP type.
    auto spp_type = Shared<TypeAst>(nullptr);
    const auto p = PosStart();
    if (Type.empty()) { spp_type = S32(p); }
    else if (Type == "s8") { spp_type = S8(p); }
    else if (Type == "s16") { spp_type = S16(p); }
    else if (Type == "s32") { spp_type = S32(p); }
    else if (Type == "s64") { spp_type = S64(p); }
    else if (Type == "s128") { spp_type = S128(p); }
    else if (Type == "s256") { spp_type = S256(p); }
    else if (Type == "sz") { spp_type = SSize(p); }
    else if (Type == "u8") { spp_type = U8(p); }
    else if (Type == "u16") { spp_type = U16(p); }
    else if (Type == "u32") { spp_type = U32(p); }
    else if (Type == "u64") { spp_type = U64(p); }
    else if (Type == "u128") { spp_type = U128(p); }
    else if (Type == "u256") { spp_type = U256(p); }
    else if (Type == "uz") { spp_type = USize(p); }
    else {
        Raise<SppInternalCompilerError>(
            {sm->CurrentScope},
            ERR_ARGS(*this, "invalid integer literal type"));
    }

    const auto sym = sm->CurrentScope->GetTypeSymbol(spp_type);
    return sym->FqName();
}

template <typename T> requires std::integral<T>
auto spp::asts::IntegerLiteralAst::CppVal() const -> T {
    const auto raw_str = Val->ToString();
    const auto signed_str = TokSign != nullptr ? "-" + raw_str : raw_str;
    if constexpr (std::is_unsigned_v<T>) { return static_cast<T>(std::stoull(signed_str)); }
    else { return static_cast<T>(std::stoll(signed_str)); }
}

// Manual instantiation of.CppVal function
template auto spp::asts::IntegerLiteralAst::CppVal<std::int8_t>() const -> std::int8_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::int16_t>() const -> std::int16_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::int32_t>() const -> std::int32_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::int64_t>() const -> std::int64_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint8_t>() const -> std::uint8_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint16_t>() const -> std::uint16_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint32_t>() const -> std::uint32_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint64_t>() const -> std::uint64_t;

SPP_MOD_END
