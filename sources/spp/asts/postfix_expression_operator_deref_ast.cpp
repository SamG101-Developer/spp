module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_deref_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.asts.generate.common_types_precompiled;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorDerefAst::PostfixExpressionOperatorDerefAst(
    decltype(TokDeref) &&tok_deref) :
    TokDeref(std::move(tok_deref)) {
}

spp::asts::PostfixExpressionOperatorDerefAst::~PostfixExpressionOperatorDerefAst() = default;

auto spp::asts::PostfixExpressionOperatorDerefAst::PosStart() const
    -> std::size_t {
    // Use the "@" token.
    return TokDeref->PosStart();
}

auto spp::asts::PostfixExpressionOperatorDerefAst::PosEnd() const
    -> std::size_t {
    // Use the "@" token.
    return TokDeref->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorDerefAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PostfixExpressionOperatorDerefAst>(
        AstClone(TokDeref));
}

auto spp::asts::PostfixExpressionOperatorDerefAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokDeref);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorDerefAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppDereferenceNonBorrowedTypeError;
    using analyse::errors::SppNonCopyableTypeError;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types_precompiled::STR_VIEW;
    using generate::common_types_precompiled::VIEW;

    // Todo: some sort of inner mutability check?
    // Get the right-hand-side expression's type for constraint checks.
    const auto lhs = meta->PostfixExpressionLhs;
    const auto lhs_type = lhs->InferType(sm, meta);
    const auto is_view =
        TypeEq(*lhs_type, *STR_VIEW, *sm->CurrentScope, *sm->CurrentScope, false) or
        TypeEq(*lhs_type, *VIEW, *sm->CurrentScope, *sm->CurrentScope, false);

    // Check the right-hand-side expression is a borrowable type.
    RaiseIf<SppDereferenceNonBorrowedTypeError>(
        lhs_type->GetConvention() == nullptr,
        {sm->CurrentScope}, ERR_ARGS(*TokDeref, *lhs, *lhs_type));

    // Check the right-hand-side expression is a "Copy" type. TODO: Add to unit tests.
    const auto lhs_type_no_conv = lhs_type->WithoutConvention();
    RaiseIf<SppNonCopyableTypeError>(
        not sm->CurrentScope->GetTypeSymbol(lhs_type)->IsCopyable() and not meta->AllowMoveDeref and not is_view,
        {sm->CurrentScope}, ERR_ARGS(*this, *lhs, *lhs_type_no_conv));
}

auto spp::asts::PostfixExpressionOperatorDerefAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // As this is cmp context, just return the "lhs" generation.
    meta->PostfixExpressionLhs->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorDerefAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the value underlying the borrow.
    const auto borrow_val = meta->PostfixExpressionLhs->Stage11_CodeGen(sm, meta, ctx);
    SPP_ASSERT(borrow_val != nullptr);

    // Dereference the borrow to get the underlying value.
    const auto deref_val = ctx->Builder.CreateLoad(
        borrow_val->getType(), borrow_val, "deref");
    return deref_val;
}

auto spp::asts::PostfixExpressionOperatorDerefAst::InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> TypeAst* {
    // Try from the cache first.
    USE_CACHED_TYPE_INFERENCE;

    // Get the right-hand-side expression's type.
    const auto lhs = meta->PostfixExpressionLhs;
    const auto lhs_type = lhs->InferType(sm, meta);
    const auto no_conv = lhs_type->WithoutConvention();
    CACHE_TYPE_INFERENCE_AND_RETURN(AstClone(no_conv));
}

SPP_MOD_END
