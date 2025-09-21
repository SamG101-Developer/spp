#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/for_each.hpp>


spp::asts::ClassPrototypeAst::ClassPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_cls) &&tok_cls,
    decltype(name) name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(impl) &&impl) :
    m_for_alias(false),
    m_cls_sym(nullptr),
    annotations(std::move(annotations)),
    tok_cls(std::move(tok_cls)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_cls, lex::SppTokenType::KW_CLS, "cls");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->impl);
}


spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;


auto spp::asts::ClassPrototypeAst::pos_start() const -> std::size_t {
    return tok_cls->pos_start();
}


auto spp::asts::ClassPrototypeAst::pos_end() const -> std::size_t {
    return name->pos_end();
}


auto spp::asts::ClassPrototypeAst::clone() const -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<ClassPrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_cls),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(impl));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->m_for_alias = m_for_alias;
    ast->m_cls_sym = m_cls_sym;
    ast->m_visibility = m_visibility;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->m_ctx = ast; });
    return ast;
}


spp::asts::ClassPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_cls).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group).append(" ");
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::ClassPrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_cls).append(" ");
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group).append(" ");
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::ClassPrototypeAst::m_generate_symbols(
    ScopeManager *sm)
    -> analyse::scopes::TypeSymbol* {
    auto sym_name = ast_clone(name->type_parts()[0]);
    sym_name->generic_arg_group = GenericArgumentGroupAst::from_params(*generic_param_group);

    // Create the symbols as TypeSymbol pointers, so AliasSymbols can also be used.
    std::shared_ptr<analyse::scopes::TypeSymbol> symbol_1 = nullptr;
    std::shared_ptr<analyse::scopes::TypeSymbol> symbol_2 = nullptr;

    // Create the symbol for the type, include generics if applicable, like Vec[T].
    symbol_1 = m_for_alias
                   ? std::make_unique<analyse::scopes::AliasSymbol>(std::move(sym_name), this, sm->current_scope, sm->current_scope, nullptr)
                   : std::make_unique<analyse::scopes::TypeSymbol>(std::move(sym_name), this, sm->current_scope, sm->current_scope);
    sm->current_scope->ty_sym = symbol_1;
    sm->current_scope->parent->add_type_symbol(std::move(symbol_1));
    m_cls_sym = sm->current_scope->ty_sym;

    // If the type was generic, like Vec[T], also create a base Vec symbol.
    if (not generic_param_group->params.empty()) {
        symbol_2 = m_for_alias
                       ? std::make_unique<analyse::scopes::AliasSymbol>(ast_clone(name->type_parts()[0]), this, sm->current_scope, sm->current_scope, nullptr)
                       : std::make_unique<analyse::scopes::TypeSymbol>(ast_clone(name->type_parts()[0]), this, sm->current_scope, sm->current_scope);
        symbol_2->generic_impl = symbol_1.get();
        const auto ret_sym = symbol_2.get();
        sm->current_scope->parent->add_type_symbol(std::move(symbol_2));
        return ret_sym;
    }
    return m_cls_sym.get();
}


auto spp::asts::ClassPrototypeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations and the body.
    Ast::stage_1_pre_process(ctx);
    annotations | genex::views::for_each([this](auto &&x) { x->stage_1_pre_process(this); });
    impl->stage_1_pre_process(this);
}


auto spp::asts::ClassPrototypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Create the class scope, which is the scope for the class prototype.
    sm->create_and_move_into_new_scope(std::dynamic_pointer_cast<TypeIdentifierAst>(name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // Run the generation steps for the annotations.
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Generate the symbols for the class prototype, and handle generic parameters.
    meta->cls_sym = m_generate_symbols(sm);
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    impl->stage_2_gen_top_level_scopes(sm, meta);

    // Move out of the class scope, as the class scope is now complete.
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Skip the class scope.
    sm->move_to_next_scope();
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Qualify the types in the class body.
    sm->move_to_next_scope();
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the class body.
    sm->move_to_next_scope();
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for the class body.
    sm->move_to_next_scope();
    impl->stage_6_pre_analyse_semantics(sm, meta);

    // Check the type isn't recursive.
    if (auto &&recursion = analyse::utils::type_utils::is_type_recursive(*this, *sm)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppRecursiveTypeError>().with_args(
            *this, *recursion).with_scopes({sm->current_scope}).raise();
    }

    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the class body.
    sm->move_to_next_scope();
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
    generic_param_group->stage_7_analyse_semantics(sm, meta);
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check memory for the class body.
    sm->move_to_next_scope();
    impl->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}
