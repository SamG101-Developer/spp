module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_deref_ast;
import spp.asts.postfix_expression_operator_early_return_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_index_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.postfix_expression_operator_slice_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lsp.resolution_index;

SPP_MOD_BEGIN
PostfixExpressionAst::PostfixExpressionAst(
  decltype(Lhs) &&lhs,
  decltype(Op) &&op) :
  Lhs(std::move(lhs)),
  Op(std::move(op)) {
  //
  Source.CachedInference = nullptr;
}

PostfixExpressionAst::~PostfixExpressionAst() = default;

auto PostfixExpressionAst::PosStart() const -> std::size_t {
  // Use the lhs.
  return Lhs->PosStart();
}

auto PostfixExpressionAst::PosEnd() const -> std::size_t {
  // Use the operator.
  return Op->PosEnd();
}

auto PostfixExpressionAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<PostfixExpressionAst>(
    AstClone(Lhs),
    AstClone(Op));
}

auto PostfixExpressionAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Lhs);
  SPP_STRING_APPEND(Op);
  SPP_STRING_END;
}

auto PostfixExpressionAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  if (Op->To<PostfixExpressionOperatorEarlyReturnAst>() != nullptr) {
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->PostfixExpressionLhs = Lhs.get();
      Op->Stage7_AnalyseSemantics(sm, meta);
    }
    return;
  }

  // The "ast_clone" is required because the "lhs" could be a uniquely owned TypeAst, which must have access to
  // "shared_from_this" (on a shared pointer, which "ast_clone" provides).
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ReturnTypeOverloadResolverType = nullptr;
    if (Lhs->To<TypeAst>() != nullptr) {
      auto temp_lhs = Shared<TypeAst>(Lhs.release()->ToUnchecked<TypeAst>());
      temp_lhs = type_resolution::ResolveWrittenType(*temp_lhs, *sm, *meta);
      Lhs = AstClone(temp_lhs); // Todo: std::move here once shared pointers are removed
    }
    else {
      // A deref under a member access is not a use of the whole
      // value: "b@.v" reads one field through the borrow and
      // "b@.v = 2" writes one, and neither copies nor moves what
      // "b" points at.
      meta->AllowMoveDeref = meta->AllowMoveDeref
        or Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr;

      // Standard analysis of the lhs (which is not a type), and
      // checking that the lhs is a valid form of primary expression.
      Lhs->Stage7_AnalyseSemantics(sm, meta);
      RaiseIf<SppInvalidPrimaryExpressionError>(
        not expr_utils::IsPrimaryExprTypeValid(*Lhs, *sm, {.AllowTypeAst = true}),
        {sm->CurrentScope}, ERR_ARGS(*Lhs.get()));
    }
  }

  // Re-attach the meta info, as it is targeting the lhs.
  const auto _meta_guard = MetaGuard(meta);
  meta->PostfixExpressionLhs = Lhs.get();
  Op->Stage7_AnalyseSemantics(sm, meta);

  // Use the hook to record information for the resolution and
  // completion plugin.
  {
    const auto produces_value =
     Op->To<PostfixExpressionOperatorFunctionCallAst>() != nullptr or
     Op->To<PostfixExpressionOperatorRuntimeMemberAccessAst>() != nullptr;

    if (produces_value) {
      lsp::resolution_index::RecordExpression(
        *this, *sm, *meta, [this, sm, meta] { return InferType(sm, meta); });
    }
  }
}

auto PostfixExpressionAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // A method call is checked in its function form, where the receiver is the "self" argument, so the lhs is not also
  // checked as a use of its own.
  const auto func = Op->To<PostfixExpressionOperatorFunctionCallAst>();
  if (const auto fn_lhs = func != nullptr ? func->GetTransformedLhs() : nullptr; fn_lhs != nullptr) {
    fn_lhs->Stage8_CheckMemory(sm, meta);
    const auto _meta_guard = MetaGuard(meta);
    meta->PostfixExpressionLhs = fn_lhs;
    Op->Stage8_CheckMemory(sm, meta);
    return;
  }

  if (Op->To<PostfixExpressionOperatorEarlyReturnAst>() != nullptr) {
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->PostfixExpressionLhs = Lhs.get();
      Op->Stage8_CheckMemory(sm, meta);
    }
    return;
  }

  // Index and slice operators desugar to borrow-based method calls (index_ref/mut, slice_ref/mut). Route memory
  // checking through their mapped function like a normal method call, rather than treating the identifier lhs as a
  // moved value here (which the desugared "&self"/"&mut self" call handles correctly).
  if (Op->To<PostfixExpressionOperatorIndexAst>() != nullptr or Op->To<PostfixExpressionOperatorSliceAst>() !=
    nullptr) {
    Op->Stage8_CheckMemory(sm, meta);
    return;
  }

  // A dereference copies out of the borrow its lhs produced, so the value this expression is being assigned to holds
  // a copy and not the borrow. Any escaping borrow the lhs establishes on the way ("xs[i]@" resolving through the
  // "index_ref" coroutine) belongs to that temporary, not to the assignment target, so clear the target while the lhs
  // is checked - otherwise the target is recorded as containing a borrow it never receives, and nothing ever releases
  // it.
  const auto saved_assignment_target = meta->AssignmentTarget;
  if (Op->To<PostfixExpressionOperatorDerefAst>() != nullptr) { meta->AssignmentTarget = nullptr; }
  if (Lhs != nullptr) { Lhs->Stage8_CheckMemory(sm, meta); }
  meta->AssignmentTarget = saved_assignment_target;

  const auto _meta_guard = MetaGuard(meta);
  meta->PostfixExpressionLhs = Lhs.get();
  if (Lhs->To<IdentifierAst>() != nullptr) {
    // Validate the receiver is usable (not moved-out / inconsistent) before applying the operator, but do not treat
    // it as a move: accessing a member/deref/etc reads or borrows the receiver, it never consumes it.
    mem_utils::ValidateSymbolMemory(*meta->PostfixExpressionLhs, *Op, *sm, false, false, false, false, meta);
  }
  Op->Stage8_CheckMemory(sm, meta);
}

auto PostfixExpressionAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Forward into the operator AST.
  const auto _meta_guard = MetaGuard(meta);
  meta->PostfixExpressionLhs = Lhs.get();
  Op->Stage9_CompTimeResolve(sm, meta);
}

auto PostfixExpressionAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // A method call is generated in its function form ("Type::m(obj)").
  const auto func = Op->To<PostfixExpressionOperatorFunctionCallAst>();
  const auto fn_lhs = func != nullptr ? func->GetTransformedLhs() : nullptr;

  // Forward into the operator AST.
  const auto _meta_guard = MetaGuard(meta);
  meta->PostfixExpressionLhs = fn_lhs != nullptr ? static_cast<ExpressionAst*>(fn_lhs) : Lhs.get();
  const auto ret_val = Op->Stage11_CodeGen(sm, meta, ctx);
  return ret_val;
}

auto PostfixExpressionAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Check cache.
  // if (Source.CachedInference != nullptr) { return Source.CachedInference; }

  // Forward into the operator AST.
  const auto _meta_guard = MetaGuard(meta);
  meta->PostfixExpressionLhs = Lhs.get();
  auto x = Op->InferType(sm, meta);
  return x;
}

auto PostfixExpressionAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  const auto _meta_guard = MetaGuard(meta);
  meta->PostfixExpressionLhs = Lhs.get();
  return Op->InferTypeRef(sm, meta);
}

auto PostfixExpressionAst::ExprParts() const -> Vec<IdentifierAst*> {
  // Recursively search the lhs, and add the rhs if it
  // exists.
  auto lhs_parts = Lhs->ExprParts();
  auto rhs_parts = Op->ExprParts();
  if (not rhs_parts.IsEmpty()) {
    lhs_parts.AppendRange(std::move(rhs_parts));
  }
  return lhs_parts;
}

auto PostfixExpressionAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &args) const -> Shared<ExpressionAst> {
  // The left-hand side is where a type is written - the
  // "A" of "A::new()", the "Self" of "Self::mo_seq_cst" -
  // and the operator carries whatever a call, an index or
  // a slice was given.
  return MakeShared<PostfixExpressionAst>(
    AstClone(Lhs->SubstituteGenericsExpr(args)),
    Op->SubstituteGenericsExpr(args));
}

auto PostfixExpressionAst::IsAllowedInDefault() const -> bool {
  // Check both the lhs and the postfix op on this ast.
  // Leaving the nullptr guards in because there is some
  // std::move(ast) for postfix iirc.
  return
    (Lhs == nullptr or Lhs->IsAllowedInDefault()) and
    (Op == nullptr or Op->IsAllowedInDefault());
}

SPP_MOD_END
