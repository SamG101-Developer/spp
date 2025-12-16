module;
#include <spp/macros.hpp>

module spp.asts.iter_pattern_variant_else_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::IterPatternVariantElseAst::IterPatternVariantElseAst(
    decltype(tok_else) &&tok_else) :
    IterPatternVariantAst(),
    tok_else(std::move(tok_else)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_else, lex::SppTokenType::KW_ELSE, "else");
}


spp::asts::IterPatternVariantElseAst::~IterPatternVariantElseAst() = default;


auto spp::asts::IterPatternVariantElseAst::pos_start() const
    -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::IterPatternVariantElseAst::pos_end() const
    -> std::size_t {
    return tok_else->pos_end();
}


auto spp::asts::IterPatternVariantElseAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantElseAst>(
        ast_clone(tok_else));
}


spp::asts::IterPatternVariantElseAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantElseAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_END;
}


auto spp::asts::IterPatternVariantElseAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx) -> llvm::Value* {
    // Not implemented yet.
    (void)sm;
    (void)meta;
    (void)ctx;
    throw std::runtime_error("IterPatternVariantElseAst::stage_10_code_gen_2 not implemented");
}
