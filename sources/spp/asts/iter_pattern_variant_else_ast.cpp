#include <spp/asts/iter_pattern_variant_else_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::IterPatternVariantElseAst::IterPatternVariantElseAst(
    decltype(tok_else) &&tok_else) :
    IterPatternVariantAst(),
    tok_else(std::move(tok_else)) {
}


spp::asts::IterPatternVariantElseAst::~IterPatternVariantElseAst() = default;


auto spp::asts::IterPatternVariantElseAst::pos_start() const -> std::size_t {
    return tok_else->pos_start();
}


auto spp::asts::IterPatternVariantElseAst::pos_end() const -> std::size_t {
    return tok_else->pos_end();
}


auto spp::asts::IterPatternVariantElseAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<IterPatternVariantElseAst>(
        ast_clone(tok_else));
}


spp::asts::IterPatternVariantElseAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_else);
    SPP_STRING_END;
}


auto spp::asts::IterPatternVariantElseAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_else);
    SPP_PRINT_END;
}
