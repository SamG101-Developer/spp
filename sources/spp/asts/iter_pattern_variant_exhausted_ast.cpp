#include <spp/asts/iter_pattern_variant_exhausted_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::IterPatternVariantExhaustedAst::IterPatternVariantExhaustedAst(
    decltype(tok_exhausted) &&tok_exhausted) :
    IterPatternVariantAst(),
    tok_exhausted(std::move(tok_exhausted)) {
}


spp::asts::IterPatternVariantExhaustedAst::~IterPatternVariantExhaustedAst() = default;


auto spp::asts::IterPatternVariantExhaustedAst::pos_start() const -> std::size_t {
    return tok_exhausted->pos_start();
}


auto spp::asts::IterPatternVariantExhaustedAst::pos_end() const -> std::size_t {
    return tok_exhausted->pos_end();
}


auto spp::asts::IterPatternVariantExhaustedAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantExhaustedAst>(
        ast_clone(tok_exhausted));
}


spp::asts::IterPatternVariantExhaustedAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_exhausted);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantExhaustedAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_exhausted);
    SPP_PRINT_END;
}
