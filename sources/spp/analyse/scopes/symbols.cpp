module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.memory_state;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.convention_mut_ast;
import spp.asts.convention_ref_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
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
  using spp::analyse::scopes::Scope;
  using spp::analyse::scopes::TypeSymbol;

  /// The one place reading a type makes an instantiation: "id" (already read where "scope" reads it), standing for
  /// "open", an open instantiation (of a class or an alias) a lookup reached ("monomorphization::InstantiateForScope",
  /// while an analysis stage lets reads make one). Nothing where "open" is no instantiation, or when nothing can be
  /// made yet.
  auto MakeOnRead(
    Scope const &scope, TypeSymbol const &open, const spp::analyse::scopes::TypeId id) -> TypeSymbol* {
    using spp::analyse::scopes::TypeKind;
    if (id == nullptr or open.InstanceOf == nullptr) { return nullptr; }
    if (open.Kind != TypeKind::Cls and open.Alias == nullptr) { return nullptr; }
    return spp::analyse::utils::monomorphization::InstantiateForScope(id, scope);
  }

  /// An instantiation's argument "name" as its identity holds it ("TypeSymbol::TypeArg", "CompArgId"), when "inst"
  /// has an identity holding one under that name.
  auto IdArgNamed(
    TypeSymbol const &inst, spp::Str const &name) -> std::optional<spp::analyse::scopes::TypeIdArg> {
    using namespace spp::analyse::scopes;
    if (inst.Id == nullptr or inst.LinkedScope == nullptr or HeadOf(inst.Id).Kind != InstanceKey::Tag::Inst) {
      return std::nullopt;
    }
    const auto name_id = InternWord(name);
    for (auto const &id_arg : ArgsOf(HeadOf(inst.Id).Args)) {
      if (id_arg.Named and id_arg.Name == name_id) { return id_arg; }
    }
    return std::nullopt;
  }

  /// What a written type names here ("Scope::FindTypeSymbol"); else, for a name written as an open instantiation not
  /// made from here yet, the one its arguments name here, made now ("MakeOnRead").
  auto FindOrMakeTypeSymbol(
    Scope const &scope, spp::asts::TypeAst const &type) -> TypeSymbol* {
    if (auto *const sym = scope.FindTypeSymbol(&type); sym != nullptr) { return sym; }
    auto *const open = scope.TypeSymbolOf(type.LastTypePart()->WrittenTypeId());
    return open != nullptr ? MakeOnRead(scope, *open, scope.ReadIn(open->Id)) : nullptr;
  }

  /// Whether a marker annotation was written on this type, or
  /// on the template it substitutes. An instantiation is built
  /// fresh rather than copied from its template, so "!thread_hazard"
  /// on "Rc" only reaches "Rc[S32]" along the "DerivesFromSymbol"
  /// chain - the same route "IsZeroType" takes.
  auto DirectThreadMarker(
    TypeSymbol const *sym, bool TypeSymbol::*flag) -> bool {
    // Look at a symbol and move through its derivations to check
    // the "flag", such as thread safety etc.
    for (auto const *s = sym; s != nullptr; s = s->DerivesFromSymbol.get()) {
      if (s->*flag) { return true; }
    }
    return false;
  }

  /// Whether a generic parameter was declared as "T: ThreadSafe"
  /// constrained, which is the only thing that can make an
  /// unbound generic safe.
  auto HasThreadSafeConstraint(
    TypeSymbol const &sym) -> bool {
    using spp::analyse::utils::type_predicates::HeadKindRef;
    using generate::common_types_precompiled::THREAD_SAFE;

    const auto scope = sym.ScopeDefinedIn != nullptr
      ? sym.ScopeDefinedIn
      : sym.LinkedScope;
    if (scope == nullptr) { return false; }

    // A constraint naming the special "ThreadSafe" type (or an
    // alias of it), as the template it stands for.
    return genex::any_of(sym.TypeConstraints, [&](auto const &c) {
      const auto head = HeadKindRef(*c, *scope);
      return head.KindSymbol() != nullptr and head.IsA(*THREAD_SAFE, *scope);
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
    if (sym->IsGn()) {
      const auto bound = sym->LinkedSymbol();
      if (bound != sym and not bound->IsGn()) { return IsThreadSafeRec(bound, seen); }
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
          const auto known_sym = arg_scope->FindTypeSymbol(known.get());
          return known_sym != nullptr and known_sym->Type == sym->Type;
        });
    }();

    // Its arguments as its identity holds them: its name spells them as they were written where it was made, which
    // they need not mean here.
    if (compiler_special_type and sym->Id != nullptr) {
      auto const &head = HeadOf(sym->Id);
      auto arg_ids = spp::Vec<TypeId>();
      if (head.Kind == InstanceKey::Tag::Inst) {
        for (auto const &arg : ArgsOf(head.Args)) { if (arg.TypeVal != nullptr) { arg_ids.EmplaceBack(arg.TypeVal); } }
      }
      else if (head.Kind == InstanceKey::Tag::Variant) { arg_ids.AppendRange(head.Members); }
      for (const auto arg_id : arg_ids) {
        if (not IsThreadSafeRec(arg_scope->TypeSymbolOf(arg_id), seen)) { return false; }
      }
    }

    // Recurse into the attributes of the type; if there is a
    // unsafe attribute type, then the overall type is also
    // unsafe.
    if (sym->LinkedScope != nullptr and sym->Type != nullptr) {
      for (auto const &attr : GetAllAttrs(*sym)) {
        if (not IsThreadSafeRec(spp::get<1>(attr).Symbol, seen)) { return false; }
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
  LlvmInfo = MakeShared<codegen::LlvmVarSymbolInfo>();
  CompTimeValue = nullptr;
}

VariableSymbol::VariableSymbol(
  VariableSymbol const &that) :
  Name(AstCloneShared(that.Name)),
  Type(that.Type),
  ScopeDefinedIn(that.ScopeDefinedIn),
  Kind(that.Kind),
  OwnParamId(that.OwnParamId),
  BindsParamId(that.BindsParamId),
  IsVariadic(that.IsVariadic),
  IsMutable(that.IsMutable),
  AliasSymbol(that.AliasSymbol),
  NarrowsSymbol(that.NarrowsSymbol),
  CallableAsType(that.CallableAsType),
  Visibility(that.Visibility),
  VisibilityAnnotation(that.VisibilityAnnotation),
  MemInfo(that.MemInfo->Clone()),
  LlvmInfo(MakeShared<codegen::LlvmVarSymbolInfo>()),
  CompTimeValue(AstClone(that.CompTimeValue)) {
  LlvmInfo->Alloca = that.LlvmInfo->Alloca;
}

VariableSymbol::~VariableSymbol() = default;

namespace {
  auto GnTypeParams() -> spp::Map<std::uint64_t, spp::analyse::scopes::TypeSymbol*>& {
    static auto params = spp::Map<std::uint64_t, spp::analyse::scopes::TypeSymbol*>();
    return params;
  }
}

auto spp::analyse::scopes::RegisterGnTypeParam(TypeSymbol &param) -> void {
  GnTypeParams()[param.OwnParamId] = &param;
}

auto spp::analyse::scopes::GnTypeParamOf(const std::uint64_t id) -> TypeSymbol* {
  const auto hit = GnTypeParams().find(id);
  return hit != GnTypeParams().end() ? hit->second : nullptr;
}

namespace {
  auto GnCompParams() -> spp::Map<std::uint64_t, spp::analyse::scopes::VariableSymbol*>& {
    static auto params = spp::Map<std::uint64_t, spp::analyse::scopes::VariableSymbol*>();
    return params;
  }
}

auto spp::analyse::scopes::RegisterGnCompParam(VariableSymbol &param) -> void {
  GnCompParams()[param.OwnParamId] = &param;
}

auto spp::analyse::scopes::GnCompParamOf(const std::uint64_t id) -> VariableSymbol* {
  const auto hit = GnCompParams().find(id);
  return hit != GnCompParams().end() ? hit->second : nullptr;
}

auto spp::analyse::scopes::ClearGnParams() -> void {
  GnTypeParams().clear();
  GnCompParams().clear();
}

auto spp::analyse::scopes::FileInstance(
  const TypeId id, TypeSymbol &instance)
  -> void {
  auto *const tmpl = HeadOf(id).Symbol();
  if (tmpl != nullptr) { tmpl->Instances[BareTypeId(id)] = &instance; }
}

auto spp::analyse::scopes::ParamsOfGroup(
  asts::GenericParameterGroupAst const &params)
  -> TypeIdParams {
  auto out = TypeIdParams();
  for (auto const &param : params.Params) {
    if (const auto pid = param->ParamId(); pid == 0) { continue; }
    else if (param->IsCompParam()) { out.CompParams.push_back(pid); }
    else { out.TypeParams.push_back(pid); }
  }
  return out;
}

auto spp::analyse::scopes::BindByName(
  TypeIdParams const &params,
  const TypeId args,
  const bool all)
  -> std::optional<GenericSubst> {
  if (args == nullptr) { return std::nullopt; }
  const auto arg_list = ArgsOf(args);
  const auto arg_named = [&arg_list](StrView name) -> TypeIdArg const* {
    const auto id = InternWord(name);
    const auto it = genex::find_if(arg_list, [id](auto const &arg) { return arg.Named and arg.Name == id; });
    return it != arg_list.end() ? &*it : nullptr;
  };
  auto subst = GenericSubst();
  for (const auto pid : params.TypeParams) {
    auto const *const param = GnTypeParamOf(pid);
    auto const *const arg = param != nullptr ? arg_named(param->Name->ToView()) : nullptr;
    if (arg == nullptr or arg->TypeVal == nullptr) {
      if (all) { return std::nullopt; }
      continue;
    }
    subst.TypeParams.emplace_back(pid, arg->TypeVal);
    if (param->IsVariadic) { subst.TypePackParams.push_back(pid); }
  }
  for (const auto comp : params.CompParams) {
    auto const *const param = GnCompParamOf(comp);
    auto const *const arg = param != nullptr ? arg_named(param->Name->Val) : nullptr;
    if (arg == nullptr or arg->CompVal == 0) {
      if (all) { return std::nullopt; }
      continue;
    }
    subst.CompParams.emplace_back(comp, arg->CompVal);
    if (param->IsVariadic) { subst.CompPackParams.push_back(comp); }
  }
  return subst;
}

auto spp::analyse::scopes::WrittenTypeIdOf(
  TypeSymbol const &sym)
  -> TypeId {
  using Tag = InstanceKey::Tag;
  if (const auto param = sym.ParamId(); param != 0) { return ParamTypeId(param); }
  auto key = InstanceKey();
  if (sym.Kind == TypeKind::Self) {
    key.Push(Tag::Self);
    return InternTypeKey(std::move(key));
  }
  if (sym.InstanceOf != nullptr and sym.Id != nullptr) { return sym.Id; }
  key.PushPtr(&sym);
  return InternTypeKey(std::move(key));
}

auto spp::analyse::scopes::WrittenCompIdOf(
  VariableSymbol const &sym)
  -> TypeId {
  auto key = InstanceKey();
  key.Push(InstanceKey::Tag::CompId, ParamCompId(sym.ParamId()));
  return InternTypeKey(std::move(key));
}

auto spp::analyse::scopes::NextGnParamId() -> std::uint64_t {
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

  // If this is a generic type, then check the copyable
  // flag on this type, and the bound type.
  if (IsGn()) {
    if (const auto bound = AsBound(); bound != this) {
      return IsDirectlyCopyable or bound->IsCopyable();
    }
  }

  const auto has_generic_args = Name->GnArgGroup != nullptr
    and not Name->GnArgGroup->Args.IsEmpty();

  if (has_generic_args and LinkedScope != nullptr) {
    for (auto const *sup_scope : LinkedScope->GetSupScopes()) {
      if (sup_scope->LinkedTypeSymbol == nullptr) { continue; }
      if (not TypeRef::OfKind(*sup_scope).IsA(*COPY, *sup_scope)) { continue; }

      // "Copy" superimposed over a generic class ("sup [T] P[T]
      // ext Copy") only holds for an instance whose attributes
      // are all copyable: copying "P[Str]" would copy the "Str",
      // which is then destroyed twice. Such an instance is linear.
      return genex::all_of(utils::type_members::GetAllAttrs(*this), [](auto const &attr) {
        auto const *const attr_sym = std::get<1>(attr).Symbol;
        return attr_sym == nullptr or attr_sym->IsCopyable();
      });
    }
    return false;
  }

  // Use the normal test of if this symbol is directly
  // copyable, or if the symbol being derived from is
  // directly copyable.
  return IsDirectlyCopyable
    or (DerivesFromSymbol != nullptr and DerivesFromSymbol->IsCopyable());
}

auto TypeSymbol::IsZeroType() const -> bool {
  return IsDirectlyZeroType
    or (DerivesFromSymbol != nullptr and DerivesFromSymbol->IsZeroType());
}

auto TypeSymbol::IsGn() const -> bool {
  return Kind == TypeKind::GnTypeParam or Kind == TypeKind::GnTypeArg;
}

auto TypeSymbol::IsMock() const -> bool {
  return Kind == TypeKind::FnMock or Kind == TypeKind::ClosureMock;
}

auto TypeSymbol::IsSelf() const -> bool {
  return Kind == TypeKind::Self or (Kind == TypeKind::GnTypeArg and Name->IsSelfType());
}

auto TypeSymbol::IsBareTemplate() const -> bool {
  return Kind == TypeKind::Cls and InstanceOf == nullptr and Alias == nullptr and Type != nullptr
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

auto VariableSymbol::IsGn() const -> bool {
  return Kind == VariableKind::GnCompParam or Kind == VariableKind::GnCompArg;
}

auto VariableSymbol::IsCompTime() const -> bool {
  // Everything with no runtime storage of its own: a "cmp"
  // constant, a function's mock constant, and a comp generic.
  return Kind == VariableKind::Constant or Kind == VariableKind::FnMock or IsGn();
}

auto VariableSymbol::IsImport() const -> bool {
  // Before stage 3 an import is marked by its kind; after, by the
  // target it found (and it takes that target's kind).
  return Kind == VariableKind::Import or AliasSymbol != nullptr;
}

auto VariableSymbol::BoundCompVal() const -> ExpressionAst* {
  // Only a comp generic carries a binding, and only once an
  // argument has been given for it.
  if (Kind != VariableKind::GnCompArg or CompTimeValue == nullptr) { return nullptr; }
  return CompTimeValue->To<ExpressionAst>();
}

auto VariableSymbol::AsBound(
  Scope const &scope) const -> VariableSymbol const* {
  auto const *var = this;
  for (auto depth = 0; depth < 16; ++depth) {
    auto const *const bound = var->BoundCompVal();
    auto const *const bound_id = bound != nullptr ? bound->To<IdentifierAst>() : nullptr;
    auto const *const next = bound_id != nullptr ? scope.FindVarSymbol(bound_id) : nullptr;
    if (next == nullptr or next == var or not next->IsGn()) { break; }
    var = next;
  }
  return var;
}

auto VariableSymbol::FqName() const -> Shared<ExpressionAst> {
  if (IsGn()) { return Name; }

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
  TypeConstraints(generic_constraints),
  Visibility(visibility),
  Convention(std::move(convention)),
  LlvmInfo(MakeShared<codegen::LlvmTypeSymbolInfo>()),
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
  OwnParamId(that.OwnParamId),
  BindsParamId(that.BindsParamId),
  InstanceOf(that.InstanceOf),
  Id(that.Id),
  TypeConstraints(that.TypeConstraints),
  BoundTypeVal(that.BoundTypeVal),
  BoundAlias(that.BoundAlias),
  DerivesFromSymbol(that.DerivesFromSymbol),
  Visibility(that.Visibility),
  Convention(AstClone(that.Convention)),
  IsDirectlyCopyable(that.IsDirectlyCopyable),
  IsDirectlyZeroType(that.IsDirectlyZeroType),
  IsDirectlyThreadHazard(that.IsDirectlyThreadHazard) {
  // Shared rather than cloned: an alias's description is
  // fixed once resolved, and an instantiation of a generic
  // alias builds its own ("CreateGnClsScope") rather
  // than mutating one it was handed.
  Alias = that.Alias;
  LlvmInfo = that.LlvmInfo;
}

TypeSymbol::~TypeSymbol() = default;

auto TypeSymbol::NeedsDeepCopy() const -> bool {
  // An alias is rewritten per instantiation, and a method's "$" mock is the instantiation's own: its function type is
  // read with the block's bindings ("Box[S32]::$Get" is a "FunRef[(&Box[S32],), S32]"), so it is not the template's.
  return Alias != nullptr or Kind == TypeKind::FnMock;
}

auto TypeSymbol::operator==(
  TypeSymbol const &that) const -> bool {
  return this == &that;
}

auto TypeSymbol::GnParams() const -> GenericParameterGroupAst* {
  if (Alias != nullptr) { return Alias->Params.get(); }
  return Type != nullptr ? Type->GnParamGroup.get() : nullptr;
}

auto TypeSymbol::LinkedSymbol() const -> TypeSymbol* {
  // Borrowed: owned by the scope it links, which outlives any caller.
  return LinkedScope != nullptr and LinkedScope->LinkedTypeSymbol != nullptr
    ? LinkedScope->LinkedTypeSymbol.get()
    : const_cast<TypeSymbol*>(this);
}

auto TypeSymbol::Rebind() const -> void {
  // Linked to a bare generic template, which nothing is bound to: the instance its value names (recorded, closed) was
  // not made when it was bound. Re-linked to it once it is.
  if (BoundAlias == nullptr and Kind == TypeKind::GnTypeArg and BoundTypeVal != nullptr and LinkedSymbol() != this) {
    if (LinkedSymbol()->IsBareTemplate()) {
      const auto written = BoundTypeVal->LastTypePart()->WrittenTypeId();
      auto const *const target = IsClosedTypeId(written)
        ? LinkedScope->TypeSymbolOf(written) : nullptr;
      if (target != nullptr and target->LinkedScope != nullptr and target->InstanceOf != nullptr) {
        const_cast<TypeSymbol*>(this)->BindTo(*target);
      }
    }
    return;
  }
  if (BoundAlias == nullptr) { return; }

  // Looked up, not made: a binding is read while keying, which makes nothing. Only once the whole chain has been made
  // is there anything to re-link to.
  auto const *const where = BoundAlias->Alias != nullptr and BoundAlias->Alias->WrittenIn != nullptr
    ? BoundAlias->Alias->WrittenIn : BoundAlias->LinkedScope;
  if (where == nullptr) { return; }
  auto const *const target = BoundAlias->AliasTarget(*where, false);
  if (not target->IsBindTarget()) { return; }
  // As "CreateGnTypeSymbol" would have bound it, had the target been made then.
  const auto self = const_cast<TypeSymbol*>(this);
  self->BindTo(*target);
  self->BoundAlias = nullptr;
}

auto TypeSymbol::LinkTo(
  TypeSymbol const &target) -> void {
  Type = target.Type;
  LinkedScope = target.LinkedScope;
  DerivesFromSymbol = const_cast<TypeSymbol&>(target).SharedFromThis<TypeSymbol>();
  LlvmInfo = target.LlvmInfo;
  InvalidateFqNameCache();
}

auto TypeSymbol::BindTo(
  TypeSymbol const &target) -> void {
  Type = target.Type;
  LinkedScope = target.LinkedScope;
  IsDirectlyCopyable = target.IsDirectlyCopyable;
  IsDirectlyZeroType = target.IsDirectlyZeroType;
  TypeConstraints = target.TypeConstraints;
}

auto TypeSymbol::AsBound() const -> TypeSymbol* {
  auto *s = const_cast<TypeSymbol*>(this);
  for (auto step = 0; step < 8 and (s->IsGn() or s->IsSelf()); ++step) {
    s->Rebind();
    auto *const linked = s->LinkedSymbol();
    if (linked == s) { break; }
    s = linked;
  }
  return s;
}

auto AliasInfo::DeclaredIn() const -> Scope* {
  if (WrittenIn == nullptr) { return nullptr; }
  return WrittenIn->AstNode != nullptr and WrittenIn->AstNode == Stmt ? WrittenIn->Parent : WrittenIn;
}

auto TypeSymbol::AliasTarget(
  Scope const &scope, const bool make) const -> TypeSymbol* {
  // Followed until nothing changes, capped against a cycle ("type A = A" is reported where it is declared). Read where
  // it was written first: its linked scope is the target class's own, where the target's spelling can name that
  // class's parameters instead ("type Mine = Vec[A]" in "sup A", read in "Vec", is "Vec[A=A]").
  const auto read = [make](Scope const *where, TypeAst const &resolved) -> TypeSymbol* {
    if (where == nullptr) { return nullptr; }
    return make ? FindOrMakeTypeSymbol(*where, resolved) : where->FindTypeSymbol(&resolved);
  };
  auto *s = const_cast<TypeSymbol*>(this);
  for (auto step = 0; step < 8 and s->Alias != nullptr and s->Alias->Resolved != nullptr; ++step) {
    auto const &resolved = *s->Alias->Resolved;
    auto *next = read(s->Alias->WrittenIn, resolved);
    if (next == nullptr) { next = read(s->LinkedScope, resolved); }
    if (next == nullptr) { next = read(&scope, resolved); }
    if (next == nullptr or next == s) { break; }
    s = next;
  }
  return s;
}

auto TypeSymbol::AliasTargetId(
  const TypeId args) const -> TypeId {
  if (Alias == nullptr or Alias->Resolved == nullptr) { return nullptr; }
  auto const &resolved = *Alias->Resolved;
  auto const *const where = Alias->WrittenIn;
  // Its stamp, unless that still has an unresolved part (stamped before "Buf::n" could be read): then keyed again.
  auto target = resolved.LastTypePart()->WrittenTypeId();
  if ((target == nullptr or target->HasUnresolved) and where != nullptr) { target = where->TypeIdOf(resolved); }
  if (target == nullptr or args == nullptr or Alias->Params == nullptr) { return target; }

  // The alias's own parameters bound to the arguments; an enclosing block's, which the target can name too, are left.
  // Qualified: GCC ICEs ("lookup_mark") on the unqualified lookup from this member.
  const auto params = spp::analyse::scopes::ParamsOfGroup(*Alias->Params);
  const auto subst = spp::analyse::scopes::BindByName(params, args, false);
  return subst.has_value() ? SubstituteTypeId(target, *subst) : nullptr;
}

auto TypeSymbol::UseTarget() const -> TypeSymbol* {
  // Capped against a cycle, as "AliasTarget" is.
  auto *s = const_cast<TypeSymbol*>(this);
  for (auto step = 0; step < 8 and s->Alias != nullptr and s->Alias->IsFromUseStmt and s->Alias->WrittenIn != nullptr
       and s->Alias->Written != nullptr; ++step) {
    auto *const next = s->Alias->WrittenIn->FindHeadSymbol(*s->Alias->Written);
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
  if (Kind == TypeKind::GnTypeParam and OwnParamId != 0) {
    if (_CachedFqName == nullptr) {
      _CachedFqName = AstCloneShared(Name);
      _CachedFqName->SetWrittenTypeId(spp::analyse::scopes::WrittenTypeIdOf(*this));
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
  if (Kind == TypeKind::GnTypeArg) { Rebind(); }
  if (Kind == TypeKind::GnTypeArg and BoundAlias != nullptr) {
    auto aliased = BoundAlias->FqName();
    return Convention != nullptr ? aliased->WithConvention(AstClone(Convention)) : aliased;
  }
  if (Kind == TypeKind::GnTypeArg and LinkedSymbol() != this) {
    auto bound = LinkedSymbol()->FqName();
    return Convention != nullptr ? bound->WithConvention(AstClone(Convention)) : bound;
  }
  if (Kind == TypeKind::GnTypeArg and BoundTypeVal != nullptr) {
    return BoundTypeVal;
  }

  // If the type is generic (an unbound binding), or is "Self",
  // return the name as-is.
  if (IsGn() or LinkedScope == nullptr or IsSelf()) {
    return Name;
  }

  if (IsMock()
    and (ignore_dollar or LinkedScope->Parent != LinkedScope->GetParentModule())) {
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
    const auto owner_sym = LinkedScope->Parent->FindHeadSymbol(*owner_name);
    if (owner_sym == nullptr or owner_sym == this or owner_sym->IsGn()
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
  // again from the scope asking ("Scope::FindWrittenTypeSymbol"), through that
  // scope's bindings of the generics they name.
  if (Kind == TypeKind::Cls and Alias == nullptr) {
    _CachedFqName->SetWrittenTypeId(spp::analyse::scopes::WrittenTypeIdOf(*this));
  }
  return _CachedFqName;
}

auto TypeSymbol::GnSelfName() const -> Shared<TypeAst> {
  // A template is named as written; as a pattern it is itself
  // over its own parameters. Matched against "sup Limits[U8]"
  // as bare "Limits" it would take every such block (and each
  // one's "SInt"), which "Limits[T=T]" correctly does not.
  auto fq = FqName();
  if (Kind == TypeKind::Cls and InstanceOf == nullptr and Alias == nullptr and Type != nullptr
    and not Type->GnParamGroup->Params.IsEmpty()) {
    return fq->WithGns(GenericArgumentGroupAst::FromParams(*Type->GnParamGroup));
  }
  return fq;
}

auto TypeSymbol::CompArgId(
  Str const &name) const -> CompId {
  // Off the identity, as "TypeArgRef"; anything without one reads its name's argument in its own scope.
  auto const &inst = *LinkedSymbol();
  if (inst.LinkedScope == nullptr) { return 0; }
  if (const auto arg = IdArgNamed(inst, name); arg.has_value()) { return arg->CompVal; }
  auto const *arg = inst.Name->GnArgGroup->At(name.c_str());
  return arg != nullptr and arg->IsCompArg() ? inst.LinkedScope->CompIdOf(*arg->CompVal) : 0;
}

auto TypeSymbol::CompArg(
  Str const &name) const -> Shared<ExpressionAst> {
  // What its identity names ("CompArgId"), as an ast ("Scope::CompAstOf"): a closed value is its literal.
  const auto id = CompArgId(name);
  return id != 0 ? LinkedSymbol()->LinkedScope->CompAstOf(id) : nullptr;
}

auto TypeSymbol::TypeArg(
  Str const &name) const -> Shared<TypeAst> {
  // Off the identity, which holds what each argument means, pointing where its name's argument was written. Only an
  // instantiation has arguments.
  auto const &inst = *LinkedSymbol();
  const auto id_arg = IdArgNamed(inst, name);
  if (not id_arg.has_value() or id_arg->TypeVal == nullptr) { return nullptr; }
  auto type = inst.LinkedScope->TypeAstOf(id_arg->TypeVal);
  auto const *arg = inst.Name->GnArgGroup->At(name.c_str());
  return type != nullptr and arg != nullptr and arg->IsTypeArg() ? type->WithSourceSpanOf(*arg->TypeVal) : type;
}

auto TypeSymbol::TypeArgRef(
  Str const &name) const -> TypeRef {
  // Off the identity, with no ast in between. Only an instantiation has arguments.
  auto const &inst = *LinkedSymbol();
  const auto id_arg = IdArgNamed(inst, name);
  return id_arg.has_value() and id_arg->TypeVal != nullptr ? TypeRef::Of(id_arg->TypeVal, *inst.LinkedScope) : TypeRef();
}

auto TypeSymbol::TypeArgs() const -> Vec<Shared<TypeAst>> {
  // As "TypeArg": off its identity. Only an instantiation has arguments.
  auto const &inst = *LinkedSymbol();
  auto types = Vec<Shared<TypeAst>>();
  if (inst.Id == nullptr or inst.LinkedScope == nullptr or HeadOf(inst.Id).Kind != InstanceKey::Tag::Inst) {
    return types;
  }
  for (auto const &arg : ArgsOf(HeadOf(inst.Id).Args)) {
    if (arg.TypeVal != nullptr) { types.EmplaceBack(inst.LinkedScope->TypeAstOf(arg.TypeVal)); }
  }
  return types;
}

TypeRef::TypeRef(
  TypeSymbol *resolved, const TypeId id, const ConventionTag conv, const bool never) :
  Symbol(resolved),
  Conv(conv),
  IsNever(never),
  Id(resolved != nullptr ? BareTypeId(id) : nullptr) {
}

auto TypeRef::Named(
  Scope const &scope, const TypeId id, TypeSymbol *open, const ConventionTag conv, const bool never,
  const bool make) -> TypeRef {
  // The symbol is what the identity names: the instantiation filed under it, made here first if it is not yet ("open"
  // is the one the lookup reached, instantiated under this scope's bindings). Only where the identity names no one
  // symbol - a part that is "Self" (keyed by spelling, standing for the implementer), a binding to nothing, or nothing
  // made yet where nothing can be - is it "open" itself.
  auto *sym = id != nullptr and not id->HasSelf ? scope.TypeSymbolOf(id) : nullptr;
  if (make and sym == nullptr and id != nullptr and not id->HasSelf and open != nullptr and open->Alias == nullptr) {
    if (HeadOf(id).IsInstance()) {
      auto *const made = MakeOnRead(scope, *open, id);
      sym = scope.TypeSymbolOf(id);

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
  auto *const found = FindOrMakeTypeSymbol(scope, type);
  if (found == nullptr) { return TypeRef(); }
  return Named(
    scope, scope.TypeIdOf(type), found->AliasTarget(scope), found->HeldConvention(&type), type.IsNeverType());
}

auto TypeRef::Of(
  const TypeId id, Scope const &scope, const std::optional<ConventionTag> conv, const std::optional<bool> never)
  -> TypeRef {
  // An identity carries its convention, and "!" is the "Never" class by identity.
  using asts::generate::common_types_precompiled::NEVER;
  const auto own_conv = id != nullptr ? static_cast<ConventionTag>(HeadOf(id).Conv) : ConventionTag::MOV;
  auto ref = Named(scope, id, nullptr, conv.value_or(own_conv), never.value_or(false));
  if (not never.has_value()) { ref.IsNever = ref.Symbol != nullptr and ref.IsA(*NEVER, scope); }
  return ref;
}

auto TypeRef::Of(
  TypeSymbol &sym, Scope const &scope, const std::optional<ConventionTag> conv, const bool make) -> TypeRef {
  // Its identity read here ("Scope::ReadIn"): a parameter as this scope binds it, an open instantiation under this
  // scope's bindings, an alias as its target. Its own scope is not read through: that binds its class's parameters to
  // its arguments, which name the parameters of where it was written ("FunMov[Args=Tup[FunMov[Args, Out], Args]]"
  // would re-read as one more level, without end).
  const auto written = BareTypeId(scope.TypeIdOfSymbol(sym, 0));
  const auto id = sym.LinkedScope == &scope ? written : scope.ReadIn(written);
  auto *const open = sym.Alias != nullptr ? sym.AliasTarget(scope) : &sym;
  return Named(
    scope, id, open, conv.value_or(sym.HeldConvention()), sym.Name != nullptr and sym.Name->IsNeverType(), make);
}

auto TypeRef::OfKind(
  TypeSymbol const &sym, Scope const &scope) -> TypeRef {
  // "Template" reads only the identity and the symbol, so the instantiation filed under it is not looked up.
  const auto written = BareTypeId(scope.TypeIdOfSymbol(sym, 0));
  const auto id = sym.LinkedScope == &scope ? written : scope.ReadIn(written);
  auto *const open = sym.Alias != nullptr ? sym.AliasTarget(scope, false) : const_cast<TypeSymbol*>(&sym);
  return TypeRef(open, id, ConventionTag::MOV, false);
}

auto TypeRef::OfKind(
  Scope const &scope) -> TypeRef {
  return OfKind(*scope.LinkedTypeSymbol, scope);
}

auto TypeRef::ReadIn(
  Scope const &scope, const bool make) const -> TypeRef {
  if (Id == nullptr) { return *this; }
  const auto id = Symbol != nullptr and Symbol->LinkedScope == &scope ? Id : scope.ReadIn(Id);
  return Named(scope, id, Symbol, Conv, IsNever, make);
}

auto TypeRef::Substitute(
  GenericSubst const &subst, Scope const &scope, const bool make) const -> TypeRef {
  if (Id == nullptr) { return *this; }
  return Named(scope, SubstituteTypeId(Id, subst), Symbol, Conv, IsNever, make);
}

auto TypeRef::Template() const -> TypeSymbol* {
  // A symbol whose identity names something unresolved has none, and is read as the symbol it is.
  auto *named = Symbol;
  if (Id != nullptr) {
    auto const &head = HeadOf(Id);
    if (head.IsInstance()) { return head.Symbol(); }
    if (head.Kind == InstanceKey::Tag::Symbol and head.Symbol() != nullptr) { named = head.Symbol(); }
  }
  if (named == nullptr) { return nullptr; }
  auto *const bound = named->AsBound();
  return bound->InstanceOf != nullptr ? bound->InstanceOf : bound;
}

auto spp::analyse::scopes::PrecompiledTemplate(
  asts::TypeAst const &tmpl, Scope const &scope) -> TypeSymbol* {
  // Only a type held shared is cached, so the address it is keyed by stays its own.
  auto &cache = asts::generate::common_types_precompiled::TEMPLATE_SYMBOLS;
  if (const auto hit = cache.find(&tmpl); hit != cache.end()) { return hit->second.second; }
  auto *sym = scope.FindTypeSymbol(&tmpl);
  if (sym == nullptr) { return nullptr; }
  sym = sym->AliasTarget(scope);
  if (sym->InstanceOf != nullptr) { sym = sym->InstanceOf; }
  if (auto held = static_shared_cast<asts::TypeAst const>(tmpl.weak_from_this().lock()); held != nullptr) {
    cache.emplace(&tmpl, std::make_pair(std::move(held), sym));
  }
  return sym;
}

auto TypeRef::IsA(
  TypeAst const &tmpl, Scope const &scope) const -> bool {
  auto *const mine = Template();
  return mine != nullptr and mine == spp::analyse::scopes::PrecompiledTemplate(tmpl, scope);
}

auto TypeRef::SameAs(
  TypeRef const &that) const -> bool {
  return Id != nullptr and Id == that.Id and Conv == that.Conv;
}

auto TypeRef::AstIn(
  Scope const &scope) const -> Shared<TypeAst> {
  if (Id == nullptr) { return nullptr; }
  auto type = Symbol != nullptr ? AstCloneShared(Symbol->FqName()) : scope.TypeAstOf(Id);
  if (type == nullptr or Conv == ConventionTag::MOV) { return type; }
  auto conv = Conv == ConventionTag::MUT
    ? Unique<ConventionAst>(MakeUnique<ConventionMutAst>(nullptr, nullptr))
    : Unique<ConventionAst>(MakeUnique<ConventionRefAst>(nullptr));
  return type->WithConvention(std::move(conv));
}

auto VariableSymbol::TypeRefIn(
  Scope const &scope) const -> TypeRef {
  return Type != nullptr ? TypeRef::Of(*Type, scope) : TypeRef{};
}

auto TypeSymbol::HeldConvention(
  TypeAst const *written) const -> ConventionTag {
  if (written != nullptr and written->GetConvention() != nullptr) { return written->GetConvention()->Tag(); }
  return Kind == TypeKind::GnTypeArg and Convention != nullptr ? Convention->Tag() : ConventionTag::MOV;
}

auto TypeSymbol::InvalidateFqNameCache() const
  -> void {
  _CachedFqName = nullptr;
  _CachedFqNameGen = 0;
}

SPP_MOD_END
