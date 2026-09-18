module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_parameter_group_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.order_utils;
import spp.analyse.utils.type_utils;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.orderable;
import spp.lex.tokens;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
auto GenericParameterGroupAst::NewEmpty() -> Unique<GenericParameterGroupAst> {
  return MakeUnique<GenericParameterGroupAst>(
    nullptr, decltype(Params)(), nullptr);
}

auto GenericParameterGroupAst::NewEmptyShared() -> Shared<GenericParameterGroupAst> {
  return MakeShared<GenericParameterGroupAst>(
    nullptr, decltype(Params)(), nullptr);
}

GenericParameterGroupAst::GenericParameterGroupAst(
  decltype(TokL) &&tok_l,
  decltype(Params) &&params,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  Params(std::move(params)),
  TokR(std::move(tok_r)) {
}

GenericParameterGroupAst::~GenericParameterGroupAst() = default;

auto GenericParameterGroupAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL != nullptr ? TokL->PosStart() : Params.IsEmpty() ? 0 : Params.Front()->PosStart();
}

auto GenericParameterGroupAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR != nullptr ? TokR->PosEnd() : Params.IsEmpty() ? 0 : Params.Back()->PosEnd();
}

auto GenericParameterGroupAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<GenericParameterGroupAst>(
    AstClone(TokL),
    AstCloneVec(Params),
    AstClone(TokR));
}

auto GenericParameterGroupAst::ToString() const -> Str {
  SPP_STRING_START;
  if (not Params.IsEmpty()) {
    SPP_STRING_APPEND_RAW("[");
    SPP_STRING_EXTEND(Params, ", ");
    SPP_STRING_APPEND_RAW("]");
  }
  SPP_STRING_END;
}

auto GenericParameterGroupAst::GetOptionalParams() const -> Vec<GenericParameterAst*> {
  // Filter by the order tag.
  using utils::OrderableTag::kOptionalParam;
  return Params
    | genex::views::filter([](auto const &param) { return param->GetOrderTag() == kOptionalParam; })
    | genex::views::transform([](auto const &param) { return param.get(); })
    | genex::to<Vec>();
}

auto GenericParameterGroupAst::GetVariadicParams() const -> GenericParameterAst* {
  // The first (and only valid) variadic parameter, by the
  // order tag.
  for (auto const &p : Params) {
    if (p->GetOrderTag() == utils::OrderableTag::kVariadicParam) { return p.get(); }
  }
  return nullptr;
}

auto GenericParameterGroupAst::GetCompParams() const -> Vec<GenericParameterAst*> {
  // Filter by the kind of parameter.
  return Params
    | genex::views::filter([](auto const &param) { return param->CompType != nullptr; })
    | genex::views::transform([](auto const &param) { return param.get(); })
    | genex::to<Vec>();
}

auto GenericParameterGroupAst::GetTypeParams() const -> Vec<GenericParameterAst*> {
  // Filter by the kind of parameter.
  return Params
    | genex::views::filter([](auto const &param) { return param->CompType == nullptr; })
    | genex::views::transform([](auto const &param) { return param.get(); })
    | genex::to<Vec>();
}

auto GenericParameterGroupAst::GetAllParams() const -> Vec<GenericParameterAst*> {
  // Return all parameters.
  return Params
    | genex::views::transform([](auto const &param) { return param.get(); })
    | genex::to<Vec>();
}

auto GenericParameterGroupAst::OptToReq() const -> Unique<GenericParameterGroupAst> {
  // Convert all optional parameters to required parameters.
  auto new_params = Vec<Unique<GenericParameterAst>>();
  for (auto const &p : Params) {
    if (p->TypeDefault != nullptr or p->CompDefault != nullptr) {
      new_params.EmplaceBack(MakeUnique<GenericParameterAst>(
        nullptr, nullptr, AstClone(p->Name), AstClone(p->Constraints),
        nullptr, AstClone(p->CompType), nullptr, nullptr, nullptr));
    }
    else {
      new_params.EmplaceBack(AstClone(p));
    }
  }

  return MakeUnique<GenericParameterGroupAst>(
    nullptr, std::move(new_params), nullptr);
}

auto GenericParameterGroupAst::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::errors::SppIdentifierDuplicateError;

  // Checked here rather than at stage 7, where the rest of
  // this group's validation lives, because the parameters
  // register their symbols in the loop below. Two parameters
  // sharing a name register twice, and analysis then carries
  // on for another five stages over a scope whose symbol
  // table already disagrees with the source - which surfaced
  // as an unrelated failure in whichever file was analysed
  // next, rather than as the duplicate that caused it.
  const auto duplicate_names = Params
    | genex::views::transform([](auto const &x) { return x->Name.get(); })
    | genex::to<Vec>()
    | genex::views::duplicates({}, genex::meta::deref)
    | genex::to<Vec>();

  RaiseIf<SppIdentifierDuplicateError>(
    not duplicate_names.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(*duplicate_names[0], *duplicate_names[1], "generic parameter"));

  // Run the generation steps on the parameters in the group.
  for (auto const &p : Params) { p->Stage2_GenTopLvlScopes(sm, meta); }
}

auto GenericParameterGroupAst::Stage4_ResolveDeclarations(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Run the type qualifier steps on each parameter in the group.
  for (auto const &p : Params) { p->Stage4_ResolveDeclarations(sm, meta); }

  // Do the constraints after all the parameters are qualified.
  // This is because of external generic symbols using unqualified
  // types when analysing generically substituted constraint types.
  // An inherited parameter's constraints were copied from its "sup"
  // block before they were qualified there, so they are qualified
  // here too; it declares no symbol, so has nothing to attach to.
  for (auto const &p : GetTypeParams()) {
    p->Constraints->Stage4_ResolveDeclarations(sm, meta);
    if (p->IsInherited) { continue; }

    // Attach the scopes of the constraint types as sup-scopes to the generic scope.
    for (auto const &constraint : p->Constraints->Constraints) {
      const auto constraint_sym = sm->CurrentScope->GetTypeSymbol(constraint.get());
      for (auto const &dummy_scope : p->GetDummyScopes()) {
        BumpTypeStructureGeneration();
        dummy_scope->DirectSupScopes.EmplaceBack(constraint_sym->LinkedScope);
      }
    }

    const auto dummy_scopes = p->GetDummyScopes();
    dummy_scopes[0]->TySym->GenericConstraints = AstCloneVecShared(
      p->Constraints->Constraints);
  }
}

auto GenericParameterGroupAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::errors::SppOrderInvalidError;

  // Duplicate parameter names are caught at stage 2, before
  // the symbols are registered.
  const auto unordered_params = analyse::utils::order_utils::DoOrderParams(Params
    | genex::views::ptr
    | genex::views::cast_dynamic<mixins::OrderableAst*>()
    | genex::to<Vec>());

  // Mark copyable generics. An inherited one resolves to its
  // "sup" block's symbol, or in an instantiation of the block
  // to the bound argument's, which is not this group's to mark.
  for (auto const &p : GetTypeParams()) {
    if (p->IsInherited) { continue; }
    for (auto const &constraint : p->Constraints->Constraints) {
      const auto constraint_sym = sm->CurrentScope->GetTypeSymbol(constraint.get());
      if (constraint_sym->IsCopyable()) {
        const auto generic_sym = sm->CurrentScope->GetTypeSymbol(p->Name.get());
        generic_sym->IsDirectlyCopyable = true;
      }
    }
  }

  // Check the parameters are in the correct order.
  RaiseIf<SppOrderInvalidError>(
    not unordered_params.IsEmpty(), {sm->CurrentScope},
    ERR_ARGS(
      unordered_params[0].first, *unordered_params[0].second,
      unordered_params[1].first, *unordered_params[1].second));

  // Run the semantic analysis steps on each parameter in the group.
  for (auto const &p : Params) { p->Stage7_AnalyseSemantics(sm, meta); }
}

auto GenericParameterGroupAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Run the memory checks on each parameter in the group.
  for (auto const &p : Params) { p->Stage8_CheckMemory(sm, meta); }
}

auto GenericParameterGroupAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Fold each comp default, to prove it is a constant even if
  // nothing ever uses it. The result is thrown away - a use
  // site folds its own copy.
  for (auto const &p : Params) {
    if (p->CompDefault == nullptr) { continue; }
    auto tm = ScopeManager(sm->GlobalScope, sm->CurrentScope);
    tm.Reset(sm->CurrentScope);
    p->CompDefault->Stage9_CompTimeResolve(&tm, meta);
    meta->CmpResult = nullptr;
  }
}

auto GenericParameterGroupAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // Run the code generation steps on each parameter in the group.
  for (auto const &p : Params) { p->Stage11_CodeGen(sm, meta, ctx); }
  return nullptr;
}

SPP_MOD_END
