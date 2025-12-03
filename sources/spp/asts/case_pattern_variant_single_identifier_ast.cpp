module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_single_identifier_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::CasePatternVariantSingleIdentifierAst::CasePatternVariantSingleIdentifierAst(
    decltype(tok_mut) &&tok_mut,
    decltype(name) &&name,
    decltype(alias) &&alias) :
    tok_mut(std::move(tok_mut)),
    name(std::move(name)),
    alias(std::move(alias)) {
}


spp::asts::CasePatternVariantSingleIdentifierAst::~CasePatternVariantSingleIdentifierAst() = default;


auto spp::asts::CasePatternVariantSingleIdentifierAst::pos_start() const
    -> std::size_t {
    return tok_mut->pos_start();
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::pos_end() const
    -> std::size_t {
    return alias->pos_end();
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<CasePatternVariantSingleIdentifierAst>(
        ast_clone(tok_mut),
        ast_clone(name),
        ast_clone(alias));
}


spp::asts::CasePatternVariantSingleIdentifierAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_mut);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(alias);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_mut);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(alias);
    SPP_PRINT_END;
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::convert_to_variable(
    CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    // Create the local variable single identifier binding AST.
    auto var = std::make_unique<LocalVariableSingleIdentifierAst>(ast_clone(tok_mut), ast_clone(name), ast_clone(alias));
    var->mark_from_case_pattern();
    return var;
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward analysis into the name and alias.
    auto var = convert_to_variable(meta);
    m_mapped_let = std::make_unique<LetStatementInitializedAst>(
        nullptr, std::move(var), nullptr, nullptr, ast_clone(meta->case_condition));
    m_mapped_let->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CasePatternVariantSingleIdentifierAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward memory checks into the name and alias.
    m_mapped_let->stage_8_check_memory(sm, meta);
}
