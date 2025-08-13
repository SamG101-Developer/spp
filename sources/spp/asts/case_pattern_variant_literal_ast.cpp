#include <format>

#include <spp/asts/case_pattern_variant_literal_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/local_variable_single_identifier_alias_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/literal_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
    decltype(literal) &&literal) :
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


auto spp::asts::CasePatternVariantLiteralAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantLiteralAst>(
        ast_clone(*literal));
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


auto spp::asts::CasePatternVariantLiteralAst::convert_to_variable(
    mixins::CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable literal binding AST.
    auto name = std::make_unique<IdentifierAst>(pos_start(), std::format("$_{}", reinterpret_cast<std::uintptr_t>(this)));
    auto var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, nullptr, nullptr);
    var->m_from_pattern = true;
    return var;
}


auto spp::asts::CasePatternVariantLiteralAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward analysis into the literal.
    literal->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantLiteralAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the literal.
    literal->stage_8_check_memory(sm, meta);
}
