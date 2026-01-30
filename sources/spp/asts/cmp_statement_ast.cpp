module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.cmp_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import llvm;
import genex;


spp::asts::CmpStatementAst::CmpStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    decltype(tok_assign) &&tok_assign,
    decltype(value) &&value) :
    annotations(std::move(annotations)),
    tok_cmp(std::move(tok_cmp)),
    name(std::move(name)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)),
    tok_assign(std::move(tok_assign)),
    value(std::move(value)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_cmp, lex::SppTokenType::KW_CMP, "cmp");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_colon, lex::SppTokenType::TK_COLON, ":");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


auto spp::asts::CmpStatementAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::CmpStatementAst::pos_end() const
    -> std::size_t {
    return value->pos_end();
}


auto spp::asts::CmpStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<CmpStatementAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_cmp),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type),
        ast_clone(tok_assign),
        ast_clone(value));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->visibility = visibility;
    for (auto const &a : ast->annotations) {
        a->set_ast_ctx(ast.get());
    }
    return ast;
}


spp::asts::CmpStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations, "\n").append(not annotations.empty() ? "\n" : "");
    SPP_STRING_APPEND(tok_cmp).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(value);
    SPP_STRING_END;
}


auto spp::asts::CmpStatementAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // No pre-processing needed for cmp statements.
    Ast::stage_1_pre_process(ctx);
    for (auto &&a : annotations) {
        a->stage_1_pre_process(this);
    }
}


auto spp::asts::CmpStatementAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // No top-level scopes needed for cmp statements.
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    for (auto &&a : annotations) {
        a->stage_2_gen_top_level_scopes(sm, meta);
    }

    // Create a symbol for this constant declaration, pin to prevent moving.
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(
        name, type, false, false, visibility.first);
    sym->memory_info->ast_pins.emplace_back(name.get());
    sym->memory_info->ast_comptime = ast_clone(this);
    sym->memory_info->initialized_by(*this, sm->current_scope);
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::CmpStatementAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Ensure that the convention type doesn't have a convention.
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(
        this, type, *sm, "global constant type");

    // Check the type exists before attaching super scopes
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name();
    sm->current_scope->get_var_symbol(name)->type = type;
}


auto spp::asts::CmpStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the type and value.
    value->stage_7_analyse_semantics(sm, meta);

    // Check the value's type is the same as the given type.
    const auto given_type = type.get();
    const auto inferred_type = value->infer_type(sm, meta);

    raise_if<analyse::errors::SppTypeMismatchError>(
        not analyse::utils::type_utils::symbolic_eq(*given_type, *inferred_type, *sm->current_scope, *sm->current_scope),
        {sm->current_scope}, ERR_ARGS(*type, *given_type, *value, *inferred_type));
}


auto spp::asts::CmpStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the type.
    value->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *value, *value, *sm, true, true, true, true, true, meta);
}


auto spp::asts::CmpStatementAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate the value and assign it to the variable symbol's compile-time value.
    if (type->operator std::string()[0] != '$') {
        const auto var_sym = sm->current_scope->get_var_symbol(name);
        value->stage_9_comptime_resolution(sm, meta);
        var_sym->comptime_value = std::move(meta->cmp_result);
        // var_sym->comptime_value == nullptr // Todo: => ERROR?
    }
}


auto spp::asts::CmpStatementAst::stage_10_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the value in a constant context.
    ctx->in_constant_context = true;
    const auto val = value->stage_11_code_gen_2(sm, meta, ctx);
    ctx->in_constant_context = false;

    // Create the global variable for the constant.
    const auto type_sym = sm->current_scope->get_type_symbol(type);
    const auto llvm_type = codegen::llvm_type(*type_sym, ctx);
    const auto llvm_global_var = new llvm::GlobalVariable(
        *ctx->llvm_module, llvm_type, true, llvm::GlobalValue::ExternalLinkage, llvm::cast<llvm::Constant>(val),
        codegen::mangle::mangle_cmp_name(*sm->current_scope, *this));

    // Register in the llvm info.
    const auto var_sym = sm->current_scope->get_var_symbol(name);
    var_sym->llvm_info->alloca = llvm_global_var;

    return nullptr;
}
