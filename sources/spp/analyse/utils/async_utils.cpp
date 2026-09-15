module spp.analyse.utils.async_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.asts.closure_expression_capture_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.statement_ast;
import spp.asts.utils.ast_utils;
import spp.utils.types;
import std;

namespace spp::analyse::utils::async_utils {
  namespace {
    // How strongly a capture uses its variable: a move outranks
    // "&mut", which outranks "&".
    auto ConvRank(asts::ConventionAst const *conv) -> std::uint8_t {
      return conv == nullptr ? 2 : *conv == asts::ConventionTag::MUT ? 1 : 0;
    }
  }

  auto CaptureOnce(
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    Unique<asts::IdentifierAst> &&name,
    Unique<asts::ConventionAst> &&conv)
    -> void {
    // Strengthen an existing capture of the same variable, rather
    // than capturing it twice.
    for (auto const &cap : captures) {
      if (cap->Val->ToUnchecked<asts::IdentifierAst>()->Val != name->Val) { continue; }
      if (ConvRank(conv.get()) > ConvRank(cap->Conv.get())) { cap->Conv = std::move(conv); }
      return;
    }
    captures.EmplaceBack(MakeUnique<asts::ClosureExpressionCaptureAst>(std::move(conv), std::move(name)));
  }

  auto CaptureBorrow(
    Unique<asts::ExpressionAst> &place,
    asts::ConventionAst const &conv,
    scopes::Scope const &scope,
    Vec<Unique<asts::StatementAst>> &prelude,
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    const std::size_t pos)
    -> void {
    // A temporary has no variable, so the closure owns it and
    // borrows that.
    const auto sym = scope.GetVarSymbolOutermost(*place).first;
    if (sym == nullptr) {
      CaptureOnce(captures, asts::BindLocal(place, prelude, pos), nullptr);
      return;
    }

    // A static member resolves on its own. Otherwise, capture
    // the outermost variable under the borrow's convention.
    if (scope.GetVarSymbol(sym->Name.get()) != sym) { return; }
    CaptureOnce(
      captures, MakeUnique<asts::IdentifierAst>(place->PosStart(), Str(sym->Name->Val)), asts::AstClone(&conv));
  }

  auto CaptureReceiver(
    asts::PostfixExpressionAst &path,
    asts::FunctionPrototypeAst const *target,
    scopes::Scope const &scope,
    Vec<Unique<asts::StatementAst>> &prelude,
    Vec<Unique<asts::ClosureExpressionCaptureAst>> &captures,
    const std::size_t pos)
    -> void {
    // "&self" and "&mut self" borrow the receiver.
    const auto self_param = target != nullptr ? target->FnParamGroup->GetSelfParam() : nullptr;
    if (self_param != nullptr and self_param->Conv != nullptr) {
      CaptureBorrow(path.Lhs, *self_param->Conv, scope, prelude, captures, pos);
      return;
    }

    // "self" moves it. A static member needs nothing, a bare
    // name is captured, and anything else is bound.
    const auto sym = scope.GetVarSymbolOutermost(*path.Lhs).first;
    if (sym != nullptr and scope.GetVarSymbol(sym->Name.get()) != sym) { return; }
    if (const auto ident = path.Lhs->To<asts::IdentifierAst>(); ident != nullptr) {
      CaptureOnce(captures, asts::AstClone(ident), nullptr);
      return;
    }
    CaptureOnce(captures, asts::BindLocal(path.Lhs, prelude, pos), nullptr);
  }
}
