module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorKeywordResAst::PostfixExpressionOperatorKeywordResAst(
  decltype(TokDot) &&tok_dot,
  decltype(TokRes) &&tok_res,
  decltype(FnArgGroup) &&arg_group) :
  TokDot(std::move(tok_dot)),
  TokRes(std::move(tok_res)),
  FnArgGroup(std::move(arg_group)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->FnArgGroup);
}

spp::asts::PostfixExpressionOperatorKeywordResAst::~PostfixExpressionOperatorKeywordResAst() = default;

auto spp::asts::PostfixExpressionOperatorKeywordResAst::PosStart() const
  -> std::size_t {
  // Use the "." token.
  return TokDot != nullptr ? TokDot->PosStart() : FnArgGroup->PosStart();
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::PosEnd() const
  -> std::size_t {
  // Use the argument group if it exists, otherwise use the "res" token.
  return FnArgGroup->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorKeywordResAst>(
    AstClone(TokDot),
    AstClone(TokRes),
    AstClone(FnArgGroup));
  ast->_MappedFunc = _MappedFunc;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW(".");
  SPP_STRING_APPEND_RAW("res");
  SPP_STRING_APPEND(FnArgGroup);
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Already analysed => return early.
  using analyse::utils::type_utils::GetGenAndYieldTypes;
  if (_MappedFunc != nullptr) { return; }

  // Check the left-hand-side is a generator type (for specific errors).
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  GetGenAndYieldTypes(
    *lhs_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "resume expression");

  // Check the argument (send value) is valid, by passing it into the ".send" function call.
  auto send = MakeUnique<IdentifierAst>(PosStart(), "send");
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(send));
  auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(meta->PostfixExpressionLhs), std::move(field));
  auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, std::move(FnArgGroup), nullptr);
  func_call->Source.OriginalExpr = this;
  _MappedFunc = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  meta->Save();
  meta->IgnoreAccessModifierViolations = true; // Because of "Generated" Todo: Too broad?
  _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
  meta->Restore();
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Forward the memory check to the mapped function, which will check the arguments, and the function call.
  _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // The three-step operation for the "res" operation is to
  // store the potential argument into the send slot of the
  // env, resume the coroutine, then use the yielded value.

  // Get the indexes for the fields into the generator state
  // object stored on the "meta" context.
  const auto idx_0 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctx->Context), 0uz);
  const auto idx_y = llvm::ConstantInt::get(
    codegen::GetLlvmGeneratorStateYieldSlotType(ctx),
    std::to_underlying(codegen::LlvmGeneratorStateStructFields::YIELD_SLOT));
  const auto idx_s = llvm::ConstantInt::get(
    codegen::GetLlvmGeneratorStateSendSlotType(ctx),
    std::to_underlying(codegen::LlvmGeneratorStateStructFields::SEND_SLOT));

  // Step 1: Place the value of the argument (if it exists),
  // into the "send" slot on the generator state struct.
  const auto llvm_send_slot = ctx->Builder.CreateGEP(
    codegen::GetLlvmGeneratorStateSendSlotType(ctx), meta->LlvmGeneratorState, {idx_0, idx_s}, "gen.send.slot");
  const auto llvm_send_value = FnArgGroup->Args.IsEmpty()
    ? llvm::Constant::getNullValue(llvm::Type::getVoidTy(*ctx->Context))
    : FnArgGroup->Args[0]->Stage11_CodeGen(sm, meta, ctx);
  ctx->Builder.CreateStore(llvm_send_value, llvm_send_slot);

  // Step 2: Invoke the llvm coroutine intrinsic, allowing
  // program to resume the coroutine execution, to get the
  // next value.
  ctx->Builder.CreateIntrinsic(
    llvm::Intrinsic::coro_resume, {}, {meta->LlvmGenerator->Handle},
    {}, "gen.coro.resume");

  // Step 3: Get the yielded value from the generator state,
  // and pass return it as the result of this operation.
  const auto llvm_yield_slot = ctx->Builder.CreateGEP(
    codegen::GetLlvmGeneratorStateYieldSlotType(ctx), meta->LlvmGeneratorState, {idx_0, idx_y}, "gen.yield.slot");
  const auto llvm_yielded_val = ctx->Builder.CreateLoad(
    codegen::GetLlvmGeneratorStateYieldSlotType(ctx), llvm_yield_slot, "gen.yield.value");
  return llvm_yielded_val;
}

auto spp::asts::PostfixExpressionOperatorKeywordResAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Get the generator type.
  using analyse::utils::type_utils::GetGenAndYieldTypes;
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  auto [_, yield_type, _] = GetGenAndYieldTypes(
    *lhs_type, *sm->CurrentScope, *meta->PostfixExpressionLhs, "resume expression");
  return yield_type;
}

SPP_MOD_END
