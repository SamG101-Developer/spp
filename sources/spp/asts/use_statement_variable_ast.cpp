module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.scope_utils;
import spp.asts.utils;
import spp.utils.strings;
import genex;


spp::asts::UseStatementVariableAst::UseStatementVariableAst(
    decltype(annotations) &&annotations,
    decltype(tok_use) &&tok_use,
    decltype(old_var) old_var) :
    annotations(std::move(annotations)),
    tok_use(std::move(tok_use)),
    old_var(std::move(old_var)) {
}


spp::asts::UseStatementVariableAst::~UseStatementVariableAst() = default;


auto spp::asts::UseStatementVariableAst::pos_start() const
    -> std::size_t {
    return tok_use->pos_start();
}


auto spp::asts::UseStatementVariableAst::pos_end() const
    -> std::size_t {
    return old_var->pos_end();
}


auto spp::asts::UseStatementVariableAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<UseStatementVariableAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_use),
        ast_clone(old_var));
    ast->set_ast_ctx(get_ast_ctx());
    ast->set_ast_scope(get_ast_scope());
    for (auto const &a : ast->annotations) {
        a->set_ast_ctx(ast.get());
    }
    return ast;
}


spp::asts::UseStatementVariableAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations, "\n").append(not annotations.empty() ? "\n" : "");
    SPP_STRING_APPEND(tok_use).append(" ");
    SPP_STRING_APPEND(old_var);
    SPP_STRING_END;
}


auto spp::asts::UseStatementVariableAst::stage_1_pre_process(
    AbstractAst *ctx)
    -> void {
    // Pre-process the annotations.
    Ast::stage_1_pre_process(ctx);
    for (const auto &annotation : annotations) {
        annotation->set_ast_ctx(this);
    }
}


auto spp::asts::UseStatementVariableAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the steps for the annotations.
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    for (auto const &a : annotations) {
        a->stage_2_gen_top_level_scopes(sm, meta);
    }

    // Create the conversion.
    // Todo: Error based on ordering. move this into stage 3?
    auto identifier = ast_clone(old_var->expr_parts().back()->to<IdentifierAst>());
    m_conversion = std::make_unique<CmpStatementAst>(
        std::move(annotations), nullptr, std::move(identifier), nullptr, nullptr, nullptr, ast_clone(old_var));
    m_conversion->mark_from_use_statement();
    m_conversion->stage_2_gen_top_level_scopes(sm, meta);
    m_generated = true;
}


auto spp::asts::UseStatementVariableAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate the top-level alias for the converted type statement.
    // const auto scope = sm->current_scope->convert_postfix_to_nested_scope(old_var->to<PostfixExpressionAst>()->lhs.get());
    const auto [old_var_sym, scope] = analyse::utils::scope_utils::get_var_symbol_outermost(*sm->current_scope, *old_var);
    if (old_var_sym != nullptr) {
        // Cmp statements
        m_conversion->type = scope->get_type_symbol(old_var_sym->type)->fq_name(false);
        old_var_sym->type = m_conversion->type;

        auto alias_sym = dynamic_cast<analyse::scopes::TypeSymbol*>(m_conversion->m_alias_sym.get());
        alias_sym->alias_sym = old_var_sym;
        alias_sym->type = m_conversion->type;
        m_conversion->stage_3_gen_top_level_aliases(sm, meta);
        return;
    }

    // const auto old_ns_sym = sm->current_scope->convert_postfix_to_nested_scope(old_var.get());
    if (old_var_sym == nullptr) {
        // and old_ns_sym == nullptr) {
        // Todo: alternatives based on lhs of the old var.
        const auto closest_match = spp::utils::strings::closest_match(
            old_var->to_string(), {});

        raise<analyse::errors::SppIdentifierUnknownError>(
            {sm->current_scope}, ERR_ARGS(*this, "constant variable", closest_match));
    }

    // if (old_ns_sym != nullptr) {
    //     throw std::runtime_error("TEST");
    // }
}


auto spp::asts::UseStatementVariableAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the types in the conversion AST.
    m_conversion->stage_4_qualify_types(sm, meta);
}


auto spp::asts::UseStatementVariableAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the conversion AST.
    m_conversion->stage_5_load_super_scopes(sm, meta);
}


auto spp::asts::UseStatementVariableAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for the conversion AST.
    m_conversion->stage_6_pre_analyse_semantics(sm, meta);
}


auto spp::asts::UseStatementVariableAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the conversion AST.
    m_conversion->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::UseStatementVariableAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for the conversion AST.
    m_conversion->stage_8_check_memory(sm, meta);
}


auto spp::asts::UseStatementVariableAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve the conversion AST.
    m_conversion->stage_9_comptime_resolution(sm, meta);
}


auto spp::asts::UseStatementVariableAst::stage_10_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Code gen for the conversion AST.
    return m_conversion->stage_10_code_gen_1(sm, meta, ctx);
}
