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
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.packs;
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

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeIdentifierAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, CompilerStage);
use_alias(spp::asts::meta, MetaGuard);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
namespace {
  /// Whether a borrow may be written somewhere in "type": a convention
  /// on it or inside its generic arguments. A type spelled any other
  /// way (a variant, a namespaced name) answers yes, so it is looked at
  /// properly; this only spares the common case its sup lookup.
  auto MayHoldWrittenBorrow(TypeAst const &type) -> bool {
    if (type.GetConvention() != nullptr) { return true; }
    auto const *const id = type.To<TypeIdentifierAst>();
    if (id == nullptr) { return true; }
    return genex::any_of(id->GnArgGroup->GetTypeArgs(), [](auto const *arg) {
      return MayHoldWrittenBorrow(*arg->TypeVal);
    });
  }
}

auto spp::asts::TypeIdentifierAst::FromIdentifier(
  IdentifierAst const &identifier) -> Shared<TypeIdentifierAst> {
  return MakeShared<TypeIdentifierAst>(identifier.PosStart(), Str(identifier.Val), nullptr);
}

auto spp::asts::TypeIdentifierAst::FromString(
  Str const &identifier) -> Shared<TypeIdentifierAst> {
  return MakeShared<TypeIdentifierAst>(0uz, Str(identifier), nullptr);
}

spp::asts::TypeIdentifierAst::TypeIdentifierAst(
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

spp::asts::TypeIdentifierAst::~TypeIdentifierAst() = default;

auto spp::asts::TypeIdentifierAst::operator<=>(
  const TypeIdentifierAst &that) const -> Ordering {
  return EqualsTypeIdentifier(that);
}

auto spp::asts::TypeIdentifierAst::operator==(
  const TypeIdentifierAst &that) const -> bool {
  return EqualsTypeIdentifier(that) == Ordering::equal;
}

auto spp::asts::TypeIdentifierAst::EqualsTypeIdentifier(
  TypeIdentifierAst const &other) const -> Ordering {
  // Equality is based on the name and generics.
  return Name == other.Name and *GnArgGroup == *other.GnArgGroup
    ? Ordering::equal
    : Ordering::less;
}

auto spp::asts::TypeIdentifierAst::Equals(
  ExpressionAst const &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsTypeIdentifier(*this);
}

auto spp::asts::TypeIdentifierAst::PosStart() const -> std::size_t {
  // Use the static pos field, unless this replaces a written type.
  if (_HasSourceSpan) { return _SpanStart; }
  return _Pos;
}

auto spp::asts::TypeIdentifierAst::PosEnd() const -> std::size_t {
  // Use the "]" of the generic arguments written after the name,
  // else the name. Arguments the compiler filled in have no "]"
  // of their own, or one that is not after this name.
  if (_HasSourceSpan) { return _SpanEnd; }
  const auto name_end = _Pos + Name.length();
  const auto tok_r = GnArgGroup != nullptr ? GnArgGroup->TokR.get() : nullptr;
  return tok_r != nullptr and tok_r->PosStart() >= name_end ? tok_r->PosEnd() : name_end;
}

auto spp::asts::TypeIdentifierAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto t = MakeUnique<TypeIdentifierAst>(
    _Pos,
    Str(Name),
    AstClone(GnArgGroup));
  t->_IsNeverType = _IsNeverType;
  t->_IsSourceWritten = _IsSourceWritten;
  t->_IsPackPosition = _IsPackPosition;
  t->_StampedTypeId = _StampedTypeId;
  t->_StampedTemplateId = _StampedTemplateId;
  CopySourceSpanTo(*t);
  return t;
}

auto spp::asts::TypeIdentifierAst::ToString() const -> Str {
  // Reuse the cached stringification (identical content: Name + GnArgGroup->ToString()) to avoid rebuilding the
  // string (and recursively re-stringifying every generic argument) on each call.
  return Str(ToView());
}

auto spp::asts::TypeIdentifierAst::Stage7_AnalyseSemantics(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Todo: Add higher order generic checks into the unit tests (self and generic type).
  IMPORT_UTILS;
  auto *solved_instance = static_cast<TypeSymbol*>(nullptr);

  // Reject abstract types everywhere except the few positions that name a type without ever producing a value of it.
  // Only allow an abstract self if we are in the abstract class itself. For example, `Clone::clone_from` must be allowed
  // to use `Clone::clone` as the default, which returns "Self", but will never be used from `Clone`, but rather the
  // implementation type.
  const auto check_abstract = [&](Scope const &scope) {
    // Judged from pre-analysis on. Signatures are compared by identity ("fn_signatures::SameSignature"), which makes
    // nothing, so the answer does not depend on whether instantiations can still be made (code generation).
    if (meta->AllowAbstractType or meta->CurrentStage<CompilerStage::kPreAnalyseSemantics) { return; }
    const auto resolved_sym = scope.FindTypeSymbol(this);
    if (resolved_sym == nullptr or resolved_sym->IsGn() or resolved_sym->LinkedScope == nullptr) { return; }

    // An instance still naming an unbound parameter ("Vec[Box[T]]"
    // in "Box"'s own generic sup) is not yet a type with a fixed
    // set of superimpositions, some of which depend on what the
    // parameter becomes, so whether it is abstract is not yet
    // known. Each concrete instance is checked where it is made.
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
      return linked != nullptr and not unexplained(*linked).empty();
    };
    const auto abstract = type_members::FindHeldByValue(resolved, scope, is_abstract);
    if (abstract.Symbol == nullptr) { return; }
    const auto unimplemented = unexplained(*abstract.Symbol->LinkedScope);
    const auto self_sym = sm->CurrentScope->FindSelfSymbol();
    if (self_sym == nullptr or self_sym->LinkedScope != resolved_sym->LinkedScope) {
      Raise<SppAbstractTypeUseError>(ERR_ARGS(*this, *unimplemented[0]));
    }
  };

  // A borrow is never stored in a value, so it cannot be a generic argument: "Vec[&Str]" would store one. A generator's
  // arguments (its yield) and a function type's (its signature) name a borrow without storing it, so they, and
  // everything inside them (the "(&T, &T)" in "FunRef[(&T, &T), Bool]"), are exempt ("AllowBorrowedTypeArgs"). Read off
  // the sups, so judged once they are attached. Only a borrow written in source is judged: the types the compiler builds
  // (a variant's "Var[&A, B]", a variadic's tuple, a closure's type) and the ones an instantiation
  // substitutes name a borrow judged where it was written (an explicit function argument is judged at its call). A
  // type with a field of the argument ("Opt[&Str]") is made, and its field judged, before stage 6: that is reported at
  // the written borrow too ("ClassAttributeAst", "InstantiatedFrom").
  const auto check_borrowed_args = [&](Scope const &scope, TypeSymbol *tmpl) {
    if (not _IsSourceWritten or meta->AllowBorrowedTypeArgs) { return; }
    if (meta->CurrentStage < CompilerStage::kPreAnalyseSemantics) { return; }
    for (auto const *const arg : GnArgGroup->GetTypeArgs()) {
      // A variant holding a borrow ("&Str or None") stores it as well.
      if (not type_predicates::IsTypeBorrowed(*arg->TypeVal, scope)) { continue; }
      if (tmpl == nullptr) { tmpl = member_lookup::FindTypeSymbolOrError(scope, *WithoutGns()->As<TypeIdentifierAst>()); }
      if (marker_sups::HoldsNoValueOfItsArgs(*tmpl, scope)) { return; }
      Raise<SppSecondClassBorrowViolationError>(ERR_ARGS(*arg->TypeVal, *arg->TypeVal, "generic argument"));
    }
  };

  // A type pack ("..Ts") is a list of types, not a type (D2): only a generic argument ("Tup[Ts]") or a variadic
  // parameter's element type ("..xs: Ts") may name it, as the parser marks ("MarkPackPosition"). An instance binds it,
  // so only a template's written use is judged. The name is resolved once, with the symbol the main path finds, as a
  // lookup per analysis of every written type is not free: the pack it names is kept ("_TypePack", not cloned, as an
  // instance binds it), for the analyses after, which return early. The first is often before the check applies (a
  // signature's types are resolved while sups are attached).
  const auto check_type_pack = [&] {
    if (_TypePack == nullptr or meta->CurrentStage < CompilerStage::kPreAnalyseSemantics) { return; }
    Raise<SppPackNotTypeError>(ERR_ARGS(*this, *_TypePack->Name));
  };
  const auto find_type_pack = [&](TypeSymbol const &sym) {
    if (not _IsSourceWritten or _IsPackPosition or not GnArgGroup->Args.empty()) { return; }
    if (sym.IsGn() and sym.IsVariadic and sym.AsBound() == &sym) { _TypePack = &sym; }
  };

  // An analysed node is still checked: a symbol's cached qualified name is shared, and can first be analysed where an
  // abstract type is allowed (a function type's arguments), then reached where it is not (an attribute's type). A
  // signature's types are first analysed before the sups are attached, so the borrow check is made on the way back.

  // A provisional resolution is made again, once, now the sups are attached: as the analysis before it, from the start.
  if (_HasAnalysed and _Provisional and meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics) {
    _HasAnalysed = false;
    _Provisional = false;
  }

  if (_HasAnalysed) {
    _RecheckArgs(sm, meta);
    check_abstract(meta->TypeAnalysisTypeScope ? *meta->TypeAnalysisTypeScope : *sm->CurrentScope);
    check_borrowed_args(meta->TypeAnalysisTypeScope ? *meta->TypeAnalysisTypeScope : *sm->CurrentScope, nullptr);
    check_type_pack();
    return;
  }

  // An instantiation that keeps nesting is reported, rather
  // than left to overflow the stack. One growing wider instead
  // ("f[(T, T)]" inside "f[T]") is refused where the function
  // is instantiated (E109).
  const auto _depth = monomorphization::InstantiationDepth(*this);
  RaiseIf<SppHigherOrderGenericsNotSupportedError>(
    IsSelfType() and GnArgGroup != nullptr and not GnArgGroup->Args.empty(),
    ERR_ARGS(*this, *GnArgGroup));
  if (IsSelfType() and meta->CurrentStage < CompilerStage::kAnalyseSemantics) {
    if (meta->CurrentStage >= CompilerStage::kLoadSupScopes) {
      const auto self_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
      ignore = member_lookup::FindTypeSymbolOrError(*self_scope, *this);
    }
    _HasAnalysed = true;
    return;
  }

  // Determine the scope and get the type symbol.
  // Resolved once: a name carrying the identity it resolved to where it was written keeps that meaning - through this
  // scope's binding of it, if there is one - rather than being looked up again by its spelling here, where the same
  // spelling can name something else (a caller's "T" substituted into a callee whose own parameter is "T"). It is still
  // checked for being abstract, as an analysed node is.
  if (_StampedTypeId != nullptr and GnArgGroup->Args.empty()
    and sm->CurrentScope->FindBoundTypeSymbolById(_StampedTypeId) != nullptr) {
    _HasAnalysed = true;
    check_abstract(meta->TypeAnalysisTypeScope ? *meta->TypeAnalysisTypeScope : *sm->CurrentScope);
    check_type_pack();
    return;
  }

  const auto scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;

  // Using a postfix type expression before stage 5 is
  // currently an error, because nested types are not
  // attached to their owner, via sup scopes, until stage 5.
  RaiseIf<SppFeatureNotYetSupportedError>(
    meta->TypeAnalysisTypeScope != nullptr and scope->LinkedTypeSymbol != nullptr
    and meta->CurrentStage < CompilerStage::kAttachSupScopes and scope->FindHeadSymbol(*this) == nullptr,
    ERR_ARGS(NotYetSupportedFeature::NestedTypeBeforeSupScopes, *scope->LinkedTypeSymbol->Name, *this));

  const auto type_sym = member_lookup::FindTypeSymbolOrError(
    *scope, *WithoutGns()->As<TypeIdentifierAst>());
  _ResolvedSym = type_sym;
  _RecheckedAt = meta->CurrentStage;
  find_type_pack(*type_sym);
  check_type_pack();

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

  // Which modules can name a package is known from the start, unlike visibility.
  if (_IsSourceWritten and type_sym->ScopeDefinedIn != nullptr and type_sym->Name->Name == Name) {
    visibility_utils::CheckPackageAccess(*type_sym, *type_sym->ScopeDefinedIn, *this, *sm, *meta);
  }

  // Visibility is on a class's symbol from stage 5, so a name read before then (a signature's or an attribute's type)
  // is checked once every module has finished it. A generic parameter has none of its own: it is its argument.
  if (_IsSourceWritten and type_sym->ScopeDefinedIn != nullptr and type_sym->Name->Name == Name) {
    if (meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics) { _CheckVisibility(*type_sym, sm, meta); }
    else if (not meta->IgnoreAccessModifierViolations and not type_sym->IsGn()) {
      _PendingVisibility.push_back({.Written = AstClone(this), .Sym = type_sym, .From = sm->CurrentScope});
    }
  }

  const auto no_gn_params = GenericParameterGroupAst::NewEmpty();
  auto *const gn_param_group = type_sym->GnParams() != nullptr ? type_sym->GnParams() : no_gn_params.get();

  if (not type_sym->IsGn()) {
    // An exempt type ("check_borrowed_args") makes its arguments, and
    // everything inside them, exempt: while they are named (which
    // analyses them) and analysed. Only asked when a borrow may be
    // written in them, as reading the sups is not free.
    const auto names_only = _NamesOnly(*type_sym, sm, meta);

    // Name all the generic arguments.
    const auto name_args = [&] {
      GnArgGroup = generic_inference::NamedGnArgs(
        *GnArgGroup, *gn_param_group, type_sym->LinkedScope, *this, *sm, *meta);
    };
    if (names_only) {
      const auto _meta_guard = MetaGuard(meta);
      meta->AllowBorrowedTypeArgs = true;
      name_args();
    }
    else { name_args(); }

    // Analyse the generic arguments. Naming an abstract type as
    // one ("B[A]") produces no value of it; an instantiation that
    // holds one in an attribute is checked there, where the value
    // would be.
    if (meta->SkipTypeAnalysisGnChecks) { return; }
    meta->TypeAnalysisTypeScope = nullptr;
    _AnalyseArgs(names_only, sm, meta);
    if (not names_only) { check_borrowed_args(*sm->CurrentScope, type_sym); }

    // Solve the arguments not written: from an object initializer's
    // attributes, the constraints and the defaults.
    if (not gn_param_group->Params.empty()) {
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
      solver.Solve(*this);
      _TypeArgsChecked = meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
        and not meta->SkipSubstitutedConstraintChecks;
      _CompArgsChecked = meta->CurrentStage >= CompilerStage::kAttachSupScopes;

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
      GnArgGroup != nullptr and not GnArgGroup->Args.empty(),
      ERR_ARGS(*this, *GnArgGroup));
  }

  // A variant is the set of its members, so it is written as its identity has it ("VariantKey"): duplicates
  // collapsed ("Str or S32 or Str" is "Str or S32"), a nested variant flattened into its parent, a bound pack's
  // elements in place, and the members in the identity's one order, which is also their tags' (the identity is the
  // only thing that orders them, so a written variant, one spelled back from an identity and the tags all agree).
  // Each member keeps its argument as written where there is one, else is spelled from its identity.
  if (GnArgGroup != nullptr and not GnArgGroup->GetTypeArgs().empty() and type_sym->UseTarget()->Alias == nullptr
    and type_predicates::IsTypeVariant(TypeRef::ForKindCheck(*type_sym, *sm->CurrentScope), *sm->CurrentScope)) {
    auto members = Vec<Shared<TypeAst>>();
    const auto id = sm->CurrentScope->TypeIdOf(*this);
    if (id != nullptr and analyse::scopes::HeaderOf(id).Kind == analyse::scopes::TypeKey::Tag::Variant) {
      auto written = Vec<Pair<analyse::scopes::TypeId, Shared<TypeAst>>>();
      for (auto const *const arg : GnArgGroup->GetTypeArgs()) {
        written.emplace_back(sm->CurrentScope->TypeIdOf(*arg->TypeVal), arg->TypeVal);
      }
      for (const auto member : analyse::scopes::HeaderOf(id).VariantMembers) {
        const auto as_written = genex::find_if(written, [member](auto const &w) { return w.first == member; });
        members.emplace_back(as_written != written.end() ? as_written->second : sm->CurrentScope->TypeAstOf(member));
        if (members.back() == nullptr) { members.clear(); break; }
      }
    }
    if (not members.empty()) {
      const auto _meta_guard = MetaGuard(meta);
      meta->TypeAnalysisTypeScope = scope;
      GnArgGroup->Args.clear();
      for (auto &member : members) {
        member->Stage7_AnalyseSemantics(sm, meta);
        GnArgGroup->Args.emplace_back(GenericArgumentAst::NewType(nullptr, std::move(member)));
      }
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
  if (not GnArgGroup->Args.empty()) {
    // Found, and made, under the identity's own template:
    // through a "use" of a class, that is the class. One
    // found by the solution's identity already is that
    // instance; anything else is keyed from its arguments.
    instance = solved_instance;
    if (instance == nullptr) {
      instance = monomorphization::FindOrMakeClsInstance(
        *this, type_sym->InstanceTemplate()->SharedFromThis<TypeSymbol>(),
        sm->CurrentScope->InstanceIdOf(*type_sym, GnArgGroup->GetAllArgs()), sm, meta);
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
  if (GnArgGroup->Args.empty()) {
    if (auto *const resolved = scope->FindTypeSymbol(this); resolved != nullptr) {
      type_resolution::StampType(*this, *resolved);
    }
  }

  _HasAnalysed = true;
  _Resolved = true;
  _Provisional = _ResolvedProvisionally(meta);
  _CachedStringification.clear();
}

auto spp::asts::TypeIdentifierAst::_ResolvedProvisionally(
  CompilerMetaData const *meta) const -> bool {
  IMPORT_UTILS;
  if (meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics or GnArgGroup == nullptr) { return false; }
  return genex::any_of(GnArgGroup->Args, [](auto const &arg) {
    if (arg->CompVal != nullptr) { return comp_generics::NeedsSupScopesToType(*arg->CompVal); }
    auto const *const part = arg->TypeVal != nullptr ? arg->TypeVal->LastTypePart() : nullptr;
    return part != nullptr and part->_Provisional;
  });
}

auto spp::asts::TypeIdentifierAst::_NamesOnly(
  TypeSymbol &type_sym, ScopeManager *sm, CompilerMetaData *meta) const -> bool {
  IMPORT_UTILS;
  return meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
    and genex::any_of(GnArgGroup->GetTypeArgs(), [](auto const *arg) { return MayHoldWrittenBorrow(*arg->TypeVal); })
    and marker_sups::HoldsNoValueOfItsArgs(type_sym, *sm->CurrentScope);
}

auto spp::asts::TypeIdentifierAst::_AnalyseArgs(
  const bool names_only, ScopeManager *sm, CompilerMetaData *meta) -> void {
  const auto _meta_guard = MetaGuard(meta);
  meta->AllowAbstractType = true;
  if (names_only) { meta->AllowBorrowedTypeArgs = true; }
  GnArgGroup->Stage7_AnalyseSemantics(sm, meta);
}

auto spp::asts::TypeIdentifierAst::_RecheckArgs(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  IMPORT_UTILS;
  // Once per stage: a check only becomes due when the stage moves on.
  if (_ResolvedSym == nullptr or _ResolvedSym->IsGn() or meta->CurrentStage <= _RecheckedAt) { return; }
  if (meta->SkipTypeAnalysisGnChecks or GnArgGroup == nullptr) { return; }
  _RecheckedAt = meta->CurrentStage;

  // The nested arguments first, as the analysis that resolved this name analysed them.
  {
    const auto _meta_guard = MetaGuard(meta);
    meta->TypeAnalysisTypeScope = nullptr;
    _AnalyseArgs(_NamesOnly(*_ResolvedSym, sm, meta), sm, meta);
  }

  // Then the argument checks not yet made, by solving again over the arguments the first analysis resolved, named and
  // analysed as the first analysis gave them to its solve (which is what stamps each name with its parameter): given
  // every argument, it infers nothing, and only checks ("GenericSolver::Solve").
  const auto types_due = meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
    and not meta->SkipSubstitutedConstraintChecks;
  const auto comps_due = meta->CurrentStage >= CompilerStage::kAttachSupScopes;
  if ((not types_due or _TypeArgsChecked) and (not comps_due or _CompArgsChecked)) { return; }
  auto const *const gn_params = _ResolvedSym->GnParams();
  if (gn_params != nullptr and not gn_params->Params.empty() and not GnArgGroup->Args.empty()) {
    auto solver = generic_inference::GenericSolver(*gn_params, *_ResolvedSym->LinkedScope, *sm, *meta);
    if (const auto target = _ResolvedSym->AliasTargetId(); target != nullptr) {
      solver.ReadDeclaredWith(InstanceBindings(TypeRef::Of(target, *sm->CurrentScope)));
    }
    auto named = generic_inference::NamedGnArgs(*GnArgGroup, *gn_params, _ResolvedSym->LinkedScope, *this, *sm, *meta);
    {
      const auto _meta_guard = MetaGuard(meta);
      meta->TypeAnalysisTypeScope = nullptr;
      meta->AllowAbstractType = true;
      if (_NamesOnly(*_ResolvedSym, sm, meta)) { meta->AllowBorrowedTypeArgs = true; }
      named->Stage7_AnalyseSemantics(sm, meta);
    }
    solver.Give(std::move(named->Args));
    solver.Solve(*this);
  }
  _TypeArgsChecked = _TypeArgsChecked or types_due;
  _CompArgsChecked = _CompArgsChecked or comps_due;
}

auto spp::asts::TypeIdentifierAst::IsNeverType() const noexcept -> bool {
  return _IsNeverType;
}

auto spp::asts::TypeIdentifierAst::MarkNeverType() -> void {
  _IsNeverType = true;
}

auto spp::asts::TypeIdentifierAst::IsSelfType() const noexcept -> bool {
  return _IsSelfType;
}

auto spp::asts::TypeIdentifierAst::NsParts() const -> Vec<IdentifierAst const*> {
  return {};
}

auto spp::asts::TypeIdentifierAst::NsParts() -> Vec<IdentifierAst*> {
  return {};
}

auto spp::asts::TypeIdentifierAst::ClearSourceWritten() -> void {
  _IsSourceWritten = false;
  for (auto const &arg : GnArgGroup->Args) {
    if (arg->IsTypeArg()) { arg->TypeVal->ClearPartsSourceWritten(); }
  }
}

auto spp::asts::TypeIdentifierAst::MarkSourceWritten() -> void {
  _IsSourceWritten = true;
}

auto spp::asts::TypeIdentifierAst::MarkPackPosition() -> void {
  _IsPackPosition = true;
}

auto spp::asts::TypeIdentifierAst::TypeParts() const -> Vec<TypeIdentifierAst const*> {
  return {this};
}

auto spp::asts::TypeIdentifierAst::TypeParts() -> Vec<TypeIdentifierAst*> {
  return {this};
}

auto spp::asts::TypeIdentifierAst::LastTypePart() const -> TypeIdentifierAst const* {
  return this;
}

auto spp::asts::TypeIdentifierAst::LastTypePart() -> TypeIdentifierAst* {
  return this;
}

auto spp::asts::TypeIdentifierAst::WithoutConvention() const -> Shared<const TypeAst> {
  return shared_from_this();
}

auto spp::asts::TypeIdentifierAst::GetConvention() const -> ConventionAst* {
  return nullptr;
}

auto spp::asts::TypeIdentifierAst::WithConvention(
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

auto spp::asts::TypeIdentifierAst::WithoutGns() const -> Shared<TypeAst> {
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
    GnArgGroup == nullptr or GnArgGroup->Args.empty() ? _StampedTypeId : _StampedTemplateId);
  return _CachedWithoutGns;
}

auto spp::asts::TypeIdentifierAst::WithGns(
  Unique<GenericArgumentGroupAst> &&arg_group) const -> Shared<TypeAst> {
  // Attach the new generic argument group to a clone of this type identifier.
  arg_group = arg_group ? std::move(arg_group) : GenericArgumentGroupAst::NewEmpty();
  const auto with_generics = MakeShared<TypeIdentifierAst>(_Pos, Str(Name), std::move(arg_group));
  with_generics->_IsNeverType = _IsNeverType;
  with_generics->_StampedTemplateId = _StampedTemplateId;
  return with_generics;
}

auto spp::asts::TypeIdentifierAst::IsCompilerGeneratedType() const -> bool {
  // Types starting with "$" are compiler generated: the
  // lexer has no "$" token, so no written name has one.
  return Name[0] == '$';
}

auto spp::asts::TypeIdentifierAst::IsTypeIdentifier() const noexcept -> bool {
  return true;
}

auto spp::asts::TypeIdentifierAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // Fully qualify this type name from the scope.
  // Have to AstClone because PostfixExpressionAst lhs (will change with removal of all shared pointers)
  const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
  const auto type_sym = type_scope->FindTypeSymbol(this);
  return type_sym->FqName();
}

auto spp::asts::TypeIdentifierAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *meta) -> TypeRef {
  // The symbol this name finds where types are being analysed, read here.
  const auto type_scope = meta->TypeAnalysisTypeScope ? meta->TypeAnalysisTypeScope : sm->CurrentScope;
  const auto type_sym = type_scope->FindTypeSymbol(this);
  return type_sym != nullptr ? TypeRef::Of(*type_sym, *sm->CurrentScope) : TypeRef();
}

auto spp::asts::TypeIdentifierAst::AnkerlHash() const -> std::size_t {
  // Hash based on the name only.
  return Hash<Str>()(Name);
}

auto spp::asts::TypeIdentifierAst::ToView() const -> StrView {
  if (GnArgGroup == nullptr or GnArgGroup->Args.empty()) {
    return Name;
  }

  if (_CachedStringification.empty() or not _Resolved) {
    _CachedStringification = Name;
    _CachedStringification.append(GnArgGroup->ToString());
  }
  return _CachedStringification;
}

auto spp::asts::TypeIdentifierAst::NsPartsInto(
  Vec<IdentifierAst const*> &) const -> void {
}

auto spp::asts::TypeIdentifierAst::TypePartsInto(
  Vec<TypeIdentifierAst const*> &out) const -> void {
  out.emplace_back(this);
}

auto spp::asts::TypeIdentifierAst::InferFromAttributes(
  Vec<Tup<Shared<IdentifierAst>, Shared<TypeAst>, Shared<TypeAst>>> &&equations) -> void {
  _AttributeEquations = std::move(equations);
}

auto spp::asts::TypeIdentifierAst::_CheckVisibility(
  TypeSymbol const &type_sym, ScopeManager *sm, CompilerMetaData *meta) const
  -> void {
  IMPORT_UTILS;

  // A type declared in a "sup" block is a member of the type
  // that block is over, so it follows the type-level rule
  // like the block's attributes and methods do. Anything else
  // is a module member.
  const auto def_node = type_sym.ScopeDefinedIn->AstNode;
  const auto in_sup_block = def_node != nullptr and (
    AstAs<SupPrototypeFunctionsAst>(def_node) != nullptr or AstAs<SupPrototypeExtensionAst>(def_node) != nullptr);
  const auto owner_sym = in_sup_block
    ? type_sym.ScopeDefinedIn->FindHeadSymbol(*AstName(def_node))
    : nullptr;

  if (owner_sym != nullptr and owner_sym->LinkedScope != nullptr) {
    visibility_utils::CheckTypeTypeVisibility(type_sym, *this, *owner_sym->LinkedScope->NonGnScope, *sm, *meta);
  }
  else {
    visibility_utils::CheckModuleTypeVisibility(type_sym, *this, *type_sym.ScopeDefinedIn, *sm, *meta);
  }
}

auto spp::asts::TypeIdentifierAst::CheckPendingVisibility(
  ScopeManager *sm)
  -> void {
  auto pending = std::move(_PendingVisibility);
  _PendingVisibility.clear();
  for (auto const &[written, sym, from] : pending) {
    auto tm = sm->NewManagerAt(from);
    auto meta = CompilerMetaData();
    meta.CurrentStage = CompilerStage::kPreAnalyseSemantics;
    written->_CheckVisibility(*sym, &tm, &meta);
  }
}

auto spp::asts::TypeIdentifierAst::ClearPendingVisibility() -> void {
  _PendingVisibility.clear();
}

SPP_MOD_END
