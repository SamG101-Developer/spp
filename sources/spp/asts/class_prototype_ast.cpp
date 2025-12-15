module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.class_prototype_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;
import genex;
import llvm;


spp::asts::ClassPrototypeAst::ClassPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_cls) &&tok_cls,
    decltype(name) name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(impl) &&impl) :
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


auto spp::asts::ClassPrototypeAst::pos_start() const
    -> std::size_t {
    return tok_cls->pos_start();
}


auto spp::asts::ClassPrototypeAst::pos_end() const
    -> std::size_t {
    return name->pos_end();
}


auto spp::asts::ClassPrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<ClassPrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_cls),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(impl));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->m_cls_sym = m_cls_sym;
    ast->visibility = visibility;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->set_ast_ctx(ast); });
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


auto spp::asts::ClassPrototypeAst::print(
    AstPrinter &printer) const
    -> std::string {
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
    symbol_1 = std::make_unique<analyse::scopes::TypeSymbol>(
        std::move(sym_name), this, sm->current_scope, sm->current_scope, sm->current_scope->parent_module());
    sm->current_scope->ty_sym = symbol_1;
    sm->current_scope->parent->add_type_symbol(symbol_1);
    m_cls_sym = sm->current_scope->ty_sym;

    // If the type was generic, like Vec[T], also create a base Vec symbol.
    if (not generic_param_group->params.empty()) {
        symbol_2 = std::make_unique<analyse::scopes::TypeSymbol>(
            ast_clone(name->type_parts()[0]), this, sm->current_scope, sm->current_scope,
            sm->current_scope->parent_module());
        symbol_2->generic_impl = symbol_1.get();
        sm->current_scope->ty_sym = symbol_2;
        const auto ret_sym = symbol_2.get();
        sm->current_scope->parent->add_type_symbol(symbol_2);
        return ret_sym;
    }

    return m_cls_sym.get();
}


auto spp::asts::ClassPrototypeAst::m_fill_llvm_mem_layout(
    analyse::scopes::ScopeManager *sm,
    analyse::scopes::TypeSymbol const *type_sym,
    codegen::LLvmCtx *)
    -> void {
    // Non-struct types are compiler known special types, or $ types - no generation needed.
    if (type_sym->llvm_info->llvm_type == nullptr or not llvm::isa<llvm::StructType>(type_sym->llvm_info->llvm_type)) {
        return;
    }

    auto types = analyse::utils::type_utils::get_all_attrs(*type_sym->fq_name(), sm)
        | genex::views::transform([](auto const &x) { return x.second->get_type_symbol(x.first->type)->llvm_info->llvm_type; })
        | genex::to<std::vector>();

    // If there are any generic types present (llvm_type is nullptr), skip the layout generation.
    if (genex::all_of(types, [](auto const &x) { return x != nullptr; })) {
        const auto struct_type = llvm::dyn_cast<llvm::StructType>(type_sym->llvm_info->llvm_type);
        struct_type->setBody(std::move(types));
    }

    // Pass this layout to aliases too.
    for (auto const &alias : type_sym->aliased_by_symbols) {
        alias->llvm_info->llvm_type = type_sym->llvm_info->llvm_type;
    }
}


auto spp::asts::ClassPrototypeAst::register_generic_substitution(
    analyse::scopes::Scope *scope,
    std::unique_ptr<ClassPrototypeAst> &&new_ast)
    -> void {
    // Just somewhere to store the new_ast as a unique_ptr.
    m_generic_substitutions.emplace_back(scope, std::move(new_ast));
}


auto spp::asts::ClassPrototypeAst::registered_generic_substitutions() const
    -> std::vector<std::pair<analyse::scopes::Scope*, ClassPrototypeAst*>> {
    // Return the generic substituted scopes as raw pointers.
    return m_generic_substitutions
        | genex::views::transform([](auto &&x) { return std::make_pair(x.first, x.second.get()); })
        | genex::to<std::vector>();
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
    CompilerMetaData *meta)
    -> void {
    // Create the class scope, which is the scope for the class prototype.
    auto scope_name = analyse::scopes::ScopeName(std::dynamic_pointer_cast<TypeIdentifierAst>(name));
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
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
    CompilerMetaData *)
    -> void {
    // Skip the class scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the types in the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load the super scopes for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_5_load_super_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_6_pre_analyse_semantics(sm, meta);

    // Check the type isn't recursive.
    if (auto &&recursion = analyse::utils::type_utils::is_type_recursive(*this, *sm)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppRecursiveTypeError>()
            .with_args(*this, *recursion)
            .raises_from(sm->current_scope);
    }

    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_7_analyse_semantics(sm, meta);
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_8_check_memory(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_9_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    const auto cls_sym = sm->current_scope->ty_sym;

    // $ types are pre-set with a packed empty body.
    if (name->type_parts().back()->name[0] == '$') {
        sm->move_out_of_current_scope();
        return nullptr;
    }

    // If this is a raw generic class like Vec[T], then generate the generic implementations.
    if (genex::any_of(sm->current_scope->all_type_symbols(), [](auto const &sym) { return sym->is_generic; })) {
        for (auto &&[generic_scope, generic_ast] : m_generic_substitutions) {
            generic_ast->m_fill_llvm_mem_layout(sm, generic_scope->ty_sym.get(), ctx);
        }
    }

    m_fill_llvm_mem_layout(sm, cls_sym.get(), ctx);

    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::ClassPrototypeAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the class symbol.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_10_code_gen_2(sm, meta, ctx);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::ClassPrototypeAst::get_cls_sym() const
    -> std::shared_ptr<analyse::scopes::TypeSymbol> {
    return m_cls_sym;
}
