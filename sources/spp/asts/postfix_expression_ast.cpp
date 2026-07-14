module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.mem_utils;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionAst::PostfixExpressionAst(
    decltype(Lhs) &&lhs,
    decltype(Op) &&op) :
    Lhs(std::move(lhs)),
    Op(std::move(op)) {
    //
    Source.CachedInference = nullptr;
}

spp::asts::PostfixExpressionAst::~PostfixExpressionAst() = default;

auto spp::asts::PostfixExpressionAst::PosStart() const
    -> std::size_t {
    // Use the lhs.
    return Lhs->PosStart();
}

auto spp::asts::PostfixExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the operator.
    return Op->PosEnd();
}

auto spp::asts::PostfixExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PostfixExpressionAst>(
        AstClone(Lhs), AstClone(Op));
}

auto spp::asts::PostfixExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Lhs);
    SPP_STRING_APPEND(Op);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::expr_utils::PrimaryExpressionOptions;
    using analyse::utils::type_utils::ResolveAndSubstituteSelfType;
    using analyse::errors::SppInvalidPrimaryExpressionError;

    // The "ast_clone" is required because the "lhs" could be a uniquely owned TypeAst, which must have access to
    // "shared_from_this" (on a shared pointer, which "ast_clone" provides).
    meta->Save();
    meta->ReturnTypeOverloadResolverType = nullptr;
    meta->PreventAutoGeneratorResume = false;
    if (Lhs->To<TypeAst>() != nullptr) {
        auto temp_lhs = Unique<TypeAst>(Lhs.Release()->To<TypeAst>());
        temp_lhs->Stage7_AnalyseSemantics(sm, meta);
        temp_lhs = ResolveAndSubstituteSelfType(*temp_lhs, *sm->CurrentScope, *sm, *meta);
        temp_lhs = AstClone(sm->CurrentScope->GetTypeSymbol(temp_lhs.Get())->FqName());
        Lhs = AstClone(temp_lhs); // Todo: std::move here once shared pointers are removed
    }
    else {
        Lhs->Stage7_AnalyseSemantics(sm, meta);
        RaiseIf<SppInvalidPrimaryExpressionError>(
            not IsPrimaryExprTypeValid(*Lhs, *sm, {.AllowTypeAst = true}),
            {sm->CurrentScope}, ERR_ARGS(*Lhs.Get()));
    }
    meta->Restore();

    // Re-attach the meta info, as it is targeting the lhs.
    meta->Save();
    meta->PostfixExpressionLhs = Lhs.Get();
    Op->Stage7_AnalyseSemantics(sm, meta);
    meta->Restore();
}

auto spp::asts::PostfixExpressionAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;

    // Memory analysis used the transformed AST to not repeat lhs as self.
    const auto func = Op->To<PostfixExpressionOperatorFunctionCallAst>();
    if (func != nullptr and func->GetTransformedAst() != nullptr) {
        func->GetTransformedAst()->Stage8_CheckMemory(sm, meta);
        return;
    }

    // Check the memory of the lhs.
    Lhs->Stage8_CheckMemory(sm, meta);
    meta->Save();
    meta->PostfixExpressionLhs = Lhs.Get();
    if (Lhs->To<IdentifierAst>() != nullptr) {
        ValidateSymbolMemory(*meta->PostfixExpressionLhs, *Op, *sm, true, false, false, false, false, meta);
    }
    Op->Stage8_CheckMemory(sm, meta);
    meta->Restore();
}

auto spp::asts::PostfixExpressionAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward into the operator AST.
    meta->Save();
    meta->PostfixExpressionLhs = Lhs.Get();
    Op->Stage9_CompTimeResolve(sm, meta);
    meta->Restore();
}

auto spp::asts::PostfixExpressionAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Memory analysis used the transformed AST to not repeat lhs as self.
    const auto func = Op->To<PostfixExpressionOperatorFunctionCallAst>();
    if (func != nullptr and func->GetTransformedAst() != nullptr) {
        const auto ret_val = func->GetTransformedAst()->Stage11_CodeGen(sm, meta, ctx);
        return ret_val;
    }

    // Forward into the operator AST.
    meta->Save();
    meta->PostfixExpressionLhs = Lhs.Get();
    const auto ret_val = Op->Stage11_CodeGen(sm, meta, ctx);
    meta->Restore();
    return ret_val;
}

auto spp::asts::PostfixExpressionAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Forward into the operator AST.
    meta->Save();
    meta->PostfixExpressionLhs = Lhs.Get();
    const auto inferred = Op->InferType(sm, meta);
    meta->Restore();

    CACHE_TYPE_INFERENCE_AND_RETURN(AstClone(inferred));
}

auto spp::asts::PostfixExpressionAst::ExprParts() const
    -> Vec<Ast*> {
    // Recursively search the lhs, and add the rhs if it exists.
    auto lhs_parts = Lhs->ExprParts();
    auto rhs_parts = Op->ExprParts();
    if (not rhs_parts.IsEmpty()) {
        lhs_parts.AppendRange(std::move(rhs_parts));
    }
    return lhs_parts;
}

SPP_MOD_END
