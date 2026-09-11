module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.asts.convention_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_sym_info;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
spp::analyse::scopes::Symbol::~Symbol() = default;

spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
  Shared<asts::IdentifierAst> name,
  Scope *scope) :
  Name(std::move(name)),
  LinkedScope(scope) {
}

spp::analyse::scopes::NamespaceSymbol::NamespaceSymbol(
  NamespaceSymbol const &that) :
  Name(that.Name),
  LinkedScope(that.LinkedScope) {
}

spp::analyse::scopes::NamespaceSymbol::~NamespaceSymbol() = default;

auto spp::analyse::scopes::NamespaceSymbol::NeedsDeepCopy() const
  -> bool {
  // Nothing about a namespace is per-instantiation.
  return false;
}

auto spp::analyse::scopes::NamespaceSymbol::operator==(
  NamespaceSymbol const &that) const
  -> bool {
  // Equality done by pointer.
  return this == &that;
}

spp::analyse::scopes::VariableSymbol::VariableSymbol(
  Shared<asts::IdentifierAst> name,
  Shared<asts::TypeAst> type,
  Scope *ScopeDefinedIn,
  const bool is_mutable,
  const bool is_generic,
  const asts::utils::Visibility visibility) :
  Name(std::move(name)),
  Type(std::move(type)),
  ScopeDefinedIn(ScopeDefinedIn),
  IsMutable(is_mutable),
  IsGeneric(is_generic),
  Visibility(visibility),
  MemInfo(MakeUnique<utils::mem_info_utils::MemoryInfo>()) {
  LlvmInfo = MakeShared<codegen::LlvmVarSymInfo>();
  CompTimeValue = nullptr;
}

spp::analyse::scopes::VariableSymbol::VariableSymbol(
  VariableSymbol const &that) :
  Name(AstCloneShared(that.Name)),
  Type(that.Type),
  ScopeDefinedIn(that.ScopeDefinedIn),
  IsMutable(that.IsMutable),
  IsGeneric(that.IsGeneric),
  IsFlowNarrowing(that.IsFlowNarrowing),
  IsCapture(that.IsCapture),
  NarrowsSym(that.NarrowsSym),
  CallableAsType(that.CallableAsType),
  Visibility(that.Visibility),
  VisibilityAnnotation(that.VisibilityAnnotation),
  MemInfo(that.MemInfo->Clone()),
  LlvmInfo(MakeShared<codegen::LlvmVarSymInfo>()),
  CompTimeValue(asts::AstClone(that.CompTimeValue)),
  AliasSym(that.AliasSym) {
  LlvmInfo->Alloca = that.LlvmInfo->Alloca;
}

spp::analyse::scopes::VariableSymbol::~VariableSymbol() = default;

auto spp::analyse::scopes::VariableSymbol::NeedsDeepCopy() const
  -> bool {
  // The memory state and the alloca belong to one instantiation.
  return true;
}

auto spp::analyse::scopes::VariableSymbol::operator==(
  VariableSymbol const &that) const
  -> bool {
  return this == &that;
}

auto spp::analyse::scopes::TypeSymbol::IsCopyable() const
  -> bool {
  using asts::generate::common_types_precompiled::COPY;
  using utils::type_compare::TypeEq;

  // Todo: Clean this mess up.
  // From the superimposition graph:
  // "sup [..Items: Copy] Tup[Items] ext Copy" makes a
  // tuple copyable only when its items are, and
  // "PruneUnsatisfiedSupConstraints" already removes the
  // attachment from the instantiations whose items do not
  // satisfy it - so the graph is the thing that knows.
  // "IsDirectlyCopyable" is set once against the template,
  // before any argument exists, and cannot express a
  // conditional answer.
  const auto has_generic_args = Name != nullptr and Name->GnArgGroup != nullptr
    and not Name->GnArgGroup->Args.IsEmpty();

  if (has_generic_args and LinkedScope != nullptr) {
    for (auto const *sup_scope : LinkedScope->SupScopesConst()) {
      if (sup_scope->TySym == nullptr) { continue; }
      if (TypeEq(*sup_scope->TySym->FqName(), *COPY, *sup_scope, *LinkedScope)) { return true; }
    }
    return false;
  }

  return IsDirectlyCopyable
    or (DerivesFromSym != nullptr and DerivesFromSym->IsCopyable());
}

auto spp::analyse::scopes::TypeSymbol::IsZeroType() const
  -> bool {
  return IsDirectlyZeroType or (DerivesFromSym != nullptr and DerivesFromSym->IsZeroType());
}

namespace {
  using spp::analyse::scopes::TypeSymbol;

  /**
   * Whether a marker annotation was written on this type or on the template it substitutes. An instantiation is built
   * fresh rather than copied from its template, so "!thread_hazard" on "Rc" only reaches "Rc[S32]" along the
   * "DerivesFromSym" chain - the same route "IsZeroType" takes.
   * @param sym The symbol to start the walk at.
   * @param flag The marker being asked about.
   * @return Whether the marker was written anywhere along the chain.
   */
  auto DirectThreadMarker(
    TypeSymbol const *sym,
    bool TypeSymbol::*flag)
    -> bool {
    // Look at a symbol and move through its derivations
    // to check the "flag", such as thread safety etc.
    for (auto const *s = sym; s != nullptr; s = s->DerivesFromSym.get()) {
      if (s->*flag) { return true; }
    }
    return false;
  }

  /**
   * Whether a generic parameter was declared with a "ThreadSafe" constraint, which is the only thing that can make an
   * unbound one safe.
   * @param sym The generic parameter's symbol.
   * @return Whether one of its constraints is "ThreadSafe".
   */
  auto HasThreadSafeConstraint(
    TypeSymbol const &sym)
    -> bool {
    using spp::analyse::utils::type_compare::TypeEq;
    using spp::asts::generate::common_types_precompiled::THREAD_SAFE;

    const auto scope = sym.ScopeDefinedIn != nullptr
      ? sym.ScopeDefinedIn
      : sym.LinkedScope;
    if (scope == nullptr) { return false; }

    // Simple sup-scope search and comparison against the
    // special "ThreadSafe" type.
    return genex::any_of(sym.GenericConstraints, [&](auto const &c) {
      return TypeEq(*c, *THREAD_SAFE, *scope, *scope);
    });
  }

  auto IsThreadSafeRec(
    TypeSymbol const *sym,
    spp::Set<TypeSymbol const*> &seen)
    -> bool {
    using spp::analyse::utils::type_compare::TypeEq;
    using spp::analyse::utils::type_members::GetAllAttrs;
    using namespace spp::asts::generate::common_types_precompiled;

    // A symbol that could not be resolved is not evidence
    // of a hazard. A type reached twice ie via a cycle,
    // adds nothing the first visit did not already account for.
    if (sym == nullptr) { return true; }
    if (not seen.insert(sym).second) { return true; }

    // A hazard stays one however it is wrapped.
    if (DirectThreadMarker(sym, &TypeSymbol::IsDirectlyThreadHazard)) {
      return false;
    }

    // A generic bound to a real type answers with whatever it
    // was bound to; an unbound one is safe only where it was
    // constrained to be, because nothing else stops it being
    // instantiated with a hazard.
    if (sym->IsGeneric) {
      const auto bound = sym->LinkedScope != nullptr ? sym->LinkedScope->TySym.get() : nullptr;
      if (bound != nullptr and bound != sym and not bound->IsGeneric) { return IsThreadSafeRec(bound, seen); }
      const auto ok = HasThreadSafeConstraint(*sym);
      return ok;
    }

    const auto arg_scope = sym->ScopeDefinedIn != nullptr
      ? sym->ScopeDefinedIn
      : sym->LinkedScope;

    // There are some types that don't hold attributes in the
    // std library, but are representative of internal values,
    // lowered directly from LLVM, such as a NonNull owning a
    // T value, but not written in the compiler. Todo: this
    // looks like is needs strengthening with the equality.
    const auto compiler_special_type = [&] {
      if (sym->Name == nullptr or arg_scope == nullptr) { return false; }
      const auto bare = sym->Name->WithoutGenerics();
      return genex::any_of(
        spp::Vec{TUP, VAR, ARR, NON_NULL, GEN, GEN_ONCE, FUT},
        [&](auto const &known) { return bare->LastTypePart()->Name == known->LastTypePart()->Name; });
    }();

    if (compiler_special_type and sym->Name->GnArgGroup != nullptr) {
      for (auto const &arg : sym->Name->GnArgGroup->Args) {
        const auto type_arg = arg->To<spp::asts::GenericArgumentTypeAst>();
        if (type_arg == nullptr) { continue; }
        if (not IsThreadSafeRec(arg_scope->GetTypeSymbol(type_arg->Val.get()), seen)) { return false; }
      }
    }

    // Recurse into the attributes of the type; if there is a
    // unsafe attribute type, then the overall type is also
    // unsafe.
    if (sym->LinkedScope != nullptr and sym->Type != nullptr) {
      for (auto const &attr : GetAllAttrs(*sym->FqName(), *sym->LinkedScope)) {
        if (not IsThreadSafeRec(spp::get<1>(attr), seen)) { return false; }
      }
    }

    return true;
  }
}

auto spp::analyse::scopes::TypeSymbol::IsThreadSafe() const
  -> bool {
  // Only ever asked where a "ThreadSafe" constraint was
  // actually written, so the walk is not memoised: it runs
  // a handful of times per program rather than once per
  // type comparison.
  auto seen = Set<TypeSymbol const*>();
  return IsThreadSafeRec(this, seen);
}

auto spp::analyse::scopes::VariableSymbol::BoundCompValue() const
  -> asts::ExpressionAst* {
  // Only a comp generic carries a binding, and only once an
  // argument has been given for it.
  if (not IsGeneric or MemInfo->AstCompTime == nullptr) { return nullptr; }

  // An instantiation records the argument the parameter was
  // bound to; a template records the parameter itself, which
  // is a declaration rather than a value, so it is not a binding.
  const auto bound = MemInfo->AstCompTime->To<asts::GenericArgumentCompAst>();
  return bound != nullptr ? bound->Val.get() : nullptr;
}

auto spp::analyse::scopes::VariableSymbol::FqName() const
  -> Shared<asts::ExpressionAst> {
  if (IsGeneric) { return Name; }

  // Fully qualify the name from the root scope.
  auto qualifier_scope = ScopeDefinedIn;
  auto scopes = Vec<Scope*>();

  while (qualifier_scope->Parent != nullptr) {
    while (std::holds_alternative<ScopeBlockName>(qualifier_scope->Name)) {
      qualifier_scope = qualifier_scope->Parent;
    }
    scopes.EmplaceBack(qualifier_scope);
    qualifier_scope = qualifier_scope->Parent;
  }

  auto qualified_name = Unique<asts::ExpressionAst>(nullptr);
  qualified_name = asts::AstClone(std::get<ScopeIdentifierName>(scopes.Back()->Name).Name);
  for (auto qualifier_scope : scopes | genex::views::reverse | genex::views::drop(1)) {
    const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.get();
    auto ns_name = MakeShared<asts::IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
    auto ns_op = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, std::move(ns_name));
    qualified_name = MakeUnique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));
    qualifier_scope = qualifier_scope->Parent;
  }

  auto ns_op = MakeUnique<asts::PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, asts::AstCloneShared(Name));
  qualified_name = MakeUnique<asts::PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));

  // Return the qualified expression (either IdentifierAst or PostfixExpressionAst)
  return qualified_name;
}

spp::analyse::scopes::TypeSymbol::TypeSymbol(
  Shared<asts::TypeIdentifierAst> name,
  asts::ClassPrototypeAst *type,
  Scope *scope,
  Scope *scope_defined_in,
  Scope *scope_module,
  const bool is_generic,
  const bool is_directly_copyable,
  const asts::utils::Visibility visibility,
  Unique<asts::ConventionAst> &&convention,
  Vec<Shared<asts::TypeAst>> const &generic_constraints) :
  Name(std::move(name)),
  Type(type),
  LinkedScope(scope),
  ScopeDefinedIn(scope_defined_in),
  ScopeModule(scope_module),
  IsGeneric(is_generic),
  GenericConstraints(generic_constraints),
  Visibility(visibility),
  Convention(std::move(convention)),
  GenericImpl(this),
  LlvmInfo(MakeShared<codegen::LlvmTypeSymInfo>()),
  IsDirectlyCopyable(is_directly_copyable),
  IsDirectlyZeroType(false) {
}

spp::analyse::scopes::TypeSymbol::TypeSymbol(TypeSymbol const &that) :
  Name(that.Name),
  Type(that.Type),
  LinkedScope(that.LinkedScope),
  ScopeDefinedIn(that.ScopeDefinedIn),
  ScopeModule(that.ScopeModule),
  IsGeneric(that.IsGeneric),
  IsVariadic(that.IsVariadic),
  GenericConstraints(that.GenericConstraints),
  GenericVal(that.GenericVal),
  DerivesFromSym(that.DerivesFromSym),
  Visibility(that.Visibility),
  Convention(asts::AstClone(that.Convention)),
  GenericImpl(that.GenericImpl),
  IsDirectlyCopyable(that.IsDirectlyCopyable),
  IsDirectlyZeroType(that.IsDirectlyZeroType),
  IsDirectlyThreadHazard(that.IsDirectlyThreadHazard) {
  // Shared rather than cloned: an alias's description is
  // fixed once resolved, and an instantiation of a generic
  // alias builds its own ("CreateGenericClsScope") rather
  // than mutating one it was handed.
  Alias = that.Alias;
  LlvmInfo = that.LlvmInfo;
}

spp::analyse::scopes::TypeSymbol::~TypeSymbol() = default;

auto spp::analyse::scopes::TypeSymbol::NeedsDeepCopy() const
  -> bool {
  // Only an alias is rewritten per instantiation.
  return Alias != nullptr;
}

auto spp::analyse::scopes::TypeSymbol::operator==(
  TypeSymbol const &that) const
  -> bool {
  return this == &that;
}

auto spp::analyse::scopes::TypeSymbol::AsClassSymbol() const
  -> TypeSymbol* {
  // Already a class, or a name with nothing behind it either
  // way. The symbol answered with is owned by the table or by
  // the scope it links to, both of which outlive any caller,
  // so it is borrowed rather than owned.
  const auto self = const_cast<TypeSymbol*>(this);
  if (Type != nullptr or LinkedScope == nullptr or LinkedScope->TySym == nullptr) { return self; }
  return LinkedScope->TySym.get();
}

auto spp::analyse::scopes::TypeSymbol::AsBoundSymbol() const
  -> TypeSymbol* {
  // Borrowed rather than owned, as with "AsClassSymbol": the
  // symbol answered with is owned by the scope it links to.
  const auto self = const_cast<TypeSymbol*>(this);
  if (IsGeneric and LinkedScope != nullptr and LinkedScope->TySym != nullptr and LinkedScope->TySym.get() != self) {
    return LinkedScope->TySym->AsBoundSymbol();
  }
  return self;
}

auto spp::analyse::scopes::TypeSymbol::FqName(
  const bool ignore_dollar) const
  -> Shared<asts::TypeAst> {
  // An alias is transparent, so it answers with the type it
  // resolves to rather than with its own name.
  if (Alias != nullptr) {
    return Alias->Resolved;
  }

  // If the type is generic, or is "Self", return the name as-is.
  if (IsGeneric or LinkedScope == nullptr or Name->IsSelfType()) {
    return Name;
  }

  if (Name->IsCompilerGeneratedType()
    and (ignore_dollar or LinkedScope->Parent != LinkedScope->ParentModule())) {
    return Name;
  }

  // Everything above returns a name that already exists. What
  // is left builds one, walking the scopes above the linked
  // scope and minting an ast node per namespace part, so it
  // is worth not doing twice: the walk reads only the shape of
  // the scope tree, which is fixed until a scope is re-parented.
  if (_CachedFqNameGen == ScopeLinkageGeneration()) {
    return _CachedFqName;
  }

  // Fully qualify the name from the root scope.
  auto qualifier_scope = LinkedScope->Parent;
  auto qualified_name = dynamic_shared_cast<asts::TypeAst>(Name);
  while (qualifier_scope->Parent != nullptr) {
    while (std::holds_alternative<ScopeBlockName>(qualifier_scope->Name)) {
      qualifier_scope = qualifier_scope->Parent;
    }
    const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.get();
    auto ns_name = MakeShared<asts::IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
    auto ns_op = MakeShared<asts::TypeUnaryExpressionOperatorNamespaceAst>(std::move(ns_name), nullptr);
    qualified_name = MakeShared<asts::TypeUnaryExpressionAst>(std::move(ns_op), std::move(qualified_name));
    qualifier_scope = qualifier_scope->Parent;
  }

  // Re-add the convention of the type if it exists.
  _CachedFqName = Convention ? qualified_name->WithConvention(asts::AstClone(Convention)) : qualified_name;
  _CachedFqNameGen = ScopeLinkageGeneration();
  return _CachedFqName;
}

auto spp::analyse::scopes::TypeSymbol::InvalidateFqNameCache() const
  -> void {
  _CachedFqName = nullptr;
  _CachedFqNameGen = 0;
}

auto spp::analyse::scopes::TypeSymbol::BoundName() const
  -> Shared<asts::TypeAst> {
  // Not a parameter, so there is no binding to follow and
  // the name is the whole answer.
  if (not IsGeneric) { return FqName(); }

  // Bound to a real type: that type's own name, carrying
  // over whatever convention the binding was written with.
  if (LinkedScope != nullptr and LinkedScope->TySym != nullptr and LinkedScope->TySym.get() != this) {
    auto bound = LinkedScope->TySym->FqName();
    return Convention != nullptr ? bound->WithConvention(asts::AstClone(Convention)) : bound;
  }

  // Bound to another parameter, which has no scope of its own
  // to reach: the recorded argument is the only record of the
  // binding. Failing that, unbound, and it stands for itself.
  return GenericVal != nullptr ? GenericVal : Name;
}

SPP_MOD_END
