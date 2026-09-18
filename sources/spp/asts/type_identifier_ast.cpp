module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_identifier_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.monomorphization_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.analyse.utils.visibility_utils;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.object_initializer_argument_group_ast;
import spp.asts.object_initializer_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.type_unary_expression_operator_ast;
import spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import genex;

SPP_MOD_BEGIN
auto TypeIdentifierAst::FromIdentifier(
  IdentifierAst const &identifier) -> Shared<TypeIdentifierAst> {
  return MakeShared<TypeIdentifierAst>(identifier.PosStart(), Str(identifier.Val), nullptr);
}

auto TypeIdentifierAst::FromString(
  Str const &identifier) -> Shared<TypeIdentifierAst> {
  return MakeShared<TypeIdentifierAst>(0uz, Str(identifier), nullptr);
}

TypeIdentifierAst::TypeIdentifierAst(
  const std::size_t pos,
  decltype(Name) &&name,
  decltype(GnArgGroup) generic_arg_group) :
  Name(std::move(name)),
  GnArgGroup(std::move(generic_arg_group)),
  _Pos(pos),
  _IsNeverType(false),
  _IsSelfType(false),
  _HasAnalysed(false),
  _Resolved(false),
  _IsSourceWritten(false) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->GnArgGroup);
  if (Name == "Self") { _IsSelfType = true; }
}

TypeIdentifierAst::~TypeIdentifierAst() = default;

auto TypeIdentifierAst::operator<=>(
  const TypeIdentifierAst &that) const -> Ordering {
  return EqualsTypeIdentifier(that);
}

auto TypeIdentifierAst::operator==(
  const TypeIdentifierAst &that) const -> bool {
  return EqualsTypeIdentifier(that) == Ordering::equal;
}

auto TypeIdentifierAst::EqualsTypeIdentifier(
  TypeIdentifierAst const &other) const -> Ordering {
  // Equality is based on the name and generics.
  return Name == other.Name and *GnArgGroup == *other.GnArgGroup
    ? Ordering::equal
    : Ordering::less;
}

auto TypeIdentifierAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsTypeIdentifier(*this);
}

auto TypeIdentifierAst::PosStart() const -> std::size_t {
  // Use the static pos field, unless this replaces a written type.
  if (_HasSourceSpan) { return _SpanStart; }
  return _Pos;
}

auto TypeIdentifierAst::PosEnd() const -> std::size_t {
  // Use the final generic argument or name.
  if (_HasSourceSpan) { return _SpanEnd; }
  return _Pos + Name.length();
}

auto TypeIdentifierAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto t = MakeUnique<TypeIdentifierAst>(
    _Pos,
    Str(Name),
    AstClone(GnArgGroup));
  t->_IsNeverType = _IsNeverType;
  t->_IsSourceWritten = _IsSourceWritten;
  t->_Stamp = _Stamp;
  t->_TemplateStamp = _TemplateStamp;
  CopySourceSpanTo(*t);
  return t;
}

auto TypeIdentifierAst::ToString() const -> Str {
  // Reuse the cached stringification (identical content: Name + GnArgGroup->ToString()) to avoid rebuilding the
  // string (and recursively re-stringifying every generic argument) on each call.
  return Str(ToView());
}

auto TypeIdentifierAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Todo: Add higher order generic checks into the unit tests (self and generic type).
  using analyse::utils::generic_bindings::EnforceGenericConstraintsAllArgs;
  using analyse::utils::generic_bindings::InferGnArgs;
  using analyse::utils::generic_bindings::NameGnArgs;
  using analyse::utils::monomorphization_utils::CreateGenericClsScope;
  using analyse::utils::type_utils::GetTypeSymOrError;
  using analyse::utils::type_members::GetUnimplementedAbstractMethods;
  using analyse::utils::type_predicates::IsTupSymbol;
  using analyse::utils::type_compare::TypeEq;
  using analyse::utils::visibility_utils::CheckModuleTypeVisibility;
  using analyse::utils::visibility_utils::CheckTypeTypeVisibility;
  using analyse::errors::SemanticError;
  using analyse::errors::SppAbstractTypeUseError;
  using analyse::errors::SppHigherOrderGenericsNotSupportedError;
  using generate::common_types::SelfType;
  using generate::common_types_precompiled::TUP;

  // Reject abstract types everywhere except the few positions that name a type without ever producing a value of it.
  // Only allow an abstract self if we are in the abstract class itself. For example, `Clone::clone_from` must be allowed
  // to use `Clone::clone` as the default, which returns "Self", but will never be used from `Clone`, but rather the
  // implementation type.
  const auto check_abstract = [&](Scope const &scope) {
    if (meta->AllowAbstractType or meta->CurrentStage < CompilerStage::kPreAnalyseSemantics) { return; }
    const auto resolved_sym = scope.GetTypeSymbol(this);
    if (resolved_sym == nullptr or resolved_sym->IsTypeGeneric() or resolved_sym->LinkedScope == nullptr) { return; }
    const auto unimplemented = GetUnimplementedAbstractMethods(*resolved_sym->LinkedScope);
    if (unimplemented.IsEmpty()) { return; }
    const auto self_sym = sm->CurrentScope->GetTypeSymbol(SelfType(0).get());
    if (self_sym == nullptr or self_sym->LinkedScope != resolved_sym->LinkedScope) {
      Raise<SppAbstractTypeUseError>(
        {unimplemented[0]->GetAstScope(), sm->CurrentScope}, ERR_ARGS(*this, *unimplemented[0]));
    }
  };

  // An analysed node is still checked: a symbol's cached qualified name is shared, and can first be analysed where an
  // abstract type is allowed (a function type's arguments), then reached where it is not (an attribute's type).

  if (_HasAnalysed) {
    check_abstract(meta->TypeAnalysisTypeScope ? *meta->TypeAnalysisTypeScope : *sm->CurrentScope);
    return;
  }

  // An instantiation that keeps nesting ("Box[T]" inside "Box"'s own "sup", with "T" bound to "Box[T]") never ends. It
  // is reported, rather than left to overflow the stack.
  static thread_local auto nesting = 0uz;
  struct NestingGuard {
    decltype(nesting) &Depth;
    explicit NestingGuard(decltype(nesting) &depth) : Depth(depth) { ++Depth; }
    ~NestingGuard() { --Depth; }
  } const nesting_guard(nesting);
  RaiseIf<analyse::errors::SppGenericInstantiationDepthError>(
    nesting > 128, {sm->CurrentScope}, ERR_ARGS(*this));
  RaiseIf<SppHigherOrderGenericsNotSupportedError>(
    Name == "Self" and GnArgGroup != nullptr and not GnArgGroup->Args.IsEmpty(),
    {sm->CurrentScope}, ERR_ARGS(*this, *GnArgGroup));
  if (Name == "Self" and meta->CurrentStage<CompilerStage::kAnalyseSemantics) {
    if (meta->CurrentStage >= CompilerStage::kLoadSupScopes) {
      const auto self_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
      static_cast<void>(GetTypeSymOrError(*self_scope, *this, *sm));
    }
    _HasAnalysed = true;
    return;
  }

  // Determine the scope and get the type symbol.
  // Resolved once: a name stamped with what it resolved to where it was written keeps that meaning - through this
  // scope's binding of it, if there is one - rather than being looked up again by its spelling here, where the same
  // spelling can name something else (a caller's "T" substituted into a callee whose own parameter is "T").
  if (_Stamp != nullptr and GnArgGroup->Args.IsEmpty() and sm->CurrentScope->Canon(*_Stamp) != nullptr) {
    _HasAnalysed = true;
    return;
  }

  const auto scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;

  // Using a postfix type expression before stage 5 is
  // currently an error, because nested types are not
  // attached to their owner, via sup scopes, until stage 5.
  RaiseIf<analyse::errors::SppFeatureNotYetSupportedError>(
    meta->TypeAnalysisTypeScope != nullptr and scope->TySym != nullptr
    and meta->CurrentStage < CompilerStage::kAttachSupScopes
    and scope->GetTypeSymbol(WithoutGenerics()->ToUnchecked<TypeIdentifierAst>(), false) == nullptr,
    {sm->CurrentScope},
    ERR_ARGS(
      analyse::errors::NotYetSupportedFeature::NestedTypeBeforeSupScopes, *scope->TySym->Name, *this));

  const auto type_sym = GetTypeSymOrError(
    *scope, *WithoutGenerics()->ToUnchecked<TypeIdentifierAst>(), *sm);
  if (Name == "Self") {
    _HasAnalysed = true;
    return;
  }

  // The head of a name is its template (or alias) wherever the name is read again - with written arguments, or with
  // none yet and defaults filled in below ("Ord" becomes "Ord[Rhs=Self]").
  if (not type_sym->IsTypeGeneric()) { _TemplateStamp = type_sym; }

  if (_IsSourceWritten and meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
    and type_sym->ScopeDefinedIn != nullptr
    and type_sym->Name->Name == Name) {
    // A type declared in a "sup" block is a member of the type that block is over, so it follows the type-level rule
    // like the block's attributes and methods do. Anything else is a module member.
    // Todo: TIDY
    const auto def_node = type_sym->ScopeDefinedIn->AstNode;
    const auto in_sup_block = def_node != nullptr and (
      AstAs<SupPrototypeFunctionsAst>(def_node) != nullptr or AstAs<SupPrototypeExtensionAst>(def_node) != nullptr);
    const auto owner_sym = in_sup_block
      ? type_sym->ScopeDefinedIn->GetTypeSymbol(AstName(def_node)->WithoutGenerics().get())
      : nullptr;

    if (owner_sym != nullptr and owner_sym->LinkedScope != nullptr) {
      CheckTypeTypeVisibility(*type_sym, *this, *owner_sym->LinkedScope->NonGenericScope, *sm, *meta);
    }
    else {
      CheckModuleTypeVisibility(*type_sym, *this, *type_sym->ScopeDefinedIn, *sm, *meta);
    }
  }

  const auto no_gn_params = GenericParameterGroupAst::NewEmpty();
  const auto gn_param_group = type_sym->Alias != nullptr
    ? type_sym->Alias->Params.get()
    : type_sym->Type != nullptr
    ? type_sym->Type->GnParamGroup.get()
    : no_gn_params.get();

  auto is_tuple = false;
  if (not type_sym->IsTypeGeneric()) {
    is_tuple = IsTupSymbol(*type_sym);

    // Name all the generic arguments.
    NameGnArgs(
      *GnArgGroup,
      *gn_param_group,
      *this, *sm, *meta, is_tuple);

    // Analyse the generic arguments.
    if (meta->SkipTypeAnalysisGenericChecks) { return; }
    meta->TypeAnalysisTypeScope = nullptr;
    GnArgGroup->Stage7_AnalyseSemantics(sm, meta);

    // Infer the generic arguments from information given from object initialisation.
    InferGnArgs(
      *gn_param_group, *GnArgGroup, meta->InferSource, meta->InferTarget,
      type_sym->FqName(), *type_sym->LinkedScope, nullptr, is_tuple, *sm, *meta);
    GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
  }
  else {
    RaiseIf<SppHigherOrderGenericsNotSupportedError>(
      GnArgGroup != nullptr and not GnArgGroup->Args.IsEmpty(),
      {sm->CurrentScope}, ERR_ARGS(*this, *GnArgGroup));
  }

  // For variant types, collapse any duplicate generic arguments, so that "Str or S32 or Str" names the same type as
  // "Str or S32", and so that a nested variant is flattened into its parent. Without this, two spellings of the same
  // set of members would produce distinct type symbols.
  if (GnArgGroup != nullptr and GnArgGroup->At("Variants") != nullptr
    and analyse::utils::type_predicates::IsTypeVariant(*type_sym, *sm->CurrentScope)) {
    auto inner_types = analyse::utils::type_compare::DedupVariableInnerTypes(*this, *sm->CurrentScope);
    if (not inner_types.IsEmpty()) {
      auto inner_types_as_tup = generate::common_types::TupleType(PosStart(), std::move(inner_types));
      {
        const auto _meta_guard = MetaGuard(meta);
        meta->TypeAnalysisTypeScope = scope;
        inner_types_as_tup->Stage7_AnalyseSemantics(sm, meta);
      }
      GnArgGroup->Args[0]->TypeVal = std::move(inner_types_as_tup);
    }
  }

  // An instantiation is identified by its template and what its arguments resolve to where they are written - not by
  // its spelling, which is all the symbol table keys on: "Box[T]" inside "sup [T] Box[T]" and inside "cls Box[T]" name
  // different "T"s, so they are different types, and "Vec[S32]" is one type however it is written. The arguments are
  // read from the current scope: "scope" is the namespace a qualified name ("main::Box[T=T]") is resolved in.
  auto *instance = static_cast<TypeSymbol*>(nullptr);
  if (not GnArgGroup->Args.IsEmpty()) {
    const auto identity = sm->CurrentScope->InstanceIdentityKey(GnArgGroup->GetAllArgs());
    if (const auto hit = type_sym->Instances.find(identity); hit != type_sym->Instances.end()) {
      instance = hit->second;
    }
    else {
      const auto *new_scope = CreateGenericClsScope(
        *this, type_sym->SharedFromThis<TypeSymbol>(), is_tuple, sm, meta);
      instance = new_scope->TySym.get();
      instance->InstanceOf = type_sym;
      instance->IdentityKey = identity;
      type_sym->Instances[identity] = instance;
    }

    // Stamped with it, so a lookup of this name from anywhere finds this instantiation, re-read through the bindings of
    // the scope asking ("Scope::Canon") rather than by spelling.
    if (_Stamp == nullptr) { _Stamp = instance; }
  }

  // Enforce generic constraints from the pre-analysis stage onwards, not just the main analysis
  // stage. Sup scopes are fully loaded by the end of stage 5, so constraints can be reliably checked here, and some
  // need to be done before stage 7 for order agnostic behaviour.
  if (not GnArgGroup->Args.IsEmpty()
    and meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
    and not meta->SkipSubstitutedConstraintChecks) {
    EnforceGenericConstraintsAllArgs(*gn_param_group, *GnArgGroup, *sm->CurrentScope, *sm, *meta, type_sym->LinkedScope);
  }

  // The generic substitution above may have created the scope this resolves to, so the symbol is re-fetched rather
  // than reusing the base "type_sym" from before it existed.
  if (not type_sym->IsTypeGeneric()) { check_abstract(*scope); }

  // The stringification is dropped rather than kept, because this pass is what settles the value it was built from;
  // the next reader rebuilds it once and every reader after that shares it, for as long as the value stands.
  // Stamp a plain name with what it resolved to, so a copy of it substituted into another scope keeps this meaning. Only
  // a parameter or a closed class: anything else still depends on the scope asking.
  if (GnArgGroup->Args.IsEmpty() and _Stamp == nullptr) {
    if (auto *const resolved = scope->GetTypeSymbol(this); resolved != nullptr and (
      (resolved->Kind == TypeKind::GenericParam and resolved->ParamId != 0)
      or (resolved->Kind == TypeKind::Class and resolved->IsConcrete and resolved->Alias == nullptr))) {
      _Stamp = resolved;
    }
  }

  _HasAnalysed = true;
  _Resolved = true;
  _CachedStringification.clear();
}

auto TypeIdentifierAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  // These are always "zero_type", so return init.
  const auto mock_init = MakeUnique<ObjectInitializerAst>(AstClone(this), nullptr);
  return mock_init->Stage11_CodeGen(sm, meta, ctx);
}

auto TypeIdentifierAst::AnyPart(
  std::function<bool(TypeIdentifierAst const &)> const &pred) const -> bool {
  // This node is a part in its own right.
  if (pred(*this)) { return true; }

  for (auto &&g : GnArgGroup->Args) {
    // A comp argument with an identifier value.
    if (g->CompVal != nullptr) {
      if (auto &&ident_val = g->CompVal->To<IdentifierAst>()) {
        // A comp argument that is a bare name stands for a type part without being one, so one is made to ask about.
        // It lives only for the question - nothing outside this call can hold on to it.
        if (const auto part = FromIdentifier(*ident_val); pred(*part)) { return true; }
      }
    }

    // A type argument => recursive walk.
    else if (g->TypeVal->AnyPart(pred)) { return true; }
  }

  return false;
}

auto TypeIdentifierAst::IsNeverType() const noexcept -> bool {
  return _IsNeverType;
}

auto TypeIdentifierAst::MarkNeverType() -> void {
  _IsNeverType = true;
}

auto TypeIdentifierAst::IsSelfType() const noexcept -> bool {
  return _IsSelfType;
}

auto TypeIdentifierAst::NsParts() const -> Vec<IdentifierAst const*> {
  return {};
}

auto TypeIdentifierAst::NsParts() -> Vec<IdentifierAst*> {
  return {};
}

auto TypeIdentifierAst::ClearSourceWritten() -> void {
  _IsSourceWritten = false;
  for (auto const &arg : GnArgGroup->Args) {
    if (arg->TypeVal != nullptr) {
      for (auto *part : arg->TypeVal->TypeParts()) { part->ClearSourceWritten(); }
    }
  }
}

auto TypeIdentifierAst::MarkSourceWritten() -> void {
  _IsSourceWritten = true;
}

auto TypeIdentifierAst::TypeParts() const -> Vec<TypeIdentifierAst const*> {
  return {this};
}

auto TypeIdentifierAst::TypeParts() -> Vec<TypeIdentifierAst*> {
  return {this};
}

auto TypeIdentifierAst::LastTypePart() const -> TypeIdentifierAst const* {
  return this;
}

auto TypeIdentifierAst::LastTypePart() -> TypeIdentifierAst* {
  return this;
}

auto TypeIdentifierAst::WithoutConvention() const -> Shared<const TypeAst> {
  return shared_from_this();
}

auto TypeIdentifierAst::GetConvention() const -> ConventionAst* {
  return nullptr;
}

auto TypeIdentifierAst::WithConvention(
  Unique<ConventionAst> &&conv) const -> Shared<TypeAst> {
  if (conv == nullptr) { return AstCloneShared(this); }

  auto borrow_op = MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(std::move(conv));
  auto wrapped = MakeShared<TypeUnaryExpressionAst>(std::move(borrow_op), AstClone(this));
  wrapped->SetStamp(_Stamp);

  // A type rebuilt in place of a written one keeps pointing at
  // what was written once it is borrowed.
  if (_HasSourceSpan) { CopySourceSpanTo(*wrapped); }
  return wrapped;
}

auto TypeIdentifierAst::WithoutGenerics() const -> Shared<TypeAst> {
  // Use cache if available.
  if (not _CachedWithoutGenerics) {
    const auto stripped = MakeShared<TypeIdentifierAst>(_Pos, Str(Name), nullptr);
    stripped->_IsNeverType = _IsNeverType;
    _CachedWithoutGenerics = stripped;
  }

  // A plain name without its (absent) generics is still itself, so it resolves as its stamp says; a generic name's
  // stripped form names the template, so it resolves as its template stamp says. Set on every call: a stamp can arrive
  // after the copy is cached, and arguments can be added to a node in place ("ClassPrototypeAst" fills its own name's).
  // The copy is this node's own - "Clone" no longer shares it, as a shared copy took whichever node asked last's stamp.
  _CachedWithoutGenerics->SetStamp(GnArgGroup == nullptr or GnArgGroup->Args.IsEmpty() ? _Stamp : _TemplateStamp);
  return _CachedWithoutGenerics;
}

auto TypeIdentifierAst::SubstituteGenerics(
  Vec<GenericArgumentAst*> const &args) const -> Shared<TypeAst> {
  if (args.IsEmpty() or GnArgGroup == nullptr) { return AstClone(this); }

  // Check whether this type is itself one of the parameters being substituted. A parameter declaration and every
  // argument group built from one are stamped with the parameter's symbol, so the two are matched by identity - a
  // callee's "T" is not taken for a caller's parameter of that name. An argument written in source names nothing
  // resolved, so that case still compares spellings; a name stamped with anything but a parameter already says what
  // it is ("T" in an instance's own name, stamped with the closed type it is bound to), which only the spelling
  // comparison has to be told.
  const auto param_id = [](TypeSymbol const *const sym) -> std::uint64_t {
    return sym == nullptr ? 0 : sym->ParamId != 0 ? sym->ParamId : sym->BindsParamId;
  };
  const auto own_id = GnArgGroup->Args.IsEmpty() ? param_id(_Stamp) : 0;
  const auto names_param = _Stamp == nullptr or _Stamp->Kind == TypeKind::GenericParam
    or _Stamp->Kind == TypeKind::GenericArg;
  for (auto const &arg : args) {
    if (arg->Name == nullptr or arg->TypeVal == nullptr) { continue; }
    auto const *const arg_name = arg->Name->ToUnchecked<TypeIdentifierAst>();
    const auto matched = own_id != 0 and param_id(arg->Name->Stamp()) == own_id
      ? true
      : names_param and *this == *arg_name;
    if (not matched) { continue; }
    auto substituted = AstClone(arg->TypeVal.get());
    for (auto *part : substituted->TypeParts()) { part->ClearSourceWritten(); }
    return substituted;
  }

  // Nothing below this point applies to a type with no arguments
  // of its own: there is nothing to substitute into.
  if (GnArgGroup->Args.IsEmpty()) { return AstClone(this); }

  // Substitute generics in the comp arguments' values - a whole expression, not only a bare parameter name: "n + 1_uz"
  // in "f[n=1_uz]" becomes "1_uz + 1_uz", folded wherever its value is read. The clone is about to become a different
  // type, so it keeps no stamp: the one copied from this node names this node's instantiation, and a lookup would
  // follow it back there ("Some[T=Opt[T]]" still claiming to be "Some[T=T]", and re-substituted without end).
  auto name_clone = AstClone(this);
  name_clone->_Stamp = nullptr;
  for (auto const &g : name_clone->GnArgGroup->GetCompArgs()) {
    g->CompVal = AstClone(g->CompVal->SubstituteGenericsExpr(args));
  }

  // Substitute generics in the type arguments' types.
  for (auto const &g : name_clone->GnArgGroup->GetTypeArgs()) {
    g->TypeVal = g->TypeVal->SubstituteGenerics(args);
  }

  // Return the cloned type with generics substituted.
  return name_clone;
}

auto TypeIdentifierAst::ContainsGenerics(
  GenericParameterAst const &generic) const -> bool {
  // Check if the parameter's name is in the type parts walked from this type.
  auto const *cast_name = generic.Name->ToUnchecked<TypeIdentifierAst>();
  return AnyPart([cast_name](TypeIdentifierAst const &part) { return part == *cast_name; });
}

auto TypeIdentifierAst::WithGenerics(
  Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> {
  // Attach the new generic argument group to a clone of this type identifier.
  arg_group = arg_group ? std::move(arg_group) : GenericArgumentGroupAst::NewEmpty();
  const auto with_generics = MakeShared<TypeIdentifierAst>(_Pos, Str(Name), std::move(arg_group));
  with_generics->_IsNeverType = _IsNeverType;
  with_generics->_TemplateStamp = _TemplateStamp;
  return with_generics;
}

auto TypeIdentifierAst::IsCompilerGeneratedType() const -> bool {
  // Types starting with "$" are compiler generated (not parsable).
  return Name[0] == '$';
}

auto TypeIdentifierAst::ResetCache() -> void {
  // Reset the cache to allow overriding the analysis skipper instruction.
  _HasAnalysed = false;
}

auto TypeIdentifierAst::IsTypeIdentifier() const noexcept -> bool {
  return true;
}

auto TypeIdentifierAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Fully qualify this type name from the scope.
  // Have to AstClone because PostfixExpressionAst lhs (will change with removal of all shared pointers)
  const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
  const auto type_sym = type_scope->GetTypeSymbol(this);
  return type_sym->FqName();
}

auto TypeIdentifierAst::AnkerlHash() const -> std::size_t {
  // Hash based on the name only.
  return Hash<Str>()(Name);
}

auto TypeIdentifierAst::ToView() const -> StrView {
  if (GnArgGroup == nullptr or GnArgGroup->Args.IsEmpty()) {
    return Name;
  }

  if (_CachedStringification.empty() or not _Resolved) {
    _CachedStringification = Name;
    _CachedStringification.append(GnArgGroup->ToString());
  }
  return _CachedStringification;
}

auto TypeIdentifierAst::NsPartsInto(
  Vec<IdentifierAst const*> &) const -> void {
}

auto TypeIdentifierAst::TypePartsInto(
  Vec<TypeIdentifierAst const*> &out) const -> void {
  out.EmplaceBack(this);
}

SPP_MOD_END
