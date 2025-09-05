#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_argument_comp_ast.hpp>
#include <spp/asts/generic_argument_type_positional_ast.hpp>
#include <spp/asts/generate/common_types_precompiled.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/mixins/compiler_stages.hpp>


#include <genex/actions/concat.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/transform.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/remove_if.hpp>
#include <genex/views/to.hpp>


auto spp::analyse::utils::type_utils::is_type_indexable(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Test for the tuple or array type.
    return
        is_type_tuple(*type.without_generics(), scope) or is_type_array(*type.without_generics(), scope);
}


auto spp::analyse::utils::type_utils::is_type_array(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::array::Arr[T, n]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::ARR, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_tuple(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::tuple::Tup[Ts...]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::TUP, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_variant(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::variant::Variant[Ts...]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::VAR, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_boolean(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::bool::Bool".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::BOOL, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_void(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::void::Void".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::VOID, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_generator(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::generator::Gen[T]/GenOpt[T]/GenRes[T, E]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN_OPT, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::GEN_RES, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_function(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> bool {
    // Check the type against "std::function::FunMov[Ret, Args]/FunMut[Ret, Args]/FunRef[Ret, Args]".
    return
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::FUN_MOV, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::FUN_MUT, scope, scope) or
        symbolic_eq(*type.without_generics(), *asts::generate::common_types_precompiled::FUN_REF, scope, scope);
}


auto spp::analyse::utils::type_utils::is_type_recursive(
    asts::ClassPrototypeAst const &type,
    scopes::ScopeManager const &sm)
    -> std::shared_ptr<asts::TypeAst> {
    // Define the internal function to recursively search for attribute types.
    auto get_attr_types = [](this auto &&self, const asts::ClassPrototypeAst *cls_proto, const scopes::Scope *cls_scope) -> std::generator<std::pair<const asts::ClassPrototypeAst*, std::shared_ptr<asts::TypeAst>>> {
        for (const auto attr : cls_proto->impl->members | genex::views::ptr | genex::views::cast_dynamic<asts::ClassAttributeAst*>()) {
            const auto attr_type_sym = cls_scope->get_type_symbol(*attr->type, true);
            if (attr_type_sym and attr_type_sym->type != nullptr) {
                co_yield std::make_tuple(attr_type_sym->type, attr->type);
                for (auto &&tup : self(attr_type_sym->type, attr_type_sym->scope)) {
                    co_yield std::move(tup);
                }
            }
        }
    };

    // Get the attribute types recursively from the class prototype, and check for a match with the class prototype.
    for (auto [cls_proto, attr_ast] : get_attr_types(&type, sm.current_scope)) {
        if (cls_proto == &type) {
            return attr_ast;
        }
    }

    // No recursive type was found, so return nullptr.
    return nullptr;
}


auto spp::analyse::utils::type_utils::is_index_within_type_bound(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &sm) -> bool {
    // For tuples, count the number of generic arguments.
    if (is_type_tuple(type, sm)) {
        return index < type.type_parts().back()->generic_arg_group->args.size();
    }

    // For arrays, check the size argument.
    if (is_type_array(type, sm)) {
        const auto size_arg = type.type_parts().back()->generic_arg_group->comp_at("n");
        const auto size_arg_cast = asts::ast_cast<asts::IntegerLiteralAst>(size_arg);
        return index < std::stoull(size_arg_cast->val->token_data);
    }

    std::unreachable();
}


auto spp::analyse::utils::type_utils::get_nth_type_of_indexable_type(
    const std::size_t index,
    asts::TypeAst const &type,
    scopes::Scope const &sm)
    -> std::shared_ptr<asts::TypeAst> {
    // For tuples, return the nth generic argument.
    if (is_type_tuple(type, sm)) {
        return type.type_parts().back()->generic_arg_group->get_type_args()[index]->val;
    }

    // For arrays, return the element type.
    if (is_type_array(type, sm)) {
        return type.type_parts().back()->generic_arg_group->get_type_args()[0]->val;
    }

    std::unreachable();
}


template <typename T>
auto spp::analyse::utils::type_utils::validate_inconsistent_types(
    std::vector<T> const &branches,
    scopes::ScopeManager const *sm,
    asts::mixins::CompilerMetaData *meta)
    -> std::tuple<std::pair<asts::Ast*, asts::TypeAst*>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>> {
    // Collect type information for each branch, pairing the branch with its inferred type.
    auto branches_type_info = branches
        | genex::views::transform([sm, meta](auto &&x) { return std::make_pair(x.get(), x->infer_type(sm, meta)); })
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
        analyse::errors::SemanticErrorBuilder<errors::SppTypeMismatchError>().with_args(
            *master_branch->body->final_member(), *master_branch_type, *mismatch_branch->body->final_member(), *mismatch_branch_type).scopes({sm->current_scope}).raise();
    }

    return std::make_tuple(master_branch_type_info, branches_type_info);
}


auto spp::analyse::utils::type_utils::get_all_attrs(
    asts::TypeAst const &type,
    scopes::ScopeManager const *sm)
    -> std::vector<std::pair<asts::ClassAttributeAst*, scopes::Scope*>> {
    // Get the symbol of the class type.
    const auto cls_sym = sm->current_scope->get_type_symbol(type);

    // Get the attribute information from the class type and all super types.
    auto all_scopes = cls_sym->scope->sup_scopes();
    all_scopes |= genex::actions::concat(std::vector{cls_sym->scope});
    auto all_attrs = all_scopes
        | genex::views::filter([](auto &&sup_scope) { return ast_cast<asts::ClassPrototypeAst>(sup_scope->ast) != nullptr; })
        | genex::views::transform([](auto &&sup_scope) {
            return ast_cast<asts::ClassPrototypeAst>(sup_scope->ast)->impl->members
                | genex::views::ptr
                | genex::views::cast_dynamic<asts::ClassAttributeAst*>()
                | genex::views::transform([sup_scope](auto &&x) { return std::make_pair(x, sup_scope); })
                | genex::views::to<std::vector>();
        })
        | genex::views::flatten
        | genex::views::to<std::vector>();

    return all_attrs;
}
