module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_identifier_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.fn_values;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
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
import spp.lsp.resolution_index;
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
  t->_StampedTypeId = _StampedTypeId;
  t->_StampedTemplateId = _StampedTemplateId;
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
  IMPORT_UTILS;
  auto *solved_instance = static_cast<TypeSymbol*>(nullptr);

  // Reject abstract types everywhere except the few positions that name a type without ever producing a value of it.
  // Only allow an abstract self if we are in the abstract class itself. For example, `Clone::clone_from` must be allowed
  // to use `Clone::clone` as the default, which returns "Self", but will never be used from `Clone`, but rather the
  // implementation type.
  const auto check_abstract = [&](Scope const &scope) {
    // Judged from pre-analysis on. Signatures are compared by identity ("fn_values::SameSignature"), which makes
    // nothing, so the answer does not depend on whether instantiations can still be made (code generation).
    if (meta->AllowAbstractType or meta->CurrentStage<CompilerStage::kPreAnalyseSemantics) { return; }
    const auto resolved_sym = scope.FindTypeSymbol(this);
    if (resolved_sym == nullptr or resolved_sym->IsGn() or resolved_sym->LinkedScope == nullptr) { return; }

    // An instance still naming an unbound parameter ("Vec[Box[T]]"
    // in "Box"'s own generic sup) is not yet a type with a fixed
    // set of superimpositions, some of which depend on what the
    // parameter becomes, so whether it is abstract is not yet
    // known. Each concrete instance is checked where it is made.
    // (The symbol's "IsConcrete" flag is not trusted for this: an
    // instance minted through an alias carries it set regardless.)
    const auto resolved = TypeRef::Of(*this, scope);
    if (not type_predicates::IsTypeConcrete(resolved)) { return; }

    // The type is abstract, or holds one by value ("C[A]" with an
    // attribute "t: T"): a value of it needs a value of the
    // abstract type, which cannot exist. Naming one as a generic
    // argument does not ("B[A]" with no such attribute).
    // A method left unimplemented by an error already reported (a
    // failed extension block, in a recovering compile) does not
    // count: the use would only repeat that error, once per use.
    const auto unexplained = [](Scope const &type_scope) {
      return type_members::GetUnimplementedAbstractMethods(type_scope)
        | genex::views::filter([&](auto const *fn) {
          return not type_members::IsLeftUnimplementedByAnError(type_scope, *fn);
        })
        | genex::to<Vec>();
    };
    const auto is_abstract = [&unexplained](TypeRef const &held) {
      auto const *const linked = held.Symbol->LinkedScope;
      return linked != nullptr and not unexplained(*linked).IsEmpty();
    };
    const auto abstract = type_members::FindHeldByValue(resolved, scope, is_abstract);
    if (abstract.Symbol == nullptr) { return; }
    const auto unimplemented = unexplained(*abstract.Symbol->LinkedScope);
    const auto self_sym = sm->CurrentScope->FindSelfSymbol();
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

  // An instantiation that keeps nesting is reported, rather than left to overflow the stack.
  // Todo: A function instantiating itself with a bigger type ("f[(T, T)]" inside "f[T]") is not caught by this - the
  //  types grow wider rather than deeper, and it runs out of memory
  //  (TestGenericInference_Recursion, disabled).
  const auto _depth = monomorphization::InstantiationDepth(*this, *sm);
  RaiseIf<SppHigherOrderGenericsNotSupportedError>(
    IsSelfType() and GnArgGroup != nullptr and not GnArgGroup->Args.IsEmpty(),
    {sm->CurrentScope}, ERR_ARGS(*this, *GnArgGroup));
  if (IsSelfType() and meta->CurrentStage < CompilerStage::kAnalyseSemantics) {
    if (meta->CurrentStage >= CompilerStage::kLoadSupScopes) {
      const auto self_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
      static_cast<void>(member_lookup::FindTypeSymbolOrError(*self_scope, *this, *sm));
    }
    _HasAnalysed = true;
    return;
  }

  // Determine the scope and get the type symbol.
  // Resolved once: a name carrying the identity it resolved to where it was written keeps that meaning - through this
  // scope's binding of it, if there is one - rather than being looked up again by its spelling here, where the same
  // spelling can name something else (a caller's "T" substituted into a callee whose own parameter is "T"). It is still
  // checked for being abstract, as an analysed node is.
  if (_StampedTypeId != nullptr and GnArgGroup->Args.IsEmpty()
    and sm->CurrentScope->FindBoundTypeSymbolById(_StampedTypeId) != nullptr) {
    _HasAnalysed = true;
    check_abstract(meta->TypeAnalysisTypeScope ? *meta->TypeAnalysisTypeScope : *sm->CurrentScope);
    return;
  }

  const auto scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;

  // Using a postfix type expression before stage 5 is
  // currently an error, because nested types are not
  // attached to their owner, via sup scopes, until stage 5.
  RaiseIf<SppFeatureNotYetSupportedError>(
    meta->TypeAnalysisTypeScope != nullptr and scope->LinkedTypeSymbol != nullptr
    and meta->CurrentStage < CompilerStage::kAttachSupScopes and scope->FindHeadSymbol(*this) == nullptr,
    {scope, sm->CurrentScope},
    ERR_ARGS(NotYetSupportedFeature::NestedTypeBeforeSupScopes, *scope->LinkedTypeSymbol->Name, *this));

  const auto type_sym = member_lookup::FindTypeSymbolOrError(
    *scope, *WithoutGns()->ToUnchecked<TypeIdentifierAst>(), *sm);

  // Use the hook to record information for the resolution and
  // completion plugin.
  if (_IsSourceWritten) {
    lsp::resolution_index::RecordType(
      *this, *sm, *meta, type_sym);
  }

  if (IsSelfType()) {
    _HasAnalysed = true;
    return;
  }

  // The head of a name is its template (or alias) wherever the
  // name is read again - with written arguments, or with none
  // yet and defaults filled in below ("Ord" becomes "Ord[Rhs=Self]").
  // Through a "use" of a class, the class: an instance made
  // from this name carries it in its own, and a block found
  // through that name is registered against the class, not the
  // "use".
  if (not type_sym->IsGn()) { _StampedTemplateId = NameTypeIdOf(*type_sym->InstanceTemplate()); }

  if (_IsSourceWritten and meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
    and type_sym->ScopeDefinedIn != nullptr
    and type_sym->Name->Name == Name) {
    // A type declared in a "sup" block is a member of the type
    // that block is over, so it follows the type-level rule
    // like the block's attributes and methods do. Anything else
    // is a module member.
    // Todo: TIDY
    const auto def_node = type_sym->ScopeDefinedIn->AstNode;
    const auto in_sup_block = def_node != nullptr and (
      AstAs<SupPrototypeFunctionsAst>(def_node) != nullptr or AstAs<SupPrototypeExtensionAst>(def_node) != nullptr);
    const auto owner_sym = in_sup_block
      ? type_sym->ScopeDefinedIn->FindHeadSymbol(*AstName(def_node))
      : nullptr;

    if (owner_sym != nullptr and owner_sym->LinkedScope != nullptr) {
      visibility_utils::CheckTypeTypeVisibility(*type_sym, *this, *owner_sym->LinkedScope->NonGnScope, *sm, *meta);
    }
    else {
      visibility_utils::CheckModuleTypeVisibility(*type_sym, *this, *type_sym->ScopeDefinedIn, *sm, *meta);
    }
  }

  const auto no_gn_params = GenericParameterGroupAst::NewEmpty();
  auto *const gn_param_group = type_sym->GnParams() != nullptr ? type_sym->GnParams() : no_gn_params.get();

  auto is_tuple = false;
  if (not type_sym->IsGn()) {
    // An alias declaring parameters of its own ("type P[T, U] =
    // (U, T)") binds its arguments to them, by name, even though
    // it names a tuple: kept positional, "P[S32, Bool]" read as
    // "(S32, Bool)", and "type P[T] = (T, T)" as a one-tuple.
    const auto own_params_alias = type_sym->Alias != nullptr and not type_sym->Alias->IsFromUseStmt
      and not type_sym->Alias->IsParamsFromTarget;
    is_tuple = type_predicates::IsTypeTuple(TypeRef::ForKindCheck(*type_sym, *sm->CurrentScope), *sm->CurrentScope)
      and not own_params_alias;

    // Name all the generic arguments.
    auto const *const params_scope = type_sym->GnParamsScope();
    GnArgGroup = generic_inference::NamedGnArgs(
      *GnArgGroup, *gn_param_group, params_scope != nullptr ? *params_scope : *sm->CurrentScope, *this, *sm, *meta,
      is_tuple);

    // Analyse the generic arguments. Naming an abstract type as
    // one ("B[A]") produces no value of it; an instantiation that
    // holds one in an attribute is checked there, where the value
    // would be.
    if (meta->SkipTypeAnalysisGnChecks) { return; }
    meta->TypeAnalysisTypeScope = nullptr;
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->AllowAbstractType = true;
      GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
    }

    // Solve the arguments not written: from an object initializer's
    // attributes, the constraints and the defaults.
    if (not is_tuple and not gn_param_group->Params.IsEmpty()) {
      // An alias's attributes are its class's, in the class's terms:
      // read through the alias's target, they name the
      // alias's own parameters.
      auto solver = generic_inference::GenericSolver(*gn_param_group, *type_sym->LinkedScope, *sm, *meta);
      if (const auto target = type_sym->AliasTargetId(); target != nullptr) {
        solver.ReadDeclaredWith(InstanceBindings(TypeRef::Of(target, *sm->CurrentScope)));
      }
      solver.Give(std::move(GnArgGroup->Args));
      for (auto &&[name, source, target] : std::exchange(_AttributeEquations, {})) {
        solver.Unify(name, std::move(source), std::move(target));
      }
      solver.Solve(*type_sym->FqName());

      // The arguments are written into this name to be shown and
      // read again, recording what they mean, so they are not
      // analysed again; an instance made already is found by the
      // solution's identity, so they are not keyed again either.
      // A variant is keyed by its normalised members instead.
      const auto is_variant = type_predicates::IsTypeVariant(
        TypeRef::ForKindCheck(*type_sym, *sm->CurrentScope), *sm->CurrentScope);
      if (const auto solved = solver.SolvedArgsId(); solved != nullptr and not is_variant) {
        solved_instance = sm->CurrentScope->FindTypeSymbolById(
          InstanceIdOfArgs(*type_sym->InstanceTemplate(), solved));
      }
      GnArgGroup->Args = solver.TakeArgs();
    }
  }
  else {
    RaiseIf<SppHigherOrderGenericsNotSupportedError>(
      GnArgGroup != nullptr and not GnArgGroup->Args.IsEmpty(),
      {sm->CurrentScope}, ERR_ARGS(*this, *GnArgGroup));
  }

  // A variant is the set of its members, so it is normalised
  // once, here: duplicates collapse ("Str or S32 or Str" is
  // "Str or S32"), a nested variant is flattened into its
  // parent, and the members go in one canonical order ("Bool
  // or S32" is "S32 or Bool"), by what each resolves to.
  // Without this, two spellings of the same set would be two
  // instances, each with its own tag order, that identity and
  // codegen would disagree about.
  if (GnArgGroup != nullptr and GnArgGroup->At("Variants") != nullptr
    and type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*type_sym, *sm->CurrentScope), *sm->CurrentScope)) {
    auto unordered = Vec<Pair<Shared<TypeAst>, TypeSymbol const*>>();
    for (auto &member : type_compare::VariantMembers(*this, *sm->CurrentScope)) {
      auto const *const member_sym = TypeRef::Of(*member, *sm->CurrentScope).Symbol;
      unordered.EmplaceBack(std::move(member), member_sym);
    }
    auto inner_types = type_compare::OrderVariantMembers(std::move(unordered));
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

  // An instantiation is identified by its template and what
  // its arguments resolve to where they are written - not by
  // its spelling, which is all the symbol table keys on:
  // "Box[T]" inside "sup [T] Box[T]" and inside "cls Box[T]"
  // name different "T"s, so they are different types, and
  // "Vec[S32]" is one type however it is written. The
  // arguments are read from the current scope: "scope" is
  // the namespace a qualified name ("main::Box[T=T]") is
  // resolved in.
  auto *instance = static_cast<TypeSymbol*>(nullptr);
  if (not GnArgGroup->Args.IsEmpty()) {
    // Found, and made, under the identity's own template:
    // through a "use" of a class, that is the class. One
    // found by the solution's identity already is that
    // instance; anything else is keyed from its arguments.
    instance = solved_instance;
    if (instance == nullptr) {
      const auto id = sm->CurrentScope->InstanceIdOf(*type_sym, GnArgGroup->GetAllArgs());
      instance = sm->CurrentScope->FindTypeSymbolById(id);
      if (instance == nullptr) {
        const auto *new_scope = monomorphization::CreateGnClsScope(
          *this, type_sym->InstanceTemplate()->SharedFromThis<TypeSymbol>(), id, is_tuple, sm, meta);
        instance = new_scope->LinkedTypeSymbol.get();
      }
    }

    // Stamped with it, so a lookup of this name from anywhere finds this instantiation, re-read through the bindings of
    // the scope asking ("Scope::FindBoundTypeSymbolById") rather than by spelling.
    if (_StampedTypeId == nullptr) { _StampedTypeId = NameTypeIdOf(*instance); }
  }

  // The generic substitution above may have created the scope this resolves to, so the symbol is re-fetched rather
  // than reusing the base "type_sym" from before it existed.
  if (not type_sym->IsGn()) { check_abstract(*scope); }

  // The stringification is dropped rather than kept, because this pass is what settles the value it was built from;
  // the next reader rebuilds it once and every reader after that shares it, for as long as the value stands.
  // Resolved once, here, where it is written: the identity a plain name resolved to is recorded, and every later read
  // (a copy substituted into another scope included) reads that identity through its own bindings
  // ("Scope::FindBoundTypeSymbolById") rather than the spelling.
  // A generic template named bare is not a type yet: its arguments are filled in place (inferred from an object
  // initializer's attributes, or its defaults), after which the name means an instantiation, so it records nothing.
  if (GnArgGroup->Args.IsEmpty()) {
    if (auto *const resolved = scope->FindTypeSymbol(this); resolved != nullptr) {
      type_resolution::StampType(*this, *resolved);
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
    if (arg->IsTypeArg()) {
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
  wrapped->StampTypeId(_StampedTypeId);

  // A type rebuilt in place of a written one keeps pointing at
  // what was written once it is borrowed.
  if (_HasSourceSpan) { CopySourceSpanTo(*wrapped); }
  return wrapped;
}

auto TypeIdentifierAst::WithoutGns() const -> Shared<TypeAst> {
  // Use cache if available.
  if (not _CachedWithoutGns) {
    const auto stripped = MakeShared<TypeIdentifierAst>(_Pos, Str(Name), nullptr);
    stripped->_IsNeverType = _IsNeverType;
    _CachedWithoutGns = stripped;
  }

  // A plain name without its (absent) generics is still itself, so it resolves as its written identity says; a generic
  // name's stripped form names the template, so it resolves as its template's does. Set on every call: the identity can
  // arrive after the copy is cached, and arguments can be added to a node in place ("ClassPrototypeAst" fills its own
  // name's). The copy is this node's own - "Clone" no longer shares it, as a shared copy took whichever node asked last's.
  _CachedWithoutGns->StampTypeId(
    GnArgGroup == nullptr or GnArgGroup->Args.IsEmpty() ? _StampedTypeId : _StampedTemplateId);
  return _CachedWithoutGns;
}

auto TypeIdentifierAst::WithGns(
  Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> {
  // Attach the new generic argument group to a clone of this type identifier.
  arg_group = arg_group ? std::move(arg_group) : GenericArgumentGroupAst::NewEmpty();
  const auto with_generics = MakeShared<TypeIdentifierAst>(_Pos, Str(Name), std::move(arg_group));
  with_generics->_IsNeverType = _IsNeverType;
  with_generics->_StampedTemplateId = _StampedTemplateId;
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
  const auto type_sym = type_scope->FindTypeSymbol(this);
  return type_sym->FqName();
}

auto TypeIdentifierAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The symbol this name finds where types are being analysed, read here.
  const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
  const auto type_sym = type_scope->FindTypeSymbol(this);
  return type_sym != nullptr ? TypeRef::Of(*type_sym, *sm->CurrentScope) : TypeRef();
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

auto TypeIdentifierAst::InferFromAttributes(
  Vec<Tup<Shared<IdentifierAst>, Shared<TypeAst>, Shared<TypeAst>>> &&equations) -> void {
  _AttributeEquations = std::move(equations);
}

SPP_MOD_END
