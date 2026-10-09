module;
#include <spp/macros.hpp>

module spp.analyse.scopes.symbols;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.substitution;
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

  auto NextTypeSymbolSerial() -> std::uint64_t {
    static auto serial = static_cast<std::uint64_t>(0);
    return ++serial;
  }

  /// The one place reading a type makes an instantiation:
  /// "id" (already read where "scope" reads it), standing
  /// for "open", an open instantiation (of a class or an
  /// alias).
  auto MakeOnRead(
    Scope const &scope, TypeSymbol const &open,
    const TypeId id) -> TypeSymbol* {
    using spp::analyse::scopes::TypeKind;
    // Check that the id is valid, and that the open type
    // symbol is an instantiation of a template. For an
    // unformed alias, ignore too.
    if (id == nullptr or open.InstanceOf == nullptr) { return nullptr; }
    if (open.Kind != TypeKind::Cls and open.Alias == nullptr) { return nullptr; }

    // Create the monomorphised version of the type symbol
    // for "id" within "scope".
    return spp::analyse::utils::monomorphization::InstantiateForScope(
      id, scope);
  }

  /// An instantiation's argument for its template's parameter "name" ("TypeSymbol::TypeArgRef",
  /// "TypeSymbol::CompArgId"): the name is read against the template's own parameters, once, and the argument found by
  /// that parameter's identity ("FindArgOf"). A variadic parameter finds nothing: its elements are positional, and no
  /// one argument is its value. The kind is as found, so a caller reads the field it wants, and gets null from a name
  /// of the other kind (each parameter's name has one kind).
  auto IdArgNamed(
    TypeSymbol const &inst, spp::Str const &name) -> std::optional<TypeIdArg> {
    using namespace spp::analyse::scopes;
    if (inst.Id == nullptr or inst.LinkedScope == nullptr or HeadOf(inst.Id).Kind != TypeKey::Tag::Inst) {
      return std::nullopt;
    }
    auto const *const tmpl = HeadOf(inst.Id).Symbol();
    auto const *const params = tmpl != nullptr ? tmpl->GnParams() : nullptr;
    if (params == nullptr) { return std::nullopt; }
    for (auto const *param : params->GetAllParams()) {
      if (param->Name->ToString() == name and param->ParamId() != 0) {
        return FindArgOf(HeadOf(inst.Id).Args, param->ParamId());
      }
    }
    return std::nullopt;
  }

  /// What a written type names here ("Scope::FindTypeSymbol");
  /// else, for a name written as an open instantiation not
  /// made from here yet, the one its arguments name here,
  /// made now ("MakeOnRead").
  auto FindOrMakeTypeSymbol(
    Scope const &scope, TypeAst const &type) -> TypeSymbol* {
    // Find the type symbol. If there is type symbol for the
    // type name, then return it.
    if (const auto sym = scope.FindTypeSymbol(&type); sym != nullptr) {
      return sym;
    }

    // The open instantiation the type was stamped with, read
    // raw from its template's instances (not re-read through
    // this scope's bindings, which is what found nothing); the
    // stamps are read in the order "FindTypeSymbol" reads them.
    // Make the instantiation its identity names here.
    auto const *const last = type.LastTypePart();
    auto open = scope.FindTypeSymbolById(type.StampedTypeId());
    if (open == nullptr and last != &type) { open = scope.FindTypeSymbolById(last->StampedTypeId()); }
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
    using generate::common_types_precompiled::THREAD_SAFE;

    const auto scope = sym.ScopeDefinedIn != nullptr
      ? sym.ScopeDefinedIn
      : sym.LinkedScope;
    if (scope == nullptr) { return false; }

    // A constraint naming the special "ThreadSafe" type (or an
    // alias of it), as the template it stands for.
    return genex::any_of(sym.TypeConstraints, [&](auto const &c) {
      const auto head = TypeRef::ForKindCheck(*c, *scope);
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
      if (head.Kind == TypeKey::Tag::Inst) {
        for (auto const &arg : ArgsOf(head.Args)) { if (arg.TypeVal != nullptr) { arg_ids.EmplaceBack(arg.TypeVal); } }
      }
      else if (head.Kind == TypeKey::Tag::Variant) { arg_ids.AppendRange(head.Members); }
      for (const auto arg_id : arg_ids) {
        if (not IsThreadSafeRec(arg_scope->FindTypeSymbolById(arg_id), seen)) { return false; }
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
  CompTimeValue(AstClone(that.CompTimeValue)),
  BoundCompId(that.BoundCompId) {
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

auto spp::analyse::scopes::FindGnTypeParamById(const std::uint64_t id) -> TypeSymbol* {
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

auto spp::analyse::scopes::FindGnCompParamById(const std::uint64_t id) -> VariableSymbol* {
  const auto hit = GnCompParams().find(id);
  return hit != GnCompParams().end() ? hit->second : nullptr;
}

namespace {
  /// The variadic packs given a parameter identity ("GnPackParamId"): by pack, and each identity's name.
  struct GnPackParams {
    spp::Map<void const*, std::uint64_t> Ids;
    spp::Map<std::uint64_t, spp::Str> Names;
  };

  auto GnPacks() -> GnPackParams& {
    static auto packs = GnPackParams();
    return packs;
  }
}

auto spp::analyse::scopes::ClearGnParams() -> void {
  GnTypeParams().clear();
  GnCompParams().clear();
  GnPacks().Ids.clear();
  GnPacks().Names.clear();
}

auto spp::analyse::scopes::GnPackParamId(
  void const *pack, Str const &name) -> std::uint64_t {
  // Minted the first time a pack is asked for, and the same identity every time after.
  auto &packs = GnPacks();
  if (const auto hit = packs.Ids.find(pack); hit != packs.Ids.end()) { return hit->second; }
  const auto id = NextGnParamId();
  packs.Ids.emplace(pack, id);
  packs.Names.emplace(id, name);
  return id;
}

auto spp::analyse::scopes::ArgNameOf(
  TypeIdArg const &arg) -> Str {
  if (not arg.Named) { return Str(); }
  return arg.Spelled ? Str(TextOfWord(arg.Slot)) : ParamNameOf(arg.Slot);
}

auto spp::analyse::scopes::ParamNameOf(
  const std::uint64_t id) -> Str {
  // "Self" is parameter 0; otherwise whichever registry holds the identity.
  if (id == 0) { return "Self"; }
  if (auto const *const type = FindGnTypeParamById(id); type != nullptr) { return Str(type->Name->ToView()); }
  if (auto const *const comp = FindGnCompParamById(id); comp != nullptr) { return comp->Name->Val; }
  const auto hit = GnPacks().Names.find(id);
  return hit != GnPacks().Names.end() ? hit->second : Str();
}

auto spp::analyse::scopes::NameTypeIdOf(
  TypeSymbol const &sym)
  -> TypeId {
  if (const auto param = sym.ParamId(); param != 0) { return ParamTypeId(param); }
  if (sym.Kind == TypeKind::Self) { return ParamTypeId(0); }
  if (sym.InstanceOf != nullptr and sym.Id != nullptr) { return sym.Id; }
  auto key = TypeKey();
  key.PushSymbol(&sym);
  return InternTypeKey(std::move(key));
}


auto spp::analyse::scopes::NextGnParamId() -> std::uint64_t {
  // Never zero, which is what a symbol that is not a parameter
  // holds.
  static auto next = static_cast<std::uint64_t>(0);
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

  // An instantiation (one of a generic class's): its "Copy" may
  // hold only for some arguments.
  if (InstanceOf != nullptr and LinkedScope != nullptr) {
    for (auto const *sup_scope : LinkedScope->GetSupScopes()) {
      if (sup_scope->LinkedTypeSymbol == nullptr) { continue; }
      if (not TypeRef::ForKindCheck(*sup_scope).IsA(*COPY, *sup_scope)) { continue; }

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

auto TypeSymbol::IsImport() const -> bool {
  return Alias != nullptr and Alias->IsFromUseStmt;
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

auto VariableSymbol::AsBound() const -> VariableSymbol const* {
  // The chain was followed where the argument was written ("BoundCompId"): a parameter at its end is that parameter.
  if (BoundCompId != nullptr and BoundCompId->Kind == CompKey::Part::Param) {
    if (auto const *const param = FindGnCompParamById(BoundCompId->ParamId); param != nullptr) { return param; }
  }
  return this;
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
  const ConventionTag convention,
  Vec<Shared<TypeAst>> const &generic_constraints) :
  Name(std::move(name)),
  Type(type),
  LinkedScope(scope),
  ScopeDefinedIn(scope_defined_in),
  Kind(kind),
  TypeConstraints(generic_constraints),
  Visibility(visibility),
  Convention(convention),
  LlvmInfo(MakeShared<codegen::LlvmTypeSymbolInfo>()),
  IsDirectlyCopyable(is_directly_copyable),
  IsDirectlyZeroType(false),
  Serial(NextTypeSymbolSerial()) {
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
  DerivesFromSymbol(that.DerivesFromSymbol),
  Visibility(that.Visibility),
  Convention(that.Convention),
  IsDirectlyCopyable(that.IsDirectlyCopyable),
  IsDirectlyZeroType(that.IsDirectlyZeroType),
  IsDirectlyThreadHazard(that.IsDirectlyThreadHazard),
  Serial(NextTypeSymbolSerial()) {
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

auto TypeSymbol::GnParamsScope() const -> Scope* {
  return Alias != nullptr ? Alias->DeclaredIn() : LinkedScope;
}

auto TypeSymbol::LinkedSymbol() const -> TypeSymbol* {
  // Borrowed: owned by the scope it links, which outlives any caller.
  return LinkedScope != nullptr and LinkedScope->LinkedTypeSymbol != nullptr
    ? LinkedScope->LinkedTypeSymbol.get()
    : const_cast<TypeSymbol*>(this);
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
  // Followed link by link until a symbol links to itself, or the walk comes back round ("Self" stand-ins can name each
  // other). The loop is found by Brent's method: a marker left at each power of two, met again only on a loop, so each
  // link is followed once, and nothing is allocated.
  auto *s = const_cast<TypeSymbol*>(this);
  auto *marker = s;
  auto power = 1uz;
  auto steps = 0uz;
  while (s->IsGn() or s->IsSelf()) {
    auto *const linked = s->LinkedSymbol();
    if (linked == s or linked == marker) { break; }
    s = linked;
    if (++steps == power) {
      marker = s;
      power *= 2;
      steps = 0;
    }
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
  auto target = resolved.LastTypePart()->StampedTypeId();
  if ((target == nullptr or target->HasUnresolved) and where != nullptr) { target = where->TypeIdOf(resolved); }
  if (target == nullptr or args == nullptr or Alias->Params == nullptr) { return target; }

  // The alias's own parameters bound to the arguments; an enclosing block's, which the target can name too, are left.
  // Qualified: GCC ICEs ("lookup_mark") on the unqualified lookup from this member.
  const auto params = spp::analyse::scopes::ParamsDeclaredBy(*Alias->Params);
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

auto TypeSymbol::InstanceTemplate() const -> TypeSymbol* {
  auto *const target = UseTarget();
  return target->Alias == nullptr ? target : const_cast<TypeSymbol*>(this);
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
      _CachedFqName->StampTypeId(spp::analyse::scopes::NameTypeIdOf(*this));
    }
    return _CachedFqName;
  }

  // A binding answers with what it is bound to: the parameter
  // it passes along, or the type it was given. Its own name
  // is only its spelling, which means something else wherever
  // it is read. A binding with no scope to reach records the
  // argument it was given instead.
  if (Kind == TypeKind::GnTypeArg and LinkedSymbol() != this) {
    auto bound = LinkedSymbol()->FqName();
    return Convention != ConventionTag::MOV ? bound->WithConvention(spp::analyse::scopes::ConventionAstOf(Convention)) : bound;
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

  // Fully qualify the name from the root scope. A class's name
  // records its identity on its last part (below), so it is
  // built from a copy: "Name" is shared with the declaration.
  const auto records_id = Kind == TypeKind::Cls and Alias == nullptr;
  auto qualifier_scope = LinkedScope->Parent;
  auto qualified_name = records_id ? AstCloneShared(Name) : dynamic_shared_cast<TypeAst>(Name);
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
  _CachedFqName = Convention != ConventionTag::MOV
    ? qualified_name->WithConvention(spp::analyse::scopes::ConventionAstOf(Convention))
    : qualified_name;
  _CachedFqNameGen = ScopeLinkageGeneration();

  // A class's name records its identity. A closed one means it
  // from anywhere; an open instantiation's arguments are read
  // again from the scope asking ("Scope::FindBoundTypeSymbolById"),
  // through that scope's bindings of the generics they name. It
  // is on the last part, where every stamp is read from (as
  // "type_resolution::StampTypeParts" records them), and on the
  // whole name too, which "Scope::FindTypeSymbol" reads first.
  if (records_id) {
    const auto id = spp::analyse::scopes::NameTypeIdOf(*this);
    _CachedFqName->StampTypeId(id);
    _CachedFqName->LastTypePart()->StampTypeId(id);
  }
  return _CachedFqName;
}

auto TypeSymbol::CompArgId(
  Str const &name) const -> CompId {
  // Off the identity, as "TypeArgRef".
  auto const &inst = *LinkedSymbol();
  const auto id_arg = IdArgNamed(inst, name);
  return id_arg.has_value() ? id_arg->CompVal : nullptr;
}

auto TypeSymbol::TypeArgRef(
  Str const &name) const -> TypeRef {
  // Get the linked symbol and create the empty type arg
  // list.
  auto const &inst = *LinkedSymbol();
  const auto id_arg = IdArgNamed(inst, name);
  return id_arg.has_value() and id_arg->TypeVal != nullptr
    ? TypeRef::Of(id_arg->TypeVal, *inst.LinkedScope)
    : TypeRef();
}

/// [CHECKED]
auto TypeSymbol::TypeArgRefs() const -> Vec<TypeRef> {
  // Get the linked symbol and create the empty type arg
  // list.
  auto const &inst = *LinkedSymbol();
  auto types = Vec<TypeRef>();

  // If the instantiation is not a type key "Inst" tag
  // type, then return the empty type list.
  if (inst.Id == nullptr or inst.LinkedScope == nullptr or HeadOf(inst.Id).Kind != TypeKey::Tag::Inst) {
    return types;
  }

  // Otherwise, extract the args off of the head (all by
  // identity), and keep the type args only, as refs.
  for (auto const &arg : ArgsOf(HeadOf(inst.Id).Args)) {
    if (arg.TypeVal != nullptr) {
      types.EmplaceBack(TypeRef::Of(arg.TypeVal, *inst.LinkedScope));
    }
  }
  return types;
}

/// [CHECKED]
auto TypeSymbol::GnSelfName() const -> Shared<TypeAst> {
  // This effectively takes a symbol like "Vec" (a template),
  // and create "Vec[T=T]" from it, when it is a template with
  // generics ("IsBareTemplate"; see the "Vec" example).
  auto fq = FqName();
  if (IsBareTemplate()) {
    // Attach the generics.
    return fq->WithGns(
      GenericArgumentGroupAst::FromParams(*Type->GnParamGroup));
  }
  return fq;
}

/// [CHECKED]
TypeRef::TypeRef(
  TypeSymbol *resolved,
  const TypeId id,
  const ConventionTag conv,
  const bool never) :
  Symbol(resolved),
  Conv(conv),
  IsNever(never),
  Id(resolved != nullptr ? BareOf(id) : nullptr) {
}

/// [CHECKED]
auto TypeRef::FromId(
  Scope const &scope, const TypeId id, TypeSymbol *open, const ConventionTag conv,
  const bool never, const OnMissing missing) -> TypeRef {
  // Find the type symbol by its id (as long as it doesn't
  // contain "Self"). Make the symbol if it's missing and
  // the flag allows creation.
  auto sym = id != nullptr and not id->HasSelf ? scope.FindTypeSymbolById(id) : nullptr;
  if (missing == OnMissing::Make and sym == nullptr and id != nullptr
    and not id->HasSelf and open != nullptr and open->Alias == nullptr) {
    if (HeadOf(id).IsInstance()) {
      static_cast<void>(MakeOnRead(scope, *open, id));
      sym = scope.FindTypeSymbolById(id);
    }
  }

  // Build the type ref from the symbol and the carried
  // flags.
  return TypeRef(
    sym != nullptr ? sym : missing == OnMissing::Null ? nullptr : open,
    id, conv, never);
}

/// [CHECKED]
auto TypeRef::Of(
  TypeAst const &type, Scope const &scope) -> TypeRef {
  // A resolved type is never an alias: it is what the
  // alias stands for ("AliasTarget"). The alias stays
  // on the written type, for visibility and messages.
  const auto found = FindOrMakeTypeSymbol(scope, type);
  if (found == nullptr) { return TypeRef(); }

  // Build the type ref from the newly created "found",
  // which is a found or monomorphised
  return FromId(
    scope, scope.TypeIdOf(type), found->AliasTarget(scope),
    found->HeldConvention(&type), type.IsNeverType());
}

/// [CHECKED]
auto TypeRef::Of(
  TypeSymbol &sym, Scope const &scope,
  const std::optional<ConventionTag> conv,
  const OnMissing missing) -> TypeRef {
  // Get the id of the type symbol, and get the "bare" id
  // version of it.
  const auto written = BareOf(scope.TypeIdOfSymbol(sym));
  const auto id = sym.LinkedScope == &scope
    ? written
    : scope.ReadIn(written);

  // Move through the alias if there is one, via the alias
  // target in the scope, then call the "FromId" function
  // too.
  const auto open = sym.Alias != nullptr ? sym.AliasTarget(scope) : &sym;
  return FromId(
    scope, id, open, conv.value_or(sym.HeldConvention()),
    sym.Name != nullptr and sym.Name->IsNeverType(), missing);
}

/// [CHECKED]
auto TypeRef::Of(
  const TypeId id, Scope const &scope,
  const std::optional<ConventionTag> conv,
  const std::optional<bool> never) -> TypeRef {
  // An identity carries its convention, and "!" is the
  // "Never" class by identity. This is the main function
  // to create a type ref.
  using generate::common_types_precompiled::NEVER;
  const auto own_conv = id != nullptr
    ? static_cast<ConventionTag>(HeadOf(id).Conv)
    : ConventionTag::MOV;

  // Call into the "FromId" function with the "null" flag
  // for a non-existing symbol; this is purely an accessor.
  auto ref = FromId(
    scope, id, nullptr, conv.value_or(own_conv),
    never.value_or(false), OnMissing::Null);

  // If "never" has been set, then set the symbol's flag
  // to a comparison with the precompiled NEVER type.
  if (not never.has_value()) {
    ref.IsNever = ref.Symbol != nullptr and ref.IsA(*NEVER, scope);
  }
  return ref;
}

auto TypeRef::ForKindCheck(
  TypeSymbol const &sym, Scope const &scope) -> TypeRef {
  // "Template" reads only the identity and the symbol, so the instantiation filed under it is not looked up.
  const auto written = BareOf(scope.TypeIdOfSymbol(sym));
  const auto id = sym.LinkedScope == &scope
    ? written
    : scope.ReadIn(written);

  const auto open = sym.Alias != nullptr
    ? sym.AliasTarget(scope, false)
    : const_cast<TypeSymbol*>(&sym);
  return TypeRef(open, id, ConventionTag::MOV, false);
}

auto TypeRef::ForKindCheck(
  TypeAst const &type, Scope const &scope) -> TypeRef {
  // Held as written: a borrow or "!" stays one, so the same checks reject it.
  auto *const head = type.IsNeverType() ? nullptr : scope.FindHeadSymbol(type);
  if (head == nullptr) { return TypeRef(); }
  auto ref = ForKindCheck(*head, scope);
  if (type.GetConvention() != nullptr) { ref.Conv = type.GetConvention()->Tag(); }
  return ref;
}

auto TypeRef::ForKindCheck(
  Scope const &scope) -> TypeRef {
  return ForKindCheck(*scope.LinkedTypeSymbol, scope);
}

/// [CHECKED]
auto TypeRef::ReadIn(
  Scope const &scope, const OnMissing missing) const -> TypeRef {
  // Read a type ref into another scope, using the scope's
  // bindings to canon it. Skip a canon if not required.
  if (Id == nullptr) { return *this; }
  const auto id = Symbol != nullptr and Symbol->LinkedScope == &scope
    ? Id
    : scope.ReadIn(Id);

  // Use the new id to build a type ref from the "FromId"
  // method.
  return FromId(
    scope, id, Symbol, Conv, IsNever, missing);
}

/// [CHECKED]
auto TypeRef::Substitute(
  GenericSubst const &subst, Scope const &scope,
  const OnMissing missing) const -> TypeRef {
  // Substitute the type by its id and substitution pack,
  // and send in all the current info (symbol, conv, never
  // flag etc).
  if (Id == nullptr) { return *this; }
  return FromId(
    scope, SubstituteTypeId(Id, subst), Symbol, Conv,
    IsNever, missing);
}

/// [CHECKED]
auto TypeRef::Template() const -> TypeSymbol* {
  // Get the head of the symbol, and if it is an instance,
  // return the symbol on the head. If the head's kind is
  // a symbol, set the tracking symbol to it. Importantly,
  // it is not an instantiation (concrete like S32, or
  // template like Vec).
  auto named = Symbol;
  if (Id != nullptr) {
    auto const &head = HeadOf(Id);
    if (head.IsInstance()) { return head.Symbol(); }
    if (head.Kind == TypeKey::Tag::Symbol and head.Symbol() != nullptr) { named = head.Symbol(); }
  }
  if (named == nullptr) { return nullptr; }

  // Get the bound version of the symbol through its linked
  // scope, and get the bound symbol's "template" if it has
  // one, otherwise it itself is the template, so return it.
  const auto bound = named->AsBound();
  return bound->InstanceOf != nullptr ? bound->InstanceOf : bound;
}

/// [CHECKED]
auto spp::analyse::scopes::ConventionAstOf(
  const ConventionTag tag) -> Unique<ConventionAst> {
  // The convention ast for the tag; nothing for "MOV".
  if (tag == ConventionTag::MUT) { return MakeUnique<ConventionMutAst>(nullptr, nullptr); }
  if (tag == ConventionTag::REF) { return MakeUnique<ConventionRefAst>(nullptr); }
  return nullptr;
}

auto spp::analyse::scopes::PrecompiledTemplate(
  TypeAst const &tmpl, Scope const &scope) -> TypeSymbol* {
  // Extract from the cache if its available and return the
  // value. If not, The cache will be updated at the end of
  // the function.
  auto &cache = generate::common_types_precompiled::TEMPLATE_SYMBOLS;
  if (const auto hit = cache.find(&tmpl); hit != cache.end()) { return hit->second.second; }

  // Find the template symbol from the scope, and map through
  // its alias target if it has one. Nullptr guard for safety.
  auto sym = scope.FindTypeSymbol(&tmpl);
  if (sym == nullptr) { return nullptr; }
  // Nothing is made: this backs "TypeRef::IsA", which kind
  // checks call while the "sup" scopes attach.
  sym = sym->AliasTarget(scope, false);

  // If this symbol contains a template (ie isn't a template
  // itself), then move into the template.
  if (sym->InstanceOf != nullptr) { sym = sym->InstanceOf; }

  // Update the cache so next time it comes directly out of
  // the cache map. Return the symbol.
  if (auto held = static_shared_cast<TypeAst const>(
    tmpl.weak_from_this().lock()); held != nullptr) {
    cache.emplace(&tmpl, std::make_pair(std::move(held), sym));
  }
  return sym;
}

/// [CHECKED]
auto TypeRef::IsA(
  TypeAst const &tmpl, Scope const &scope) const -> bool {
  // Get the template symbol for this type ref, and check by
  // pointer against the precompiled template for the type
  // passed in.
  const auto mine = Template();
  return mine != nullptr and mine == spp::analyse::scopes::PrecompiledTemplate(
    tmpl, scope);
}

/// [CHECKED]
auto TypeRef::SameAs(
  TypeRef const &that) const -> bool {
  // Two type refs are the same if their ids are the same,
  // and their conventions are the same too.
  return Id != nullptr and Id == that.Id and Conv == that.Conv;
}

/// [CHECKED]
auto TypeRef::AstIn(
  Scope const &scope) const -> Shared<TypeAst> {
  // Get the type ast from either the symbols fq name (should
  // the symbol exist), and otherwise the scope's "type ast of"
  // for the id.
  if (Id == nullptr) { return nullptr; }
  auto type = Symbol != nullptr ? AstCloneShared(Symbol->FqName()) : scope.TypeAstOf(Id);
  if (type == nullptr or Conv == ConventionTag::MOV) { return type; }

  // Given a convention is now guaranteed, build the convention
  // ast and add it into the type.
  return type->WithConvention(spp::analyse::scopes::ConventionAstOf(Conv));
}

/// [CHECKED]
auto VariableSymbol::TypeRefIn(
  Scope const &scope) const -> TypeRef {
  // Take the current type and re-create it in the provided
  // scope. Null guard (should never be hit).
  return Type != nullptr ? TypeRef::Of(*Type, scope) : TypeRef{};
}

auto TypeSymbol::HeldConvention(
  TypeAst const *written) const -> ConventionTag {
  // Pull the convention from the "written" if it has one,
  // otherwise the symbol's convention for the generic types.
  if (written != nullptr and written->GetConvention() != nullptr) {
    return written->GetConvention()->Tag();
  }

  return Kind == TypeKind::GnTypeArg
    ? Convention
    : ConventionTag::MOV;
}

/// [CHECKED]
auto TypeSymbol::InvalidateFqNameCache() const -> void {
  // Clear caches.
  _CachedFqName = nullptr;
  _CachedFqNameGen = 0;
}

SPP_MOD_END
