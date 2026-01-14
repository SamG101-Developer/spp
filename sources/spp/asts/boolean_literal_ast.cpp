module;
#include <spp/macros.hpp>

module spp.asts.boolean_literal_ast;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.generate.common_types;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::BooleanLiteralAst::BooleanLiteralAst(
    decltype(tok_bool) &&tok_bool) :
    tok_bool(std::move(tok_bool)) {
}


auto spp::asts::BooleanLiteralAst::equals(
    ExpressionAst const &other) const
    -> std::strong_ordering {
    return other.equals_boolean_literal(*this);
}


auto spp::asts::BooleanLiteralAst::equals_boolean_literal(
    BooleanLiteralAst const &other) const
    -> std::strong_ordering {
    if (*tok_bool == *other.tok_bool) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::BooleanLiteralAst::pos_start() const
    -> std::size_t {
    return tok_bool->pos_start();
}


auto spp::asts::BooleanLiteralAst::pos_end() const
    -> std::size_t {
    return tok_bool->pos_end();
}


auto spp::asts::BooleanLiteralAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<BooleanLiteralAst>(
        ast_clone(tok_bool));
}


spp::asts::BooleanLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_bool);
    SPP_STRING_END;
}


auto spp::asts::BooleanLiteralAst::True(
    const std::size_t pos)
    -> std::unique_ptr<BooleanLiteralAst> {
    auto tok = std::make_unique<TokenAst>(pos, lex::SppTokenType::KW_TRUE, "true");
    return std::make_unique<BooleanLiteralAst>(std::move(tok));
}


auto spp::asts::BooleanLiteralAst::False(
    const std::size_t pos)
    -> std::unique_ptr<BooleanLiteralAst> {
    auto tok = std::make_unique<TokenAst>(pos, lex::SppTokenType::KW_FALSE, "false");
    return std::make_unique<BooleanLiteralAst>(std::move(tok));
}


auto spp::asts::BooleanLiteralAst::stage_10_code_gen_2(
    ScopeManager *,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Map the boolean literal to an LLVM constant integer.
    const auto value = tok_bool->token_type == lex::SppTokenType::KW_TRUE ? 1ul : 0ul;
    return llvm::ConstantInt::get(ctx->builder.getIntNTy(1), value);
}


auto spp::asts::BooleanLiteralAst::infer_type(
    ScopeManager *,
    CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // The boolean ast is always inferred as "std::boolean::Bool".
    return generate::common_types::boolean_type(pos_start());
}
