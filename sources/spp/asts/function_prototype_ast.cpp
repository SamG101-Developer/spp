module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_prototype_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.func_utils;
import spp.asts.annotation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.class_implementation_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_implementation_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_prototype_ast;
import spp.asts.object_initializer_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.mixins.compiler_stages;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_mangle;
import spp.lex.tokens;
import genex;


spp::asts::FunctionPrototypeAst::FunctionPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_fun) &&tok_fun,
    decltype(name) &&name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(param_group) &&param_group,
    decltype(tok_arrow) &&tok_arrow,
    decltype(return_type) &&return_type,
    decltype(impl) &&impl) :
    abstract_annotation(nullptr),
    virtual_annotation(nullptr),
    temperature_annotation(nullptr),
    no_impl_annotation(nullptr), inline_annotation(nullptr),
    llvm_func(nullptr),
    annotations(std::move(annotations)),
    tok_fun(std::move(tok_fun)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    param_group(std::move(param_group)),
    tok_arrow(std::move(tok_arrow)),
    return_type(std::move(return_type)),
    impl(std::move(impl)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_fun, lex::SppTokenType::KW_FUN, "fun");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_arrow, lex::SppTokenType::TK_ARROW_RIGHT, "->");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->impl);
}


spp::asts::FunctionPrototypeAst::~FunctionPrototypeAst() = default;


auto spp::asts::FunctionPrototypeAst::pos_start() const
    -> std::size_t {
    return tok_fun->pos_start();
}


auto spp::asts::FunctionPrototypeAst::pos_end() const
    -> std::size_t {
    return return_type->pos_end();
}


auto spp::asts::FunctionPrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    throw std::runtime_error("Use SubroutinePrototypeAst or CoroutinePrototypeAst instead");
}


spp::asts::FunctionPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_fun).append(" ");
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(param_group).append(" ");
    SPP_STRING_APPEND(tok_arrow).append(" ");
    SPP_STRING_APPEND(return_type).append(" ");
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::FunctionPrototypeAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_fun);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(param_group).append(" ");
    SPP_PRINT_APPEND(tok_arrow).append(" ");
    SPP_PRINT_APPEND(return_type).append(" ");
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::FunctionPrototypeAst::m_deduce_mock_class_type() const
    -> std::shared_ptr<TypeAst> {
    // Extract the parameter types.
    auto param_types = param_group->params
        | genex::views::transform([](auto &&x) { return x->type; })
        | genex::to<std::vector>();

    // Module level functions, and static methods, are always FunRef.
    if (m_ctx->to<ModulePrototypeAst>() == nullptr or param_group->get_self_param() == nullptr) {
        return generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    // Class methods with "self" are the FunMov type.
    if (param_group->get_self_param()->conv == nullptr) {
        return generate::common_types::fun_mov_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    // Class methods with "&mut self" are the FunMut type.
    if (*param_group->get_self_param()->conv == ConventionTag::MUT) {
        return generate::common_types::fun_mut_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    // Class methods with "&self" are the FunRef type.
    if (*param_group->get_self_param()->conv == ConventionTag::REF) {
        return generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    std::unreachable();
}


auto spp::asts::FunctionPrototypeAst::m_generate_llvm_declaration(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Function* {
    // Generate the return and parameter types.
    const auto llvm_ret_type = sm->current_scope->get_type_symbol(return_type)->llvm_info->llvm_type;
    const auto llvm_param_types = param_group->params
        | genex::views::transform([&sm](auto const &x) { return sm->current_scope->get_type_symbol(x->type)->llvm_info->llvm_type; })
        | genex::to<std::vector>();

    if (llvm_ret_type != nullptr and genex::all_of(llvm_param_types, [](auto const &x) { return x != nullptr; })) {
        // Create the LLVM function type.
        const auto is_var_arg = param_group->get_variadic_param() != nullptr;
        const auto llvm_fun_type = llvm::FunctionType::get(llvm_ret_type, llvm_param_types, is_var_arg);

        // Create the LLVM function and add it to the context.
        const auto created_llvm_func = llvm::Function::Create(
            llvm_fun_type, llvm::Function::ExternalLinkage, codegen::mangle::mangle_fun_name(*sm->current_scope, *this),
            ctx->module.get());
        llvm_func = created_llvm_func;

        // Generate the parameters as variables.
        param_group->stage_10_code_gen_2(sm, meta, ctx);
    }
    return llvm_func;

    // Name the parameters from the ASTs.
    // auto llvm_param_iter = created_llvm_func->arg_begin();
    // for (const auto &param : param_group->params) {
    //     llvm_param_iter->setName(param->extract_name()->val);
    //     ++llvm_param_iter;
    // }
}


auto spp::asts::FunctionPrototypeAst::print_signature(
    std::string const &owner) const
    -> std::string {
    SPP_STRING_START;
    raw_string += owner;
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(param_group).append(" ");
    SPP_STRING_APPEND(tok_arrow).append(" ");
    SPP_STRING_APPEND(return_type);
    SPP_STRING_END;
}


auto spp::asts::FunctionPrototypeAst::register_generic_substitution(
    std::unique_ptr<analyse::scopes::Scope> &&scope,
    std::unique_ptr<FunctionPrototypeAst> &&new_ast)
    -> void {
    // Store the scope for object persistence (and codegen).
    m_generic_substitutions.emplace_back(std::move(scope), std::move(new_ast));
}


auto spp::asts::FunctionPrototypeAst::registered_generic_substitutions() const
    -> std::vector<std::pair<analyse::scopes::Scope*, FunctionPrototypeAst*>> {
    return m_generic_substitutions
        | genex::views::transform([](auto const &x) { return std::make_pair(x.first.get(), x.second.get()); })
        | genex::to<std::vector>();
}


auto spp::asts::FunctionPrototypeAst::registered_generic_substitutions()
    -> std::vector<std::pair<std::unique_ptr<analyse::scopes::Scope>, std::unique_ptr<FunctionPrototypeAst>>>& {
    return m_generic_substitutions;
}


auto spp::asts::FunctionPrototypeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Get the name of either the module, sup, or sup-ext context name.
    Ast::stage_1_pre_process(ctx);

    // Substitute the "Self" parameter's type with the name of the method.
    if (ctx->to<ModulePrototypeAst>() == nullptr and param_group->get_self_param() != nullptr) {
        const auto self_gen_sub = std::make_unique<GenericArgumentTypeKeywordAst>(
            generate::common_types::self_type(pos_start()), nullptr, ast_name(ctx));
        auto gen_sub = std::vector<GenericArgumentAst*>(1);
        gen_sub[0] = self_gen_sub.get();

        for (auto const &x : param_group->params) { x->type = x->type->substitute_generics(gen_sub); }
        return_type = return_type->substitute_generics(gen_sub);
    }

    // Convert the "fun" function to a "sup" superimposition of a "Fun[Mov|Mut|Ref]" type over a mock type.
    auto mock_class_name = TypeIdentifierAst::from_identifier(*name->to_function_identifier());
    auto function_type = m_deduce_mock_class_type();
    auto function_call = std::make_unique<IdentifierAst>(name->pos_start(), "call");

    // If this is the first overload being converted, then the class needs to be made for the mock type.
    const auto needs_generation = (ast_body(ctx)
        | genex::views::cast_dynamic<ClassPrototypeAst*>()
        | genex::views::filter([&mock_class_name](auto &&x) { return x->name->without_generics() == mock_class_name->without_generics(); })
        | genex::to<std::vector>()).empty();

    if (needs_generation) {
        auto mock_class_ast = std::make_unique<ClassPrototypeAst>(SPP_NO_ANNOTATIONS, nullptr, ast_clone(mock_class_name), nullptr, nullptr);
        auto mock_constant_value = std::make_unique<ObjectInitializerAst>(ast_clone(mock_class_name), nullptr);
        auto mock_constant_ast = std::make_unique<CmpStatementAst>(SPP_NO_ANNOTATIONS, nullptr, ast_clone(name), nullptr, ast_clone(mock_class_name), nullptr, std::move(mock_constant_value));

        if (const auto mod_ctx = ctx->to<ModulePrototypeAst>(); mod_ctx != nullptr) {
            mod_ctx->impl->members.emplace_back(std::move(mock_class_ast));
            mod_ctx->impl->members.emplace_back(std::move(mock_constant_ast));
        }
        else if (const auto sup_ctx = ctx->to<SupPrototypeFunctionsAst>(); sup_ctx != nullptr) {
            sup_ctx->impl->members.emplace_back(std::move(mock_class_ast));
            sup_ctx->impl->members.emplace_back(std::move(mock_constant_ast));
        }
        else if (const auto ext_ctx = ctx->to<SupPrototypeExtensionAst>(); ext_ctx != nullptr) {
            ext_ctx->impl->members.emplace_back(std::move(mock_class_ast));
            ext_ctx->impl->members.emplace_back(std::move(mock_constant_ast));
        }
    }

    // Superimpose the function type over the mock class.
    orig_name = ast_clone(name);
    auto sup_ext_impl_members = std::vector<std::unique_ptr<SupMemberAst>>();
    auto clone = ast_clone(this);
    for (auto const &x : clone->annotations) { x->stage_1_pre_process(clone.get()); }
    sup_ext_impl_members.emplace_back(std::move(clone));
    auto mock_sup_ext_impl = std::make_unique<SupImplementationAst>(nullptr, std::move(sup_ext_impl_members), nullptr);
    auto mock_sup_ext = std::make_unique<SupPrototypeExtensionAst>(
        nullptr, generic_param_group->opt_to_req(), std::move(mock_class_name), nullptr, std::move(function_type),
        std::move(mock_sup_ext_impl));
    mock_sup_ext->set_ast_ctx(m_ctx);

    // Manipulate the context body with the new mock superimposition extension.
    if (const auto mod_ctx = ctx->to<ModulePrototypeAst>()) {
        mod_ctx->impl->members.insert(mod_ctx->impl->members.begin(), std::move(mock_sup_ext));
        mod_ctx->impl->members |= genex::actions::remove_if([this](auto &&x) { return x.get() == this; });
    }
    else if (const auto sup_ctx = ctx->to<SupPrototypeFunctionsAst>()) {
        sup_ctx->impl->members.insert(sup_ctx->impl->members.begin(), std::move(mock_sup_ext));
        sup_ctx->impl->members |= genex::actions::remove_if([this](auto &&x) { return x.get() == this; });
    }
    else if (const auto ext_ctx = ctx->to<SupPrototypeExtensionAst>()) {
        ext_ctx->impl->members.insert(ext_ctx->impl->members.begin(), std::move(mock_sup_ext));
        ext_ctx->impl->members |= genex::actions::remove_if([this](auto &&x) { return x.get() == this; });
    }
}


auto spp::asts::FunctionPrototypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Create a new scope for the function prototype, and move into it.
    auto scope_name = analyse::scopes::ScopeBlockName("<function#" + orig_name->val + "#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // If there is a self parameter in a free function, throw as error.
    if (m_ctx->to<ModulePrototypeAst>() and param_group->get_self_param()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSelfParamInFreeFunctionError>()
            .with_args(*this, *param_group->get_self_param())
            .raises_from(sm->current_scope);
    }

    // Run steps for the annotations.
    for (auto &&x : annotations) {
        x->stage_2_gen_top_level_scopes(sm, meta);
    }

    // Ensure the function's return type does not have a convention.
    if (const auto conv = return_type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>()
            .with_args(*return_type, *conv, "function return type")
            .raises_from(sm->current_scope);
    }

    // Generate the generic parameters and attributes of the function.
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Skip the function scope, as it is already generated.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the function scope, as it is already qualified.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the parameter and return types before sup scopes are attached.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    param_group->stage_7_analyse_semantics(sm, meta);
    return_type->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Perform conflict checking before standard semantic analysis errors due to multiple possible prototypes.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    const auto mod_ctx = m_ctx->to<ModulePrototypeAst>();
    const auto type_scope = mod_ctx
                                ? sm->current_scope->parent_module()
                                : m_ctx->get_ast_scope()->get_type_symbol(ast_name(m_ctx))->scope;

    // Error if there are conflicts.
    if (const auto conflict = analyse::utils::func_utils::check_for_conflicting_overload(*sm->current_scope, type_scope, *this)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionPrototypeConflictError>()
            .with_args(*this, *conflict)
            .raises_from(sm->current_scope);
    }

    // Move out of the function scope, as it is now complete.
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the function scope, as it is now ready for semantic analysis.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Repeated convention check for generic substitutions.
    if (const auto conv = return_type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>()
            .with_args(*return_type, *conv, "function return type")
            .raises_from(sm->current_scope);
    }

    // Analyse the generic parameter group, and the parameter group.
    generic_param_group->stage_7_analyse_semantics(sm, meta);
    param_group->stage_7_analyse_semantics(sm, meta);

    // There is no scope exit, as subclasses will call this method, and finish the analysis themselves.
}


auto spp::asts::FunctionPrototypeAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Move into the function scope, as it is now ready for memory checking.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Check the memory for the parameter group and implementation.
    param_group->stage_8_check_memory(sm, meta);
    impl->stage_8_check_memory(sm, meta);

    // Move out of the function scope, as it is now complete.
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_9_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Create the declaration, but not the definition, of the function. This allows for order-agnostic behaviour.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Handle the main function prototype.
    m_generate_llvm_declaration(sm, meta, ctx);

    // Handle generic substitutions of a function.
    for (auto &&[generic_scope, generic_ast] : m_generic_substitutions) {
        auto tm = ScopeManager(sm->global_scope, generic_scope.get());
        generic_ast->m_generate_llvm_declaration(&tm, meta, ctx);
    }

    // Manual scope skipping.
    const auto final_scope = sm->current_scope->final_child_scope();
    while (sm->current_scope != final_scope) {
        sm->move_to_next_scope(false);
    }

    return nullptr;
}


auto spp::asts::FunctionPrototypeAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Build the function body.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // If there is an implementation, generate its code.
    const auto is_extern = no_impl_annotation || abstract_annotation;
    if (no_impl_annotation and no_impl_annotation->name->val == "compiler_builtin") {
        impl->stage_10_code_gen_2(sm, meta, ctx); // scope skipping
        // auto manual_ir =
    }
    else if (not is_extern) {
        // Add the entry block to the function.
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", llvm_func);
        ctx->builder.SetInsertPoint(entry_bb);
        impl->stage_10_code_gen_2(sm, meta, ctx);
    }
    else {
        impl->stage_10_code_gen_2(sm, meta, ctx);
    }

    sm->move_out_of_current_scope();
    return llvm_func;
}
