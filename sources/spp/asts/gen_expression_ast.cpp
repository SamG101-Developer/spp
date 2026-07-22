module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.gen_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.lex.tokens;
import spp.utils.uid;
import llvm;

SPP_MOD_BEGIN
spp::asts::GenExpressionAst::GenExpressionAst(
    decltype(TokGen) &&tok_gen,
    decltype(Conv) &&conv,
    decltype(Expr) &&expr) :
    TokGen(std::move(tok_gen)),
    Conv(std::move(conv)),
    Expr(std::move(expr)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokGen, lex::SppTokenType::KW_GEN, "gen");
}

spp::asts::GenExpressionAst::~GenExpressionAst() = default;

auto spp::asts::GenExpressionAst::PosStart() const
    -> std::size_t {
    // Use the "gen" token.
    return TokGen->PosStart();
}

auto spp::asts::GenExpressionAst::PosEnd() const
    -> std::size_t {
    // Use the expression.
    return Expr->PosEnd();
}

auto spp::asts::GenExpressionAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<GenExpressionAst>(
        AstClone(TokGen), AstClone(Conv), AstClone(Expr));
}

auto spp::asts::GenExpressionAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokGen).append(" ");
    SPP_STRING_APPEND(Conv);
    SPP_STRING_APPEND(Expr);
    SPP_STRING_END;
}

auto spp::asts::GenExpressionAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
    using analyse::utils::type_utils::GetGenAndYieldTypes;
    using analyse::utils::type_utils::ResolveAndSubstituteSelfType;
    using analyse::utils::type_utils::TypeEq;
    using generate::common_types::GenType;
    using generate::common_types::VoidType;
    using analyse::errors::SppInvalidPrimaryExpressionError;
    using analyse::errors::SppFunctionSubroutineContainsGenExpressionError;
    using analyse::errors::SppYieldedTypeMismatchError;

    // Check the enclosing function is a coroutine and not a subroutine.
    const auto function_flavour = meta->EnclosingFunctionFlavour;
    RaiseIf<SppFunctionSubroutineContainsGenExpressionError>(
        function_flavour->TokenType != lex::SppTokenType::KW_COR,
        {sm->CurrentScope}, ERR_ARGS(*function_flavour, *TokGen));

    // Analyse the expression if it exists, and determine the type of the expression.
    auto expr_type = VoidType(PosStart());
    if (Expr != nullptr) {
        meta->Save();
        auto [gen_type, yield_type, _] = GetGenAndYieldTypes(
            *meta->EnclosingFunctionRetType[0], *sm->CurrentScope, *meta->EnclosingFunctionRetType[0], "coroutine");

        meta->AssignmentTargetType = meta->EnclosingFunctionRetType.IsEmpty() ? nullptr : yield_type;
        meta->AssignmentTargetType = ResolveAndSubstituteSelfType(*meta->AssignmentTargetType, *sm->CurrentScope, *sm, *meta);
        meta->AssignmentTarget = meta->AssignmentTargetType ? IdentifierAst::FromType(*meta->AssignmentTargetType) : nullptr;
        SPP_RETURN_TYPE_OVERLOAD_HELPER(Expr.get()) { meta->ReturnTypeOverloadResolverType = std::move(yield_type); }
        Expr->Stage7_AnalyseSemantics(sm, meta);

        RaiseIf<SppInvalidPrimaryExpressionError>(
            not IsPrimaryExprTypeValid(*Expr, *sm),
            {sm->CurrentScope}, ERR_ARGS(*Expr));

        expr_type = Expr->InferType(sm, meta);
        if (Conv) expr_type = expr_type->WithConvention(AstClone(Conv));
        meta->Restore();
    }

    // Functions provide the return type, closures require inference; handle the inference.
    if (meta->EnclosingFunctionRetType.IsEmpty()) {
        _GenType = GenType(Expr->PosStart(), expr_type);
        _GenType->Stage7_AnalyseSemantics(sm, meta);
        meta->EnclosingFunctionRetType.EmplaceBack(_GenType);
        meta->EnclosingFunctionSourceRetType.EmplaceBack(expr_type);
    }
    else {
        // Todo - this list isn't getting cleared, so [0] != [last] (using .Back() hides the bug - TEMP FIX).
        _GenType = meta->EnclosingFunctionRetType.Back();
    }

    // Determine the "Yield" type of the enclosing function (to type check the expression against).
    auto [_, yield_type, _] = GetGenAndYieldTypes(*_GenType, *sm->CurrentScope, *_GenType, "coroutine");
    const auto direct_match = TypeEq(
        *yield_type, *expr_type, *meta->EnclosingFunctionScope, *sm->CurrentScope);

    // Todo: Known issue with the "yield_type" ast position being wrong.
    RaiseIf<SppYieldedTypeMismatchError>(
        not direct_match, {sm->CurrentScope},
        ERR_ARGS(*yield_type, *yield_type, Expr ? *Expr->To<Ast>() : *TokGen->To<Ast>(), *expr_type));
}

auto spp::asts::GenExpressionAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    //
    using analyse::utils::mem_utils::ValidateSymbolMemory;
    using analyse::errors::SppInvalidMutationError;

    // If there is no expression, then now ork needs to be done.
    if (Expr == nullptr) return;

    // Ensure the argument isn't moved or partially moved (for all conventions)
    Expr->Stage8_CheckMemory(sm, meta);
    ValidateSymbolMemory(*Expr, *TokGen, *sm, true, true, false, false, meta);

    // If the value is non-symbolic, then there is no borrow logic to implement, so return.
    auto [sym, _] = sm->CurrentScope->GetVarSymbolOutermost(*Expr);
    if (sym == nullptr) { return; }

    if (Conv == nullptr) {
        // Ensure that attributes aren't being moved off of a borrowed value and that pins are maintained. Mark the move
        // or partial move of the argument.
        ValidateSymbolMemory(*Expr, *TokGen, *sm, false, false, true, true, meta);
    }

    else if (*Conv == ConventionTag::MUT) {
        // Immutable symbols cannot be mutated.
        RaiseIf<SppInvalidMutationError>(
            not sym->IsMutable, {sm->CurrentScope},
            ERR_ARGS(*sym->Name, *Conv, *std::get<0>(sym->MemInfo->AstInitialization), "immutable symbol"));

        // Immutable borrows, even if their symbol is mutable, cannot be mutated.
        RaiseIf<SppInvalidMutationError>(
            std::get<0>(sym->MemInfo->AstBorrowed) and *sym->Type->GetConvention() == ConventionTag::REF,
            {sm->CurrentScope}, ERR_ARGS(*sym->Name, *Conv, *std::get<0>(sym->MemInfo->AstBorrowed), "immutable borrow"));
    }
}

auto spp::asts::GenExpressionAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Get the generator environment (the env pointer bound as the resume function's first argument) and its struct type
    // (for GEPs into the frame).
    const auto coro = meta->EnclosingFunctionScope->AstNode->To<CoroutinePrototypeAst>();
    const auto llvm_gen_env = coro->LlvmCoroGenEnv;
    const auto env_type = coro->LlvmCoroGenEnvType;
    SPP_ASSERT(llvm_gen_env != nullptr and env_type != nullptr);

    const auto uid = "." + spp::utils::Uid(this);

    // Mark the coroutine as suspended-with-a-value (state: YIELDED) and record the location to resume at. This "gen" is
    // the i-th yield, and the resume switch maps location i+1 to its continuation block.
    const auto location = ctx->YieldContinuations.Len() + 1;
    ctx->Builder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctx->Context), std::to_underlying(codegen::CoroutineState::YIELDED)),
        ctx->Builder.CreateStructGEP(env_type, llvm_gen_env, std::to_underlying(codegen::GenEnvField::STATE), "gen.state" + uid));
    ctx->Builder.CreateStore(
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx->Context), location),
        ctx->Builder.CreateStructGEP(env_type, llvm_gen_env, std::to_underlying(codegen::GenEnvField::LOCATION), "gen.loc" + uid));

    // Store the yielded value into the yield slot (skipped for a bare "gen" with no expression).
    if (Expr != nullptr) {
        ctx->Builder.CreateStore(
            Expr->Stage11_CodeGen(sm, meta, ctx),
            ctx->Builder.CreateStructGEP(env_type, llvm_gen_env, std::to_underlying(codegen::GenEnvField::YIELD_SLOT), "gen.yield.slot" + uid));
    }

    // To suspend, return control to the caller.
    ctx->Builder.CreateRetVoid();

    // The continuation block: where the resume switch jumps to on the next ".res()". Register it so the coroutine
    // prototype can add its switch case.
    const auto cont_bb = llvm::BasicBlock::Create(
        *ctx->Context, "gen.cont" + uid,
        ctx->Builder.GetInsertBlock()->getParent());
    ctx->YieldContinuations.push_back(cont_bb);
    ctx->Builder.SetInsertPoint(cont_bb);

    // The value of the "gen" expression is whatever was sent in via ".res(x)", so read it back off the send slot.
    const auto send_slot_ty = env_type->getElementType(std::to_underlying(codegen::GenEnvField::SEND_SLOT));
    const auto send_slot_ptr = ctx->Builder.CreateStructGEP(
        env_type, llvm_gen_env, std::to_underlying(codegen::GenEnvField::SEND_SLOT), "gen.send.slot" + uid);
    return ctx->Builder.CreateLoad(send_slot_ty, send_slot_ptr, "gen.send.val" + uid);
}

auto spp::asts::GenExpressionAst::InferType(
    ScopeManager *,
    CompilerMetaData *)
    -> Shared<TypeAst> {
    // Get the "Send" generic type parameter from the generator type.
    auto send_type = _GenType->TypeParts().Back()->GnArgGroup->TypeAt("Send")->Val;
    return send_type;
}

SPP_MOD_END
