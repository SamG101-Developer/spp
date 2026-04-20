module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts;
import spp.analyse.errors;
import spp.analyse.scopes;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.scope_utils;
import spp.analyse.utils.type_utils;
import spp.asts.utils;
import spp.codegen.llvm_type;
import spp.lex;
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


spp::asts::ClassPrototypeAst::~ClassPrototypeAst() = default;


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
    for (auto const &a : ast->annotations) {
        a->set_ast_ctx(ast.get());
    }
    return ast;
}


spp::asts::ClassPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations, "\n").append(not annotations.empty() ? "\n" : "");
    SPP_STRING_APPEND(tok_cls).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group).append(" ");
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::ClassPrototypeAst::m_generate_symbols(
    ScopeManager *sm)
    -> AbstractSymbol* {
    auto is_dollar_type = name->to<TypeIdentifierAst>()->name[0] == '$';
    auto sym_name = ast_clone(name->type_parts()[0]);
    sym_name->generic_arg_group = GenericArgumentGroupAst::from_params(*generic_param_group);

    // Create the symbols as TypeSymbol pointers, so AliasSymbols can also be used.
    std::shared_ptr<analyse::scopes::TypeSymbol> symbol_1 = nullptr;
    std::shared_ptr<analyse::scopes::TypeSymbol> symbol_2 = nullptr;

    // Create the symbol for the type, include generics if applicable, like Vec[T].
    symbol_1 = std::make_unique<analyse::scopes::TypeSymbol>(
        std::move(sym_name), this, sm->current_scope, sm->current_scope, sm->current_scope->parent_module(), false,
        is_dollar_type);
    sm->current_scope->ty_sym = symbol_1;
    analyse::utils::scope_utils::add_type_symbol_check_conflict(*sm->current_scope->parent, symbol_1);
    m_cls_sym = sm->current_scope->ty_sym;

    // If the type was generic, like Vec[T], also create a base Vec symbol.
    if (not generic_param_group->params.empty()) {
        symbol_2 = std::make_unique<analyse::scopes::TypeSymbol>(
            ast_clone(name->type_parts()[0]), this, sm->current_scope, sm->current_scope,
            sm->current_scope->parent_module(), false, is_dollar_type);
        symbol_2->generic_impl = symbol_1.get();
        sm->current_scope->ty_sym = symbol_2;
        const auto ret_sym = symbol_2.get();
        analyse::utils::scope_utils::add_type_symbol_check_conflict(*sm->current_scope->parent, symbol_2);
        return ret_sym;
    }

    return m_cls_sym.get();
}


auto spp::asts::ClassPrototypeAst::m_fill_llvm_mem_layout(
    analyse::scopes::ScopeManager *sm,
    AbstractSymbol const *sym,
    codegen::LlvmCtx *ctx) const
    -> void {
    // Todo: error if attribute's default value if a comp generic value?? Also TEST THIS.
    // Todo: move this into llvm modules (doesn't need the class at all -> done from symbol).

    // Non-struct types are compiler known special types, or $ types - no generation needed.
    const auto type_sym = dynamic_cast<analyse::scopes::TypeSymbol const*>(sym);
    if (codegen::llvm_type(*type_sym, ctx) == nullptr or not llvm::isa<llvm::StructType>(codegen::llvm_type(*type_sym, ctx))) {
        return;
    }

    auto types = analyse::utils::type_utils::get_all_attrs(*type_sym->fq_name(), sm)
        | genex::views::transform([&](auto const &pair) { return codegen::llvm_type(*pair.second, ctx); })
        | genex::to<std::vector>();

    // If there are any generic types present (llvm_type is nullptr), skip the layout generation.
    if (genex::all_of(types, [](auto const &x) { return x != nullptr; })) {
        const auto struct_type = llvm::dyn_cast<llvm::StructType>(codegen::llvm_type(*type_sym, ctx));
        struct_type->setBody(std::move(types));
    }

    // Pass this layout to aliases too.
    for (auto const &alias : type_sym->aliased_by_symbols) {
        alias->llvm_info->llvm_type = codegen::llvm_type(*type_sym, ctx);
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


auto spp::asts::ClassPrototypeAst::get_cls_sym() const
    -> std::shared_ptr<AbstractSymbol> {
    return m_cls_sym;
}


auto spp::asts::ClassPrototypeAst::stage_1_pre_process(
    AbstractAst *ctx)
    -> void {
    // Pre-process the AST by calling the base class method and then processing annotations and the body.
    Ast::stage_1_pre_process(ctx);
    for (auto const &a : annotations) {
        a->stage_1_pre_process(this);
    }
    impl->stage_1_pre_process(this);
}


auto spp::asts::ClassPrototypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create the class scope, which is the scope for the class prototype.
    auto scope_name = analyse::scopes::ScopeName(analyse::scopes::ScopeTypeIdentifierName(name));
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // Run the generation steps for the annotations.
    for (auto const &a : annotations) {
        a->stage_2_gen_top_level_scopes(sm, meta);
    }

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
    for (auto const &a : annotations) {
        a->stage_4_qualify_types(sm, meta);
    }
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
    for (auto const &a : annotations) {
        a->stage_5_load_super_scopes(sm, meta);
    }
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
    const auto recursion = analyse::utils::type_utils::is_type_recursive(*this, *sm);
    raise_if<analyse::errors::SppRecursiveTypeError>(
        recursion != nullptr, {sm->current_scope},
        ERR_ARGS(*this, *recursion));

    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    for (auto const &a : annotations) {
        a->stage_7_analyse_semantics(sm, meta);
    }
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


auto spp::asts::ClassPrototypeAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the class body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    for (auto const &a : annotations) {
        a->stage_9_comptime_resolution(sm, meta);
    }
    impl->stage_9_comptime_resolution(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::ClassPrototypeAst::stage_10_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LlvmCtx *ctx)
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
    if (genex::any_of(analyse::utils::scope_utils::all_type_symbols(sm->current_scope), [](auto const &sym) { return sym->is_generic; })) {
        for (auto &&[generic_scope, generic_ast] : m_generic_substitutions) {
            generic_ast->m_fill_llvm_mem_layout(sm, generic_scope->ty_sym.get(), ctx);
        }
    }

    m_fill_llvm_mem_layout(sm, cls_sym.get(), ctx);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::ClassPrototypeAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* {
    // Get the class symbol.
    sm->move_to_next_scope();
    // SPP_ASSERT(sm->current_scope == m_scope);
    impl->stage_11_code_gen_2(sm, meta, ctx);
    sm->move_out_of_current_scope();
    return nullptr;
}
