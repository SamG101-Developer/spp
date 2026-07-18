module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_utils;
import spp.utils.types;
import std;

namespace spp::analyse::errors {
    SPP_EXP_CLS struct SemanticError;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::meta {
    SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::analyse::utils::overload_utils {
    using OverloadInfo = std::tuple<
        scopes::Scope const *,
        asts::FunctionPrototypeAst *,
        Unique<asts::GenericArgumentGroupAst>,
        Shared<asts::TypeAst>>;

    using PassOverloadInfo = std::tuple<
        scopes::Scope const *,
        asts::FunctionPrototypeAst *,
        Unique<asts::FunctionCallArgumentGroupAst>,
        Vec<asts::GenericArgumentAst *>>;

    using FailOverloadInfo = std::tuple<
        scopes::Scope const *,
        asts::FunctionPrototypeAst *,
        Str,
        Str>;

    SPP_EXP_FUN auto DetermineOverload(
        asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> Pair<PassOverloadInfo, bool>;

    SPP_EXP_FUN auto PropagateMethodToFunction(
        asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
        asts::TypeAst const &fn_owner_type,
        asts::IdentifierAst const &fn_name,
        asts::PostfixExpressionAst const &cast_lhs,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<PassOverloadInfo, bool, Unique<asts::PostfixExpressionAst>>;

    SPP_EXP_FUN auto RetrieveAllOverloads(
        asts::IdentifierAst const *fn_name,
        scopes::Scope const &fn_owner_scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<bool, Unique<asts::FunctionPrototypeAst>, Vec<OverloadInfo>>;

    SPP_EXP_FUN auto RetrieveImplicitGenericArgsForCall(
        Shared<asts::TypeAst> const &fwd_type,
        Vec<Unique<asts::GenericArgumentAst>> &&sup_gn_args,
        asts::meta::CompilerMetaData const *meta)
        -> Unique<asts::GenericArgumentGroupAst>;

    SPP_EXP_FUN auto InferAllGenerics(
        asts::FunctionPrototypeAst const &fn_proto,
        asts::FunctionParameterGroupAst const &fn_params,
        asts::GenericParameterGroupAst const &gn_params,
        asts::FunctionCallArgumentGroupAst &fn_args,
        asts::GenericArgumentGroupAst &explicit_gn_args,
        asts::GenericArgumentGroupAst const &implicit_gn_args,
        bool is_variadic_fn,
        scopes::Scope const *fn_scope,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto PotentiallyGenerateGenericSubstitutedPrototype(
        asts::FunctionPrototypeAst *fn_proto,
        scopes::Scope const *fn_scope,
        asts::GenericArgumentGroupAst &implicit_generic_args,
        asts::GenericArgumentGroupAst &explicit_generic_args,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> std::tuple<asts::FunctionPrototypeAst *, scopes::Scope const *>;

    SPP_EXP_FUN auto ValidateArgsMatchParams(
        asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
        asts::FunctionPrototypeAst const &fn_proto,
        scopes::Scope const *fn_scope,
        asts::FunctionCallArgumentGroupAst const &func_args,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;

    SPP_EXP_FUN auto ManageMatchedOverloads(
        asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
        Vec<PassOverloadInfo> const &pass_overloads,
        Vec<FailOverloadInfo> const &fail_overloads,
        asts::FunctionCallArgumentGroupAst const &arg_group,
        scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta)
        -> void;
}
