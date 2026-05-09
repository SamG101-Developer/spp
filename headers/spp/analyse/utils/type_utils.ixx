module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import ankerl.unordered_dense;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct CaseExpressionBranchAst;
    SPP_EXP_CLS struct ClassAttributeAst;
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypeStatementAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
    SPP_EXP_CLS struct Symbol;
    SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::analyse::utils::type_utils {
    SPP_EXP_CLS using GenericInferenceMap = ankerl::unordered_dense::map<
        Shared<asts::TypeIdentifierAst>, asts::ExpressionAst*,
        spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>>;

    SPP_EXP_FUN auto ConventionEq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type)
        -> bool;

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
    SPP_EXP_FUN auto TypeEq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope,
        bool check_variant = true)
        -> bool;

    SPP_EXP_FUN auto TypeEq(
        asts::ExpressionAst const &lhs_expr,
        asts::ExpressionAst const &rhs_expr,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope)
        -> bool;

    SPP_EXP_FUN auto RelaxedTypeEq(
        asts::TypeAst const &lhs_type,
        asts::TypeAst const &rhs_type,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope,
        GenericInferenceMap &generic_args,
        bool check_variant = false)
        -> bool;

    SPP_EXP_FUN auto RelaxedTypeEq(
        asts::ExpressionAst const &lhs_expr,
        asts::ExpressionAst const &rhs_expr,
        scopes::Scope const &lhs_scope,
        scopes::Scope const &rhs_scope,
        GenericInferenceMap &generic_args)
        -> bool;

    SPP_EXP_FUN auto IsTypeCompTimeIndexable(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeRuntimeIndexable(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeArr(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeTup(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeVariant(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeBool(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeVoid(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeNever(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeGen(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeTry(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeFunc(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto IsTypeRecursive(
        asts::ClassPrototypeAst const &type,
        scopes::ScopeManager const &sm)
        -> Shared<asts::TypeAst>;

    SPP_EXP_FUN auto IsTypeBorrowed(
        asts::TypeAst const &type,
        scopes::ScopeManager const &sm,
        bool deep = true)
        -> bool;

    SPP_EXP_FUN auto IsTypeCopyable(
        asts::TypeAst const &type,
        scopes::ScopeManager const &sm)
        -> bool;

    SPP_EXP_FUN auto GetAttrTypes(
        const asts::ClassPrototypeAst *cls_proto,
        const scopes::Scope *cls_scope,
        Vec<Pair<Shared<scopes::TypeSymbol>, asts::ClassAttributeAst*>> &attr_symbols)
        -> void;

    SPP_EXP_FUN auto IsIndexWithinBound(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> bool;

    SPP_EXP_FUN auto GetNthTypeOfIndexableType(
        std::size_t index,
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> Shared<asts::TypeAst>;

    SPP_EXP_FUN auto GetGenAndYieldTypes(
        asts::TypeAst const &type,
        scopes::Scope const &scope,
        asts::ExpressionAst const &expr,
        StrView what)
        -> std::tuple<Shared<const asts::TypeAst>, Shared<asts::TypeAst>, bool>;

    SPP_EXP_FUN auto GetTryType(
        asts::TypeAst const &type,
        asts::ExpressionAst const &expr,
        scopes::ScopeManager const &sm)
        -> Shared<const asts::TypeAst>;

    SPP_EXP_FUN auto GetFwdTypes(
        asts::TypeAst const &type,
        scopes::ScopeManager const &sm)
        -> Pair<Shared<const asts::TypeAst>, Shared<const asts::TypeAst>>;

    SPP_EXP_FUN auto ValidateInconsistentTypes(
        Vec<asts::CaseExpressionBranchAst*> const &branches,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<Pair<asts::Ast*, Shared<asts::TypeAst>>, Vec<Pair<asts::Ast*, Shared<asts::TypeAst>>>>;

    SPP_EXP_FUN auto GetAllAttrs(
        asts::TypeAst const &type,
        scopes::ScopeManager const *sm)
        -> Vec<Pair<Shared<asts::IdentifierAst>, Shared<scopes::TypeSymbol>>>;

    SPP_EXP_FUN auto CreateGenericClsScope(
        asts::TypeIdentifierAst &type_part,
        scopes::TypeSymbol const &old_cls_sym,
        SharedVec<scopes::Symbol> const &external_generic_syms,
        bool is_tuple,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> scopes::Scope*;

    SPP_EXP_FUN auto CreateGenericFunScope(
        scopes::Scope const &old_fun_scope,
        asts::GenericArgumentGroupAst const &generic_args,
        SharedVec<scopes::Symbol> const &external_generic_syms,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> scopes::Scope*;

    SPP_EXP_FUN auto CreateGenericSupScope(
        scopes::Scope &old_sup_scope,
        scopes::Scope &new_cls_scope,
        asts::GenericArgumentGroupAst const &generic_args,
        SharedVec<scopes::Symbol> const &external_generic_syms,
        scopes::ScopeManager const *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<scopes::Scope*, scopes::Scope*>;

    SPP_EXP_FUN auto CreateGenericSym(
        asts::GenericArgumentAst const &generic,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta,
        scopes::ScopeManager *tm = nullptr)
        -> Shared<scopes::Symbol>;

    SPP_EXP_FUN auto RegisterGenericSyms(
        SharedVec<scopes::Symbol> const &external_generic_syms,
        UniqueVec<asts::GenericArgumentAst> const &generic_args,
        scopes::Scope *scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto GetTypeSymOrError(
        scopes::Scope const &scope,
        asts::TypeIdentifierAst const &type_part,
        scopes::ScopeManager const &sm,
        asts::meta::CompilerMetaData *meta)
        -> scopes::TypeSymbol*;

    SPP_EXP_FUN auto get_ns_scope_or_error(
        scopes::Scope const &scope,
        asts::IdentifierAst const &ns,
        scopes::ScopeManager const &sm)
        -> scopes::Scope*;

    SPP_EXP_FUN auto EnforceGenericConstraints(
        SharedVec<asts::TypeAst> const &constraints,
        asts::TypeAst const &concrete_type,
        scopes::Scope const &generic_scope,
        scopes::Scope const &concrete_scope)
        -> void;

    SPP_EXP_FUN auto DedupVariableInnerTypes(
        asts::TypeAst const &type,
        scopes::Scope const &scope)
        -> SharedVec<asts::TypeAst>;

    SPP_EXP_FUN auto SubstituteSupScopeName(
        Str const &old_sup_scope_name,
        asts::GenericArgumentGroupAst const &generic_args)
        -> Str;

    SPP_EXP_FUN auto RecursiveAliasSearch(
        asts::TypeStatementAst const &alias_stmt,
        bool from_use_stmt,
        scopes::Scope *tracking_scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<Shared<asts::TypeAst>, Shared<asts::GenericParameterGroupAst>, scopes::Scope*>;

    SPP_EXP_FUN auto GetFieldIndexInType(
        asts::TypeAst const &type_sym,
        asts::IdentifierAst const &field_name,
        scopes::ScopeManager *sm)
        -> std::size_t;
}
