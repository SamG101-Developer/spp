#include <vector>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/coroutine_prototype_ast.hpp>
#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/type_ast.hpp>

#include <genex/algorithms/none_of.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/map.hpp>
#include <genex/views/to.hpp>


auto spp::asts::CoroutinePrototypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Perform default function prototype semantic analysis
    FunctionPrototypeAst::stage_7_analyse_semantics(sm, meta);
    const auto ret_type_sym = sm->current_scope->get_type_symbol(*return_type);

    // Update the meta information for enclosing function information.
    meta->save();
    meta->enclosing_function_flavour = this->tok_fun.get();
    meta->enclosing_function_ret_type.emplace_back(ret_type_sym->fq_name());
    meta->enclosing_function_scope = sm->current_scope;

    // Check the return type superimposes the generator type.
    auto temp = std::vector<std::shared_ptr<TypeAst>>();
    temp.emplace_back(ret_type_sym->fq_name()->without_generics());
    auto superimposed_types = ret_type_sym->scope->sup_types()
        | genex::views::concat(std::move(temp))
        | genex::views::map([](auto &&x) { return x->without_generics(); })
        | genex::views::to<std::vector>();

    if (genex::algorithms::none_of(superimposed_types, [sm](auto &&x) { return analyse::utils::type_utils::is_type_generator(*x->without_generics(), *sm->current_scope); })) {
        analyse::errors::SppCoroutineInvalidReturnTypeError(*this, *return_type)
            .scopes({sm->current_scope})
            .raise();
    }

    // Analyse the semantics of the function body, and move out the scope.
    impl->stage_7_analyse_semantics(sm, meta);
    sm->move_out_of_current_scope();
}
