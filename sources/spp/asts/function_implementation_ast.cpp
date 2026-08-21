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
auto spp::asts::FunctionImplementationAst::NewEmpty()
  -> Unique<FunctionImplementationAst> {
  return MakeUnique<FunctionImplementationAst>(nullptr, decltype(Members)(), nullptr);
}

spp::asts::FunctionImplementationAst::~FunctionImplementationAst() = default;

auto spp::asts::FunctionImplementationAst::Clone() const
  -> Unique<Ast> {
  auto ast = MakeUnique<FunctionImplementationAst>(
    AstClone(TokL),
    AstCloneVec(Members),
    AstClone(TokR));
  return ast;
}

auto spp::asts::FunctionImplementationAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  // A function has one scope, shared by every call to it, so the values this call writes into that scope's symbols
  // are the caller's values as far as an enclosing call is concerned. Take them out for the duration of the call and
  // put them back on the way out, or a recursive call returns having overwritten the parameters and locals its caller
  // was still working with - and the recursion never converges. Moving them out is also what leaves this call's
  // locals unassigned, which is what entering a call should do.
  auto caller_values = Vec<Pair<analyse::scopes::VariableSymbol*, Unique<Ast>>>();
  const auto take_values = [&caller_values](auto const &self, analyse::scopes::Scope const &scope) -> void {
    for (auto *sym : scope.AllVarSymbols(true)) {
      caller_values.EmplaceBack(sym, std::move(sym->CompTimeValue));
    }
    for (auto const &child : scope.Children) { self(self, *child); }
  };
  take_values(take_values, *sm->CurrentScope);

  // Inject the argument values. Todo: && & std::move?
  // A parameter resolves through the scope chain, so it can sit above the body's own scope and not be covered above.
  for (auto const &[arg_name, arg_comp] : meta->CmpArgs) {
    const auto arg_sym = sm->CurrentScope->GetVarSymbol(arg_name.get());
    caller_values.EmplaceBack(arg_sym.get(), std::move(arg_sym->CompTimeValue));
    arg_sym->CompTimeValue = AstClone(arg_comp);
  }

  // Comptime resolve each member of the inner scope. The call is its own frame: the caller is mid-statement, so its
  // "returned" state has to survive this one rather than be inherited by it.
  const auto caller_returned = meta->CmpReturned;
  meta->CmpReturned = false;
  for (auto const &member : this->Members) {
    member->Stage9_CompTimeResolve(sm, meta);
    if (meta->CmpReturned) { break; }
  }
  meta->CmpReturned = caller_returned;

  // Hand the scope back to the caller as it was. A semantic error thrown out of the body skips this, which is fine:
  // nothing catches a comp-time error, so the compile is over either way.
  for (auto &&[sym, value] : caller_values) { sym->CompTimeValue = std::move(value); }
}

SPP_MOD_END
