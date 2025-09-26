#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/coroutine_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/function_parameter_group_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/pch.hpp>

#include <genex/algorithms/none_of.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/to.hpp>


spp::asts::CoroutinePrototypeAst::~CoroutinePrototypeAst() = default;


auto spp::asts::CoroutinePrototypeAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<CoroutinePrototypeAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_fun),
        ast_clone(name),
        ast_clone(generic_param_group),
        ast_clone(param_group),
        ast_clone(tok_arrow),
        ast_clone(return_type),
        ast_clone(impl));
    ast->orig_name = ast_clone(orig_name);
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->m_abstract_annotation = m_abstract_annotation;
    ast->m_virtual_annotation = m_virtual_annotation;
    ast->m_temperature_annotation = m_temperature_annotation;
    ast->m_no_impl_annotation = m_no_impl_annotation;
    ast->m_inline_annotation = m_inline_annotation;
    ast->m_visibility = m_visibility;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto const &a) { a->m_ctx = ast; });
    return ast;
}


auto spp::asts::CoroutinePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::stage_7_analyse_semantics(sm, meta);
    const auto ret_type_sym = sm->current_scope->get_type_symbol(return_type);

    // Update the meta information for enclosing function information.
    meta->save();
    meta->enclosing_function_flavour = this->tok_fun.get();
    meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
    meta->enclosing_function_scope = sm->current_scope;
    impl->stage_7_analyse_semantics(sm, meta);

    // Check the return type superimposes the generator type.
    auto temp = std::vector<std::shared_ptr<TypeAst>>();
    temp.emplace_back(ret_type_sym->fq_name()->without_generics());
    auto superimposed_types = ret_type_sym->scope->sup_types()
        | genex::views::concat(std::move(temp))
        | genex::views::transform([](auto &&x) { return x->without_generics(); })
        | genex::views::to<std::vector>();

    if (genex::algorithms::none_of(superimposed_types, [sm](auto &&x) { return analyse::utils::type_utils::is_type_generator(*x->without_generics(), *sm->current_scope); })) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppCoroutineInvalidReturnTypeError>().with_args(
            *this, *return_type).with_scopes({sm->current_scope}).raise();
    }

    // Analyse the semantics of the function body, and move out the scope.
    meta->restore();
    sm->move_out_of_current_scope();
}
