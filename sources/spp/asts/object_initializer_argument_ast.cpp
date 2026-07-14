module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.object_initializer_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::ObjectInitializerArgumentAst::ObjectInitializerArgumentAst(
    decltype(Name) &&name,
    decltype(Val) &&val) :
    Name(std::move(name)),
    Val(std::move(val)) {
}

spp::asts::ObjectInitializerArgumentAst::~ObjectInitializerArgumentAst() = default;

auto spp::asts::ObjectInitializerArgumentAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

    // Forward analysis into the value expression.
    Val->Stage7_AnalyseSemantics(sm, meta);
    RaiseIf<SppInvalidPrimaryExpressionError>(
        not IsPrimaryExprTypeValid(*Val, *sm),
        {sm->CurrentScope}, ERR_ARGS(*Val));
}

auto spp::asts::ObjectInitializerArgumentAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the memory of the value expression.
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    Val->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Val, *this, *sm, true, true, true, true, true, meta);
}

auto spp::asts::ObjectInitializerArgumentAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Comptime resolve the value expression.
    Val->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::ObjectInitializerArgumentAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Infer the type of the value expression.
    const auto inferred = Val->InferType(sm, meta);
    CACHE_TYPE_INFERENCE_AND_RETURN(AstClone(inferred));
}

SPP_MOD_END
