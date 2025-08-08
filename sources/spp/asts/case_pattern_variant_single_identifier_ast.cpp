#include <spp/asts/case_pattern_variant_single_identifier_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/local_variable_single_identifier_alias_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantSingleIdentifierAst::CasePatternVariantSingleIdentifierAst(
    decltype(tok_mut) &&tok_mut,
    decltype(name) &&name,
    decltype(alias) &&alias):
    tok_mut(std::move(tok_mut)),
    name(std::move(name)),
    alias(std::move(alias)) {
}


spp::asts::CasePatternVariantSingleIdentifierAst::~CasePatternVariantSingleIdentifierAst() = default;


auto spp::asts::CasePatternVariantSingleIdentifierAst::pos_start() const -> std::size_t {
    return tok_mut->pos_start();
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::pos_end() const -> std::size_t {
    return alias->pos_end();
}


spp::asts::CasePatternVariantSingleIdentifierAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_mut);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(alias);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_mut);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(alias);
    SPP_PRINT_END;
}
