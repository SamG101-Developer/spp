module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.binary_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.bin_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.type_predicates;
import spp.asts.boolean_literal_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import spp.utils.uid;
import genex;
import llvm;

SPP_MOD_BEGIN
namespace {
  auto IsLogicalToken(
    spp::asts::TokenAst const *const tok)
    -> bool {
    using spp::lex::SppTokenType;
    return tok != nullptr and (tok->TokenType == SppTokenType::KW_AND or tok->TokenType == SppTokenType::KW_OR);
  }
}

spp::asts::BinaryExpressionAst::BinaryExpressionAst(
  decltype(Lhs) &&lhs,
  decltype(TokOp) &&tok_op,
  decltype(Rhs) &&rhs) :
  Lhs(std::move(lhs)),
  TokOp(std::move(tok_op)),
  Rhs(std::move(rhs)),
  _MappedFunc(nullptr),
  _IsLogical(IsLogicalToken(TokOp.get())) {
  Source.OriginalPosStart = Lhs ? Lhs->PosStart() : 0;
  Source.OriginalPosEnd = Rhs ? Rhs->PosEnd() : 0;
}

spp::asts::BinaryExpressionAst::~BinaryExpressionAst() = default;

auto spp::asts::BinaryExpressionAst::PosStart() const
  -> std::size_t {
  // Use the left hand side operand.
  return Lhs ? Lhs->PosStart() : Source.OriginalPosStart;
}

auto spp::asts::BinaryExpressionAst::PosEnd() const
  -> std::size_t {
  // Use the right hand side operand.
  return Rhs ? Rhs->PosEnd() : Source.OriginalPosEnd;
}

auto spp::asts::BinaryExpressionAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<BinaryExpressionAst>(
    AstClone(Lhs),
    AstClone(TokOp),
    AstClone(Rhs));
  ast->_MappedFunc = _MappedFunc;
  ast->Source = Source;
  return ast;
}

auto spp::asts::BinaryExpressionAst::ToString() const
  -> Str {
  SPP_STRING_START;
  if (Lhs != nullptr) {
    raw_string.append("(");
    SPP_STRING_APPEND(Lhs).append(" ");
    SPP_STRING_APPEND(TokOp).append(" ");
    SPP_STRING_APPEND(Rhs).append(")");
  }
  else {
    SPP_STRING_APPEND(_MappedFunc);
  }
  SPP_STRING_END;
}

auto spp::asts::BinaryExpressionAst::IsLogicalOperator() const
  -> bool {
  return _IsLogical;
}

auto spp::asts::BinaryExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Alias the common utils functions and types.
  using analyse::utils::bin_utils::CombineComparisonChain;
  using analyse::utils::bin_utils::ConvertBinExprToFuncCall;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;
  using analyse::utils::type_predicates::IsTypeBool;
  using analyse::utils::type_predicates::IsTypeTup;
  using analyse::errors::SppExpressionNotBooleanError;
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::errors::SppMemberAccessNonIndexableError;
  using analyse::errors::SppInvalidBinaryFoldExpressionError;

  // Todo: this guard shouldn't be needed?
  if (_MappedFunc or _LogicalAnalysed) { return; }

  // Handle lhs-folding.
  if (Lhs->To<FoldExpressionAst>()) {
    RaiseIf<SppInvalidPrimaryExpressionError>(
      not IsPrimaryExprTypeValid(*Rhs, *sm),
      {sm->CurrentScope}, ERR_ARGS(*Rhs));

    // Check the rhs is a tuple.
    const auto rhs_tuple_type = Rhs->InferType(sm, meta);
    RaiseIf<SppMemberAccessNonIndexableError>(
      not IsTypeTup(*rhs_tuple_type, *sm->CurrentScope),
      {sm->CurrentScope}, ERR_ARGS(*Rhs, *rhs_tuple_type, *Lhs));

    // Get the parts of the tuple.
    const auto rhs_num_elems = rhs_tuple_type->TypeParts()[0]->GnArgGroup->Args.Len();
    RaiseIf<SppInvalidBinaryFoldExpressionError>(
      rhs_num_elems < 2,
      {sm->CurrentScope}, ERR_ARGS(*Rhs, *rhs_tuple_type, rhs_num_elems));

    auto new_asts = UniqueVec<PostfixExpressionAst>();
    for (auto i = 0u; i < rhs_num_elems; ++i) {
      auto field = MakeUnique<IdentifierAst>(Rhs->PosStart(), std::to_string(i));
      auto new_ast = MakeUnique<PostfixExpressionAst>(
        AstClone(Rhs),
        MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
      new_ast->Stage7_AnalyseSemantics(sm, meta);
      new_asts.EmplaceBack(std::move(new_ast));
    }

    // Convert "t = (0, 1, 2, 3)", ".. + t" into "(((t.0 + t.1) + t.2) + t.3)".
    Lhs = std::move(new_asts[0]);
    Rhs = std::move(new_asts[1]);
    for (auto &&new_ast : new_asts | genex::views::move | genex::views::drop(2)) {
      Lhs = MakeUnique<BinaryExpressionAst>(std::move(Lhs), AstClone(TokOp), std::move(Rhs));
      Rhs = std::move(new_ast);
    }
    _MappedFunc = ConvertBinExprToFuncCall(*this, sm, meta);
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
  }

  // Handle rhs-folding.
  else if (Rhs->To<FoldExpressionAst>()) {
    RaiseIf<SppInvalidPrimaryExpressionError>(
      not IsPrimaryExprTypeValid(*Lhs, *sm),
      {sm->CurrentScope}, ERR_ARGS(*Lhs));

    // Check the lhs is a tuple.
    const auto lhs_tuple_type = Lhs->InferType(sm, meta);
    RaiseIf<SppMemberAccessNonIndexableError>(
      not IsTypeTup(*lhs_tuple_type, *sm->CurrentScope),
      {sm->CurrentScope}, ERR_ARGS(*Lhs, *lhs_tuple_type, *Rhs));

    // Get the parts of the tuple.
    const auto lhs_num_elems = lhs_tuple_type->TypeParts()[0]->GnArgGroup->Args.Len();
    RaiseIf<SppInvalidBinaryFoldExpressionError>(
      lhs_num_elems < 2,
      {sm->CurrentScope}, ERR_ARGS(*Lhs, *lhs_tuple_type, lhs_num_elems));

    auto new_asts = UniqueVec<PostfixExpressionAst>();
    for (auto i = 0U; i < lhs_num_elems; ++i) {
      auto field = MakeUnique<IdentifierAst>(Lhs->PosStart(), std::to_string(i));
      auto new_ast = MakeUnique<PostfixExpressionAst>(
        AstClone(Lhs),
        MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
      new_ast->Stage7_AnalyseSemantics(sm, meta);
      new_asts.EmplaceBack(std::move(new_ast));
    }

    // Convert "t = (0, 1, 2, 3)", "t + .." into "(t.0 + (t.1 + (t.2 + t.3)))".
    Lhs = std::move(new_asts[new_asts.Len() - 2]);
    Rhs = std::move(new_asts[new_asts.Len() - 1]);
    for (auto &&new_ast : new_asts | genex::views::move_reverse | genex::views::drop(2)) {
      Rhs = MakeUnique<BinaryExpressionAst>(std::move(Lhs), AstClone(TokOp), std::move(Rhs));
      Lhs = std::move(new_ast);
    }
    _MappedFunc = ConvertBinExprToFuncCall(*this, sm, meta);
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
  }

  else {
    // A chain of comparisons is rewritten into an "and" of
    // its pairs first, so that the "and" it produces is
    // analysed as one - conditional right operand and all -
    // rather than being turned straight into a call.
    auto combined = CombineComparisonChain(*this, sm, meta);
    Lhs = std::move(combined->Lhs);
    TokOp = std::move(combined->TokOp);
    Rhs = std::move(combined->Rhs);
    _IsLogical = IsLogicalToken(TokOp.get());

    RaiseIf<SppInvalidPrimaryExpressionError>(
      not IsPrimaryExprTypeValid(*Lhs, *sm),
      {sm->CurrentScope}, ERR_ARGS(*Lhs));

    RaiseIf<SppInvalidPrimaryExpressionError>(
      not IsPrimaryExprTypeValid(*Rhs, *sm),
      {sm->CurrentScope}, ERR_ARGS(*Rhs));

    // "and" and "or" are not mapped to a method; they are built
    // in, like "not", for short-circuiting. Both operands must
    // be "Bool", again like "not".
    if (IsLogicalOperator()) {
      Lhs->Stage7_AnalyseSemantics(sm, meta);
      Rhs->Stage7_AnalyseSemantics(sm, meta);

      const auto what = Str("\"") + TokOp->TokenData + "\" expression";
      const auto lhs_type = Lhs->InferType(sm, meta);
      RaiseIf<SppExpressionNotBooleanError>(
        lhs_type->GetConvention() != nullptr or not IsTypeBool(*lhs_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Lhs, *lhs_type, what));

      const auto rhs_type = Rhs->InferType(sm, meta);
      RaiseIf<SppExpressionNotBooleanError>(
        rhs_type->GetConvention() != nullptr or not IsTypeBool(*rhs_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*Rhs, *rhs_type, what));

      _LogicalAnalysed = true;
      return;
    }

    // Standard non-folding binary expression.
    _MappedFunc = ConvertBinExprToFuncCall(*this, sm, meta);
    _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
  }
}

auto spp::asts::BinaryExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // A logical operator has no mapped function to forward to.
  // Both operands are checked as at worst, they both evaluate,
  // so must both be valid. Maintains consistency in all code.
  if (IsLogicalOperator()) {
    Lhs->Stage8_CheckMemory(sm, meta);
    Rhs->Stage8_CheckMemory(sm, meta);
    return;
  }

  // Forward the memory checking to the mapped function.
  _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::BinaryExpressionAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // Do the short-circuiting at compile time by evaluating
  // the left-hand-side, and if it is true, and we are not
  // doing an "and", then we can return true already.
  if (IsLogicalOperator()) {
    Lhs->Stage9_CompTimeResolve(sm, meta);
    const auto lhs_is_true = meta->CmpResult->ToUnchecked<BooleanLiteralAst>()->IsTrue();
    if (lhs_is_true != (TokOp->TokenType == lex::SppTokenType::KW_AND)) { return; }
    Rhs->Stage9_CompTimeResolve(sm, meta);
    return;
  }

  // Forward the compile-time resolution to the mapped function.
  _MappedFunc->Stage9_CompTimeResolve(sm, meta);
}

auto spp::asts::BinaryExpressionAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Forward the code generation to the mapped function. The common
  // expressions like "1 + 2" follow these steps:
  // |- 1 + 2
  // |- 1.add(2)
  // |- S32::add(self=1, that=2) [inlined]
  //    |- std::intrinsics::add(1, 2) [inlined]
  //    |- ctx->Builder.CreateAdd(a, b)
  //    |- add #1 #2
  // This allows the optimal instruction to be used, whilst maintaining
  // the uniform function processing for all operations in s++.
  if (not IsLogicalOperator()) { return _MappedFunc->Stage11_CodeGen(sm, meta, ctx); }

  // The "and" and "or" operations cannot map from the function
  // as there is no function to map from. Instead, they have
  // manual codegen, like the "not" operator.
  const auto uid = "." + spp::utils::Uid(this);
  const auto is_and = TokOp->TokenType == lex::SppTokenType::KW_AND;
  const auto llvm_bool_ty = llvm::Type::getInt1Ty(*ctx->Context);

  // Both operands are owned booleans, so each generates as an
  // "i1"
  const auto llvm_lhs = Lhs->Stage11_CodeGen(sm, meta, ctx);
  SPP_ASSERT(llvm_lhs->getType()->isIntegerTy(1));
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto rhs_bb = llvm::BasicBlock::Create(*ctx->Context, "logical.rhs" + uid, func);
  const auto join_bb = llvm::BasicBlock::Create(*ctx->Context, "logical.join" + uid, func);

  // Taken from here rather than from before the operand was
  // generated, because generating it may have left the builder
  // in a block of its own making, like from a "case" on the left
  // of an "and" expression.
  const auto lhs_end_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateCondBr(llvm_lhs, is_and ? rhs_bb : join_bb, is_and ? join_bb : rhs_bb);

  // Do the right-hand-side codegen (doesn't execute the LLVM,
  // just creates it), and hook it into the short-circuiting
  // mechanism.
  ctx->Builder.SetInsertPoint(rhs_bb);
  const auto llvm_rhs = Rhs->Stage11_CodeGen(sm, meta, ctx);
  SPP_ASSERT(llvm_rhs->getType()->isIntegerTy(1));
  const auto rhs_end_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(join_bb);

  // The short-circuit edge carries the answer the left operand
  // already gave: "false" for an "and", "true" for an "or". The
  // other edge carries whatever the right operand evaluated to.
  ctx->Builder.SetInsertPoint(join_bb);
  const auto llvm_res = ctx->Builder.CreatePHI(llvm_bool_ty, 2, "logical" + uid);
  llvm_res->addIncoming(llvm::ConstantInt::getBool(*ctx->Context, not is_and), lhs_end_bb);
  llvm_res->addIncoming(llvm_rhs, rhs_end_bb);
  return llvm_res;
}

auto spp::asts::BinaryExpressionAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // A logical operator is boolean by construction - both operands
  // are required to be, and the result is one of them.
  if (IsLogicalOperator()) { return generate::common_types::BooleanType(PosStart()); }

  // Infer the type from the function mapping of the binary expression.
  return _MappedFunc->InferType(sm, meta);
}

auto spp::asts::BinaryExpressionAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const
  -> Shared<ExpressionAst> {
  // Both operands are expressions.
  return MakeShared<BinaryExpressionAst>(
    AstClone(Lhs->SubstituteGenericsExpr(args)),
    AstClone(TokOp),
    AstClone(Rhs->SubstituteGenericsExpr(args)));
}

SPP_MOD_END
