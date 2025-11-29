module;
#include <spp/macros.hpp>

module spp.asts.use_statement_ast;
import spp.asts.annotation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::UseStatementAst::UseStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_use) &&tok_use,
    decltype(old_type) old_type) :
    m_conversion(nullptr),
    annotations(std::move(annotations)),
    tok_use(std::move(tok_use)),
    old_type(std::move(old_type)) {
}


spp::asts::UseStatementAst::~UseStatementAst() = default;


auto spp::asts::UseStatementAst::pos_start() const
    -> std::size_t {
    return tok_use->pos_start();
}


auto spp::asts::UseStatementAst::pos_end() const
    -> std::size_t {
    return old_type->pos_end();
}


auto spp::asts::UseStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<UseStatementAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_use),
        ast_clone(old_type));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->m_ctx = ast; });
    return ast;
}


spp::asts::UseStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_use).append(" ");
    SPP_STRING_APPEND(old_type);
    SPP_STRING_END;
}


auto spp::asts::UseStatementAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_use).append(" ");
    SPP_PRINT_APPEND(old_type);
    SPP_PRINT_END;
}


auto spp::asts::UseStatementAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the annotations.
    Ast::stage_1_pre_process(ctx);
    annotations | genex::views::for_each([this](const auto &annotation) { annotation->stage_1_pre_process(this); });
}


auto spp::asts::UseStatementAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the steps for the annotations.
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    annotations | genex::views::for_each([sm, meta](const auto &annotation) { annotation->stage_2_gen_top_level_scopes(sm, meta); });

    // Create the type statement AST conversion.
    const auto new_type = std::dynamic_pointer_cast<TypeIdentifierAst>(old_type->type_parts().back()->without_generics());
    m_conversion = std::make_unique<TypeStatementAst>(
        std::move(annotations), nullptr, new_type, nullptr, nullptr, ast_clone(old_type));
    m_conversion->m_for_use_statement = true;
    m_conversion->stage_2_gen_top_level_scopes(sm, meta);
    m_generated = true;
}


auto spp::asts::UseStatementAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate the top-level alias for the converted type statement.
    m_conversion->stage_3_gen_top_level_aliases(sm, meta);
}


auto spp::asts::UseStatementAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the types in the conversion AST.
    m_conversion->stage_4_qualify_types(sm, meta);
}


auto spp::asts::UseStatementAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the conversion AST.
    m_conversion->stage_5_load_super_scopes(sm, meta);
}


auto spp::asts::UseStatementAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip all scopes, as this is a pre-generated AST.
    m_conversion->stage_6_pre_analyse_semantics(sm, meta);
}


auto spp::asts::UseStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the conversion AST.
    m_conversion->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::UseStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for the conversion AST.
    m_conversion->stage_8_check_memory(sm, meta);
}
