#pragma once
#include <tuple>
#include <map>
#include <spp/asts/_fwd.hpp>


namespace spp::analyse::scopes {
    class Scope;
    class ScopeManager;
}


namespace spp::asts::mixins {
    struct CompilerMetaData;
}


namespace spp::analyse::utils::func_utils {
    /**
     * Get the function owner type, scope and name from an expression AST. This is used to determine information related
     * to getting the overloads of a function. This function owner type is the type of the class the method belongs to
     * if the callable is a method rather than a free-function. The scope is for the function itself, not its owner. The
     * following cases are handled:
     *      - @code object.method()@endcode: runtime access into an instance.
     *      - @code Type::method()@endcode: static access into a type.
     *      - @code namespace::function()@endcode: direct access into a namespaced free function.
     *      - @code function()@endcode: direct free function call.
     *      - @code<anything else>@endcode: closure identifier, or invalid function call.
     * @param sm The scope manager to access function scopes.
     * @param lhs The left-hand-side of the function call (ie remove the @code (...)@endcode part).
     * @param meta Associated metadata.
     * @return A 3-tuple containing:
     *      1. The owner type of the function (method: class, free function: module, closure: nullptr)
     *      2. The function scope (the scope generated when the @c FunctionPrototypeAst was analysed).
     *      3. The function name (the identifier that is used to call the function).
     */
    auto get_function_owner_type_and_function_name(
        asts::ExpressionAst const &lhs,
        scopes::ScopeManager const &sm,
        asts::mixins::CompilerMetaData *meta)
        -> std::tuple<asts::Ast*, scopes::Scope*, asts::IdentifierAst*>;

    auto convert_method_to_function_form(
        asts::Ast const &function_owner_type,
        asts::IdentifierAst const &function_name,
        asts::ExpressionAst const &lhs,
        asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
        scopes::ScopeManager const &sm,
        asts::mixins::CompilerMetaData *meta)
        -> std::pair<asts::PostfixExpressionAst*, asts::PostfixExpressionOperatorFunctionCallAst*>;

    auto get_all_function_scopes(
        asts::IdentifierAst const &target_fn_name,
        scopes::Scope const &target_scope,
        bool for_override = false)
        -> std::vector<std::tuple<scopes::Scope*, asts::FunctionPrototypeAst*, asts::GenericArgumentGroupAst*>>;

    auto check_for_conflicting_overload(
        scopes::Scope const &this_scope,
        scopes::Scope const &target_scope,
        asts::FunctionPrototypeAst const &new_func)
        -> asts::FunctionPrototypeAst*;

    auto check_for_conflicting_override(
        scopes::Scope const &this_scope,
        scopes::Scope const &target_scope,
        asts::FunctionPrototypeAst const &new_func,
        scopes::Scope *exclude_scope = nullptr)
        -> asts::FunctionPrototypeAst*;

    auto name_args(
        std::vector<std::unique_ptr<asts::FunctionCallArgumentAst>> &args,
        std::vector<asts::FunctionParameterAst*> params,
        scopes::ScopeManager const &sm)
        -> void;

    auto name_generic_args(
        std::vector<std::unique_ptr<asts::GenericArgumentAst>> &args,
        std::vector<asts::GenericParameterAst*> params,
        asts::Ast const &owner,
        scopes::ScopeManager const &sm,
        bool is_tuple_owner = false)
        -> void;

    auto infer_generic_args(
        std::vector<asts::GenericParameterAst*> params,
        std::vector<asts::GenericParameterAst*> opt_params,
        std::vector<asts::GenericArgumentAst*> explicit_args,
        std::map<asts::IdentifierAst*, std::shared_ptr<asts::TypeAst>> infer_source,
        std::map<asts::IdentifierAst*, std::shared_ptr<asts::TypeAst>> infer_target,
        asts::Ast const *owner = nullptr,
        asts::IdentifierAst *variadic_param_identifier = nullptr,
        bool is_tuple_owner = false,
        scopes::ScopeManager const &sm,
        asts::mixins::CompilerMetaData *meta = nullptr)
        -> std::vector<asts::GenericArgumentAst*>;

    auto is_target_callable(
        asts::ExpressionAst const &expr,
        scopes::ScopeManager const &sm,
        asts::mixins::CompilerMetaData *meta)
        -> std::unique_ptr<asts::TypeAst>;

    auto create_callable_prototype(
        asts::TypeAst const &expr_type)
        -> std::unique_ptr<asts::FunctionPrototypeAst>;
}
