module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_static_member_access_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_sym_info;
import spp.utils.interner;
import spp.utils.ptr;
import genex;

namespace {
  using spp::analyse::scopes::TypeSymbol;

  /// Whether a marker annotation was written on this type, or
  /// on the template it substitutes. An instantiation is built
  /// fresh rather than copied from its template, so "!thread_hazard"
  /// on "Rc" only reaches "Rc[S32]" along the "DerivesFromSym"
  /// chain - the same route "IsZeroType" takes.
  auto DirectThreadMarker(
    TypeSymbol const *sym, bool TypeSymbol::*flag) -> bool {
    // Look at a symbol and move through its derivations to check
    // the "flag", such as thread safety etc.
    for (auto const *s = sym; s != nullptr; s = s->DerivesFromSym.get()) {
      if (s->*flag) { return true; }
    }
    return false;
  }

  /// Whether a generic parameter was declared as "T: ThreadSafe"
  /// constrained, which is the only thing that can make an
  /// unbound generic safe.
  auto HasThreadSafeConstraint(
    TypeSymbol const &sym) -> bool {
    using spp::analyse::utils::type_compare::IsTemplate;
    using generate::common_types_precompiled::THREAD_SAFE;

    const auto scope = sym.ScopeDefinedIn != nullptr
      ? sym.ScopeDefinedIn
      : sym.LinkedScope;
    if (scope == nullptr) { return false; }

    // A constraint naming the special "ThreadSafe" type (or an
    // alias of it), as the template it stands for.
    return genex::any_of(sym.GenericConstraints, [&](auto const &c) {
      const auto ref = TypeRef::OfHead(*c, *scope);
      return ref.KindSym() != nullptr and IsTemplate(*ref.Sym, *THREAD_SAFE, *scope);
    });
  }

  auto IsThreadSafeRec(
    TypeSymbol const *sym,
    spp::Set<TypeSymbol const*> &seen)
    -> bool {
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
    if (sym->IsTypeGeneric()) {
      const auto bound = sym->LinkedScope != nullptr ? sym->LinkedScope->TySym.get() : nullptr;
      if (bound != nullptr and bound != sym and not bound->IsTypeGeneric()) { return IsThreadSafeRec(bound, seen); }
      const auto ok = HasThreadSafeConstraint(*sym);
      return ok;
    }

    // The arguments are resolved through the instantiation's own
    // scope, which lives as long as the symbol. "ScopeDefinedIn"
    // is where the type was first written, which can be a body
    // scope of a generic function instantiation that is later
    // discarded. The arguments are qualified by then.
    const auto arg_scope = sym->LinkedScope;

    // There are some types that don't hold attributes in the
    // std library, but are representative of internal values,
    // lowered directly from LLVM, such as a NonNull owning a
    // T value, but not written in the compiler. Each is known by
    // its class, not its name, so a user's own "Arr" is not one;
    // an instantiation shares its template's class.
    const auto compiler_special_type = [&] {
      if (sym->Name == nullptr or arg_scope == nullptr or sym->Type == nullptr) { return false; }
      return genex::any_of(
        spp::Vec{TUP, VAR, ARR, NON_NULL, GEN, GEN_ONCE, FUT},
        [&](auto const &known) {
          const auto known_sym = arg_scope->GetTypeSymbol(known.get());
          return known_sym != nullptr and known_sym->Type == sym->Type;
        });
    }();

    // Its arguments as its identity holds them: its name spells them as they were written where it was made, which
    // they need not mean here.
    if (compiler_special_type and sym->Id != nullptr) {
      auto const &head = HeadOf(sym->Id);
      auto arg_ids = spp::Vec<TypeId>();
      if (head.Kind == InstanceKey::Tag::Inst) {
        for (auto const &arg : ArgsOf(head.Args)) { if (arg.Type != nullptr) { arg_ids.EmplaceBack(arg.Type); } }
      }
      else if (head.Kind == InstanceKey::Tag::Variant) { arg_ids.AppendRange(head.Members); }
      for (const auto arg_id : arg_ids) {
        if (not IsThreadSafeRec(arg_scope->SymbolOf(arg_id), seen)) { return false; }
      }
    }

    // Recurse into the attributes of the type; if there is a
    // unsafe attribute type, then the overall type is also
    // unsafe.
    if (sym->LinkedScope != nullptr and sym->Type != nullptr) {
      for (auto const &attr : GetAllAttrs(*sym)) {
        if (not IsThreadSafeRec(spp::get<1>(attr).Sym, seen)) { return false; }
      }
    }

    return true;
  }
}

SPP_MOD_BEGIN
Symbol::~Symbol() = default;

NamespaceSymbol::NamespaceSymbol(
  Shared<IdentifierAst> name,
  Scope *scope) :
  Name(std::move(name)),
  LinkedScope(scope) {
}

NamespaceSymbol::NamespaceSymbol(
  NamespaceSymbol const &that) :
  Name(that.Name),
  LinkedScope(that.LinkedScope) {
}

NamespaceSymbol::~NamespaceSymbol() = default;

auto NamespaceSymbol::NeedsDeepCopy() const -> bool {
  // Nothing about a namespace is per-instantiation.
  return false;
}

auto NamespaceSymbol::operator==(
  NamespaceSymbol const &that) const -> bool {
  // Equality done by pointer.
  return this == &that;
}

VariableSymbol::VariableSymbol(
  Shared<IdentifierAst> name,
  Shared<TypeAst> type,
  Scope *ScopeDefinedIn,
  const VariableKind kind,
  const bool is_mutable,
  const asts::utils::Visibility visibility) :
  Name(std::move(name)),
  Type(std::move(type)),
  ScopeDefinedIn(ScopeDefinedIn),
  Kind(kind),
  IsMutable(is_mutable),
  Visibility(visibility),
  MemInfo(MakeUnique<utils::memory_state::MemoryInfo>()) {
  LlvmInfo = MakeShared<codegen::LlvmVarSymInfo>();
  CompTimeValue = nullptr;
}

VariableSymbol::VariableSymbol(
  VariableSymbol const &that) :
  Name(AstCloneShared(that.Name)),
  Type(that.Type),
  ScopeDefinedIn(that.ScopeDefinedIn),
  Kind(that.Kind),
  ParamId(that.ParamId),
  BindsParamId(that.BindsParamId),
  IsVariadic(that.IsVariadic),
  IsMutable(that.IsMutable),
  AliasSym(that.AliasSym),
  NarrowsSym(that.NarrowsSym),
  CallableAsType(that.CallableAsType),
  Visibility(that.Visibility),
  VisibilityAnnotation(that.VisibilityAnnotation),
  MemInfo(that.MemInfo->Clone()),
  LlvmInfo(MakeShared<codegen::LlvmVarSymInfo>()),
  CompTimeValue(AstClone(that.CompTimeValue)) {
  LlvmInfo->Alloca = that.LlvmInfo->Alloca;
}

VariableSymbol::~VariableSymbol() = default;

namespace {
  auto GenericParams() -> spp::Map<std::uint64_t, spp::analyse::scopes::TypeSymbol*>& {
    static auto params = spp::Map<std::uint64_t, spp::analyse::scopes::TypeSymbol*>();
    return params;
  }
}

auto spp::analyse::scopes::RegisterGenericParam(TypeSymbol &param) -> void {
  GenericParams()[param.ParamId] = &param;
}

auto spp::analyse::scopes::GenericParamOf(const std::uint64_t id) -> TypeSymbol* {
  const auto hit = GenericParams().find(id);
  return hit != GenericParams().end() ? hit->second : nullptr;
}

namespace {
  auto GenericCompParams() -> spp::Map<std::uint64_t, spp::analyse::scopes::VariableSymbol*>& {
    static auto params = spp::Map<std::uint64_t, spp::analyse::scopes::VariableSymbol*>();
    return params;
  }
}

auto spp::analyse::scopes::RegisterGenericCompParam(VariableSymbol &param) -> void {
  GenericCompParams()[param.ParamId] = &param;
}

auto spp::analyse::scopes::GenericCompParamOf(const std::uint64_t id) -> VariableSymbol* {
  const auto hit = GenericCompParams().find(id);
  return hit != GenericCompParams().end() ? hit->second : nullptr;
}

auto spp::analyse::scopes::NextGenericParamId() -> std::uint64_t {
  // Never zero, which is what a symbol that is not a parameter
  // holds.
  static auto next = std::uint64_t{0};
  return ++next;
}

auto VariableSymbol::NeedsDeepCopy() const -> bool {
  // The memory state and the alloca belong to one instantiation.
  return true;
}

auto VariableSymbol::operator==(
  VariableSymbol const &that) const -> bool {
  return this == &that;
}

auto TypeSymbol::IsCopyable() const -> bool {
  using generate::common_types_precompiled::COPY;
  using utils::type_compare::IsTemplate;

  // If this is a generic type, then check the copyable
  // flag on this type, and the bound type.
  if (IsTypeGeneric()) {
    if (const auto bound = AsBoundSymbol(); bound != this) {
      return IsDirectlyCopyable or bound->IsCopyable();
    }
  }

  const auto has_generic_args = Name->GnArgGroup != nullptr
    and not Name->GnArgGroup->Args.IsEmpty();

  if (has_generic_args and LinkedScope != nullptr) {
    for (auto const *sup_scope : LinkedScope->SupScopes()) {
      if (sup_scope->TySym == nullptr) { continue; }
      if (not IsTemplate(*sup_scope->TySym, *COPY, *sup_scope)) { continue; }

      // "Copy" superimposed over a generic class ("sup [T] P[T]
      // ext Copy") only holds for an instance whose attributes
      // are all copyable: copying "P[Str]" would copy the "Str",
      // which is then destroyed twice. Such an instance is linear.
      return genex::all_of(utils::type_members::GetAllAttrs(*this), [](auto const &attr) {
        auto const *const attr_sym = std::get<1>(attr).Sym;
        return attr_sym == nullptr or attr_sym->IsCopyable();
      });
    }
    return false;
  }

  // Use the normal test of if this symbol is directly
  // copyable, or if the symbol being derived from is
  // directly copyable.
  return IsDirectlyCopyable
    or (DerivesFromSym != nullptr and DerivesFromSym->IsCopyable());
}

auto TypeSymbol::IsZeroType() const -> bool {
  return IsDirectlyZeroType
    or (DerivesFromSym != nullptr and DerivesFromSym->IsZeroType());
}

auto TypeSymbol::IsTypeGeneric() const -> bool {
  return Kind == TypeKind::GenericParam or Kind == TypeKind::GenericArg;
}

auto TypeSymbol::IsMock() const -> bool {
  return Kind == TypeKind::FunctionMock or Kind == TypeKind::ClosureMock;
}

auto TypeSymbol::IsSelf() const -> bool {
  return Kind == TypeKind::Self or (Kind == TypeKind::GenericArg and Name->IsSelfType());
}

auto TypeSymbol::IsBareTemplate() const -> bool {
  return Kind == TypeKind::Class and InstanceOf == nullptr and Alias == nullptr and Type != nullptr
    and not Type->GnParamGroup->Params.IsEmpty();
}

auto TypeSymbol::IsThreadSafe() const -> bool {
  // Only ever asked where a "ThreadSafe" constraint was
  // actually written, so the walk is not memoised: it runs
  // a handful of times per program rather than once per
  // type comparison.
  auto seen = Set<TypeSymbol const*>();
  return IsThreadSafeRec(this, seen);
}

auto VariableSymbol::IsCompGeneric() const -> bool {
  return Kind == VariableKind::GenericCompParam or Kind == VariableKind::GenericCompArg;
}

auto VariableSymbol::IsCompTime() const -> bool {
  // Everything with no runtime storage of its own: a "cmp"
  // constant, a function's mock constant, and a comp generic.
  return Kind == VariableKind::Constant or Kind == VariableKind::Function or IsCompGeneric();
}

auto VariableSymbol::IsImport() const -> bool {
  // Before stage 3 an import is marked by its kind; after, by the
  // target it found (and it takes that target's kind).
  return Kind == VariableKind::Import or AliasSym != nullptr;
}

auto VariableSymbol::BoundCompValue() const -> ExpressionAst* {
  // Only a comp generic carries a binding, and only once an
  // argument has been given for it.
  if (Kind != VariableKind::GenericCompArg or CompTimeValue == nullptr) { return nullptr; }
  return CompTimeValue->To<ExpressionAst>();
}

auto VariableSymbol::FqName() const -> Shared<ExpressionAst> {
  if (IsCompGeneric()) { return Name; }

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

  auto qualified_name = Unique<ExpressionAst>(nullptr);
  qualified_name = AstClone(std::get<ScopeIdentifierName>(scopes.Back()->Name).Name);
  for (auto qualifier_scope : scopes | genex::views::reverse | genex::views::drop(1)) {
    const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.get();
    auto ns_name = MakeShared<IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
    auto ns_op = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, std::move(ns_name));
    qualified_name = MakeUnique<PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));
    qualifier_scope = qualifier_scope->Parent;
  }

  auto ns_op = MakeUnique<PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, AstCloneShared(Name));
  qualified_name = MakeUnique<PostfixExpressionAst>(std::move(qualified_name), std::move(ns_op));

  // Return the qualified expression (either IdentifierAst or
  // PostfixExpressionAst)
  return qualified_name;
}

TypeSymbol::TypeSymbol(
  Shared<TypeIdentifierAst> name,
  ClassPrototypeAst *type,
  Scope *scope,
  Scope *scope_defined_in,
  const TypeKind kind,
  const bool is_directly_copyable,
  const asts::utils::Visibility visibility,
  Unique<ConventionAst> &&convention,
  Vec<Shared<TypeAst>> const &generic_constraints) :
  Name(std::move(name)),
  Type(type),
  LinkedScope(scope),
  ScopeDefinedIn(scope_defined_in),
  Kind(kind),
  GenericConstraints(generic_constraints),
  Visibility(visibility),
  Convention(std::move(convention)),
  LlvmInfo(MakeShared<codegen::LlvmTypeSymInfo>()),
  IsDirectlyCopyable(is_directly_copyable),
  IsDirectlyZeroType(false) {
}

TypeSymbol::TypeSymbol(TypeSymbol const &that) :
  Name(that.Name),
  Type(that.Type),
  LinkedScope(that.LinkedScope),
  ScopeDefinedIn(that.ScopeDefinedIn),
  Kind(that.Kind),
  IsVariadic(that.IsVariadic),
  ParamId(that.ParamId),
  BindsParamId(that.BindsParamId),
  InstanceOf(that.InstanceOf),
  Id(that.Id),
  GenericConstraints(that.GenericConstraints),
  GenericVal(that.GenericVal),
  BoundAlias(that.BoundAlias),
  DerivesFromSym(that.DerivesFromSym),
  Visibility(that.Visibility),
  Convention(AstClone(that.Convention)),
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

TypeSymbol::~TypeSymbol() = default;

auto TypeSymbol::NeedsDeepCopy() const -> bool {
  // Only an alias is rewritten per instantiation.
  return Alias != nullptr;
}

auto TypeSymbol::operator==(
  TypeSymbol const &that) const -> bool {
  return this == &that;
}

auto TypeSymbol::AsClassSymbol() const -> TypeSymbol* {
  // Already a class, or a name with nothing behind it either
  // way. The symbol answered with is owned by the table or by
  // the scope it links to, both of which outlive any caller,
  // so it is borrowed rather than owned.
  const auto self = const_cast<TypeSymbol*>(this);
  if (Type != nullptr or LinkedScope == nullptr or LinkedScope->TySym == nullptr) { return self; }
  return LinkedScope->TySym.get();
}

auto TypeSymbol::FollowBoundAlias() const -> void {
  // Linked to a bare generic template, which nothing is bound to: the instance its value names (recorded, closed) was
  // not made when it was bound. Re-linked to it once it is.
  if (BoundAlias == nullptr and Kind == TypeKind::GenericArg and GenericVal != nullptr and LinkedScope != nullptr
    and LinkedScope->TySym != nullptr) {
    if (LinkedScope->TySym->IsBareTemplate()) {
      const auto written = GenericVal->LastTypePart()->Written();
      auto const *const target = written != nullptr and not written->HasSelf and ParamsOf(written).Types.empty()
        and ParamsOf(written).Comps.empty() ? LinkedScope->SymbolOf(written) : nullptr;
      if (target != nullptr and target->LinkedScope != nullptr and target->InstanceOf != nullptr) {
        const auto self = const_cast<TypeSymbol*>(this);
        self->Type = target->Type;
        self->LinkedScope = target->LinkedScope;
        self->IsDirectlyCopyable = target->IsDirectlyCopyable;
        self->IsDirectlyZeroType = target->IsDirectlyZeroType;
        self->GenericConstraints = target->GenericConstraints;
      }
    }
    return;
  }
  if (BoundAlias == nullptr) { return; }

  // Looked up, not made: a binding is read while keying, which makes nothing. Each alias's target is read where
  // "AliasTarget" reads it, and only once the whole chain has been made is there anything to re-link to.
  auto const *target = BoundAlias;
  for (auto step = 0; step < 8 and target != nullptr and target->Alias != nullptr; ++step) {
    if (target->Alias->Resolved == nullptr) { return; }
    auto const *const where = target->Alias->DeclScope != nullptr ? target->Alias->DeclScope : target->LinkedScope;
    target = where != nullptr ? where->GetTypeSymbol(target->Alias->Resolved.get()) : nullptr;
  }
  if (target == nullptr or target->Alias != nullptr or (not target->IsConcrete and target->InstanceOf == nullptr)) {
    return;
  }
  // As "CreateGenericSym" would have bound it, had the target been made then.
  const auto self = const_cast<TypeSymbol*>(this);
  self->Type = target->Type;
  self->LinkedScope = target->LinkedScope;
  self->IsDirectlyCopyable = target->IsDirectlyCopyable;
  self->IsDirectlyZeroType = target->IsDirectlyZeroType;
  self->GenericConstraints = target->GenericConstraints;
  self->BoundAlias = nullptr;
}

auto TypeSymbol::AsBoundSymbol() const -> TypeSymbol* {
  // Borrowed rather than owned, as with "AsClassSymbol": the
  // symbol answered with is owned by the scope it links to.
  FollowBoundAlias();
  const auto self = const_cast<TypeSymbol*>(this);
  if (IsTypeGeneric() and LinkedScope != nullptr and LinkedScope->TySym != nullptr and LinkedScope->TySym.get() !=
    self) {
    return LinkedScope->TySym->AsBoundSymbol();
  }
  return self;
}

auto TypeSymbol::AliasTarget(
  Scope const &scope) const -> TypeSymbol* {
  // Followed until nothing changes, capped against a cycle ("type A = A" is reported where it is declared).
  auto *s = const_cast<TypeSymbol*>(this);
  for (auto step = 0; step < 8 and s->Alias != nullptr and s->Alias->Resolved != nullptr; ++step) {
    auto const &resolved = *s->Alias->Resolved;
    // Read where it was written: its linked scope is the target class's own, where the target's spelling can name
    // that class's parameters instead ("type Mine = Vec[A]" in "sup A", read in "Vec", is "Vec[A=A]").
    auto *next = s->Alias->DeclScope != nullptr ? s->Alias->DeclScope->ResolveTypeSymbol(&resolved) : nullptr;
    if (next == nullptr and s->LinkedScope != nullptr) { next = s->LinkedScope->ResolveTypeSymbol(&resolved); }
    if (next == nullptr) { next = scope.ResolveTypeSymbol(&resolved); }
    if (next == nullptr or next == s) { break; }
    s = next;
  }
  return s;
}

auto TypeSymbol::UseTarget() const -> TypeSymbol* {
  // Capped against a cycle, as "AliasTarget" is.
  auto *s = const_cast<TypeSymbol*>(this);
  for (auto step = 0; step < 8 and s->Alias != nullptr and s->Alias->FromUseStmt and s->Alias->DeclScope != nullptr
       and s->Alias->Written != nullptr; ++step) {
    auto *const next = s->Alias->DeclScope->GetTypeSymbol(s->Alias->Written->WithoutGenerics().get());
    if (next == nullptr or next == s) { break; }
    s = next;
  }
  return s;
}

auto TypeSymbol::FqName(
  const bool ignore_dollar) const -> Shared<TypeAst> {
  // An alias is transparent, so it answers with the type it
  // resolves to rather than with its own name.
  if (Alias != nullptr) {
    return Alias->Resolved;
  }

  // A parameter answers with a name of its own, recording
  // it, so a use of it is looked up by identity, rather than
  // by spelling. Not "Name" itself: that node is shared with
  // the arguments made from the parameter and with every
  // binding made from those, so an identity recorded on it would follow
  // them everywhere.
  if (Kind == TypeKind::GenericParam and ParamId != 0) {
    if (_CachedFqName == nullptr) {
      _CachedFqName = AstCloneShared(Name);
      _CachedFqName->SetWritten(Scope::WrittenIdOf(*this));
    }
    return _CachedFqName;
  }

  // A binding answers with what it is bound to: the parameter
  // it passes along, or the type it was given. Its own name
  // is only its spelling, which means something else wherever
  // it is read. A binding with no scope to reach records the
  // argument it was given instead.
  // One still waiting on an alias's target ("T=U8" before its "SizedInteger" instance is made) links only the target's
  // template, so it answers with the alias, which names its target by the arguments it records.
  if (Kind == TypeKind::GenericArg) { FollowBoundAlias(); }
  if (Kind == TypeKind::GenericArg and BoundAlias != nullptr) {
    auto aliased = BoundAlias->FqName();
    return Convention != nullptr ? aliased->WithConvention(AstClone(Convention)) : aliased;
  }
  if (Kind == TypeKind::GenericArg and LinkedScope != nullptr and LinkedScope->TySym != nullptr
    and LinkedScope->TySym.get() != this) {
    auto bound = LinkedScope->TySym->FqName();
    return Convention != nullptr ? bound->WithConvention(AstClone(Convention)) : bound;
  }
  if (Kind == TypeKind::GenericArg and GenericVal != nullptr) {
    return GenericVal;
  }

  // If the type is generic (an unbound binding), or is "Self",
  // return the name as-is.
  if (IsTypeGeneric() or LinkedScope == nullptr or IsSelf()) {
    return Name;
  }

  if (IsMock()
    and (ignore_dollar or LinkedScope->Parent != LinkedScope->ParentModule())) {
    // A method's mock is declared in its "sup" block, so it
    // is named through the type that block is over:
    // "main::A::$Method". A closure's mock, and a declaration
    // site, keep the bare name.
    const auto sup_node = LinkedScope->Parent->AstNode;
    const auto in_sup_block = asts::AstAs<SupPrototypeFunctionsAst>(sup_node) != nullptr
      or asts::AstAs<SupPrototypeExtensionAst>(sup_node) != nullptr;
    if (ignore_dollar or not in_sup_block or Kind == TypeKind::ClosureMock) { return Name; }

    // Todo: a generic owner ("sup [T] A[T]", or "sup Str" over a
    //  defaulted "Str[A]") would need its arguments carried through
    //  each instantiation, so it keeps the bare name.
    const auto owner_name = AstName(sup_node);
    const auto owner_gn = owner_name->LastTypePart()->GnArgGroup.get();
    if (owner_gn != nullptr and not owner_gn->Args.IsEmpty()) { return Name; }
    const auto owner_sym = LinkedScope->Parent->GetTypeSymbol(owner_name->WithoutGenerics().get());
    if (owner_sym == nullptr or owner_sym == this or owner_sym->IsTypeGeneric()
      or owner_sym->IsMock()
      or (owner_sym->Type != nullptr and not owner_sym->Type->GnParamGroup->Params.IsEmpty())) { return Name; }
    // Built from copies: the owner's name is its shared cached
    // one, and analysing this type would mark that as analysed
    // for every other use of it (skipping, say, its abstract-type
    // check).
    return MakeShared<TypePostfixExpressionAst>(
      AstCloneShared(owner_sym->FqName()),
      MakeShared<TypePostfixExpressionOperatorNestedTypeAst>(nullptr, AstCloneShared(Name)));
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
  auto qualified_name = dynamic_shared_cast<TypeAst>(Name);
  while (qualifier_scope->Parent != nullptr) {
    while (std::holds_alternative<ScopeBlockName>(qualifier_scope->Name)) {
      qualifier_scope = qualifier_scope->Parent;
    }
    const auto raw_ns_name = std::get<ScopeIdentifierName>(qualifier_scope->Name).Name.get();
    auto ns_name = MakeShared<IdentifierAst>(raw_ns_name->PosStart(), raw_ns_name->Val);
    auto ns_op = MakeShared<TypeUnaryExpressionOperatorNamespaceAst>(std::move(ns_name), nullptr);
    qualified_name = MakeShared<TypeUnaryExpressionAst>(std::move(ns_op), std::move(qualified_name));
    qualifier_scope = qualifier_scope->Parent;
  }

  // Re-add the convention of the type if it exists.
  _CachedFqName = Convention ? qualified_name->WithConvention(AstClone(Convention)) : qualified_name;
  _CachedFqNameGen = ScopeLinkageGeneration();

  // A class's name records its identity. A closed one means it
  // from anywhere; an open instantiation's arguments are read
  // again from the scope asking ("Scope::Canon"), through that
  // scope's bindings of the generics they name.
  if (Kind == TypeKind::Class and Alias == nullptr) {
    _CachedFqName->SetWritten(Scope::WrittenIdOf(*this));
  }
  return _CachedFqName;
}

auto TypeSymbol::GenericSelfName() const -> Shared<TypeAst> {
  // A template is named as written; as a pattern it is itself
  // over its own parameters. Matched against "sup Limits[U8]"
  // as bare "Limits" it would take every such block (and each
  // one's "SInt"), which "Limits[T=T]" correctly does not.
  auto fq = FqName();
  if (Kind == TypeKind::Class and InstanceOf == nullptr and Alias == nullptr and Type != nullptr
    and not Type->GnParamGroup->Params.IsEmpty()) {
    return fq->WithGenerics(GenericArgumentGroupAst::FromParams(*Type->GnParamGroup));
  }
  return fq;
}

auto TypeSymbol::BoundCompArg(
  Str const &name) const -> ExpressionAst const* {
  // The binding is registered in the instantiation's own scope,
  // under the parameter's name.
  if (LinkedScope == nullptr) { return nullptr; }
  const auto sym_name = MakeShared<IdentifierAst>(0, name);
  const auto var = LinkedScope->GetVarSymbol(sym_name.get(), true);
  return var != nullptr ? var->BoundCompValue() : nullptr;
}

auto TypeSymbol::BoundTypeArg(
  Str const &name) const -> TypeSymbol* {
  // As "BoundCompArg": the binding is in the instantiation's
  // own scope under the parameter's name, and it links to the
  // type it is bound to - read from there, rather than by
  // respelling the argument from the instantiation's name.
  if (LinkedScope == nullptr) { return nullptr; }
  const auto sym_name = TypeIdentifierAst::FromString(name);
  const auto binding = LinkedScope->GetTypeSymbol(sym_name.get(), true);
  return binding != nullptr ? binding->AsBoundSymbol() : nullptr;
}

auto TypeSymbol::TypeArgType(
  Str const &name) const -> Shared<TypeAst> {
  // The instantiation holds its arguments on its own name; an
  // alias's target, a binding's bound type and "Self"'s class
  // are each the class their linked scope belongs to.
  // Read off its identity, which holds what each argument means; its name spells them as they were written where it was
  // made. Anything without one answers with its name's.
  // It points where its name's argument was written.
  auto const &inst = LinkedScope != nullptr and LinkedScope->TySym != nullptr ? *LinkedScope->TySym : *this;
  auto const *arg = inst.Name->GnArgGroup->At(name.c_str());
  if (inst.Id != nullptr and inst.LinkedScope != nullptr and HeadOf(inst.Id).Kind == InstanceKey::Tag::Inst) {
    const auto name_id = static_cast<std::uint64_t>(spp::utils::Intern(name));
    for (auto const &id_arg : ArgsOf(HeadOf(inst.Id).Args)) {
      if (not id_arg.Named or id_arg.Name != name_id or id_arg.Type == nullptr) { continue; }
      auto type = inst.LinkedScope->TypeAstOf(id_arg.Type);
      return type != nullptr and arg != nullptr and arg->TypeVal != nullptr ? type->WithSourceSpanOf(*arg->TypeVal) : type;
    }
  }
  return arg != nullptr ? arg->TypeVal : nullptr;
}

auto TypeSymbol::TypeArgTypes() const -> Vec<Shared<TypeAst>> {
  // As "TypeArgType": off its identity, else its name.
  auto const &inst = LinkedScope != nullptr and LinkedScope->TySym != nullptr ? *LinkedScope->TySym : *this;
  auto types = Vec<Shared<TypeAst>>();
  if (inst.Id != nullptr and inst.LinkedScope != nullptr and HeadOf(inst.Id).Kind == InstanceKey::Tag::Inst) {
    for (auto const &arg : ArgsOf(HeadOf(inst.Id).Args)) {
      if (arg.Type != nullptr) { types.EmplaceBack(inst.LinkedScope->TypeAstOf(arg.Type)); }
    }
    return types;
  }
  for (const auto arg : inst.Name->GnArgGroup->GetTypeArgs()) { types.EmplaceBack(arg->TypeVal); }
  return types;
}

TypeRef::TypeRef(
  TypeSymbol *resolved, const TypeId id, const ConventionTag conv, const bool never) :
  Sym(resolved),
  Conv(conv),
  IsNever(never),
  Id(resolved != nullptr ? BareTypeId(id) : nullptr) {
}

auto TypeRef::Named(
  Scope const &scope, const TypeId id, TypeSymbol *open, const ConventionTag conv, const bool never) -> TypeRef {
  // The symbol is what the identity names: the instantiation filed under it, made here first if it is not yet ("open"
  // is the one the lookup reached, instantiated under this scope's bindings). Only where the identity names no one
  // symbol - a part that is "Self" (keyed by spelling, standing for the implementer), a binding to nothing, or nothing
  // made yet where nothing can be - is it "open" itself.
  auto *sym = id != nullptr and not id->HasSelf ? scope.SymbolOf(id) : nullptr;
  if (sym == nullptr and id != nullptr and not id->HasSelf and open != nullptr and open->InstanceOf != nullptr
    and open->Alias == nullptr and Scope::OnInstantiationMissing) {
    const auto kind = HeadOf(id).Kind;
    if (kind == InstanceKey::Tag::Inst or kind == InstanceKey::Tag::Variant) {
      auto *const made = Scope::OnInstantiationMissing(*open, scope);
      sym = scope.SymbolOf(id);

      // Made under the identity of what was instantiated, which differs from the one asked for when that names it
      // through a "use" of its class (a "use" files its own instances apart): what was made, under its own identity.
      if (sym == nullptr and made != nullptr and made->Id != nullptr) {
        return TypeRef(made, made->Id, conv, never);
      }
    }
  }
  return TypeRef(sym != nullptr ? sym : open, id, conv, never);
}

auto TypeRef::Of(
  TypeAst const &type, Scope const &scope) -> TypeRef {
  // A resolved type is never an alias: it is what the alias stands for ("AliasTarget"). The alias stays on the
  // written type, for visibility and messages.
  auto *const found = scope.ResolveTypeSymbol(&type);
  if (found == nullptr) { return TypeRef(); }

  // Held under the written convention, else a binding's own ("T" bound to "&mut Str" is "&mut Str"), as "OfSym" holds a
  // binding.
  const auto conv = type.GetConvention();
  const auto tag = conv != nullptr ? conv->Tag()
    : found->Kind == TypeKind::GenericArg and found->Convention != nullptr ? found->Convention->Tag()
    : ConventionTag::MOV;
  return Named(scope, scope.TypeIdOf(type), found->AliasTarget(scope), tag, type.IsNeverType());
}

/// [CHECKED]
auto TypeRef::OfHead(
  TypeAst const &type, Scope const &scope) -> TypeRef {
  // An alias's head is the template of what it stands for:
  // "Opt" is the "Var" its target instantiates.
  const auto conv = type.GetConvention();
  const auto head = scope.GetTypeSymbol(type.WithoutGenerics().get());
  auto target = head != nullptr ? head->AliasTarget(scope) : nullptr;

  // Get what the target symbol is an instance of, ie its
  // template (also a symbol), assuming we dont already have
  // the template.
  if (target != head and target->InstanceOf != nullptr) { target = target->InstanceOf; }

  // Create a new type reference for the template type, with
  // its type id/convention/is-never flags copied over.
  return TypeRef(
    target, target != nullptr ? scope.TypeIdOfSym(*target, 0) : nullptr,
    conv != nullptr ? conv->Tag() : ConventionTag::MOV, type.IsNeverType());
}

auto TypeRef::OfResolved(
  TypeSymbol &sym, Scope const &scope, const ConventionTag conv) -> TypeRef {
  // Its identity read here ("Scope::ReadIn"): a parameter as this scope binds it, an open instantiation under this
  // scope's bindings, an alias as its target. Its own scope is not read through: that binds its class's parameters to
  // its arguments, which name the parameters of where it was written ("FunMov[Args=Tup[FunMov[Args, Out], Args]]"
  // would re-read as one more level, without end).
  const auto written = BareTypeId(scope.TypeIdOfSym(sym, 0));
  const auto id = sym.LinkedScope == &scope ? written : scope.ReadIn(written);
  auto *const open = sym.Alias != nullptr ? sym.AliasTarget(scope) : &sym;
  return Named(scope, id, open, conv, sym.Name != nullptr and sym.Name->IsNeverType());
}

auto VariableSymbol::TypeRefIn(
  Scope const &scope) const -> TypeRef {
  return Type != nullptr ? TypeRef::Of(*Type, scope) : TypeRef{};
}

auto TypeRef::OfSym(
  TypeSymbol &sym, Scope const &scope) -> TypeRef {
  // A binding is held under its own convention, as its qualified name is.
  const auto conv = sym.Kind == TypeKind::GenericArg and sym.Convention != nullptr
    ? sym.Convention->Tag()
    : ConventionTag::MOV;
  return OfResolved(sym, scope, conv);
}

auto TypeSymbol::InvalidateFqNameCache() const
  -> void {
  _CachedFqName = nullptr;
  _CachedFqNameGen = 0;
}

SPP_MOD_END
