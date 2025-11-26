module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_else_ast;
import spp.lex.tokens;
import spp.asts.ast;
import spp.asts.token_ast;


spp::asts::CasePatternVariantElseAst::CasePatternVariantElseAst(
    decltype(tok_else) &&tok_else) :
    tok_else(std::move(tok_else)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_else, lex::SppTokenType::KW_ELSE, "else");
}


spp::asts::CasePatternVariantElseAst::~CasePatternVariantElseAst() = default;


auto spp::asts::CasePatternVariantElseAst::pos_start() const
    -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::CasePatternVariantElseAst::pos_end() const
    -> std::size_t {
    return tok_else->pos_end();
}


auto spp::asts::CasePatternVariantElseAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantElseAst>(
        ast_clone(tok_else));
}


spp::asts::CasePatternVariantElseAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantElseAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantElseAst::stage_10_code_gen_2(
    ScopeManager *,
    meta::CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // The "else" pattern always matches, so return "true".
    return llvm::ConstantInt::getTrue(ctx->context);
}
