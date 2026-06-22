module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.ret_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_materialize;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::RetStatementAst::RetStatementAst(
    decltype(TokRet) &&tok_ret,
    decltype(Expr) &&val) :
    TokRet(std::move(tok_ret)),
    Expr(std::move(val)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokRet, lex::SppTokenType::KW_RET, "ret", Expr ? Expr->PosStart() : 0);
    Source._OriginalRetType = nullptr;
    _RetType = nullptr;
}

spp::asts::RetStatementAst::~RetStatementAst() = default;

auto spp::asts::RetStatementAst::PosStart() const
    -> std::size_t {
    // Use the "ret" token.
    return TokRet->PosStart();
}

auto spp::asts::RetStatementAst::PosEnd() const
    -> std::size_t {
    // Use the expression if it exists, otherwise use the "ret" token.
    return Expr ? Expr->PosEnd() : TokRet->PosEnd();
}

auto spp::asts::RetStatementAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<RetStatementAst>(
        AstClone(TokRet), AstClone(Expr));
}

auto spp::asts::RetStatementAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokRet).append(" ");
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::RetStatementAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::type_utils::IsTypeVoid;
    using analyse::utils::type_utils::TypeEq;
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::errors::SppCoroutineContainsReturnStatementError;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppInvalidVoidValueError;
    using analyse::errors::SppTypeMismatchError;
    using generate::common_types::VoidType;

    // Analyse the expression.
    RaiseIf<SppInvalidPrimaryExpressionError>(
        Expr and not IsPrimaryExprTypeValid(*Expr),
        {sm->CurrentScope}, ERR_ARGS(*Expr));

    // Check the enclosing function is a subroutine and not a subroutine, if a value is being returned.
    const auto function_flavour = meta->EnclosingFunctionFlavour;
    RaiseIf<SppCoroutineContainsReturnStatementError>(
        function_flavour->TokenType != lex::SppTokenType::KW_FUN and Expr != nullptr,
        {sm->CurrentScope}, ERR_ARGS(*function_flavour, *TokRet));

    // Analyse the expression if it exists, and determine the type of the expression.
    auto expr_type = VoidType(PosStart());
    if (Expr != nullptr) {
        meta->Save();
        SPP_RETURN_TYPE_OVERLOAD_HELPER(Expr.get()) {
            meta->ReturnTypeOverloadResolverType = meta->EnclosingFunctionRetType.Back();
        }

        // For case conditions, we need an assignment target in case of variants.
        meta->AssignmentTargetType = meta->EnclosingFunctionRetType.IsEmpty() ? nullptr : meta->EnclosingFunctionRetType.Back();
        meta->AssignmentTarget = meta->AssignmentTargetType ? IdentifierAst::FromType(*meta->AssignmentTargetType) : nullptr;
        Expr->Stage7_AnalyseSemantics(sm, meta);
        expr_type = Expr->InferType(sm, meta);
        meta->Restore();

        // Check the expr_type isn't Void (don't allow "ret void_func()" => "void_func(); ret").
        RaiseIf<SppInvalidVoidValueError>(
            IsTypeVoid(*expr_type, *sm->CurrentScope),
            {sm->CurrentScope}, ERR_ARGS(*Expr, "return statement"));
    }

    // Functions provide the return type, closures require inference; handle the inference.
    if (meta->EnclosingFunctionRetType.IsEmpty()) {
        _RetType = expr_type;
        Source._OriginalRetType = _RetType;
        meta->EnclosingFunctionRetType.EmplaceBack(_RetType);
        meta->EnclosingFunctionSourceRetType.EmplaceBack(_RetType);
    }
    else {
        _RetType = meta->EnclosingFunctionRetType.Back();
        Source._OriginalRetType = meta->EnclosingFunctionSourceRetType.Back();
    }

    // Type check the expression type against the return type of the enclosing subroutine.
    if (function_flavour->TokenType == lex::SppTokenType::KW_FUN) {
        const auto direct_match = TypeEq(*_RetType, *expr_type, *meta->EnclosingFunctionScope, *sm->CurrentScope);
        const auto expr_for_err = Expr ? Expr->To<Ast>() : TokRet->To<Ast>();
        RaiseIf<SppTypeMismatchError>(
            not direct_match, {meta->EnclosingFunctionScope, sm->CurrentScope},
            ERR_ARGS(*Source._OriginalRetType, *_RetType, *expr_for_err, *expr_type));
    }
}

auto spp::asts::RetStatementAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is no expression, then now ork needs to be done.
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    if (Expr == nullptr) return;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *TokRet, *sm, true, true, true, true, true, meta);
}

auto spp::asts::RetStatementAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If there is no expression, then return nullptr.
    if (Expr == nullptr) { return; }

    // Resolve the expression.
    Expr->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::RetStatementAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Use the return void instruction if there is no return value.
    if (Expr == nullptr) {
        ctx->Builder.CreateRetVoid();
        return nullptr;
    }

    // Temp holder for non-symbolic condition.
    if (sm->CurrentScope->GetVarSymbolOutermost(*Expr).First == nullptr) {
        meta->Save();
        meta->AssignmentTargetType = _RetType;
        const auto ret_val = codegen::llvm_materialize(*Expr, sm, meta, ctx);
        const auto llvm_ret_val = ret_val->Stage11_CodeGen(sm, meta, ctx);
        ctx->Builder.CreateRet(llvm_ret_val);
        meta->Restore();
    }

    // Otherwise, generate normally.
    else {
        const auto llvm_ret_val = Expr->Stage11_CodeGen(sm, meta, ctx);
        ctx->Builder.CreateRet(llvm_ret_val);
    }

    return nullptr;
}

auto spp::asts::RetStatementAst::Terminates() const
    -> bool {
    // This is the only statement that always terminates.
    return true;
}

SPP_MOD_END
