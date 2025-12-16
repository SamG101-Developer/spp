module;
#include <spp/macros.hpp>

module spp.asts.iter_pattern_variant_exhausted_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::IterPatternVariantExhaustedAst::IterPatternVariantExhaustedAst(
    decltype(tok_exhausted) &&tok_exhausted) :
    IterPatternVariantAst(),
    tok_exhausted(std::move(tok_exhausted)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_exhausted, lex::SppTokenType::TK_DOUBLE_EXCLAMATION_MARK, "!!");
}


spp::asts::IterPatternVariantExhaustedAst::~IterPatternVariantExhaustedAst() = default;


auto spp::asts::IterPatternVariantExhaustedAst::pos_start() const
    -> std::size_t {
    return tok_exhausted->pos_start();
}


auto spp::asts::IterPatternVariantExhaustedAst::pos_end() const
    -> std::size_t {
    return tok_exhausted->pos_end();
}


auto spp::asts::IterPatternVariantExhaustedAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantExhaustedAst>(
        ast_clone(tok_exhausted));
}


spp::asts::IterPatternVariantExhaustedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exhausted);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantExhaustedAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_exhausted);
    SPP_PRINT_END;
}


auto spp::asts::IterPatternVariantExhaustedAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Not implemented yet.
    (void)sm;
    (void)meta;
    (void)ctx;
    throw std::runtime_error("IterPatternVariantExhaustedAst::stage_10_code_gen_2 not implemented yet.");
}
