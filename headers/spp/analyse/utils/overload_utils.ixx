module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_utils;
import spp.analyse.scopes;
import spp.asts;
import std;

namespace spp::analyse::errors {
    SPP_EXP_CLS struct SemanticError;
}


namespace spp::analyse::utils::overload_utils {
    using OverloadInfo = std::tuple<
        scopes::Scope const*,
        asts::FunctionPrototypeAst*,
        std::unique_ptr<asts::GenericArgumentGroupAst>,
        std::shared_ptr<asts::TypeAst>>;

    using PassOverloadInfo = std::tuple<
        scopes::Scope const*,
        asts::FunctionPrototypeAst*,
        std::unique_ptr<asts::FunctionCallArgumentGroupAst>,
        std::vector<asts::GenericArgumentAst*>>;

    using FailOverloadInfo = std::tuple<
        scopes::Scope const*,
        asts::FunctionPrototypeAst*,
        std::unique_ptr<errors::SemanticError>,
        std::string>;

    SPP_EXP_FUN auto determine_overload(
        asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::pair<PassOverloadInfo, bool>;

    SPP_EXP_FUN auto propagate_method_to_function(
        asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
        asts::TypeAst const &fn_owner_type,
        asts::IdentifierAst const &fn_name,
        asts::PostfixExpressionAst const &cast_lhs,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<PassOverloadInfo, bool, std::unique_ptr<asts::PostfixExpressionAst>>;

    SPP_EXP_FUN auto retrieve_all_overloads(
        asts::IdentifierAst const *fn_name,
        scopes::Scope const &fn_owner_scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<bool, std::unique_ptr<asts::FunctionPrototypeAst>, std::vector<OverloadInfo>>;

    SPP_EXP_FUN auto retrieve_implicit_generic_args_for_call(
        std::shared_ptr<asts::TypeAst> const &fwd_type,
        std::vector<std::unique_ptr<asts::GenericArgumentAst>> &&sup_gn_args,
        asts::meta::CompilerMetaData const *meta)
        -> std::unique_ptr<asts::GenericArgumentGroupAst>;

    SPP_EXP_FUN auto infer_all_generics(
        asts::FunctionPrototypeAst const &fn_proto,
        asts::FunctionParameterGroupAst const &fn_params,
        asts::GenericParameterGroupAst const &gn_params,
        asts::FunctionCallArgumentGroupAst &fn_args,
        asts::GenericArgumentGroupAst &gn_args,
        asts::GenericArgumentGroupAst const &implicit_generic_args,
        bool is_variadic_fn,
        scopes::Scope const *fn_scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto generate_generic_substituted_prototype(
        asts::FunctionPrototypeAst *fn_proto,
        scopes::Scope const *fn_scope,
        asts::GenericArgumentGroupAst &implicit_generic_args,
        asts::GenericArgumentGroupAst &explicit_generic_args,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::pair<asts::FunctionPrototypeAst*, scopes::Scope const*>;

    SPP_EXP_FUN auto validate_args_match_params(
        asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
        asts::FunctionPrototypeAst const &fn_proto,
        scopes::Scope const *fn_scope,
        asts::FunctionCallArgumentGroupAst const &func_args,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto manage_matched_overloads(
        asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
        std::vector<PassOverloadInfo> const &pass_overloads,
        std::vector<FailOverloadInfo> const &fail_overloads,
        asts::FunctionCallArgumentGroupAst const &arg_group,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
