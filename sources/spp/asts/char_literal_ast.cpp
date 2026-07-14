module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.char_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import llvm;

SPP_MOD_BEGIN
spp::asts::CharLiteralAst::CharLiteralAst(
    decltype(BytePrefix) &&byte_prefix,
    decltype(Val) &&val) :
    BytePrefix(std::move(byte_prefix)),
    Val(std::move(val)) {
}

spp::asts::CharLiteralAst::~CharLiteralAst() = default;

auto spp::asts::CharLiteralAst::EqualsCharLiteral(
    CharLiteralAst const &other) const
    -> Ordering {
    // Equality based on the token data.
    const auto matching_byte_prefix = (BytePrefix == nullptr) == (other.BytePrefix == nullptr);
    return matching_byte_prefix and Val->TokenData == other.Val->TokenData ? Ordering::equal : Ordering::less;
}

auto spp::asts::CharLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsCharLiteral(*this);
}

auto spp::asts::CharLiteralAst::PosStart() const
    -> std::size_t {
    // Use the value.
    return Val->PosStart();
}

auto spp::asts::CharLiteralAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::CharLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<CharLiteralAst>(
        AstClone(BytePrefix), AstClone(Val));
}

auto spp::asts::CharLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(BytePrefix);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::CharLiteralAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *meta)
    -> void {
    // Clone and return the char literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::CharLiteralAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the LLVM constant integer value.
    const auto ap_int = llvm::APInt(8, Val->TokenData, false);
    return llvm::ConstantInt::get(*ctx->Context, ap_int);
}

auto spp::asts::CharLiteralAst::InferType(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *)
    -> TypeAst* {
    // Try from the cache first.
    using generate::common_types::U8;
    using generate::common_types::CharType;
    USE_CACHED_TYPE_INFERENCE;

    // A char literal is either a Char or U8 type, depending
    // on the "b" byte prefix.
    auto inferred = BytePrefix != nullptr ? U8(Val->PosStart()) : CharType(Val->PosStart());
    CACHE_TYPE_INFERENCE_AND_RETURN(inferred);
}

SPP_MOD_END
