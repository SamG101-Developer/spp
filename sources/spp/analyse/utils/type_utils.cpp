#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>

#include <genex/generator.hpp>
#include <genex/actions/concat.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flat.hpp>
#include <genex/views/map.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/remove.hpp>
#include <genex/views/to.hpp>


template <typename T>
auto spp::analyse::utils::type_utils::validate_inconsistent_types(
    std::vector<T> const &branches,
    ScopeManager const *sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<std::pair<asts::Ast*, asts::TypeAst*>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>> {
    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::map([sm, meta](auto &&x) { return std::make_pair(x.get(), x->infer_type(sm, meta)); })
        | genex::views::to<std::vector>();

    // Filter the branch types down to variant types for custom analysis.
    auto variant_branches_type_info = branches_type_info
        | genex::views::filter([sm](auto &&x) { return type_utils::is_type_variant(*x.second, *sm->current_scope); })
        | genex::views::to<std::vector>();

    // Set the master branch type to the first branch's type, if it exists. This is the default and may be subsequently changed.
    auto master_branch_type_info = not branches.empty()
                                       ? std::make_pair(branches_type_info[0].first, branches_type_info[0].second.get())
                                       : std::make_pair(nullptr, nullptr);

    // Override the master type if a pre-provided type (for assignment) has been given.
    if (meta->assignment_target_type != nullptr) {
        master_branch_type_info = std::make_pair(nullptr, meta->assignment_target_type.get());
    }

    // Otherwise, if there are variant branches, use the most variant type as the master branch type.
    else if (not variant_branches_type_info.empty()) {
        auto most_inner_types = 0uz;
        for (auto &&[variant_branch, variant_type] : variant_branches_type_info) {
            const auto variant_size = variant_type->type_parts().back()->generic_args->type_at("Variant")->val->type_parts().back()->generic_args->args.size();
            if (variant_size > most_inner_types) {
                master_branch_type_info = std::make_tuple(variant_branch, variant_type.get());
                most_inner_types = variant_size;
            }
        }
    }

    // Remove the master branch pointer from the list of remaining branch types and check all types match.
    auto mismatch_branches_type_info = branches_type_info
        | genex::views::remove_if([master_branch_type_info](auto &&x) { return x.first == master_branch_type_info.first; })
        | genex::views::filter([master_branch_type_info, sm](auto &&x) { return not type_utils::symbolic_eq(*master_branch_type_info.second, *x.second, *sm->current_scope, *sm->current_scope); })
        | genex::views::to<std::vector>();

    if (mismatch_branches_type_info.empty()) {
        auto [mismatch_branch, mismatch_branch_type] = std::move(mismatch_branches_type_info[0]);
        auto [master_branch, master_branch_type] = master_branch_type_info;
        errors::SppTypeMismatchError(*master_branch->body->final_member(), *master_branch_type, *mismatch_branch->body->final_member(), *mismatch_branch_type)
            .scopes({sm->current_scope})
            .raise();
    }

    return std::make_tuple(master_branch_type_info, branches_type_info);
}


auto spp::analyse::utils::type_utils::get_all_attrs(
    asts::TypeAst const &type,
    ScopeManager const *sm)
    -> std::vector<std::pair<asts::ClassAttributeAst*, scopes::Scope*>> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->current_scope->get_type_symbol(type);

    // Get the attribute information from the class type and all super types.
    auto all_scopes = cls_sym->scope->sup_scopes();
    all_scopes |= genex::actions::concat(std::vector{cls_sym->scope});
    auto all_attrs = all_scopes
        | genex::views::filter([](auto &&sup_scope) { return ast_cast<asts::ClassPrototypeAst>(sup_scope->ast) != nullptr; })
        | genex::views::map([](auto &&sup_scope) {
            return ast_cast<asts::ClassPrototypeAst>(sup_scope->ast)->impl->members
                | genex::views::ptr_unique
                | genex::views::cast.operator()<asts::ClassAttributeAst*>()
                | genex::views::map([sup_scope](auto &&x) { return std::make_pair(x, sup_scope); })
                | genex::views::to<std::vector>();
        })
        | genex::views::flat
        | genex::views::to<std::vector>();

    return all_attrs;
}
