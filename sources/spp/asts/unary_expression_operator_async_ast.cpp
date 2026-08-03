module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_operator_async_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.utils.uid;

SPP_MOD_BEGIN
spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
  decltype(TokAsync) &&tok_async) :
  TokAsync(std::move(tok_async)) {
}

spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;

auto spp::asts::UnaryExpressionOperatorAsyncAst::PosStart() const
  -> std::size_t {
  // Use the "async" token.
  return TokAsync->PosStart();
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::PosEnd() const
  -> std::size_t {
  // Use the "async" token.
  return TokAsync->PosEnd();
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<UnaryExpressionOperatorAsyncAst>(
    AstClone(TokAsync));
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::ToString() const
  -> Str {
  SPP_STRING_START;
  if (_TransformedFunc != nullptr) {
    SPP_STRING_APPEND(_TransformedFunc);
    SPP_STRING_END;
  }
  SPP_STRING_APPEND(TokAsync).append(" ");
  SPP_STRING_END;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppAsyncTargetNotFunctionCallError;

  // Check the right-hand-side is a function call expression.
  const auto rhs = meta->UnaryExpressionRhs->To<PostfixExpressionAst>();
  auto rhs_fn_call = rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>();
  RaiseIf<SppAsyncTargetNotFunctionCallError>(
    rhs == nullptr or rhs_fn_call == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*TokAsync, *meta->UnaryExpressionRhs));

  // Mark the function call as async.
  rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>()->MarkAsAsync(this);

  // Construct the "sppc::async" namespaced identifier, preparing
  // for the initial lowered function call.
  auto fut_type = AstClone(InferType(sm, meta));
  auto method_name = MakeUnique<IdentifierAst>(0uz, "async_");
  auto static_member = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
    nullptr, std::move(method_name));
  auto pf = MakeUnique<PostfixExpressionAst>(
    std::move(fut_type), std::move(static_member));

  // Move the arguments over from the function call to the lowered
  // function call, inserting the target function as the first
  // argument, like "sppc::async(function_target, 1, 2, 3)".
  auto provided_args = rhs_fn_call->FnArgGroup->ConvertToPositional();
  auto provided_target = std::move(rhs->Lhs);
  auto arg = MakeUnique<FunctionCallArgumentPositionalAst>(
    nullptr, nullptr, std::move(provided_target));
  provided_args->Args.Insert(provided_args->Args.begin(), std::move(arg));
  auto fn = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, std::move(provided_args), nullptr);
  auto mapped = MakeUnique<PostfixExpressionAst>(std::move(pf), std::move(fn));

  // Analyse the object initializer for safety in codegen, and ensure
  // that all private fields are generated.
  _TransformedFunc = std::move(mapped);
  // TODO : _TransformedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  // Generate the mapped object initialization, which handles the sppc
  // lowering.
  const auto value = _TransformedFunc->Stage11_CodeGen(sm, meta, ctx);
  return value;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  //
  using generate::common_types::FutureType;

  // Wrap the function call inside a "Future" type.
  auto inner_type = meta->UnaryExpressionRhs->InferType(sm, meta);
  auto future_type = FutureType(
    TokAsync->PosStart(), std::move(inner_type));
  future_type->Stage7_AnalyseSemantics(sm, meta);
  return future_type;
}

SPP_MOD_END
