module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.string_literal_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.convention_ref_ast;
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
    const auto matching_byte_prefix = (BytePrefix == nullptr) == (other.BytePrefix == nullptr);
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
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *meta)
    -> void {
    // Clone and return the float literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::StringLiteralAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create a global string for the string literal.
    const auto bytes = Val->TokenData;
    const auto str_alloc = ctx->Builder.CreateGlobalString(
        bytes, "string_literal", 0, ctx->Module.Get(), false);
    return str_alloc;
}

auto spp::asts::StringLiteralAst::InferType(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;
    using generate::common_types::StringViewType;
    using generate::common_types::ViewU8Type;

    // A char literal is either a StrView or Vec[U8] type, depending on the "b" byte prefix.
    const auto inferred = BytePrefix != nullptr
        ? ViewU8Type(PosStart())->WithConvention(MakeUnique<ConventionRefAst>(nullptr))
        : StringViewType(PosStart())->WithConvention(MakeUnique<ConventionRefAst>(nullptr));
    CACHE_TYPE_INFERENCE_AND_RETURN(AstClone(inferred));
}

SPP_MOD_END
