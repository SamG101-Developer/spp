#include <algorithm>
#include <vector>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/func_utils.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/function_parameter_self_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_type_keyword_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/object_initializer_ast.hpp>
#include <spp/asts/object_initializer_argument_group_ast.hpp>
#include <spp/asts/module_prototype_ast.hpp>
#include <spp/asts/module_implementation_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/sup_prototype_functions_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/generate/common_types.hpp>

#include <genex/actions/remove.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/for_each.hpp>


spp::asts::FunctionPrototypeAst::FunctionPrototypeAst(
    decltype(annotations) &&annotations,
    decltype(tok_fun) &&tok_fun,
    decltype(name) &&name,
    decltype(generic_param_group) &&generic_param_group,
    decltype(param_group) &&param_group,
    decltype(tok_arrow) &&tok_arrow,
    decltype(return_type) &&return_type,
    decltype(impl) &&impl) :
    annotations(std::move(annotations)),
    tok_fun(std::move(tok_fun)),
    name(std::move(name)),
    generic_param_group(std::move(generic_param_group)),
    param_group(std::move(param_group)),
    tok_arrow(std::move(tok_arrow)),
    return_type(std::move(return_type)),
    impl(std::move(impl)) {
}


spp::asts::FunctionPrototypeAst::~FunctionPrototypeAst() = default;


auto spp::asts::FunctionPrototypeAst::pos_start() const -> std::size_t {
    return tok_fun->pos_start();
}


auto spp::asts::FunctionPrototypeAst::pos_end() const -> std::size_t {
    return impl->pos_end();
}


auto spp::asts::FunctionPrototypeAst::clone() const -> std::unique_ptr<Ast> {
    auto f = std::make_unique<FunctionPrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_fun),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(param_group),
        ast_clone(tok_arrow),
        ast_clone(return_type),
        ast_clone(impl));
    f->m_ctx = m_ctx;
    f->m_scope = m_scope;
    f->m_orig_name = m_orig_name;
    f->m_abstract_annotation = m_abstract_annotation;
    f->m_virtual_annotation = m_virtual_annotation;
    f->m_temperature_annotation = m_temperature_annotation;
    f->m_no_impl_annotation = m_no_impl_annotation;
    f->m_inline_annotation = m_inline_annotation;
    return f;
}


spp::asts::FunctionPrototypeAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_fun);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(generic_param_group);
    SPP_STRING_APPEND(param_group);
    SPP_STRING_APPEND(tok_arrow);
    SPP_STRING_APPEND(return_type);
    SPP_STRING_APPEND(impl);
    SPP_STRING_END;
}


auto spp::asts::FunctionPrototypeAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_fun);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(generic_param_group);
    SPP_PRINT_APPEND(param_group);
    SPP_PRINT_APPEND(tok_arrow);
    SPP_PRINT_APPEND(return_type);
    SPP_PRINT_APPEND(impl);
    SPP_PRINT_END;
}


auto spp::asts::FunctionPrototypeAst::m_deduce_mock_class_type() const
    -> std::shared_ptr<TypeAst> {
    // Extract the parameter types.
    auto param_types = param_group->params
        | genex::views::transform([](auto &&x) { return x->type; })
        | genex::views::to<std::vector>();

    // Module level functions, and static methods, are always FunRef.
    if (ast_cast<ModulePrototypeAst>(m_ctx) != nullptr or param_group->get_self_param() != nullptr) {
        return generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    // Class methods with "self" are the FunMov type.
    if (param_group->get_self_param()->conv->tag == ConventionAst::ConventionTag::MOV) {
        return generate::common_types::fun_mov_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    // Class methods with "&mut self" are the FunMut type.
    if (param_group->get_self_param()->conv->tag == ConventionAst::ConventionTag::MUT) {
        return generate::common_types::fun_mut_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    // Class methods with "&self" are the FunRef type.
    if (param_group->get_self_param()->conv->tag == ConventionAst::ConventionTag::REF) {
        return generate::common_types::fun_ref_type(pos_start(), generate::common_types::tuple_type(pos_start(), std::move(param_types)), return_type);
    }

    std::unreachable();
}


auto spp::asts::FunctionPrototypeAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Get the name of either the module, sup, or sup-ext context name.
    Ast::stage_1_pre_process(ctx);

    // Substitute the "Self" parameter's type with the name of the method.
    const auto self_gen_sub = std::make_unique<GenericArgumentTypeKeywordAst>(generate::common_types::self_type(pos_start()), nullptr, ast_name(ctx));
    auto gen_sub = std::vector<GenericArgumentAst*>();
    gen_sub.emplace_back(self_gen_sub.get());
    if (ast_cast<ModulePrototypeAst>(ctx) != nullptr and param_group->get_self_param()) {
        param_group->get_self_param()->type = param_group->get_self_param()->type->substitute_generics(gen_sub);
    }
    param_group->params | genex::views::for_each([gen_sub](auto &&x) { x->type = x->type->substitute_generics(gen_sub); });
    return_type = return_type->substitute_generics(gen_sub);

    // Preprocess the annotations.
    annotations | genex::views::for_each([ctx](auto &&x) { x->stage_1_pre_process(ctx); });

    // Convert the "fun" function to a "sup" superimposition of a "Fun[Mov|Mut|Ref]" type over a mock type.
    auto mock_class_name = TypeIdentifierAst::from_identifier(*name->to_function_identifier());
    auto function_type = m_deduce_mock_class_type();
    auto function_call = std::make_unique<IdentifierAst>(name->pos_start(), "call");

    // If this is the first overload being converted, then the class needs to be made for the mock type.
    const auto needs_generation = (ast_body(ctx)
        | genex::views::cast_dynamic<ClassPrototypeAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::filter([&mock_class_name](auto &&x) { return x->name->without_generics() == mock_class_name->without_generics(); })
        | genex::views::to<std::vector>()).empty();
    if (needs_generation) {
        auto mock_class_ast = std::make_unique<ClassPrototypeAst>(SPP_NO_ANNOTATIONS, nullptr, std::move(mock_class_name), nullptr, nullptr);
        auto mock_constant_value = std::make_unique<ObjectInitializerAst>(std::move(mock_class_name), nullptr);
        auto mock_constant_ast = std::make_unique<CmpStatementAst>(SPP_NO_ANNOTATIONS, nullptr, ast_clone(name), nullptr, ast_clone(mock_class_name), nullptr, std::move(mock_constant_value));

        if (const auto mod_ctx = ast_cast<ModulePrototypeAst>(ctx)) {
            mod_ctx->impl->members.emplace_back(std::move(mock_class_ast));
            mod_ctx->impl->members.emplace_back(std::move(mock_constant_ast));
        }
        else if (const auto sup_ctx = ast_cast<SupPrototypeFunctionsAst>(ctx)) {
            sup_ctx->impl->members.emplace_back(std::move(mock_class_ast));
            sup_ctx->impl->members.emplace_back(std::move(mock_constant_ast));
        }
        else if (const auto ext_ctx = ast_cast<SupPrototypeExtensionAst>(ctx)) {
            ext_ctx->impl->members.emplace_back(std::move(mock_class_ast));
            ext_ctx->impl->members.emplace_back(std::move(mock_constant_ast));
        }
    }

    // Superimpose the function type over the mock class.
    auto function_ast = std::vector<std::unique_ptr<SupMemberAst>>();
    m_orig_name = name.get();
    function_ast.emplace_back(std::unique_ptr<FunctionPrototypeAst>(this));
    auto mock_sup_ext_impl = std::make_unique<SupImplementationAst>(nullptr, std::move(function_ast), nullptr);
    auto mock_sup_ext = std::make_unique<SupPrototypeExtensionAst>(nullptr, generic_param_group->opt_to_req(), std::move(mock_class_name), nullptr, std::move(function_type), std::move(mock_sup_ext_impl));
    mock_sup_ext->m_ctx = m_ctx;

    // Manipulate the context body with the new mock superimposition extension.
    if (const auto mod_ctx = ast_cast<ModulePrototypeAst>(ctx)) {
        mod_ctx->impl->members.emplace_back(std::move(mock_sup_ext));
        mod_ctx->impl->members |= genex::actions::remove_if([this](auto &&x) { return static_cast<void*>(x.get()) != static_cast<void*>(this); });
    }
    else if (const auto sup_ctx = ast_cast<SupPrototypeFunctionsAst>(ctx)) {
        sup_ctx->impl->members.emplace_back(std::move(mock_sup_ext));
        sup_ctx->impl->members |= genex::actions::remove_if([this](auto &&x) { return static_cast<void*>(x.get()) != static_cast<void*>(this); });
    }
    else if (const auto ext_ctx = ast_cast<SupPrototypeExtensionAst>(ctx)) {
        ext_ctx->impl->members.emplace_back(std::move(mock_sup_ext));
        ext_ctx->impl->members |= genex::actions::remove_if([this](auto &&x) { return static_cast<void*>(x.get()) != static_cast<void*>(this); });
    }
}


auto spp::asts::FunctionPrototypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Create a new scope for the function prototype, and move into it.
    auto scope_name = analyse::scopes::ScopeBlockName("<function#" + m_orig_name->val + "#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);

    // If there is a self parameter in a free function, throw as error.
    if (ast_cast<ModulePrototypeAst>(m_ctx) and param_group->get_self_param()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSelfParamInFreeFunctionError>().with_args(
            *this, *param_group->get_self_param()).with_scopes({sm->current_scope}).raise();
    }

    // Run steps for the annotations.
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Ensure the function's return type does not have a convention.
    if (const auto conv = return_type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *return_type, *conv, "function return type").with_scopes({sm->current_scope}).raise();
    }

    // Generate the generic parameters and attributes of the function.
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *) -> void {
    // Skip the function scope, as it is already generated.
    sm->move_to_next_scope();
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Skip the function scope, as it is already qualified.
    sm->move_to_next_scope();
    generic_param_group->stage_4_qualify_types(sm, meta);
    impl->stage_4_qualify_types(sm, meta);
}


auto spp::asts::FunctionPrototypeAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Analyse the parameter and return types before sup scopes are attached.
    sm->move_to_next_scope();
    param_group->params | genex::views::for_each([sm, meta](auto &&x) { x->type->stage_7_analyse_semantics(sm, meta); });
    return_type->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *) -> void {
    // Perform conflict checking before standard semantic analysis errors due to multiple possible prototypes.
    sm->move_to_next_scope();
    const auto type_scope = ast_cast<ModulePrototypeAst>(m_ctx)
                                ? sm->current_scope->parent_module()
                                : m_ctx->m_scope->get_type_symbol(*ast_name(m_ctx))->scope;

    // Error if there are conflicts.
    if (const auto conflict = analyse::utils::func_utils::check_for_conflicting_overload(*sm->current_scope, *type_scope, *this)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppFunctionPrototypeConflictError>().with_args(
            *this, *conflict).with_scopes({sm->current_scope}).raise();
    }

    // Move out of the function scope, as it is now complete.
    sm->move_out_of_current_scope();
}


auto spp::asts::FunctionPrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Move into the function scope, as it is now ready for semantic analysis.
    sm->move_to_next_scope();

    // Repeated convention check for generic substitutions.
    if (const auto conv = return_type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *return_type, *conv, "function return type").with_scopes({sm->current_scope}).raise();
    }

    // Analyse the generic parameter group, and the parameter group.
    generic_param_group->stage_7_analyse_semantics(sm, meta);
    param_group->stage_7_analyse_semantics(sm, meta);

    // There is no scope exit, as subclasses will call this method, and finish the analysis themselves.
}


auto spp::asts::FunctionPrototypeAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Move into the function scope, as it is now ready for memory checking.
    sm->move_to_next_scope();

    // Check the memory for the parameter group and implementation.
    param_group->stage_8_check_memory(sm, meta);
    impl->stage_8_check_memory(sm, meta);

    // Move out of the function scope, as it is now complete.
    sm->move_out_of_current_scope();
}
