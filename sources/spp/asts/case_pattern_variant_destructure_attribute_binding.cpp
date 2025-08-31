#include <spp/asts/case_pattern_variant_destructure_attribute_binding_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_destructure_attribute_binding_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantDestructureAttributeBindingAst::CasePatternVariantDestructureAttributeBindingAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    name(std::move(name)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
}


spp::asts::CasePatternVariantDestructureAttributeBindingAst::~CasePatternVariantDestructureAttributeBindingAst() = default;


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::pos_end() const -> std::size_t {
    return val->pos_end();
}


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantDestructureAttributeBindingAst>(
        ast_clone(name),
        ast_clone(tok_assign),
        ast_clone(val));
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


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::convert_to_variable(
    mixins::CompilerMetaData *meta)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable destructure attribute binding AST.
    auto var = std::make_unique<LocalVariableDestructureAttributeBindingAst>(ast_clone(name), nullptr, val->convert_to_variable(meta));
    var->m_from_pattern = true;
    return var;
}
