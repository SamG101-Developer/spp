module;
#include <spp/macros.hpp>

module spp.asts.function_parameter_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.let_statement_uninitialized_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.asts.meta.compiler_meta_data;
import spp.lex.tokens;


spp::asts::FunctionParameterAst::FunctionParameterAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_colon, lex::SppTokenType::TK_COLON, ":", var ? var->pos_end() : 0);
    if (this->var == nullptr) {
        auto var_name = std::make_unique<IdentifierAst>(0, std::format("$_{}", reinterpret_cast<std::uintptr_t>(this)));
        this->var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);
    }
}


spp::asts::FunctionParameterAst::~FunctionParameterAst() = default;


auto spp::asts::FunctionParameterAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return var->extract_names();
}


auto spp::asts::FunctionParameterAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return var->extract_name();
}


auto spp::asts::FunctionParameterAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name()->with_convention(ast_clone(type->get_convention()));

    // Create the variable for the parameter (use temp copies and put them back).
    const auto ast = std::make_unique<LetStatementUninitializedAst>(nullptr, std::move(var), nullptr, type);
    ast->stage_7_analyse_semantics(sm, meta);
    var = std::move(ast->var);

    // Mark the symbol as initialized.
    const auto conv = type->get_convention();
    for (auto const &name : extract_names()) {
        const auto sym = sm->current_scope->get_var_symbol(name);
        sym->memory_info->initialized_by(*this, sm->current_scope);
        sym->memory_info->ast_borrowed = {conv, sm->current_scope};
    }
}


auto spp::asts::FunctionParameterAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Check the memory of each name.
    for (auto &&name : extract_names()) {
        const auto sym = sm->current_scope->get_var_symbol(name);
        sym->memory_info->initialized_by(*this, sm->current_scope);
    }
}


auto spp::asts::FunctionParameterAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the local variable so that the symbol table receives the alloca.
    var->stage_10_code_gen_2(sm, meta, ctx);
    return nullptr;
}
