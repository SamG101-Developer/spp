module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_await_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorKeywordAwaitAst::PostfixExpressionOperatorKeywordAwaitAst(
  decltype(TokDot) &&tok_dot,
  decltype(TokAwait) &&tok_await) :
  TokDot(std::move(tok_dot)),
  TokAwait(std::move(tok_await)),
  _MappedFunc(nullptr) {
}

spp::asts::PostfixExpressionOperatorKeywordAwaitAst::~PostfixExpressionOperatorKeywordAwaitAst() = default;

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::PosStart() const
  -> std::size_t {
  // Use the "." token.
  return TokDot != nullptr
    ? TokDot->PosStart()
    : 0uz;
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::PosEnd() const
  -> std::size_t {
  // Use the "await" token.
  return TokAwait != nullptr
    ? TokAwait->PosEnd()
    : 0uz;
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto ast = MakeUnique<PostfixExpressionOperatorKeywordAwaitAst>(
    AstClone(TokDot),
    AstClone(TokAwait));
  ast->_MappedFunc = _MappedFunc;
  return ast;
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW(".");
  SPP_STRING_APPEND_RAW("await");
  SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppAwaitTargetNotFutureError;
  using analyse::utils::type_compare::TypeEq;
  using generate::common_types_precompiled::FUT;

  // Already analysed => return early.
  if (_MappedFunc != nullptr) { return; }

  // Check the lhs is the future type. This is the only
  // type that can be "awaited" on.
  const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
  RaiseIf<SppAwaitTargetNotFutureError>(
    not TypeEq(*lhs_type->WithoutGenerics(), *FUT->WithoutGenerics(), *sm->CurrentScope, *sm->CurrentScope),
    {sm->CurrentScope}, ERR_ARGS(*TokAwait, *meta->PostfixExpressionLhs, *lhs_type));

  // Map to the private "await_" method, so the wait
  // itself stays written in s++ rather than being built
  // here.
  auto name = MakeUnique<IdentifierAst>(PosStart(), "await_");
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(
    nullptr, std::move(name));
  auto member_access = MakeUnique<PostfixExpressionAst>(
    AstClone(meta->PostfixExpressionLhs), std::move(field));

  // Convert the "await_" field into a function call by
  // applying the "()"; there are no arguments that this
  // function can receive.
  auto arg_group = MakeUnique<FunctionCallArgumentGroupAst>(
    nullptr, Vec<Unique<FunctionCallArgumentAst>>(), nullptr);
  auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, std::move(arg_group), nullptr);
  func_call->Source.OriginalExpr = this;

  // Analyse the function call to the "await_" method,
  // temporarily ignoring access modifiers because that
  // method is private.
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->IgnoreAccessModifierViolations = true;
  _MappedFunc = MakeUnique<PostfixExpressionAst>(
    std::move(member_access), std::move(func_call));
  _MappedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Release what the future was keeping pinned, freeing up
  // any escaping borrows. Todo: Maybe move into mem_utils?
  if (const auto lhs = meta->PostfixExpressionLhs->To<IdentifierAst>(); lhs != nullptr) {
    if (const auto sym = sm->CurrentScope->GetVarSymbolOutermost(*lhs).first; sym != nullptr) {
      const auto contained = sym->MemInfo->AstContainedEscapingBorrows;
      for (auto const &ceb : contained) {
        sym->MemInfo->AstContainedEscapingBorrows |= genex::actions::remove(ceb);
        const auto borrowed = sm->CurrentScope->GetVarSymbolOutermost(*spp::get<0>(ceb)).first;
        if (borrowed == nullptr) { continue; }
        borrowed->MemInfo->AstContainersOfEscapingBorrows |= genex::actions::remove_if(
          [&](auto info) {
            const auto container = spp::get<0>(info)->template To<IdentifierAst>();
            return container != nullptr and *container == *sym->Name;
          });
      }
    }
  }

  // Call the memory check on the mapped function. The borrows
  // are managed beforehand to stay uniform with standard mem
  // analysis rules, where async needs a bypass (sneaky hack).
  const auto _meta_guard = meta::MetaGuard(meta);
  meta->IgnoreAccessModifierViolations = true;
  _MappedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Nothing of its own to emit: the wait is whatever "await_"
  // compiled to.
  return _MappedFunc->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::InferType(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // The type wrapped inside the "Fut[T]" object being awaited
  // on: "T".
  return _MappedFunc->InferType(sm, meta);
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::SubstituteGenericsExpr(
  Vec<GenericArgumentAst*> const &) const
  -> Unique<PostfixExpressionOperatorAst> {
  // Nothing inside the operator is an expression, so there is
  // nothing to substitute into.
  return MakeUnique<PostfixExpressionOperatorKeywordAwaitAst>(
    AstClone(TokDot), AstClone(TokAwait));
}

auto spp::asts::PostfixExpressionOperatorKeywordAwaitAst::IsAllowedInDefault() const
  -> bool {
  // Waits on a future the way a call waits on its callee,
  // so nothing leaves the code it is in.
  return true;
}

SPP_MOD_END
