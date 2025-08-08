#include <spp/asts/case_pattern_variant_destructure_skip_single_argument_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::CasePatternVariantDestructureSkipSingleArgumentAst(
    decltype(tok_underscore) &&tok_underscore):
    tok_underscore(std::move(tok_underscore)) {
}


auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::pos_start() const -> std::size_t {
    return tok_underscore->pos_start();
}


auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::pos_end() const -> std::size_t {
    return tok_underscore->pos_end();
}


spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_underscore);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_underscore);
    SPP_PRINT_END;
}
