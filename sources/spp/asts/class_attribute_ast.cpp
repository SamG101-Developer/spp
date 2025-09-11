#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/for_each.hpp>


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


spp::asts::ClassAttributeAst::~ClassAttributeAst() = default;


auto spp::asts::ClassAttributeAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::ClassAttributeAst::pos_end() const -> std::size_t {
    return type->pos_end();
}


auto spp::asts::ClassAttributeAst::clone() const -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<ClassAttributeAst>(
        ast_clone_vec(annotations),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type),
        ast_clone(default_val));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->m_ctx = ast; });
    return ast;
}


spp::asts::ClassAttributeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(default_val);
    SPP_STRING_END;
}


auto spp::asts::ClassAttributeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(default_val);
    SPP_PRINT_END;
}


auto spp::asts::ClassAttributeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations.
    Ast::stage_1_pre_process(ctx);
    annotations | genex::views::for_each([this](auto &&x) { x->stage_1_pre_process(this); });
}


auto spp::asts::ClassAttributeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Run the generation steps for the annotations.
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Ensure that the type does not have a convention.
    if (const auto conv = type->get_convention()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *type, *conv, "attribute type").with_scopes({sm->current_scope}).raise();
    }

    // Create a variable symbol for this attribute in the current scope (class scope).
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(name, type, false, false, m_visibility.first);
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::ClassAttributeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Check the type is valid before scopes are attached.
    type->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::ClassAttributeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Repeated convention check for generic substitutions.
    if (const auto conv = type->get_convention()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *type, *conv, "attribute type").with_scopes({sm->current_scope}).raise();
    }

    type->stage_7_analyse_semantics(sm, meta);
    if (default_val != nullptr) {
        default_val->stage_7_analyse_semantics(sm, meta);
        const auto default_type = default_val->infer_type(sm, meta);

        // Make sure the default's inferred type matches the attribute's type.
        if (not analyse::utils::type_utils::symbolic_eq(*type, *default_type, *sm->current_scope, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
                *this, *type, *default_val, *default_type).with_scopes({sm->current_scope}).raise();
        }
    }
}


auto spp::asts::ClassAttributeAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // If there is a default value, check it for memory errors.
    if (default_val != nullptr) {
        default_val->stage_8_check_memory(sm, meta);
        analyse::utils::mem_utils::validate_symbol_memory(
            *default_val, *default_val, *sm, true, true, true, true, true, true, meta);
    }
}
