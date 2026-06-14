module;
#include <spp/macros.hpp>

module spp.asts.string_literal_ast;
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
spp::asts::StringLiteralAst::StringLiteralAst(
    decltype(BytePrefix) &&byte_prefix,
    decltype(Val) &&val) :
    BytePrefix(std::move(byte_prefix)),
    Val(std::move(val)) {
}

spp::asts::StringLiteralAst::~StringLiteralAst() = default;

auto spp::asts::StringLiteralAst::EqualsStringLiteral(
    StringLiteralAst const &other) const
    -> Ordering {
    // Equality is based on the internal string value.
    const auto matching_byte_prefix = static_cast<bool>(BytePrefix) == static_cast<bool>(other.BytePrefix);
    return matching_byte_prefix and Val->TokenData == other.Val->TokenData ? Ordering::equal : Ordering::less;
}

auto spp::asts::StringLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsStringLiteral(*this);
}

auto spp::asts::StringLiteralAst::PosStart() const
    -> std::size_t {
    // Use the value.
    return Val->PosStart();
}

auto spp::asts::StringLiteralAst::PosEnd() const
    -> std::size_t {
    // Use the value.
    return Val->PosEnd();
}

auto spp::asts::StringLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<StringLiteralAst>(
        AstClone(BytePrefix), AstClone(Val));
}

auto spp::asts::StringLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(BytePrefix);
    SPP_STRING_APPEND(Val);
    SPP_STRING_END;
}

auto spp::asts::StringLiteralAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *meta)
    -> void {
    // Clone and return the float literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::StringLiteralAst::Stage11_CodeGen(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create a global string for the string literal.
    const auto bytes = Val->TokenData;
    const auto str_alloc = ctx->Builder.CreateGlobalString(
        bytes, "string_literal", 0, ctx->Module.get(), false);
    return str_alloc;
}

auto spp::asts::StringLiteralAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // A char literal is either a StrView or Vec[U8] type, depending on the "b" byte prefix.
    using generate::common_types::StringViewType;
    using generate::common_types::VecU8Type;
    return BytePrefix != nullptr
        ? VecU8Type(PosStart())
        : StringViewType(PosStart());
}

SPP_MOD_END
