module;
#include <spp/macros.hpp>

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
    decltype(Val) &&val) :
    Val(std::move(val)) {
}

spp::asts::CharLiteralAst::~CharLiteralAst() = default;

auto spp::asts::CharLiteralAst::EqualsCharLiteral(
    CharLiteralAst const &other) const
    -> Ordering {
    // Equality based on the token data.
    return Val->TokenData == other.Val->TokenData ? Ordering::equal : Ordering::less;
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
        AstClone(Val));
}

auto spp::asts::CharLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::CharLiteralAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Clone and return the char literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::CharLiteralAst::Stage11_CodeGen(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the LLVM constant integer value.
    const auto ap_int = llvm::APInt(8, Val->TokenData, false);
    return llvm::ConstantInt::get(*ctx->Context, ap_int);
}

auto spp::asts::CharLiteralAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // A char literal is always a U8 type.
    using generate::common_types::U8;
    return U8(Val->PosStart());
}

SPP_MOD_END
