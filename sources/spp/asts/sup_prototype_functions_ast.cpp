module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.sup_prototype_functions_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;


spp::asts::SupPrototypeFunctionsAst::SupPrototypeFunctionsAst(
    decltype(tok_sup) &&tok_sup,
    decltype(generic_param_group) &&generic_param_group,
    decltype(name) &&name,
    decltype(impl) &&impl) :
    tok_sup(std::move(tok_sup)),
    generic_param_group(std::move(generic_param_group)),
    name(std::move(name)),
    impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_sup, lex::SppTokenType::KW_SUP, "sup");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->impl);
}


spp::asts::SupPrototypeFunctionsAst::~SupPrototypeFunctionsAst() = default;


auto spp::asts::SupPrototypeFunctionsAst::pos_start() const
    -> std::size_t {
    return tok_sup->pos_start();
}


auto spp::asts::SupPrototypeFunctionsAst::pos_end() const
    -> std::size_t {
    return impl->pos_start();
}


auto spp::asts::SupPrototypeFunctionsAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<SupPrototypeFunctionsAst>(
        ast_clone(tok_sup),
        ast_clone(generic_param_group),
        ast_clone(name),
        ast_clone(impl));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    return ast;
}


spp::asts::SupPrototypeFunctionsAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_sup).append(" ");
    SPP_STRING_APPEND(generic_param_group).append(generic_param_group->params.empty() ? "" : " ");
    SPP_STRING_APPEND(name).append(" ");
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::SupPrototypeFunctionsAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_sup).append(" ");
    SPP_PRINT_APPEND(generic_param_group).append(generic_param_group->params.empty() ? "" : " ");
    SPP_PRINT_APPEND(name).append(" ");
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::SupPrototypeFunctionsAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing the implementation.
    Ast::stage_1_pre_process(ctx);
    impl->stage_1_pre_process(this);
}


auto spp::asts::SupPrototypeFunctionsAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a new scope for the superimposition extension.
    auto scope_name = analyse::scopes::ScopeBlockName("<sup#" + static_cast<std::string>(*name) + "#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // Check there are optional generic parameters.
    if (const auto optional = generic_param_group->get_optional_params(); not optional.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSuperimpositionOptionalGenericParameterError>()
            .with_args(*optional[0])
            .raises_from(sm->current_scope);
    }

    // Check every generic parameter is constrained by the type.
    if (const auto unconstrained = generic_param_group->get_all_params() | genex::views::filter([this](auto &&x) { return not name->contains_generic(*x); }) | genex::to<std::vector>(); not unconstrained.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSuperimpositionUnconstrainedGenericParameterError>()
            .with_args(*unconstrained[0])
            .raises_from(sm->current_scope);
    }

    // No conventions allowed on the name.
    if (auto &&conv = name->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>()
            .with_args(*name, *conv, "superimposition type")
            .raises_from(sm->current_scope);
    }

    // Generate symbols for the generic parameter group, and the self type.
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    impl->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_3_gen_top_level_aliases(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Forward to the implementation.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the superimposition scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Analyse the type being superimposed over.
    name->stage_7_analyse_semantics(sm, meta);
    name = sm->current_scope->get_type_symbol(name)->fq_name();

    // Register the superimposition against the base symbol.
    const auto base_cls_sym = sm->current_scope->get_type_symbol(name->without_generics());
    if (sm->current_scope->parent == sm->current_scope->parent_module()) {
        if (not base_cls_sym->is_generic) {
            sm->normal_sup_blocks[base_cls_sym.get()].emplace_back(sm->current_scope);
        }
        else {
            sm->generic_sup_blocks.emplace_back(sm->current_scope);
        }
    }

    // Add the "Self" symbol into the scope.
    if (name->type_parts().back()->name[0] != '$') {
        const auto cls_sym = sm->current_scope->get_type_symbol(name);
        const auto self_sym = std::make_shared<analyse::scopes::TypeSymbol>(
            std::make_unique<TypeIdentifierAst>(name->pos_start(), "Self", nullptr),
            cls_sym->type, cls_sym->scope, sm->current_scope);
        self_sym->alias_stmt = std::make_unique<TypeStatementAst>(
            SPP_NO_ANNOTATIONS, nullptr,
            TypeIdentifierAst::from_string("Self"), nullptr, nullptr, name);
        sm->current_scope->add_type_symbol(self_sym);
        sm->current_scope->get_type_symbol(name)->aliased_by_symbols.emplace_back(self_sym);
    }

    // Load the implementation and move out of the scope.
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    name->stage_7_analyse_semantics(sm, meta);
    impl->stage_6_pre_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    name->stage_7_analyse_semantics(sm, meta);
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::SupPrototypeFunctionsAst::stage_9_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_9_code_gen_1(sm, meta, ctx);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::SupPrototypeFunctionsAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Move to the next scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_10_code_gen_2(sm, meta, ctx);
    sm->move_out_of_current_scope();
    return nullptr;
}
