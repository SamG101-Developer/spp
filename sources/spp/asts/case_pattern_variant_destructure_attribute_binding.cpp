#include <spp/asts/case_pattern_variant_destructure_attribute_binding_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantDestructureAttributeBindingAst::CasePatternVariantDestructureAttributeBindingAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val):
    name(std::move(name)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
}


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


spp::asts::CasePatternVariantDestructureAttributeBindingAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(val);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(val);
    SPP_PRINT_END;
}
