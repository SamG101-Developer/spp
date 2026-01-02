module;
#include <spp/macros.hpp>

export module spp.analyse.utils.func_utils;
import spp.asts.meta.compiler_meta_data;
import ankerl;
import std;


namespace spp::asts {
    SPP_EXP_CLS struct Ast;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
}


namespace spp::analyse::utils::func_utils {
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
    SPP_EXP_FUN auto get_function_owner_type_and_function_name(
        asts::ExpressionAst const &lhs,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<std::shared_ptr<asts::TypeAst>, scopes::Scope const*, std::shared_ptr<asts::IdentifierAst>>;

    SPP_EXP_FUN auto convert_method_to_function_form(
        asts::TypeAst const &function_owner_type,
        asts::IdentifierAst const &function_name,
        asts::PostfixExpressionAst const &lhs,
        asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> std::pair<std::unique_ptr<asts::PostfixExpressionAst>, std::unique_ptr<asts::PostfixExpressionOperatorFunctionCallAst>>;

    SPP_EXP_FUN auto get_all_function_scopes(
        asts::IdentifierAst const &target_fn_name,
        scopes::Scope const *target_scope,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> std::vector<std::tuple<scopes::Scope const*, asts::FunctionPrototypeAst*, std::unique_ptr<asts::GenericArgumentGroupAst>, std::shared_ptr<asts::TypeAst>>>;

    SPP_EXP_FUN auto check_for_conflicting_overload(
        scopes::Scope const &this_scope,
        scopes::Scope const *target_scope,
        asts::FunctionPrototypeAst const &new_fn,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> asts::FunctionPrototypeAst*;

    SPP_EXP_FUN auto check_for_conflicting_override(
        scopes::Scope const &this_scope,
        scopes::Scope const *target_scope,
        asts::FunctionPrototypeAst const &new_fn,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta,
        scopes::Scope const *exclude_scope = nullptr)
        -> asts::FunctionPrototypeAst*;

    SPP_EXP_FUN auto name_args(
        std::vector<std::unique_ptr<asts::FunctionCallArgumentAst>> &args,
        std::vector<asts::FunctionParameterAst*> params,
        scopes::ScopeManager &sm)
        -> void;

    SPP_EXP_FUN auto name_generic_args(
        std::vector<std::unique_ptr<asts::GenericArgumentAst>> &args,
        std::vector<asts::GenericParameterAst*> params,
        asts::Ast const &owner,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta,
        bool is_tuple_owner = false)
        -> void;

    SPP_EXP_FUN template <typename GenericArgType, typename GenericParamType>
    auto name_generic_args_impl(
        std::vector<std::unique_ptr<GenericArgType>> &args,
        std::vector<GenericParamType*> params,
        asts::Ast const &owner,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto infer_generic_args(
        std::vector<std::unique_ptr<asts::GenericArgumentAst>> &args,
        std::vector<asts::GenericParameterAst*> params,
        std::vector<asts::GenericParameterAst*> opt_params,
        std::vector<asts::GenericArgumentAst*> explicit_args,
        std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>, ankerl::ptr_cmp<std::shared_ptr<asts::IdentifierAst>>> const &infer_source,
        std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>, ankerl::ptr_cmp<std::shared_ptr<asts::IdentifierAst>>> const &infer_target,
        std::shared_ptr<asts::Ast> const &owner,
        scopes::Scope const *owner_scope,
        std::shared_ptr<asts::IdentifierAst> const &variadic_param_identifier,
        bool is_tuple_owner,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto infer_generic_args_impl_type(
        std::vector<std::unique_ptr<asts::GenericArgumentTypeKeywordAst>> &args,
        std::vector<asts::GenericParameterTypeAst*> params,
        std::vector<asts::GenericParameterTypeAst*> opt_params,
        std::vector<asts::GenericArgumentTypeKeywordAst*> explicit_args,
        std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>, ankerl::ptr_cmp<std::shared_ptr<asts::IdentifierAst>>> const &infer_source,
        std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>, ankerl::ptr_cmp<std::shared_ptr<asts::IdentifierAst>>> const &infer_target,
        std::shared_ptr<asts::Ast> const &owner,
        scopes::Scope const *owner_scope,
        std::shared_ptr<asts::IdentifierAst> const &variadic_param_identifier,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto infer_generic_args_impl_comp(
        std::vector<std::unique_ptr<asts::GenericArgumentCompKeywordAst>> &args,
        std::vector<asts::GenericParameterCompAst*> params,
        std::vector<asts::GenericParameterCompAst*> opt_params,
        std::vector<asts::GenericArgumentCompKeywordAst*> explicit_args,
        std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>, ankerl::ptr_cmp<std::shared_ptr<asts::IdentifierAst>>> const &infer_source,
        std::map<std::shared_ptr<asts::IdentifierAst>, std::shared_ptr<asts::TypeAst>, ankerl::ptr_cmp<std::shared_ptr<asts::IdentifierAst>>> const &infer_target,
        std::shared_ptr<asts::Ast> const &owner,
        scopes::Scope const *owner_scope,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto is_target_callable(
        asts::ExpressionAst &expr,
        scopes::ScopeManager &sm,
        asts::meta::CompilerMetaData *meta)
        -> std::shared_ptr<asts::TypeAst>;

    SPP_EXP_FUN auto create_callable_prototype(
        asts::TypeAst const &expr_type)
        -> std::unique_ptr<asts::FunctionPrototypeAst>;
}
