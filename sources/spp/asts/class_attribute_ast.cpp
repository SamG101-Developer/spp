module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.class_attribute_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::ClassAttributeAst::ClassAttributeAst(
    decltype(annotations) &&annotations,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type,
    decltype(default_val) &&default_val) :
    annotations(std::move(annotations)),
    name(std::move(name)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)),
    default_val(std::move(default_val)) {
}


auto spp::asts::ClassAttributeAst::pos_start() const
    -> std::size_t {
    return name->pos_start();
}


auto spp::asts::ClassAttributeAst::pos_end() const
    -> std::size_t {
    return type->pos_end();
}


auto spp::asts::ClassAttributeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<ClassAttributeAst>(
        ast_clone_vec(annotations),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type),
        ast_clone(default_val));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    for (auto const &a: ast->annotations) {
        a->set_ast_ctx(ast.get());
    }
    return ast;
}


spp::asts::ClassAttributeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations, "\n").append(not annotations.empty() ? "\n" : "");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon).append(" ");
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::ClassAttributeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations.
    Ast::stage_1_pre_process(ctx);
    for (auto const &a : annotations) {
        a->set_ast_ctx(this);
    }
}


auto spp::asts::ClassAttributeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run the generation steps for the annotations.
    for (auto const &a : annotations) {
        a->stage_2_gen_top_level_scopes(sm, meta);
    }

    // Create a variable symbol for this attribute in the current scope (class scope).
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(name, type, false, false, visibility.first);
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::ClassAttributeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the type is valid before scopes are attached.
    type->stage_7_analyse_semantics(sm, meta);
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(type, type, *sm, "attribute type");
    type = sm->current_scope->get_type_symbol(type)->fq_name();
    sm->current_scope->get_var_symbol(name)->type = type;
}


auto spp::asts::ClassAttributeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Repeated convention check for generic substitutions.
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(type, type, *sm, "attribute type");

    // Todo: I hate this, yet it works. Will fix later.
    const auto meta_depth = meta->depth();
    try {
        type->stage_7_analyse_semantics(sm, meta);
        type = sm->current_scope->get_type_symbol(type)->fq_name();
        const auto var_sym = sm->current_scope->get_var_symbol(name);
        var_sym->type = type;
    }
    catch (analyse::errors::SppIdentifierUnknownError const &) {
        while (meta->depth() > meta_depth) {
            meta->restore();
        }
    }

    if (default_val != nullptr) {
        default_val->stage_7_analyse_semantics(sm, meta);
        const auto default_type = default_val->infer_type(sm, meta);

        // Make sure the default's inferred type matches the attribute's type.
        if (not analyse::utils::type_utils::symbolic_eq(*type, *default_type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
                .with_args(*this, *type, *default_val, *default_type)
                .raises_from(sm->current_scope);
        }
    }
}


auto spp::asts::ClassAttributeAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is a default value, check it for memory errors.
    if (default_val != nullptr) {
        default_val->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *default_val, *default_val, *sm, true, true, true, true, true, true, meta);
    }
}
