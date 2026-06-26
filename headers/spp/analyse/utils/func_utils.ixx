module;
#include <spp/macros.hpp>

export module spp.analyse.utils.func_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import ankerl.unordered_dense;
import llvm;
import std;


namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
}


namespace spp::analyse::utils::func_utils {
    using InferenceSourceMap = ankerl::unordered_dense::map<
        Shared<asts::IdentifierAst>,
        Shared<asts::TypeAst>,
        spp::utils::ptr::ptr_hash<Shared<asts::IdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::IdentifierAst>>>;

    using InferenceTargetMap = ankerl::unordered_dense::map<
        Shared<asts::IdentifierAst>,
        Shared<asts::TypeAst>,
        spp::utils::ptr::ptr_hash<Shared<asts::IdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::IdentifierAst>>>;

    using InferenceResultCompMap = ankerl::unordered_dense::map<
        Shared<asts::TypeIdentifierAst>,
        Vec<asts::ExpressionAst*>,
        spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>>;

    using InferenceResultTypeMap = ankerl::unordered_dense::map<
        Shared<asts::TypeIdentifierAst>,
        Vec<Shared<asts::TypeAst>>,
        spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>>;

    SPP_EXP_CLS using InferenceFinalCompMap = ankerl::unordered_dense::map<
        Shared<asts::TypeIdentifierAst>,
        asts::ExpressionAst*,
        spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>>;

    SPP_EXP_CLS using InferenceFinalTypeMap = ankerl::unordered_dense::map<
        Shared<asts::TypeIdentifierAst>,
        Shared<asts::TypeAst>,
        spp::utils::ptr::ptr_hash<Shared<asts::TypeIdentifierAst>>,
        spp::utils::ptr::ptr_eq<Shared<asts::TypeIdentifierAst>>>;

    /**
     * Get the function owner type, scope and name from an expression AST. This is used to determine information related
     * to getting the overloads of a function. This function owner type is the type of the class the method belongs to
     * if the callable is a method rather than a free-function. The scope is for the function itself, not its owner. The
     * following cases are handled:
     *      - @c object.method(): runtime access into an instance.
     *      - @c Type::method(): static access into a type.
     *      - @c namespace::function(): direct access into a namespaced free function.
     *      - @c function(): direct free function call.
     *      - @c<anything else>: closure identifier, or invalid function call.
     * @param sm The scope manager to access function scopes.
     * @param lhs The left-hand-side of the function call (ie remove the @c (...) part).
     * @param meta Associated metadata.
     * @return A 3-tuple containing:
     *      1. The owner type of the function (method: class, free function: module, closure: nullptr)
     *      2. The function scope (the scope generated when the @c FunctionPrototypeAst was analysed).
     *      3. The function name (the identifier that is used to call the function).
     */
    SPP_EXP_FUN auto GetFuncOwnerTypeAndFuncName(
        asts::ExpressionAst const &lhs,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<Shared<asts::TypeAst>, scopes::Scope const*, Shared<asts::IdentifierAst>>;

    SPP_EXP_FUN auto ConvertMethodToFuncForm(
        asts::TypeAst const &function_owner_type,
        asts::IdentifierAst const &function_name,
        asts::PostfixExpressionAst const &lhs,
        asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> Pair<Unique<asts::PostfixExpressionAst>, Unique<asts::PostfixExpressionOperatorFunctionCallAst>>;

    SPP_EXP_FUN auto GetAllFunctionScopes(
        asts::IdentifierAst const &target_fn_name,
        scopes::Scope const *target_scope,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> Vec<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, Unique<asts::GenericArgumentGroupAst>, Shared<asts::TypeAst>>>;

    SPP_EXP_FUN auto CheckForConflictingOverload(
        scopes::Scope const &this_scope,
        scopes::Scope const *target_scope,
        asts::FunctionPrototypeAst const &new_fn,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> asts::FunctionPrototypeAst*;

    SPP_EXP_FUN auto CheckForConflictingOverride(
        scopes::Scope const &this_scope,
        scopes::Scope const *target_scope,
        asts::FunctionPrototypeAst const &new_fn,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta,
        scopes::Scope const *exclude_scope = nullptr)
        -> asts::FunctionPrototypeAst*;

    SPP_EXP_FUN auto EnforceNoInvalidFnArgs(
        Vec<asts::FunctionParameterAst*> const &params,
        Vec<asts::FunctionCallArgumentKeywordAst*> const &named_args,
        scopes::ScopeManager &sm)
        -> void;

    SPP_EXP_FUN template <typename GenericArgType, typename GenericParamType>
    auto EnforceNoInvalidGnArgs(
        Vec<asts::GenericParameterAst*> const &params,
        Vec<asts::GenericArgumentAst*> const &named_args,
        scopes::ScopeManager &sm)
        -> void;

    SPP_EXP_FUN template <typename InferenceResultMap>
    auto EnforceNoConflictingInferredGnArgs(
        InferenceResultMap const &inferred,
        scopes::ScopeManager &sm)
        -> void;

    SPP_EXP_FUN auto EnforceNoUninferredGnArgs(
        Vec<Shared<asts::TypeIdentifierAst>> const &p_names,
        Vec<Shared<asts::TypeIdentifierAst>> const &i_names,
        scopes::Scope const &owner_scope,
        Shared<asts::Ast> const &owner,
        scopes::ScopeManager &sm)
        -> void;

    SPP_EXP_FUN auto EnforceGenericConstraintsAllArgs(
        asts::GenericParameterGroupAst const &p_group,
        asts::GenericArgumentGroupAst const &a_group,
        scopes::Scope const &owner_scope,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData &meta)
        -> void;

    SPP_EXP_FUN auto NameFnArgs(
        asts::FunctionCallArgumentGroupAst &a_group,
        asts::FunctionParameterGroupAst const &p_group,
        scopes::ScopeManager &sm)
        -> void;

    SPP_EXP_FUN auto NameGnArgs(
        asts::GenericArgumentGroupAst &a_group,
        asts::GenericParameterGroupAst const &p_group,
        asts::Ast const &owner,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData &meta,
        bool is_tuple_owner = false)
        -> void;

    SPP_EXP_FUN template <typename GenericArgType, typename GenericParamType, typename GenericParamVariadicType>
    auto NameGnArgsImpl(
        asts::GenericArgumentGroupAst &a_group,
        Vec<asts::GenericParameterAst*> const &params,
        asts::Ast const &owner,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData &meta)
        -> void;

    SPP_EXP_FUN auto InferGnArgs(
        asts::GenericParameterGroupAst const &p_group,
        asts::GenericArgumentGroupAst &a_group,
        InferenceSourceMap infer_source,
        InferenceTargetMap infer_target,
        Shared<asts::Ast> const &owner,
        scopes::Scope const &owner_scope,
        Shared<asts::IdentifierAst> const &variadic_fn_param_name,
        bool is_tuple_owner,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData &meta)
        -> void;

    SPP_EXP_FUN auto IsTargetCallable(
        asts::ExpressionAst &expr,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> Shared<const asts::TypeAst>;

    SPP_EXP_FUN auto CreateCallablePrototype(
        asts::TypeAst const &expr_type)
        -> Unique<asts::FunctionPrototypeAst>;

    SPP_EXP_FUN auto GetOverloadTypes(
        asts::TypeAst const &overload_set_type,
        scopes::Scope const& scope)
        -> Vec<Shared<asts::TypeAst>>;
}
