#pragma once

#include <map>

#include <spp/asts/_fwd.hpp>
#include <spp/macros.hpp>


namespace spp::analyse::scopes {
    class Scope;
    class ScopeManager;
    struct Symbol;
    struct TypeSymbol;
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
     * The type checking of generic types, such as @code Vec[Str]@endcode vs @code Vec[Str]@endcode, require the generic
     * arguments to be symbolically equal too. A recursive algorithm is used to inspect arguments. However, non-type
     * generic arguments (comp generic arguments) can't be "type-compared", so their values are compared instead. See
     * the other overload of this function.
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

    auto symbolic_eq(
        asts::ExpressionAst const &lhs_expr,
        asts::ExpressionAst const &rhs_expr,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope)
        -> bool;

    auto relaxed_symbolic_eq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type,
        scopes::Scope const *lhs_scope,
        scopes::Scope const *rhs_scope,
        std::map<std::shared_ptr<asts::TypeAst>, asts::ExpressionAst const*> &generic_args)
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

    auto is_type_boolean(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_void(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_generator(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_try(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    auto is_type_function(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_NO_ASAN auto is_type_recursive(
        asts::ClassPrototypeAst const &type,
        scopes::ScopeManager const &sm)
        -> std::shared_ptr<asts::TypeAst>;

    SPP_NO_ASAN auto get_attr_types(
        const asts::ClassPrototypeAst *cls_proto,
        const scopes::Scope *cls_scope)
        -> std::generator<std::pair<const asts::ClassPrototypeAst*, std::shared_ptr<asts::TypeAst>>>;

    SPP_NO_ASAN auto is_index_within_type_bound(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &sm)
        -> bool;

    auto get_nth_type_of_indexable_type(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &sm)
        -> std::shared_ptr<asts::TypeAst>;

    auto get_generator_and_yield_type(
        asts::TypeAst const &type,
        scopes::ScopeManager const &sm,
        asts::ExpressionAst const &expr,
        std::string_view what)
        -> std::tuple<std::shared_ptr<const asts::TypeAst>, std::shared_ptr<asts::TypeAst>, bool, bool, bool, std::shared_ptr<asts::TypeAst>>;

    auto get_try_type(
        asts::TypeAst const &type,
        asts::ExpressionAst const &expr,
        scopes::ScopeManager const &sm)
        -> std::shared_ptr<const asts::TypeAst>;

    template <typename T>
    auto validate_inconsistent_types(
        std::vector<T> const &branches,
        scopes::ScopeManager *sm,
        asts::mixins::CompilerMetaData *meta)
        -> std::tuple<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>>;

    auto get_all_attrs(
        asts::TypeAst const &type,
        scopes::ScopeManager const *sm)
        -> std::vector<std::pair<asts::ClassAttributeAst*, scopes::Scope*>>;

    auto create_generic_cls_scope(
        asts::TypeIdentifierAst &type_part,
        scopes::TypeSymbol const &old_cls_sym,
        std::vector<scopes::Symbol*> external_generic_syms,
        bool is_tuple,
        scopes::ScopeManager *sm,
        asts::mixins::CompilerMetaData *meta)
        -> scopes::Scope*;

    auto create_generic_fun_scope(
        scopes::Scope const &old_fun_scope,
        asts::GenericArgumentGroupAst const &generic_args,
        std::vector<scopes::Symbol*> external_generic_syms,
        scopes::ScopeManager *sm,
        asts::mixins::CompilerMetaData *meta)
        -> scopes::Scope*;

    auto create_generic_sup_scope(
        scopes::Scope &old_sup_scope,
        scopes::Scope &new_cls_scope,
        asts::GenericArgumentGroupAst const &generic_args,
        std::vector<scopes::Symbol*> external_generic_syms,
        scopes::ScopeManager *sm,
        asts::mixins::CompilerMetaData *meta)
        -> std::tuple<scopes::Scope*, scopes::Scope*>;

    auto create_generic_sym(
        asts::GenericArgumentAst const &generic,
        scopes::ScopeManager &sm,
        asts::mixins::CompilerMetaData *meta,
        scopes::ScopeManager *tm = nullptr)
        -> std::shared_ptr<scopes::Symbol>;

    auto get_type_part_symbol_with_error(
        scopes::Scope const &scope,
        asts::TypeIdentifierAst const &type_part,
        scopes::ScopeManager const &sm,
        asts::mixins::CompilerMetaData *meta,
        bool ignore_alias = false)
        -> scopes::TypeSymbol*;

    auto get_namespaced_scope_with_error(
        scopes::ScopeManager const &sm,
        asts::IdentifierAst const &ns)
        -> scopes::Scope*;

    auto deduplicate_variant_inner_types(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> std::vector<std::shared_ptr<asts::TypeAst>>;
}
