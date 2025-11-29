module;
#include <spp/macros.hpp>

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
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;
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


spp::asts::CmpStatementAst::~CmpStatementAst() = default;


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
    ast->m_visibility = m_visibility;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->m_ctx = ast; });
    return ast;
}


spp::asts::CmpStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_cmp).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(value);
    SPP_STRING_END;
}


auto spp::asts::CmpStatementAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_cmp).append(" ");
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon).append(" ");
    SPP_PRINT_APPEND(type).append(" ");
    SPP_PRINT_APPEND(tok_assign).append(" ");
    SPP_PRINT_APPEND(value);
    SPP_PRINT_END;
}


auto spp::asts::CmpStatementAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // No pre-processing needed for cmp statements.
    Ast::stage_1_pre_process(ctx);
    annotations | genex::views::for_each([this](auto &&x) { x->stage_1_pre_process(this); });
}


auto spp::asts::CmpStatementAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // No top-level scopes needed for cmp statements.
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Ensure that the convention type doesn't have a convention.
    if (const auto conv = type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *this, *conv, "global constant type").with_scopes({sm->current_scope}).raise();
    }

    // Create a symbol for this constant declaration, pin to prevent moving.
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(
        name, type, false, false, m_visibility.first);
    sym->memory_info->ast_pins.emplace_back(name.get());
    sym->memory_info->ast_comptime = ast_clone(this);
    sym->memory_info->initialized_by(*this, sm->current_scope);
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::CmpStatementAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the type exists before attaching super scopes
    type->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CmpStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the type and value.
    type->stage_7_analyse_semantics(sm, meta);
    value->stage_7_analyse_semantics(sm, meta);

    // Check the value's type is the same as the given type.
    const auto given_type = type.get();
    const auto inferred_type = value->infer_type(sm, meta);

    if (not analyse::utils::type_utils::symbolic_eq(*given_type, *inferred_type, *sm->current_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
            *type, *given_type, *value, *inferred_type).with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::CmpStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory of the type.
    value->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *value, *value, *sm, true, true, true, true, true, true, meta);
}


auto spp::asts::CmpStatementAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the value and store the constant.
    const auto val = value->stage_10_code_gen_2(sm, meta, ctx);
    ctx->global_constants[codegen::mangle::mangle_cmp_name(*sm->current_scope, *this)] = llvm::cast<llvm::Constant>(val);
    return nullptr;
}
