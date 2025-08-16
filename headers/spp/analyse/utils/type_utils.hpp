#pragma once

#include <spp/asts/_fwd.hpp>

namespace spp::analyse::scopes {
    class Scope;
    class ScopeManager;
}


namespace spp::asts::mixins {
    struct CompilerMetaData;
}


namespace spp::analyse::utils::type_utils {
    /**
     * The symbolic equality type checker is a complex type checking algorithm that takes namespacing, scopes, aliases,
     * variants, etc, all into account, and returns whether two types are indeed the same. Generic arguments are also
     * taken into account, as-well as the variadic generic variation.
     *
     * Variant type matching allows the rhs to match against the lhs side, when the lhs is a variant, such as
     * @code Opt[T]@endcode, and the rhs is a inner type to the variant, such as @code Some[T]@endcode, or
     * @code None@endcode.
     *
     * @param lhs_type The left hand side type to compare.
     * @param rhs_type The right hand side type to compare.
     * @param lhs_scope The scope to identify the lhs type in. This is used to resolve aliases and namespaces.
     * @param rhs_scope The scope to identify the rhs type in. This is used to resolve aliases and namespaces.
     * @param check_variant Whether to allow "variant matches" for types.
     * @param lhs_ignore_alias Whether the lhs symbol retrieval should not move into the alias.
     * @return If the two types are symbolically equal, meaning they are the same type, or one is a variant of the
     * other.
     */
    auto symbolic_eq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope,
        bool check_variant = true,
        bool lhs_ignore_alias = false)
        -> bool;

    auto is_type_indexable(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_array(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_tuple(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_variant(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_generator(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_function(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_recursive(
        asts::ClassPrototypeAst const &type,
        scopes::ScopeManager const &sm)
        -> asts::TypeAst*;

    auto is_index_within_type_bound(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &sm) -> bool;

    auto get_nth_type_of_indexable_type(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &sm) -> bool;

    auto get_generator_and_yield_type(
        asts::TypeAst const &type,
        scopes::ScopeManager const &sm,
        asts::ExpressionAst const &expr,
        std::string_view what)
        -> std::tuple<std::unique_ptr<asts::TypeAst>, std::unique_ptr<asts::TypeAst>, bool, bool, bool, std::unique_ptr<asts::TypeAst>>;

    template <typename T>
    auto validate_inconsistent_types(
        std::vector<T> const &branches,
        scopes::ScopeManager const *sm,
        asts::mixins::CompilerMetaData *meta)
        -> std::tuple<std::pair<asts::Ast*, asts::TypeAst*>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>>;
}
