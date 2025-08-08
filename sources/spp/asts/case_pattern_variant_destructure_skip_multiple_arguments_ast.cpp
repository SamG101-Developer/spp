#include <spp/asts/case_pattern_variant_destructure_skip_multiple_arguments_ast.hpp>
#include <spp/asts/case_pattern_variant_single_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    std::unique_ptr<CasePatternVariantAst> &&binding):
    tok_ellipsis(std::move(tok_ellipsis)),
    binding(ast_cast<CasePatternVariantSingleIdentifierAst>(std::move(binding))) {
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::pos_start() const -> std::size_t {
    return tok_ellipsis->pos_start();
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::pos_end() const -> std::size_t {
    return binding ? binding->pos_end() : tok_ellipsis->pos_end();
}


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_ellipsis);
    SPP_STRING_APPEND(binding);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_ellipsis);
    SPP_PRINT_APPEND(binding);
    SPP_PRINT_END;
}
