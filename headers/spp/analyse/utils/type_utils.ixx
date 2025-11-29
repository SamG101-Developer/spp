module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_utils;
import spp.utils.ptr_cmp;
import spp.asts.meta.compiler_meta_data;

import ankerl;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ClassAttributeAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct Symbol;
}


namespace spp::analyse::utils::type_utils {
    SPP_EXP_CLS using GenericInferenceMap = ankerl::unordered_dense::map<
        std::shared_ptr<asts::TypeIdentifierAst>, asts::ExpressionAst const*,
        spp::utils::PtrHash<std::shared_ptr<asts::TypeIdentifierAst>>,
        spp::utils::PtrEq<std::shared_ptr<asts::TypeIdentifierAst>>>;

    /**
     * The symbolic equality type checker is a complex type checking algorithm that takes namespacing, scopes, aliases,
     * variants, etc, all into account, and returns whether two types are indeed the same. Generic arguments are also
     * taken into account, as-well as the variadic generic variation.
     *
     * Variant type matching allows the rhs to match against the lhs side, when the lhs is a variant, such as
     * @c {Opt[T]}, and the rhs is a inner type to the variant, such as @c {Some[T]}, or @c {None}.
     *
     * The type checking of generic types, such as @c {Vec[Str]} vs @c {Vec[Str]}, require the generic arguments to be
     * symbolically equal too. A recursive algorithm is used to inspect arguments. However, non-type generic arguments
     * (comp generic arguments) can't be "type-compared", so their values are compared instead. See the other overload
     * of this function.
     *
     * @param lhs_type The left hand side type to compare.
     * @param rhs_type The right hand side type to compare.
     * @param lhs_scope The scope to identify the lhs type in. This is used to resolve aliases and namespaces.
     * @param rhs_scope The scope to identify the rhs type in. This is used to resolve aliases and namespaces.
     * @param check_variant Whether to allow "variant matches" for types.
     * @return If the two types are symbolically equal, meaning they are the same type, or one is a variant of the
     * other.
     */
    SPP_EXP_FUN auto symbolic_eq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope,
        bool check_variant = true)
        -> bool;

    SPP_EXP_FUN auto symbolic_eq(
        asts::ExpressionAst const &lhs_expr,
        asts::ExpressionAst const &rhs_expr,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope)
        -> bool;

    SPP_EXP_FUN auto relaxed_symbolic_eq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type,
        scopes::Scope const *lhs_scope,
        scopes::Scope const *rhs_scope,
        GenericInferenceMap &generic_args,
        bool check_variant = false)
        -> bool;

    SPP_EXP_FUN auto relaxed_symbolic_eq(
        asts::ExpressionAst const &lhs_expr,
        asts::ExpressionAst const &rhs_expr,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope,
        GenericInferenceMap &generic_args)
        -> bool;

    SPP_EXP_FUN auto is_type_comptime_indexable(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_array(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_tuple(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_variant(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_boolean(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_void(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_generator(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_runtime_indexable(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_try(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_function(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto is_type_recursive(
        asts::ClassPrototypeAst const &type,
        scopes::ScopeManager const &sm)
        -> std::shared_ptr<asts::TypeAst>;

    SPP_EXP_FUN auto get_attr_types(
        const asts::ClassPrototypeAst *cls_proto,
        const scopes::Scope *cls_scope)
        -> std::generator<std::pair<const asts::ClassPrototypeAst*, std::shared_ptr<asts::TypeAst>>>;

    SPP_EXP_FUN auto is_index_within_type_bound(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &sm)
        -> bool;

    SPP_EXP_FUN auto get_nth_type_of_indexable_type(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &sm)
        -> std::shared_ptr<asts::TypeAst>;

    SPP_EXP_FUN auto get_generator_and_yield_type(
        asts::TypeAst const &type,
        scopes::ScopeManager const &sm,
        asts::ExpressionAst const &expr,
        std::string_view what)
        -> std::tuple<std::shared_ptr<const asts::TypeAst>, std::shared_ptr<asts::TypeAst>, bool, bool, bool, std::shared_ptr<asts::TypeAst>>;

    SPP_EXP_FUN auto get_try_type(
        asts::TypeAst const &type,
        asts::ExpressionAst const &expr,
        scopes::ScopeManager const &sm)
        -> std::shared_ptr<const asts::TypeAst>;

    SPP_EXP_CLS template <typename T>
    auto validate_inconsistent_types(
        std::vector<T> const &branches,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>, std::vector<std::pair<asts::Ast*, std::shared_ptr<asts::TypeAst>>>>;

    SPP_EXP_FUN auto get_all_attrs(
        asts::TypeAst const &type,
        scopes::ScopeManager const *sm)
        -> std::vector<std::pair<asts::ClassAttributeAst*, scopes::Scope*>>;

    SPP_EXP_FUN auto create_generic_cls_scope(
        asts::TypeIdentifierAst &type_part,
        scopes::TypeSymbol const &old_cls_sym,
        std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
        bool is_tuple,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> scopes::Scope*;

    SPP_EXP_FUN auto create_generic_fun_scope(
        scopes::Scope const &old_fun_scope,
        asts::GenericArgumentGroupAst const &generic_args,
        std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> scopes::Scope*;

    SPP_EXP_FUN auto create_generic_sup_scope(
        scopes::Scope &old_sup_scope,
        scopes::Scope &new_cls_scope,
        asts::GenericArgumentGroupAst const &generic_args,
        std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
        scopes::ScopeManager const *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<scopes::Scope*, scopes::Scope*>;

    SPP_EXP_FUN auto create_generic_sym(
        asts::GenericArgumentAst const &generic,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta,
        scopes::ScopeManager *tm = nullptr)
        -> std::shared_ptr<scopes::Symbol>;

    SPP_EXP_FUN auto register_generic_syms(
        std::vector<std::shared_ptr<scopes::Symbol>> const &external_generic_syms,
        std::vector<std::unique_ptr<asts::GenericArgumentAst>> const &generic_args,
        scopes::Scope *scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto get_type_part_symbol_with_error(
        scopes::Scope const &scope,
        asts::TypeIdentifierAst const &type_part,
        scopes::ScopeManager const &sm,
        asts::meta::CompilerMetaData *meta)
        -> scopes::TypeSymbol*;

    SPP_EXP_FUN auto get_namespaced_scope_with_error(
        scopes::ScopeManager const &sm,
        asts::IdentifierAst const &ns)
        -> scopes::Scope*;

    SPP_EXP_FUN auto deduplicate_variant_inner_types(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> std::vector<std::shared_ptr<asts::TypeAst>>;

    SPP_EXP_FUN auto substitute_sup_scope_name(
        std::string const &old_sup_scope_name,
        asts::GenericArgumentGroupAst const &generic_args)
        -> std::string;

    SPP_EXP_FUN auto recursive_alias_search(
        asts::TypeStatementAst const &alias_stmt,
        scopes::Scope *tracking_scope,
        std::shared_ptr<asts::TypeAst> actual_old_type,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<std::shared_ptr<asts::TypeAst>, std::shared_ptr<asts::GenericParameterGroupAst>, scopes::Scope*, scopes::Scope*>;
}
