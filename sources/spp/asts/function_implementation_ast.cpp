module;
#include <spp/macros.hpp>

module spp.asts.function_implementation_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
auto FunctionImplementationAst::NewEmpty() -> Unique<FunctionImplementationAst> {
  return MakeUnique<FunctionImplementationAst>(nullptr, decltype(Members)(), nullptr);
}

FunctionImplementationAst::~FunctionImplementationAst() = default;

auto FunctionImplementationAst::Clone() const -> Unique<Ast> {
  auto ast = MakeUnique<FunctionImplementationAst>(
    AstClone(TokL),
    AstCloneVec(Members),
    AstClone(TokR));
  return ast;
}

auto FunctionImplementationAst::DiscardsFinalMember() const -> bool {
  // Values leave a function through "ret", so the final statement
  // of a body is discarded like every other statement in it.
  return true;
}

auto FunctionImplementationAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // A function has one scope, shared by every call to it, so the values this call writes into that scope's symbols
  // are the caller's values as far as an enclosing call is concerned. Take them out for the duration of the call and
  // put them back on the way out, or a recursive call returns having overwritten the parameters and locals its caller
  // was still working with - and the recursion never converges. Moving them out is also what leaves this call's
  // locals unassigned, which is what entering a call should do.
  auto caller_values = Vec<Pair<VariableSymbol*, Unique<Ast>>>();
  const auto take_values = [&caller_values](auto const &self, Scope const &scope) -> void {
    for (auto *sym : scope.GetAllVarSymbols(true)) {
      caller_values.emplace_back(sym, std::move(sym->CompTimeValue));
    }
    for (auto const &child : scope.Children) { self(self, *child); }
  };
  take_values(take_values, *sm->CurrentScope);

  // The body is walked as a call enters it, not as Stage 7 left its scope: the bindings written in it are hidden
  // until each is reached, so a name means what is written before it - the parameter "x", not the "x" an "is" further
  // down narrows - and the argument goes to the parameter. Revealed again on the way out, as leaving a scope does.
  sm->CurrentScope->HideLetBindings();

  // Inject the argument values. Todo: && & std::move?
  // A parameter resolves through the scope chain, so it can sit above the body's own scope and not be covered above.
  for (auto const &[arg_name, arg_comp] : meta->CompTimeCall.FnArgs) {
    const auto arg_sym = sm->CurrentScope->FindVarSymbol(arg_name.get());
    caller_values.emplace_back(arg_sym, std::move(arg_sym->CompTimeValue));
    arg_sym->CompTimeValue = AstClone(arg_comp);
  }

  // And the comp generics the call bound ("n" for "get[5]()"):
  // the template's body is what runs, so its comp parameters
  // are given their values for the call, as its parameters are.
  for (auto const &[gn_name, gn_value] : meta->CompTimeCall.GnCompValues) {
    const auto gn_sym = sm->CurrentScope->FindVarSymbol(gn_name.get());
    if (gn_sym == nullptr or gn_sym->Kind != VariableKind::GnCompParam) { continue; }
    caller_values.emplace_back(gn_sym, std::move(gn_sym->CompTimeValue));
    gn_sym->CompTimeValue = sm->CurrentScope->CompAstOf(gn_value);
  }

  // Comptime resolve each member of the inner scope. The call is its own frame: the caller is mid-statement, so its
  // "returned" state has to survive this one rather than be inherited by it.
  const auto caller_returned = meta->CompTimeReturned;
  meta->CompTimeReturned = false;
  for (auto const &member : this->Members) {
    member->EvaluateCompTime(sm, meta);
    if (meta->CompTimeReturned) { break; }
  }
  meta->CompTimeReturned = caller_returned;

  // Each parameter's final value, for the call to write back
  // to the place a "&mut" argument came from.
  for (auto &[arg_name, arg_comp] : meta->CompTimeCall.FnArgs) {
    const auto arg_sym = sm->CurrentScope->FindVarSymbol(arg_name.get());
    const auto value = arg_sym->CompTimeValue != nullptr
      ? arg_sym->CompTimeValue->To<ExpressionAst>()
      : nullptr;

    if (value != nullptr) {
      arg_comp = AstClone(value);
    }
  }

  // Hand the scope back to the caller as it was. A semantic error thrown out of the body skips this, which is fine:
  // nothing catches a comp-time error, so the compile is over either way.
  sm->CurrentScope->RevealLetBindings();
  for (auto &&[sym, value] : caller_values) { sym->CompTimeValue = std::move(value); }
}

SPP_MOD_END
