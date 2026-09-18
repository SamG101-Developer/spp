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
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_utils;
import spp.asts.convention_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.ret_statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_defer;
import spp.codegen.llvm_materialize;
import spp.lex.tokens;
import spp.utils.ptr;
import spp.utils.uid;
import llvm;

SPP_MOD_BEGIN
GenExpressionAst::GenExpressionAst(
  decltype(TokGen) &&tok_gen,
  decltype(Conv) &&conv,
  decltype(Expr) &&expr) :
  TokGen(std::move(tok_gen)),
  Conv(std::move(conv)),
  Expr(std::move(expr)),
  _GenType(nullptr),
  _IsOnce(false) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokGen, lex::SppTokenType::KW_GEN, "gen");
}

GenExpressionAst::~GenExpressionAst() = default;

auto GenExpressionAst::PosStart() const -> std::size_t {
  // Use the "gen" token.
  return TokGen->PosStart();
}

auto GenExpressionAst::PosEnd() const -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto GenExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto g = MakeUnique<GenExpressionAst>(
    AstClone(TokGen),
    AstClone(Conv),
    AstClone(Expr));
  g->_GenType = AstClone(_GenType);
  g->_IsOnce = _IsOnce;
  return g;
}

auto GenExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokGen).append(" ");
  SPP_STRING_APPEND(Conv);
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto GenExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::errors::SppFunctionSubroutineContainsGenExpressionError;
  using analyse::errors::SppYieldedTypeMismatchError;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
  using analyse::utils::type_utils::GetGenAndYieldTypes;
  using analyse::utils::type_utils::SubstituteSelfTypeAndAnalyse;
  using analyse::utils::type_compare::TypeEq;
  using analyse::utils::type_compare::TypeFwdEq;
  using analyse::utils::type_utils::BuildFwdCall;
  using generate::common_types::GenType;
  using generate::common_types::VoidType;

  // Check the enclosing function is a coroutine and not
  // a subroutine.
  const auto function_flavour = meta->EnclosingFunctionFlavour;
  RaiseIf<SppFunctionSubroutineContainsGenExpressionError>(
    function_flavour->TokenType != lex::SppTokenType::KW_COR,
    {sm->CurrentScope}, ERR_ARGS(*function_flavour, *TokGen));

  // Analyse the expression if it exists, and determine
  // the type of the expression.
  auto expr_type = VoidType(PosStart());
  if (Expr != nullptr) {
    const auto _meta_guard = MetaGuard(meta);
    if (not meta->EnclosingFunctionRetType.IsEmpty()) {
      auto const &ret_type = meta->EnclosingFunctionRetType[0];
      auto [_, yield_type, _] = GetGenAndYieldTypes(
        TypeRef::Of(*ret_type, *sm->CurrentScope), *sm->CurrentScope, *ret_type,
        [&] { return ret_type; }, "coroutine");

      meta->AssignmentTargetType = yield_type;
      meta->AssignmentTargetType = SubstituteSelfTypeAndAnalyse(
        *meta->AssignmentTargetType, *sm->CurrentScope, *sm, *meta);
      meta->AssignmentTarget = IdentifierAst::FromType(*meta->AssignmentTargetType);
      SPP_RETURN_TYPE_OVERLOAD_HELPER(Expr.get()) {
        meta->ReturnTypeOverloadResolverType = yield_type != nullptr
          ? MakeShared<TypeRef>(TypeRef::Of(*yield_type, *sm->CurrentScope))
          : nullptr;
      }
    }
    else {
      meta->AssignmentTargetType = nullptr;
      meta->AssignmentTarget = nullptr;
    }
    Expr->Stage7_AnalyseSemantics(sm, meta);

    RaiseIf<SppInvalidPrimaryExpressionError>(
      not IsPrimaryExprTypeValid(*Expr, *sm),
      {sm->CurrentScope}, ERR_ARGS(*Expr));

    expr_type = Expr->InferType(sm, meta);
    if (Conv) { expr_type = expr_type->WithConvention(AstClone(Conv)); }
  }

  // Functions provide the return type, closures require
  // inference; handle the inference.
  if (meta->EnclosingFunctionRetType.IsEmpty()) {
    _GenType = GenType(Expr ? Expr->PosStart() : TokGen->PosStart(), expr_type);
    _GenType->Stage7_AnalyseSemantics(sm, meta);
    meta->EnclosingFunctionRetType.EmplaceBack(_GenType);
    meta->EnclosingFunctionSourceRetType.EmplaceBack(expr_type);
  }
  else {
    // Todo - this list isn't getting cleared, so [0] != [last] (using .Back() hides the bug - TEMP FIX).
    _GenType = meta->EnclosingFunctionRetType.Back();
  }

  // Determine the "Yield" type of the enclosing function
  // (to type check the expression against).
  const auto gen_ref = TypeRef::Of(*_GenType, *sm->CurrentScope);
  auto [gen_sym, yield_type, is_once] = GetGenAndYieldTypes(
    gen_ref, *sm->CurrentScope, *_GenType, [&] { return _GenType; }, "coroutine");

  // When we are yielding a value that *forwards* to the return
  // type, we need to call the forwarding function and inject
  // it into the expression field of this ast.
  if (Expr != nullptr and TypeFwdEq(
    *expr_type, *yield_type, *sm->CurrentScope, *meta->EnclosingFunctionScope)) {
    const auto expr_ref = TypeRef::Of(*expr_type, *sm->CurrentScope);
    if (auto fwd_call = BuildFwdCall(*Expr, expr_ref, sm, meta); fwd_call != nullptr) {
      Expr = std::move(fwd_call);
      Expr->Stage7_AnalyseSemantics(sm, meta);
      expr_type = Expr->InferType(sm, meta);
      if (Conv) { expr_type = expr_type->WithConvention(AstClone(Conv)); }
    }
  }

  const auto direct_match = TypeEq(
    *yield_type, *expr_type, *meta->EnclosingFunctionScope, *sm->CurrentScope);

  // The enclosing return type, unless it only reaches a generator
  // through a super class, which is then named.
  if (gen_sym != gen_ref.Sym) { _GenType = gen_sym->FqName(); }
  _IsOnce = is_once;

  // Todo: Known issue with the "yield_type" ast position being
  //  wrong.
  RaiseIf<SppYieldedTypeMismatchError>(
    not direct_match, {sm->CurrentScope},
    ERR_ARGS(*yield_type, *yield_type, Expr ? *Expr->To<Ast>() : *TokGen->To<Ast>(), *expr_type));
}

auto GenExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::errors::SppInvalidMutationError;
  using analyse::utils::mem_utils::ValidateSymbolMemory;

  // If there is no expression, then now ork needs to be
  // done.
  if (Expr == nullptr) return;

  // Ensure the argument isn't moved or partially moved
  // (for all conventions)
  Expr->Stage8_CheckMemory(sm, meta);
  ValidateSymbolMemory(
    *Expr, *TokGen, *sm, true, true, false, false, meta);

  // If the value is non-symbolic, then there is no borrow
  // logic to implement, so return.
  auto [sym, _] = sm->CurrentScope->GetVarSymbolOutermost(*Expr);
  if (sym == nullptr) { return; }

  if (Conv == nullptr) {
    // Ensure that attributes aren't being moved off of a
    // borrowed value and that pins are maintained. Mark the
    // move or partial move of the argument.
    ValidateSymbolMemory(
      *Expr, *TokGen, *sm, false, false, true, true, meta);
  }

  else if (*Conv == ConventionTag::MUT) {
    // Immutable symbols cannot be mutated.
    RaiseIf<SppInvalidMutationError>(
      not sym->IsMutable, {sm->CurrentScope},
      ERR_ARGS(*sym->Name, *Conv, *spp::get<0>(sym->MemInfo->AstInitialization), "immutable symbol"));

    // Immutable borrows, even if their symbol is mutable,
    // cannot be mutated.
    RaiseIf<SppInvalidMutationError>(
      spp::get<0>(sym->MemInfo->AstBorrowed) and *sym->Type->GetConvention() == ConventionTag::REF,
      {sm->CurrentScope}, ERR_ARGS(*sym->Name, *Conv, *spp::get<0>(sym->MemInfo->AstBorrowed), "immutable borrow"));
  }
}

auto GenExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Consider if we are in a subroutine by desugar (ie for a
  // lowered GenOnce check. If this is the case, use the return
  // instruction instead.
  if (meta->EnclosingFunctionFlavour->TokenType == lex::SppTokenType::KW_FUN) {
    if (Conv != nullptr) {
      ctx->Builder.CreateRet(codegen::llvm_addr_of(*Expr, sm, meta, ctx));
      return nullptr;
    }

    // Todo: Just CreateRet here?
    const auto mock_ret_statement = MakeUnique<RetStatementAst>(
      nullptr, std::move(Expr));
    return mock_ret_statement->Stage11_CodeGen(sm, meta, ctx);
  }

  // The three-step operation for the "gen" expression is
  // to store the expression into the yield slot of the env,
  // suspend the coroutine, then receive the sent value.

  // Step 1: Generate the expression into an llvm value, and
  // store it in the generator state object. The slot layout
  // is taken from the promise's own allocation rather than
  // rebuilt here, so the store cannot be wider than the
  // storage the frame reserved for it.
  const auto llvm_gen_state_ty = meta->LlvmGeneratorState->getAllocatedType();
  const auto llvm_yield_val = Expr != nullptr ? Expr->Stage11_CodeGen(sm, meta, ctx) : nullptr;

  // A bare "gen" yields Void, so there is nothing to store.
  if (llvm_yield_val != nullptr) {
    const auto llvm_yield_slot = codegen::GetLlvmGeneratorSlotPtr(
      meta->LlvmGeneratorState, llvm_gen_state_ty,
      codegen::LlvmGeneratorStateStructFields::YIELD_SLOT,
      "gen.yield.slot", ctx);
    ctx->Builder.CreateStore(llvm_yield_val, llvm_yield_slot);
  }

  // Step 2: Invoke the coroutine suspension intrinsic,
  // allowing the caller to use the yielded value. Control
  // comes back into the block this leaves the builder in.
  const auto uid = "." + spp::utils::Uid(this);
  const auto parked_bb = ctx->Builder.GetInsertBlock();
  const auto destroy_bb = llvm::BasicBlock::Create(
    *ctx->Context, "gen.coro.destroy" + uid, parked_bb->getParent());

  ctx->Builder.SetInsertPoint(destroy_bb);
  codegen::EmitDeferredUnwind(*sm->CurrentScope, meta->EnclosingFunctionScope, true, sm, meta, ctx);
  ctx->Builder.CreateBr(meta->LlvmGenerator->CleanupBlock);

  ctx->Builder.SetInsertPoint(parked_bb);
  codegen::EmitLlvmGeneratorSuspend(
    false, meta->LlvmGenerator->SuspendBlock, destroy_bb,
    "gen.coro.suspend", "gen.resume", ctx);

  // Step 3: Read the value from the send slot on the generator
  // state, and as this is an expression, return the value out
  // of this function.
  const auto llvm_send_slot = codegen::GetLlvmGeneratorSlotPtr(
    meta->LlvmGeneratorState, llvm_gen_state_ty,
    codegen::LlvmGeneratorStateStructFields::SEND_SLOT,
    "gen.send.slot", ctx);

  const auto llvm_recv_val = ctx->Builder.CreateLoad(
    llvm::cast<llvm::StructType>(llvm_gen_state_ty)->getElementType(
      static_cast<unsigned>(std::to_underlying(
        codegen::LlvmGeneratorStateStructFields::SEND_SLOT))),
    llvm_send_slot, "gen.send.value");
  return llvm_recv_val;
}

auto GenExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *) -> Shared<TypeAst> {
  // Get the "Send" generic type parameter from the generator
  // type, read off its symbol. As there is no "Send" on
  // "GenOnce", use "Void" in this case.
  using generate::common_types_precompiled::VOID;
  return not _IsOnce
    ? sm->CurrentScope->GetTypeSymbol(_GenType.get())->TypeArgType("Send")
    : VOID;
}

SPP_MOD_END
