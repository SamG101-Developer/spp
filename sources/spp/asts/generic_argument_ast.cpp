module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_resolution;
import spp.asts.binary_expression_ast;
import spp.asts.convention_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
auto GenericArgumentAst::NewType(
  Shared<TypeAst> name, decltype(TypeVal) val) -> Unique<GenericArgumentAst> {
  // Build a new generic argument for a type argument.
  return MakeUnique<GenericArgumentAst>(std::move(name), nullptr, std::move(val), nullptr);
}

auto GenericArgumentAst::NewComp(
  Shared<TypeAst> name, decltype(CompVal) val) -> Unique<GenericArgumentAst> {
  // Build a new generic argument for a comp argument.
  return MakeUnique<GenericArgumentAst>(std::move(name), nullptr, nullptr, std::move(val));
}

auto GenericArgumentAst::FromSymbol(
  TypeSymbol const &sym) -> Unique<GenericArgumentAst> {
  IMPORT_UTILS;
  // A binding, as the type it is bound to: its scope's type, else the value recorded for it. Only a binding is an
  // argument ("Scope::GetGns" skips the rest).
  auto const *const linked = sym.LinkedSymbol();
  RaiseIf<SppInternalCompilerError>(
    linked == &sym and sym.BoundTypeVal == nullptr, {}, ERR_ARGS(*sym.Name, "Generic argument from an unbound symbol"));
  auto value = linked != &sym
    ? linked->FqName()->WithConvention(ConventionAstOf(sym.Convention))
    : AstCloneShared(sym.BoundTypeVal);
  return NewType(sym.Name, std::move(value));
}

auto GenericArgumentAst::FromSymbol(
  VariableSymbol const &sym) -> Unique<GenericArgumentAst> {
  IMPORT_UTILS;
  // "FromSymbol" for a comp binding, as the value it is bound to.
  auto const *const bound = sym.BoundCompVal();
  RaiseIf<SppInternalCompilerError>(bound == nullptr, {}, ERR_ARGS(*sym.Name, "Generic argument from an unbound symbol"));
  Shared<ExpressionAst> value = AstClone(bound);
  if (const auto value_as_type = value->To<TypeIdentifierAst>(); value_as_type != nullptr) {
    value = IdentifierAst::FromType(*value_as_type);
  }
  return NewComp(TypeIdentifierAst::FromIdentifier(*sym.Name), std::move(value));
}

GenericArgumentAst::GenericArgumentAst(
  Shared<TypeAst> name,
  decltype(TokAssign) &&tok_assign,
  decltype(TypeVal) type_val,
  decltype(CompVal) comp_val) :
  OrderableAst(name != nullptr ? utils::OrderableTag::kKeywordArg : utils::OrderableTag::kPositionalArg),
  TokAssign(std::move(tok_assign)),
  TypeVal(std::move(type_val)),
  CompVal(std::move(comp_val)),
  _KeywordName(std::move(name)) {
  if (_KeywordName != nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
  }
}

GenericArgumentAst::~GenericArgumentAst() = default;

auto GenericArgumentAst::PosStart() const -> std::size_t {
  // Use the name, or the value for a positional argument.
  return _KeywordName != nullptr ? _KeywordName->PosStart() : Value()->PosStart();
}

auto GenericArgumentAst::PosEnd() const -> std::size_t {
  // Use the value.
  return Value()->PosEnd();
}

auto GenericArgumentAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<GenericArgumentAst>(
    AstCloneShared(_KeywordName), AstClone(TokAssign), AstCloneShared(TypeVal), AstCloneShared(CompVal));
}

auto GenericArgumentAst::ToString() const -> Str {
  SPP_STRING_START;
  if (_KeywordName != nullptr) {
    SPP_STRING_APPEND(_KeywordName);
    SPP_STRING_APPEND(TokAssign);
  }
  if (IsTypeArg()) { SPP_STRING_APPEND(TypeVal); }
  else { SPP_STRING_APPEND(CompVal); }
  SPP_STRING_END;
}

auto GenericArgumentAst::operator==(
  GenericArgumentAst const &other) const -> bool {
  // Equal when given the same way (keyword or positional),
  // under the same name, with values of the same kind that
  // are equal.
  if ((_KeywordName == nullptr) != (other._KeywordName == nullptr)) { return false; }
  if (_KeywordName != nullptr and *_KeywordName != *other._KeywordName) { return false; }
  if (IsTypeArg() and other.IsTypeArg()) { return *TypeVal == *other.TypeVal; }
  return IsCompArg() and other.IsCompArg() and *CompVal == *other.CompVal;
}

auto GenericArgumentAst::IsTypeArg() const -> bool {
  return TypeVal != nullptr;
}

auto GenericArgumentAst::IsCompArg() const -> bool {
  return CompVal != nullptr;
}

auto GenericArgumentAst::KeywordName() const -> Shared<TypeAst> const& {
  return _KeywordName;
}

auto GenericArgumentAst::CompNameAsId() const -> Shared<IdentifierAst> const& {
  if (_CompNameAsId == nullptr and _KeywordName != nullptr) { _CompNameAsId = IdentifierAst::FromType(*_KeywordName); }
  return _CompNameAsId;
}

auto GenericArgumentAst::Value() const -> ExpressionAst* {
  return IsTypeArg() ? TypeVal.get() : CompVal.get();
}

auto GenericArgumentAst::ViewName() const -> StrView {
  // Get the name from the keyword part.
  if (_KeywordName == nullptr) { return ""; }
  return _KeywordName->ToUnchecked<TypeIdentifierAst>()->Name;
}

auto GenericArgumentAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Analysed by its kind of value.
  if (IsTypeArg()) { AnalyseTypeVal(sm, meta); }
  else { AnalyseCompVal(sm, meta); }
}

auto GenericArgumentAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  IMPORT_UTILS;

  // Ensure a comp value isn't moved or partially moved
  // (for all conventions). A type value holds no memory.
  if (not IsCompArg() or not IsCompValAnalysedInPlace(*sm->CurrentScope)) { return; }
  CompVal->Stage8_CheckMemory(sm, meta);
  mem_utils::ValidateSymbolMemory(
    *CompVal, *CompVal, *sm, meta);
}

auto GenericArgumentAst::AnalyseTypeVal(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // "Self" outside a function body is kept, for the caller
  // to decide per use; inside one it is the enclosing type.
  if (TypeVal->IsSelfType()
    and sm->CurrentScope->AstNode != nullptr
    and AstAs<InnerScopeExpressionAst>(sm->CurrentScope->AstNode) == nullptr) { return; }
  if (TypeVal->IsSelfType()) { TypeVal = sm->CurrentScope->FindEnclosingSelfType(*meta)->WithSourceSpanOf(*TypeVal); }
  TypeVal->Stage7_AnalyseSemantics(sm, meta);
  auto const &scope = *sm->CurrentScope;

  // An argument keeps its own node, recording what it names
  // here ("type_resolution::StampType"), so it is read by
  // identity rather than by spelling. One naming a generic
  // is not rewritten to a binding's value: that would re-bind
  // it with an instantiation's own parameters ("Single[Arr[T]]"
  // written inside "Single" never ends).
  if (auto const *const val_sym = scope.FindTypeSymbol(TypeVal.get()); val_sym != nullptr) {
    analyse::utils::type_resolution::StampType(*TypeVal, *val_sym);
  }
}

auto GenericArgumentAst::IsCompValAnalysedInPlace(
  Scope const &scope) const -> bool {
  namespace comp_generics = analyse::utils::comp_generics;
  if (comp_generics::IsCompOperator(*CompVal)) { return false; }
  return not comp_generics::NeedsSupScopesToType(*CompVal) or scope.FoldedCompAstOf(*CompVal) == nullptr;
}

auto GenericArgumentAst::AnalyseCompVal(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;

  // A comp expression ("n + 1") resolves its operator through
  // the sup scopes of its operand's type, which are only
  // attached once stage 5 ends, so it waits until then; a
  // literal or a name needs no sup scope.
  const auto needs_sup_scopes = comp_generics::NeedsSupScopesToType(*CompVal);
  if (needs_sup_scopes and meta->CurrentStage<CompilerStage::kPreAnalyseSemantics) { return; }

  // A comp expression that folds has been evaluated by the
  // comp-time intrinsics ("Scope::FoldedCompAstOf"): its
  // value is checked against its type's bounds, and it is
  // not analysed as the operator call it desugars to.
  if (needs_sup_scopes) {
    if (const auto folded = sm->CurrentScope->FoldedCompAstOf(*CompVal); folded != nullptr) {
      if (auto const *const lit = folded->To<IntegerLiteralAst>(); lit != nullptr) {
        lit->ValidateBounds(*CompVal, *sm->CurrentScope);
      }
      type_resolution::StampCompParts(*CompVal, *sm->CurrentScope);
      return;
    }
  }

  // Analysing an operator expression desugars it into a call
  // and moves its operands there ("n + 1" becomes "n.add(1)"),
  // so one is checked on a copy: the written expression is what
  // is folded, keyed and substituted. A comp argument is also
  // an expression of its own: nothing the enclosing one set up
  // - the return type a "ret" resolves overloads against, an
  // assignment target - applies to that call.
  const auto checked = comp_generics::IsCompOperator(*CompVal) ? AstClone(CompVal) : nullptr;
  auto &target = checked != nullptr ? *checked : *CompVal;
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ReturnTypeOverloadResolverType = nullptr;
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    target.Stage7_AnalyseSemantics(sm, meta);
  }
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not expr_utils::IsPrimaryExprTypeValid(target, *sm),
    {sm->CurrentScope}, ERR_ARGS(target));

  // Stamp every comp parameter named here - nested ones too
  // ("n + 1") - with that parameter, so a copy of this argument
  // carried into another scope keeps naming it there, where the
  // same spelling may name another.
  type_resolution::StampCompParts(*CompVal, *sm->CurrentScope);
}

SPP_MOD_END
