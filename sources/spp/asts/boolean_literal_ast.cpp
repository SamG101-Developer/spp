module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.boolean_literal_ast;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.generate.common_types;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
spp::asts::BooleanLiteralAst::BooleanLiteralAst(
    decltype(TokBool) &&tok_bool) :
    TokBool(std::move(tok_bool)) {
}

spp::asts::BooleanLiteralAst::~BooleanLiteralAst() = default;

auto spp::asts::BooleanLiteralAst::EqualsBooleanLiteral(
    BooleanLiteralAst const &other) const
    -> Ordering {
    // Equality is based off the bool value.
    return *TokBool == *other.TokBool ? Ordering::equal : Ordering::less;
}

auto spp::asts::BooleanLiteralAst::Equals(
    ExpressionAst const &other) const
    -> Ordering {
    // Reverse hook (double dispatch).
    return other.EqualsBooleanLiteral(*this);
}

auto spp::asts::BooleanLiteralAst::PosStart() const
    -> std::size_t {
    // Use the bool token.
    return TokBool->PosStart();
}

auto spp::asts::BooleanLiteralAst::PosEnd() const
    -> std::size_t {
    // Use the bool token.
    return TokBool->PosEnd();
}

auto spp::asts::BooleanLiteralAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<BooleanLiteralAst>(
        AstClone(TokBool));
}

auto spp::asts::BooleanLiteralAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokBool);
    SPP_STRING_END;
}

auto spp::asts::BooleanLiteralAst::True(
    const std::size_t pos)
    -> Unique<BooleanLiteralAst> {
    // Create a boolean literal AST representing the "true" value.
    auto tok = MakeUnique<TokenAst>(pos, lex::SppTokenType::KW_TRUE, "true");
    return MakeUnique<BooleanLiteralAst>(std::move(tok));
}

auto spp::asts::BooleanLiteralAst::False(
    const std::size_t pos)
    -> Unique<BooleanLiteralAst> {
    // Create a boolean literal AST representing the "false" value.
    auto tok = MakeUnique<TokenAst>(pos, lex::SppTokenType::KW_FALSE, "false");
    return MakeUnique<BooleanLiteralAst>(std::move(tok));
}

auto spp::asts::BooleanLiteralAst::IsTrue() const
    -> bool {
    // Check if the boolean literal represents a true value.
    return TokBool->TokenType == lex::SppTokenType::KW_TRUE;
}

auto spp::asts::BooleanLiteralAst::CppVal() const
    -> bool {
    // Return the C++ boolean value of the boolean literal.
    return IsTrue();
}

auto spp::asts::BooleanLiteralAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *meta)
    -> void {
    // Clone and return the boolean literal as is for compile-time resolution.
    meta->CmpResult = AstClone(this);
}

auto spp::asts::BooleanLiteralAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Map the boolean literal to an LLVM constant integer.
    SPP_ASSERT(
        TokBool->TokenType == lex::SppTokenType::KW_TRUE or
        TokBool->TokenType == lex::SppTokenType::KW_FALSE);
    const auto value = TokBool->TokenType == lex::SppTokenType::KW_TRUE ? 1ul : 0ul;
    return llvm::ConstantInt::get(ctx->Builder.getIntNTy(1), value);
}

auto spp::asts::BooleanLiteralAst::InferType(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // The boolean ast is always inferred as "std::boolean::Bool".
    using generate::common_types::BooleanType;
    auto inferred = BooleanType(PosStart());
    CACHE_TYPE_INFERENCE_AND_RETURN(inferred);
}

SPP_MOD_END
