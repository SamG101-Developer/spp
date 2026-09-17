module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.generic_argument_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_utils;
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
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
auto GenericArgumentAst::NewType(
  decltype(Name) name, decltype(TypeVal) val) -> Unique<GenericArgumentAst> {
  // Build a new generic argument for a type argument.
  return MakeUnique<GenericArgumentAst>(std::move(name), nullptr, std::move(val), nullptr);
}

auto GenericArgumentAst::NewComp(
  decltype(Name) name, decltype(CompVal) &&val) -> Unique<GenericArgumentAst> {
  // Build a new generic argument for a comp argument.
  return MakeUnique<GenericArgumentAst>(std::move(name), nullptr, nullptr, std::move(val));
}

auto GenericArgumentAst::FromSym(
  TypeSymbol const &sym) -> Unique<GenericArgumentAst> {
  // Extract the value from the symbol's scope, if it exists.
  // Without a scope, fall back to the recorded generic value
  // (the value is an unresolved generic parameter, so it
  // names itself rather than a scope / true type).
  auto value = sym.LinkedScope != nullptr
    ? sym.LinkedScope->TySym->FqName()->WithConvention(AstClone(sym.Convention.get()))
    : sym.GenericVal != nullptr
    ? AstCloneShared(sym.GenericVal)
    : MakeShared<TypeIdentifierAst>(0, "Self", nullptr);
  return NewType(sym.Name, std::move(value));
}

auto GenericArgumentAst::FromSym(
  VariableSymbol const &sym) -> Unique<GenericArgumentAst> {
  // A bound parameter contributes the value it was bound to;
  // an unbound one contributes its own name, which is what
  // keeps a template's signature written in terms of its
  // parameters.
  Unique<ExpressionAst> value = nullptr;
  if (const auto *bound = sym.BoundCompValue(); bound != nullptr) {
    value = AstClone(bound);
  }
  else if (sym.Kind == VariableKind::GenericCompParam) {
    value = AstClone(sym.Name);
  }
  if (const auto value_as_type = value->To<TypeIdentifierAst>(); value_as_type != nullptr) {
    value = IdentifierAst::FromType(*AstCloneShared(value_as_type));
    // Don't remove "shared_ptr". Todo: Try new "AstCloneShared"
  }
  return NewComp(TypeIdentifierAst::FromIdentifier(*sym.Name), std::move(value));
}

GenericArgumentAst::GenericArgumentAst(
  decltype(Name) name,
  decltype(TokAssign) &&tok_assign,
  decltype(TypeVal) type_val,
  decltype(CompVal) &&comp_val) :
  OrderableAst(name != nullptr ? utils::OrderableTag::kKeywordArg : utils::OrderableTag::kPositionalArg),
  Name(std::move(name)),
  TokAssign(std::move(tok_assign)),
  TypeVal(std::move(type_val)),
  CompVal(std::move(comp_val)) {
  if (Name != nullptr) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokAssign, lex::SppTokenType::TK_ASSIGN, "=");
  }
}

GenericArgumentAst::~GenericArgumentAst() = default;

auto GenericArgumentAst::PosStart() const -> std::size_t {
  // Use the name, or the value for a positional argument.
  return Name != nullptr ? Name->PosStart() : Value()->PosStart();
}

auto GenericArgumentAst::PosEnd() const -> std::size_t {
  // Use the value.
  return Value()->PosEnd();
}

auto GenericArgumentAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<GenericArgumentAst>(
    AstCloneShared(Name), AstClone(TokAssign), AstCloneShared(TypeVal), AstClone(CompVal));
}

auto GenericArgumentAst::ToString() const -> Str {
  SPP_STRING_START;
  if (Name != nullptr) {
    SPP_STRING_APPEND(Name);
    SPP_STRING_APPEND(TokAssign);
  }
  if (TypeVal != nullptr) { SPP_STRING_APPEND(TypeVal); }
  else { SPP_STRING_APPEND(CompVal); }
  SPP_STRING_END;
}

auto GenericArgumentAst::operator==(
  GenericArgumentAst const &other) const -> bool {
  // Equal when given the same way (keyword or positional),
  // under the same name, with values of the same kind that
  // are equal.
  if ((Name == nullptr) != (other.Name == nullptr)) { return false; }
  if (Name != nullptr and *Name != *other.Name) { return false; }
  if (TypeVal != nullptr and other.TypeVal != nullptr) { return *TypeVal == *other.TypeVal; }
  return CompVal != nullptr and other.CompVal != nullptr and *CompVal == *other.CompVal;
}

auto GenericArgumentAst::Value() const -> ExpressionAst* {
  return TypeVal != nullptr ? TypeVal.get() : CompVal.get();
}

auto GenericArgumentAst::ViewName() const -> StrView {
  // Get the name from the keyword part.
  if (Name == nullptr) { return ""; }
  return Name->ToUnchecked<TypeIdentifierAst>()->Name;
}

auto GenericArgumentAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Analysed by its kind of value.
  if (TypeVal != nullptr) { AnalyseTypeVal(sm, meta); }
  else { AnalyseCompVal(sm, meta); }
}

auto GenericArgumentAst::Stage8_CheckMemory(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  using analyse::utils::mem_utils::ValidateSymbolMemory;

  // Ensure a comp value isn't moved or partially moved
  // (for all conventions). A type value holds no memory.
  if (CompVal == nullptr) { return; }
  CompVal->Stage8_CheckMemory(sm, meta);
  ValidateSymbolMemory(
    *CompVal, *CompVal, *sm, true, true, true, true, meta);
}

auto GenericArgumentAst::AnalyseTypeVal(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // "Self" outside a function body is kept, for the caller
  // to decide per use; inside one it is the enclosing type.
  if (TypeVal->IsSelfType()
    and sm->CurrentScope->AstNode != nullptr
    and AstAs<InnerScopeExpressionAst>(sm->CurrentScope->AstNode) == nullptr) { return; }
  if (TypeVal->IsSelfType()) { TypeVal = sm->CurrentScope->GetEnclosingSelfType(*meta)->WithSourceSpanOf(*TypeVal); }
  TypeVal->Stage7_AnalyseSemantics(sm, meta);
  auto const &scope = *sm->CurrentScope;

  // An argument naming a generic keeps its own node and stamp:
  // rewriting it to a binding's value would re-bind it with an
  // instantiation's own parameters ("Single[Arr[T]]" written
  // inside "Single" never ends). Anything else keeps its written
  // node, stamped with the symbol it names here, so it is read
  // by identity rather than by spelling.
  const auto val_sym = scope.GetTypeSymbol(TypeVal.get());

  // An argument naming a binding is stamped with it, and so
  // reads as that binding's parameter wherever it is read
  // ("Scope::Canon"), not as whatever its spelling finds there.
  if (val_sym != nullptr and val_sym->Kind == TypeKind::GenericArg
    and val_sym->BindsParamId != 0
    and TypeVal->Stamp() == nullptr) {
    TypeVal->SetStamp(val_sym);
    return;
  }
  if (val_sym == nullptr or val_sym->IsTypeGeneric()) { return; }

  // An alias is stamped as itself, not as its target: one
  // analysed before its target resolves (Stage 4) would reach
  // only the target's template, and "Limits[U8]" and
  // "Limits[U16]" would name one type.
  if (TypeVal->Stamp() == nullptr) { TypeVal->SetStamp(val_sym); }
}

auto GenericArgumentAst::AnalyseCompVal(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  using analyse::errors::SppInvalidPrimaryExpressionError;
  using analyse::utils::cmp_utils::StampCompGenerics;
  using analyse::utils::expr_utils::IsPrimaryExprTypeValid;

  // A comp expression ("n + 1") resolves its operator through
  // the sup scopes of its operand's type, which are only
  // attached once stage 5 ends, so it waits until then; a
  // literal or a name needs no sup scope.
  const auto is_expression = CompVal->To<LiteralAst>() == nullptr and CompVal->To<IdentifierAst>() == nullptr;
  if (is_expression and meta->CurrentStage<CompilerStage::kPreAnalyseSemantics) { return; }

  // A comp expression that folds has been evaluated by the
  // comp-time intrinsics ("cmp_utils::FoldCompExpr"): its
  // value is checked against its type's bounds, and it is
  // not analysed as the operator call it desugars to.
  if (is_expression) {
    if (const auto folded = analyse::utils::cmp_utils::FoldCompExpr(*CompVal, *sm->CurrentScope); folded != nullptr) {
      if (auto const *const lit = folded->To<IntegerLiteralAst>(); lit != nullptr) {
        lit->ValidateBounds(*CompVal, *sm->CurrentScope);
      }
      StampCompGenerics(*CompVal, *sm->CurrentScope);
      return;
    }
  }

  // Analysing an operator expression desugars it into a call
  // and moves its operands there ("n + 1" becomes "n.add(1)"),
  // so one is checked on a copy: the written expression is what
  // is folded, keyed and substituted. A comp argument is also
  // an expression of its own: nothing the enclosing one set up
  // - the return type a "ret" resolves overloads against, an
  // assignment target, object-initializer inference - applies to
  // that call.
  const auto is_operator = CompVal->To<BinaryExpressionAst>() != nullptr
    or CompVal->To<ParenthesisedExpressionAst>() != nullptr;
  const auto checked = is_operator ? AstClone(CompVal) : nullptr;
  auto &target = checked != nullptr ? *checked : *CompVal;
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->ReturnTypeOverloadResolverType = nullptr;
    meta->AssignmentTarget = nullptr;
    meta->AssignmentTargetType = nullptr;
    meta->InferSource = MakeShared<GenericInferenceBindings>();
    meta->InferTarget = MakeShared<GenericInferenceBindings>();
    target.Stage7_AnalyseSemantics(sm, meta);
  }
  RaiseIf<SppInvalidPrimaryExpressionError>(
    not IsPrimaryExprTypeValid(target, *sm),
    {sm->CurrentScope}, ERR_ARGS(target));

  // Stamp every comp parameter named here - nested ones too
  // ("n + 1") - with that parameter, so a copy of this argument
  // carried into another scope keeps naming it there, where the
  // same spelling may name another.
  StampCompGenerics(*CompVal, *sm->CurrentScope);
}

SPP_MOD_END
