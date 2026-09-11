module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_operator_async_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.async_utils;
import spp.asts.closure_expression_ast;
import spp.asts.closure_expression_capture_ast;
import spp.asts.closure_expression_capture_group_ast;
import spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.convention_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.literal_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.object_initializer_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_type;
import spp.lex.lexer;
import spp.lex.tokens;
import spp.parse.parser_spp;

SPP_MOD_BEGIN
spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
  decltype(TokAsync) &&tok_async) :
  TokAsync(std::move(tok_async)),
  _TransformedFunc(nullptr) {
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
  auto ast = MakeUnique<UnaryExpressionOperatorAsyncAst>(
    AstClone(TokAsync));
  ast->_TransformedFunc = AstClone(_TransformedFunc);
  return ast;
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
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppAsyncTargetNotFunctionCallError;
  using analyse::utils::async_utils::CaptureBorrow;
  using analyse::utils::async_utils::CaptureOnce;
  using analyse::utils::async_utils::CaptureReceiver;

  // Check that the right-hand-side to the "async" keyword is
  // a function call ast. This blocks things like "async 123"
  // before any downstream errors occur.
  const auto rhs = meta->UnaryExpressionRhs->To<PostfixExpressionAst>();
  const auto rhs_fn_call = rhs != nullptr
    ? rhs->Op->To<PostfixExpressionOperatorFunctionCallAst>()
    : nullptr;

  RaiseIf<SppAsyncTargetNotFunctionCallError>(
    rhs_fn_call == nullptr,
    {sm->CurrentScope}, ERR_ARGS(*TokAsync, *meta->UnaryExpressionRhs));
  rhs_fn_call->MarkAsAsync(this);

  // "async f(a, b)" becomes "Fut[T]::spawn(() f(a, b))".

  // Save the future type that will be generated wrapping the
  // target's type inside.
  auto fut_type = AstClone(InferType(sm, meta));

  // Cast out the rhs into the function call asts, to begin
  // the closure transformation procedure.
  SPP_ASSERT(Source._OriginalRhs != nullptr);
  auto inner_call = std::move(Source._OriginalRhs);
  const auto pristine = inner_call->ToUnchecked<PostfixExpressionAst>();
  const auto pristine_call = pristine->Op->ToUnchecked<PostfixExpressionOperatorFunctionCallAst>();

  // Temp helper methods until TokenAst is properly refactored
  // to handle stringification properly.
  const auto pos = TokAsync->PosStart();
  const auto tok = [pos](const lex::SppTokenType type) {
    return TokenAst::NewEmpty(type, lex::tok_to_string(type), pos);
  };

  // The prelude holds the "let" bindings made before the closure,
  // and the captures are what the closure takes from this frame.
  // A variable is captured once however many parts of the call
  // use it.
  auto prelude = Vec<Unique<StatementAst>>();
  auto captures = Vec<Unique<ClosureExpressionCaptureAst>>();
  auto const &scope = *sm->CurrentScope;

  // If the call target is an identifier, ie "async hello()",
  // then add the "hello" symbol into the captures, by move -
  // but only when it names something in this frame ie a closure.
  // A module function needs nothing: it is a compile-time
  // constant the body resolves on its own, and capturing it
  // would consume a global.
  // Todo: Can we borrow here so no extra check needed? Simpler.
  if (const auto target = pristine->Lhs->To<IdentifierAst>(); target != nullptr) {
    const auto sym = scope.GetVarSymbol(target);
    if (sym != nullptr and sym->ScopeDefinedIn != scope.ParentModule()) {
      CaptureOnce(captures, AstClone(target), nullptr);
    }
  }

  // A runtime member access, "async a.b.c()" - a method on an
  // object - has a receiver, used the way the method's "self"
  // says - see "CaptureReceiver".
  else if (IsRuntimeMemberAccess(pristine->Lhs.get())) {
    CaptureReceiver(
      *pristine->Lhs->ToUnchecked<PostfixExpressionAst>(),
      rhs_fn_call->Target(), scope, prelude, captures, pos);
  }

  // Any other postfix target - a static path, "Type::f" - is left
  // for the body. Anything else is an expression that produces the
  // callable - "async (chooser())()" - so it is evaluated here into
  // a local of its own, and that local is captured.
  else if (pristine->Lhs->To<PostfixExpressionAst>() == nullptr) {
    CaptureOnce(
      captures,
      BindLocal(pristine->Lhs, prelude, pos), nullptr);
  }

  for (auto const &arg : pristine_call->FnArgGroup->Args) {
    // A borrow - see "CaptureBorrow".
    if (arg->Conv != nullptr) {
      CaptureBorrow(arg->Val, *arg->Conv, scope, prelude, captures, pos);
      continue;
    }

    // Pass literals in directly, no extra mapping needed
    // for these - they are temporary.
    if (arg->Val->To<LiteralAst>() != nullptr) { continue; }

    // A bare name is captured rather than bound, so that the
    // body reads the caller's own symbol - which is what makes
    // a moved argument report against the right variable.
    if (const auto ident = arg->Val->To<IdentifierAst>(); ident != nullptr) {
      CaptureOnce(captures, AstClone(ident), nullptr);
      continue;
    }

    // Anything else is bound to a local the closure owns.
    CaptureOnce(captures, BindLocal(arg->Val, prelude, pos), nullptr);
  }

  // Copy the async flag into the original function for
  // memory borrow rules to correctly apply.
  pristine_call->MarkAsAsync(this);

  // Create the closure, with its required parameters and
  // captures, and place the inner call into the closures
  // body. It'll look like "() inner_call(arg1)" - no {}.
  auto param_group = MakeUnique<FunctionParameterGroupAst>(
    tok(lex::SppTokenType::TK_LEFT_PARENTHESIS),
    Vec<Unique<FunctionParameterAst>>(),
    tok(lex::SppTokenType::TK_RIGHT_PARENTHESIS));
  auto capture_group = captures.IsEmpty()
    ? nullptr
    : MakeUnique<ClosureExpressionCaptureGroupAst>(
      tok(lex::SppTokenType::KW_CAPS), std::move(captures));
  auto pc_group = MakeUnique<ClosureExpressionParameterAndCaptureGroupAst>(
    tok(lex::SppTokenType::TK_LEFT_PARENTHESIS), std::move(param_group),
    std::move(capture_group), tok(lex::SppTokenType::TK_RIGHT_PARENTHESIS));
  auto closure = MakeUnique<ClosureExpressionAst>(
    nullptr, std::move(pc_group), nullptr, nullptr, std::move(inner_call));

  // "Fut[T]::spawn(<closure>)". Wrap the pre-generated closure
  // into the future's spawn method.
  auto method_name = MakeUnique<IdentifierAst>(0uz, "spawn");
  auto static_member = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(
    nullptr, std::move(method_name));
  auto pf = MakeUnique<PostfixExpressionAst>(
    std::move(fut_type), std::move(static_member));

  auto spawn_args = Vec<Unique<FunctionCallArgumentAst>>();
  spawn_args.EmplaceBack(MakeUnique<FunctionCallArgumentPositionalAst>(
    nullptr, nullptr, std::move(closure)));
  auto spawn_arg_group = MakeUnique<FunctionCallArgumentGroupAst>(
    tok(lex::SppTokenType::TK_LEFT_PARENTHESIS),
    std::move(spawn_args),
    tok(lex::SppTokenType::TK_RIGHT_PARENTHESIS));
  auto fn = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(
    nullptr, std::move(spawn_arg_group), nullptr);
  auto mapped = MakeUnique<PostfixExpressionAst>(
    std::move(pf), std::move(fn));

  // The prelude and the call are one scope: the locals are bound,
  // then the future is built from them, and the scope's value is
  // the future.
  if (prelude.IsEmpty()) {
    _TransformedFunc = std::move(mapped);
  }
  else {
    prelude.EmplaceBack(std::move(mapped));
    _TransformedFunc = MakeUnique<InnerScopeExpressionAst>(
      tok(lex::SppTokenType::TK_LEFT_CURLY_BRACE), std::move(prelude),
      tok(lex::SppTokenType::TK_RIGHT_CURLY_BRACE));
  }

  // Analysed here so that codegen has a fully resolved call to
  // emit.
  _TransformedFunc->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage8_CheckMemory(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> void {
  // Failsafe - Todo: is this ever hittable? Not sure if it is
  // needed
  if (_TransformedFunc == nullptr) {
    meta->UnaryExpressionRhs->Stage8_CheckMemory(sm, meta);
    return;
  }

  // Map the analysis to the inner transformation ast - the mapped
  // closure.
  _TransformedFunc->Stage8_CheckMemory(sm, meta);
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Generate the mapped object initialization, which handles the
  // sppc lowering.
  const auto value = _TransformedFunc->Stage11_CodeGen(sm, meta, ctx);
  return value;
}

auto spp::asts::UnaryExpressionOperatorAsyncAst::InferType(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta)
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

auto spp::asts::UnaryExpressionOperatorAsyncAst::IsAllowedInDefault() const
  -> bool {
  // Lowers into a closure => not allowed as a default.
  return false;
}

SPP_MOD_END
