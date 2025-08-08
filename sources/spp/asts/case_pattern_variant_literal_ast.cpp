#include <spp/asts/case_pattern_variant_literal_ast.hpp>
#include <spp/asts/literal_ast.hpp>


spp::asts::CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
    decltype(literal) &&literal):
    CasePatternVariantAst(),
    literal(std::move(literal)) {
}


spp::asts::CasePatternVariantLiteralAst::~CasePatternVariantLiteralAst() = default;


auto spp::asts::CasePatternVariantLiteralAst::pos_start() const -> std::size_t {
    return literal->pos_start();
}


auto spp::asts::CasePatternVariantLiteralAst::pos_end() const -> std::size_t {
    return literal->pos_end();
}


spp::asts::CasePatternVariantLiteralAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(literal);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantLiteralAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(literal);
    SPP_PRINT_END;
}
