module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.generic_inference;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.utils.ice;
import spp.utils.interner;
import genex;

use_alias(spp::analyse::scopes, ExprSubst);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts, AstCloneShared);
use_alias(spp::asts, LiteralAst);
use_alias(spp::asts, TupleLiteralAst);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::asts::meta, CompilerStage);
use_alias(spp::asts::meta, MetaGuard);

namespace spp::analyse::utils::generic_inference {
  namespace {
    /// [CHECKED]
    /// Reject the first argument name no parameter has, reported
    /// against the first parameter (or the argument itself, when
    /// there are none).
    auto EnforceGnArgNamesKnown(
      Vec<GenericParameterAst*> const &params, Vec<GenericArgumentAst*> const &args) -> void {
      // A pack is never named: it takes the positionals after the parameters before it (pack-design phase 11).
      for (auto const *arg : args) {
        const auto known = genex::any_of(params, [arg](auto const *p) {
          return *p->Name == *arg->KeywordName() and not p->IsVariadic();
        });
        if (known) { continue; }
        Raise<errors::SppArgumentNameInvalidError>(
          ERR_ARGS(
            params.empty() ? static_cast<Ast const&>(*arg->KeywordName()) : *params[0], StrView("gn param"),
            *arg->KeywordName(), StrView("gn arg")));
      }
    }

    /// Whether "type" names the type parameter "param" anywhere in it, its generic arguments included ("Add[Ts]").
    auto NamesTypeParam(
      TypeAst const &type, GenericParameterAst const &param) -> bool {
      auto const *const name = param.Name->To<TypeIdentifierAst>();
      for (auto const *part : type.TypeParts()) {
        if (name != nullptr and part->Name == name->Name and part->GnArgGroup->Args.empty()) { return true; }
        for (auto const *arg : part->GnArgGroup->GetTypeArgs()) {
          if (NamesTypeParam(*arg->TypeVal, param)) { return true; }
        }
      }
      return false;
    }

    /// Check each type argument ("args": its parameter, and the
    /// types it is checked as: the identity bound to it and the
    /// type shown for it in an error, one per element for a pack)
    /// against its parameter's constraints, read where the
    /// parameters are written ("written_scope", which an
    /// unsatisfied one is reported from) with the arguments bound
    /// ("bindings"), from the use site.
    auto CheckTypeArgConstraints(
      Vec<Pair<GenericParameterAst const*, Vec<Pair<TypeId, Shared<TypeAst>>>>> const &args,
      Scope const &written_scope, GenericSubst const &bindings,
      ScopeManager &sm, CompilerMetaData &meta) -> void {
      using errors::SppGenericConstraintError;
      auto const &scope = *sm.CurrentScope;
      for (auto const &[param, targets] : args) {
        // This parameter's constraints, with the arguments bound ("with" in place of "bindings"), read from the scope
        // of the type checked ("con_sm"), where "Self" is that type: each element's, for a pack.
        const auto read_constraints = [&](GenericSubst const &with, ScopeManager &con_sm) {
          auto cons = Vec<Shared<TypeAst>>();
          for (auto const &p_con : param->TypeConstraints->Constraints) {
            const auto sub = type_resolution::ReadType(*p_con, ExprSubst::Across(written_scope, with, scope));
            {
              // Resolved from the argument's scope, but written, and
              // checked for visibility, where the parameter is declared:
              // a module-private alias there is not visible from, say,
              // a closure argument's (global) scope, and needn't be.
              const auto _meta_guard = MetaGuard(&meta);
              meta.AllowAbstractType = true;
              meta.IgnoreAccessModifierViolations = true;
              sub->Stage7_AnalyseSemantics(&con_sm, &meta);
            }
            cons.push_back(sub->WithSourceSpanOf(*p_con));
          }
          return cons;
        };

        // A pack named in its own constraint ("..Ts: Add[Ts]") stands for each other element there, not its list:
        // every element meets the constraint as read with the pack bound to each element in turn ("Self" being the
        // element checked), so "xs#i + xs#j" holds for every pair a fold over the pack combines.
        const auto names_itself = param->IsVariadic() and genex::any_of(
          param->TypeConstraints->Constraints, [&](auto const &p_con) { return NamesTypeParam(*p_con, *param); });
        const auto pairwise = [&](scopes::TypeId target, scopes::TypeId other, ScopeManager &con_sm) {
          auto with = bindings;
          for (auto &[id, value] : with.TypeParams) {
            if (id == param->ParamId()) { value = other; }
          }
          std::erase_if(with.TypeParams, [](auto const &binding) { return binding.first == 0; });
          with.TypeParams.emplace_back(0, target);
          return read_constraints(with, con_sm);
        };

        // Raise an error if any constraint of this argument is not satisfied.
        for (auto const &[target, target_ast] : targets) {
          auto const *const target_sym = TypeRef::Of(target, scope).Symbol;
          if (target_sym == nullptr) { continue; }
          auto con_sm = sm.NewManagerAt(target_sym->LinkedScope != nullptr ? target_sym->LinkedScope : sm.CurrentScope);
          auto per_other = Vec<Vec<Shared<TypeAst>>>();
          if (names_itself) {
            for (auto const &[other, other_ast] : targets) { per_other.emplace_back(pairwise(target, other, con_sm)); }
          }
          else { per_other.emplace_back(read_constraints(bindings, con_sm)); }
          for (auto const &cons : per_other) {
            const auto unsatisfied = type_compare::UnmetConstraint(
              cons, TypeRef::Of(target, scope), scopes::IsSelfTypeId(target), scope, scope);
            RaiseIf<SppGenericConstraintError>(
              unsatisfied != nullptr,
              ERR_ARGS(*unsatisfied, *target_ast));
          }
        }
      }
    }

    /// Name the positional arguments of one kind after the
    /// parameters they bind ("type_resolution::ParamsBoundByArgs";
    /// "NameTypeArgs", "NameCompArgs"): each is copied under
    /// its parameter's name by "bind", until a trailing variadic
    /// parameter takes the rest, its pack's elements, which stay
    /// positional ("f[U32, U32]" for "f[..Ts]", "f[1_u32, 2_u32]"
    /// for "f[cmp ..s]"), each analysed by "elements": the pack's
    /// list is keyed from them ("Scope::ArgsIdOf").
    auto NameArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Ast const &owner,
      CompilerMetaData &meta,
      FuncRef<void(Vec<GenericArgumentAst*> const &)> elements,
      FuncRef<void(GenericArgumentAst &, GenericArgumentAst const &)> bind) -> void {
      const auto keyword_args = args
        | genex::views::ptr
        | genex::views::filter([](auto const *x) { return x->KeywordName() != nullptr; })
        | genex::to<Vec>();
      EnforceGnArgNamesKnown(params, keyword_args);

      // Each argument names its parameter by identity as well as by
      // name: a keyword argument's name, matched to its parameter
      // by spelling just above, is stamped with the parameter's
      // identity, and a positional one is given the parameter's
      // name, stamped. A parameter not declared yet has none.
      const auto name_of = [](GenericParameterAst const &param) -> Shared<TypeAst> {
        if (param.Name->StampedTypeId() != nullptr or param.ParamId() == 0) { return param.Name; }
        auto name = AstCloneShared(param.Name);
        name->StampTypeId(scopes::ParamTypeId(param.ParamId()));
        return name;
      };
      for (auto const *arg : keyword_args) {
        auto const &name = *arg->KeywordName();
        const auto param = genex::find_if(params, [&name](auto const *p) { return *p->Name == name; });
        if (param != params.end() and (*param)->ParamId() != 0 and name.StampedTypeId() == nullptr) {
          name.StampTypeId(scopes::ParamTypeId((*param)->ParamId()));
        }
      }

      const auto targets = type_resolution::ParamsBoundByArgs(args | genex::views::ptr | genex::to<Vec>(), params);

      const auto _meta_guard = MetaGuard(&meta);
      meta.TypeAnalysisTypeScope = nullptr;

      auto const *keyword_before = static_cast<GenericArgumentAst const*>(nullptr);
      for (auto i = 0uz; i < args.size(); ++i) {
        auto const &positional = args[i];
        if (positional->KeywordName() != nullptr) {
          keyword_before = positional.get();
          continue;
        }
        auto const *const param = targets[i];
        RaiseIf<errors::SppGenericArgumentTooManyError>(
          param == nullptr,
          ERR_ARGS(params.empty() ? owner : *params[0], owner, *positional));

        // A positional after a keyword is only a variadic parameter's tail ("P[A=S32, Bool, U8]").
        RaiseIf<errors::SppOrderInvalidError>(
          keyword_before != nullptr and not param->IsVariadic(),
          ERR_ARGS(Str("kKeywordArg"), *keyword_before, Str("kPositionalArg"), *positional));
        auto named = MakeUnique<GenericArgumentAst>(name_of(*param), nullptr, nullptr, nullptr);

        if (param->IsVariadic()) {
          elements(args | genex::views::ptr | genex::views::drop(i) | genex::to<Vec>());
          break;
        }
        bind(*named, *positional);
        args[i] = std::move(named);
      }
    }

    /// "NameArgs" for type arguments. A type pack's elements stay
    /// positional, each analysed: an element naming a bound pack
    /// is spliced into the list where it is keyed ("A[S32, Ts]"
    /// with "Ts" bound to "Bool, U8" is "A[S32, Bool, U8]"). Each
    /// is analysed once types resolve.
    auto NameTypeArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Ast const &owner,
      ScopeManager &sm, CompilerMetaData &meta) -> void {
      const auto analyse = [&](GenericArgumentAst &named) {
        if (meta.CurrentStage >= CompilerStage::kResolveDeclarations) {
          named.TypeVal->Stage7_AnalyseSemantics(&sm, &meta);
        }
      };
      const auto elements = [&](Vec<GenericArgumentAst*> const &rest) {
        for (auto *const elem : rest) { analyse(*elem); }
      };
      const auto bind = [&](GenericArgumentAst &named, GenericArgumentAst const &positional) {
        named.TypeVal = AstCloneShared(positional.TypeVal);
        analyse(named);
      };
      NameArgs(args, params, owner, meta, elements, bind);
    }

    /// "NameTypeArgs" for comp arguments: a comp pack's elements stay positional, each analysed, one naming a bound
    /// comp pack spliced into the list where it is keyed. A literal or a name is analysed once types resolve; anything
    /// else waits until stage 5 ends, as a comp expression ("n + 1") resolves its operator through the sup scopes of
    /// its operand's type, attached only then. An operator expression is never analysed here - analysing it consumes it
    /// - but by the argument's own "AnalyseCompVal", which folds it or checks a copy.
    auto NameCompArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Ast const &owner,
      ScopeManager &sm, CompilerMetaData &meta) -> void {
      const auto analyse = [&](GenericArgumentAst &arg) {
        auto &val = *arg.CompVal;
        const auto stage = meta.CurrentStage;
        if (stage >= CompilerStage::kResolveDeclarations and (not comp_generics::NeedsSupScopesToType(val)
          or (stage >= CompilerStage::kPreAnalyseSemantics and not comp_generics::IsCompOperator(val)))) {
          arg.AnalyseCompExpr(val, &sm, &meta);
        }
      };
      const auto elements = [&](Vec<GenericArgumentAst*> const &rest) {
        for (auto *const elem : rest) { analyse(*elem); }
      };
      const auto bind = [&](GenericArgumentAst &named, GenericArgumentAst const &positional) {
        named.CompVal = AstCloneShared(positional.CompVal);
        named.ExpectedCompType = positional.ExpectedCompType;
        analyse(named);
      };
      NameArgs(args, params, owner, meta, elements, bind);
    }

    /// A comp pack's elements, an element forwarding an instance's pack (bound to its tuple) taken as its elements.
    auto PackSources(
      Vec<Shared<ExpressionAst>> const &elems, Scope const &scope) -> Vec<ExpressionAst*> {
      auto out = Vec<ExpressionAst*>();
      for (auto const &elem : elems) {
        if (auto spread = packs::BoundCompPackElements(*elem, scope); spread.has_value()) { out.append_range(*spread); }
        else { out.emplace_back(elem.get()); }
      }
      return out;
    }

    /// A comp pack's element, typed: one naming a template's open pack is what that pack declares each element to be
    /// ("PackInfo"), not the open tuple it is as a whole.
    auto CompElementType(
      ExpressionAst &elem, ScopeManager &sm, CompilerMetaData &meta) -> Shared<TypeAst> {
      auto const *const name = elem.To<IdentifierAst>();
      auto const *const sym = name != nullptr ? sm.CurrentScope->FindVarSymbol(name) : nullptr;
      if (sym != nullptr and sym->Pack.has_value() and sym->Pack->Open) { return sym->Pack->Element; }
      return elem.InferType(&sm, &meta);
    }
  }
}

SPP_MOD_BEGIN
/// A type a parameter is offered. Its identity is what the solver compares, substitutes and checks; it is null only
/// for a type given that does not key (yet), which is passed on as written. "Written" is the type as given, or the
/// default as declared; "Site" what an inferred type, or a default, is read back pointing at. One "Written" with no
/// "Site" is shown as given.
struct GenericSolver::_TypeVal {
  TypeId Id = nullptr;
  Shared<TypeAst> Written;
  Shared<TypeAst> Site;

  /// A type pack's elements as they were given ("Id" is their list, "scopes::PackIdOf"); "Written" is null for a pack,
  /// which has no type to spell it.
  Vec<Shared<TypeAst>> Elems;
};

/// A comp value a parameter is offered, and for one inferred, the type of what it was read off ("Site"), pointing at
/// where that was given, so an error names it there; a comp value has no position of its own once read back from its
/// identity.
struct GenericSolver::_CompVal {
  /// The value; null for a pack's, which is a list.
  Shared<ExpressionAst> Val;
  Shared<TypeAst> Site;

  /// A pack's list: its identity ("CompKey::OfPack") when read back from one, else keyed from "Elems" as given.
  bool IsPack = false;
  scopes::CompId Id = nullptr;
  Vec<Shared<ExpressionAst>> Elems;
};

/// Everything one generic parameter (or a name given alongside
/// the parameters, like a pinned "Self") is offered.
struct GenericSolver::_Entry {
  /// The parameter, or @c nullptr for a given name that is not
  /// one ("Self", or a parameter of another declaration).
  GenericParameterAst const *Param;

  /// The parameter's identity ("ParamId"), which is what a name
  /// is found by; 0 for a name that is no parameter, or one not
  /// declared yet.
  std::uint64_t Id = 0;

  /// The parameter's name: what the solution's argument is named,
  /// and what an error shows. Entries are found by identity
  /// ("_Find"), never by it.
  Shared<TypeIdentifierAst> Name;
  Vec<_TypeVal> TypeVals;
  Vec<_CompVal> CompVals;

  /// Given a type rather than inferred one, so inference neither
  // offers it another nor rewrites it.
  bool IsTypeGiven = false;

  /// Given a comp value rather than inferred one.
  bool IsCompGiven = false;

  /// Bound to its default, which is written in the declaration's
  /// own terms and so may name the others.
  bool IsTypeFromDefault = false;
  bool IsCompFromDefault = false;

  /// Whether the pinned "Self" is part of the solution (a
  /// declared parameter always is).
  bool Emit = false;

  /// Whether the comp value's type, or the bound type's constraints, have been inferred from yet: each is read once,
  /// once there is something to read, which is what makes the solve a worklist rather than a fixed pass order.
  bool IsCompValueRead = false;
  bool IsConstraintsRead = false;

  SPP_ATTR_NODISCARD auto IsBound() const -> bool { return not TypeVals.empty() or not CompVals.empty(); }
  SPP_ATTR_NODISCARD auto IsTypeParam() const -> bool { return Param != nullptr and Param->IsTypeParam(); }
  SPP_ATTR_NODISCARD auto IsCompParam() const -> bool { return Param != nullptr and Param->IsCompParam(); }
};

/// What was given (the source) where something is declared (the target), under the name of that declaration.
struct spp::analyse::utils::generic_inference::GenericSolver::_Equation {
  Shared<IdentifierAst> Name;
  Shared<TypeAst> Source;
  Shared<TypeAst> Target;

  /// A pack's ("UnifyPack"): the identities of the given list and of the declared one, "Source" only its site.
  scopes::TypeId GivenId = nullptr;
  scopes::TypeId DeclaredId = nullptr;
};

GenericSolver::GenericSolver(
  GenericParameterGroupAst const &params, Scope const &owner_scope, ScopeManager &sm, CompilerMetaData &meta) :
  _Params(&params), _OwnerScope(&owner_scope), _Sm(&sm), _Meta(&meta) {
  for (auto const *param : params.GetAllParams()) {
    auto entry = MakeUnique<_Entry>();
    entry->Param = param;
    entry->Id = param->ParamId();
    entry->Name = shared_cast<TypeIdentifierAst>(param->Name);
    _Entries.emplace_back(std::move(entry));
  }
}

GenericSolver::~GenericSolver() = default;

auto GenericSolver::_FindById(
  const std::uint64_t pid) const -> _Entry* {
  // The entry of the parameter "pid", if this declaration declares it.
  if (pid == 0) { return nullptr; }
  for (auto const &entry : _Entries) {
    if (entry->Id == pid) { return entry.get(); }
  }
  return nullptr;
}

auto GenericSolver::_Find(
  TypeIdentifierAst const &name) const -> _Entry* {
  // By identity: a name stamped as one of this declaration's parameters ("TypeParam", a binding of it included) is
  // that parameter's entry. Every name a solver is given records one ("NameArgs" and
  // "Scope::TypeAstOf" stamp the names they make; "Self" has its own entry, "_Self").
  if (const auto id = name.StampedTypeId(); id != nullptr) {
    auto const &head = scopes::HeaderOf(id);
    if (head.Kind == scopes::TypeKey::Tag::TypeParam) {
      return _FindById(head.TypeParamId);
    }
  }
  return nullptr;
}

auto GenericSolver::_AstOf(
  _TypeVal const &val) const -> Shared<TypeAst> {
  // A type given, or one that does not key (a default's own copy), as it was written; anything else read back from its
  // identity, on a copy, as the solution is analysed in place. A default is shown as the type it was declared as; an
  // inferred type at what it was read from, in its own spelling: "T" read off a "Vec[T]" is not that "Vec".
  if (val.Id == nullptr or (val.Written != nullptr and val.Site == nullptr)) { return val.Written; }
  const auto type = _Sm->CurrentScope->TypeAstOf(val.Id);
  if (val.Site == nullptr) { return AstCloneShared(type); }
  return val.Written != nullptr ? type->WithSourceSpanOf(*val.Site) : type->WithSourceSpanAt(*val.Site);
}

auto GenericSolver::_Bindings(
  _Entry const *except) const -> scopes::GenericSubst {
  // Every parameter bound so far, by identity, but "except": what a type or value written in the declaration's terms
  // is read with. A comp value is keyed where it was given.
  auto out = scopes::GenericSubst();
  for (auto const &entry : _Entries) {
    if (entry.get() == except or entry->Param == nullptr or entry->Id == 0) { continue; }
    if (entry->IsTypeParam() and not entry->TypeVals.empty() and entry->TypeVals[0].Id != nullptr) {
      out.TypeParams.emplace_back(entry->Id, entry->TypeVals[0].Id);
    }
    else if (entry->IsCompParam() and not entry->CompVals.empty()) {
      if (const auto id = _EntryCompId(*entry); id != nullptr) { out.CompParams.emplace_back(entry->Id, id); }
    }
  }
  return out;
}

auto GenericSolver::Give(
  Vec<Unique<GenericArgumentAst>> args) -> void {
  // Taken by value, so the caller's list is emptied rather
  // than left holding moved-from arguments. A type pack's elements
  // come positionally, after the named arguments ("NamedGnArgs"),
  // and are offered together, as the pack's list.
  auto pack_elems = Vec<Shared<TypeAst>>();
  auto comp_pack_elems = Vec<Shared<ExpressionAst>>();
  for (auto &arg : args) {
    if (arg->KeywordName() == nullptr and arg->IsTypeArg()) {
      pack_elems.emplace_back(arg->TypeVal);
      _Given.emplace_back(std::move(arg));
      continue;
    }
    if (arg->KeywordName() == nullptr and arg->IsCompArg()) {
      comp_pack_elems.emplace_back(AstCloneShared(arg->CompVal));
      _Given.emplace_back(std::move(arg));
      continue;
    }

    // ICE if we bug the argument ast translations.
    if (arg->KeywordName() == nullptr or (not arg->IsTypeArg() and not arg->IsCompArg())) {
      const auto err = "generic argument '" + arg->ToString() + "' is still positional where a binding is expected";
      Ice(*arg, *_Sm->CurrentScope, err);
    }

    // A layer given earlier outranks this one: the first binding
    // offered for a name is the one it keeps. "Self" is the
    // pinned "Self", only seen by the inference.
    const auto name = shared_cast<TypeIdentifierAst>(arg->KeywordName());
    if (not name->IsSelfType() and name->StampedTypeId() == nullptr) {
      const auto err = "generic argument '" + arg->ToString() + "' is not named by its parameter's identity";
      Ice(*arg, *_Sm->CurrentScope, err);
    }
    auto entry = name->IsSelfType() ? _TakeSelf(false) : _Find(*name);
    if (entry == nullptr and name->IsSelfType()) { continue; }
    if (entry != nullptr and entry->IsBound()) { continue; }
    if (entry == nullptr) {
      auto extra = MakeUnique<_Entry>();
      extra->Param = nullptr;
      extra->Name = name;
      entry = extra.get();
      _Entries.emplace_back(std::move(extra));
    }
    if (arg->IsTypeArg()) {
      entry->TypeVals.emplace_back(
        _TypeVal{.Id = _Sm->CurrentScope->TypeIdOf(*arg->TypeVal), .Written = arg->TypeVal, .Site = nullptr});
      entry->IsTypeGiven = true;
    }
    else {
      entry->CompVals.emplace_back(_CompVal{.Val = arg->CompVal, .Site = nullptr});
      entry->IsCompGiven = true;
    }
    _Given.emplace_back(std::move(arg));
    _AnyGiven = true;
  }

  // A comp pack's elements, offered together as its list ("_ValCompId" keys it).
  auto const *const pack = _Params != nullptr ? _Params->GetVariadicParam() : nullptr;
  if (not comp_pack_elems.empty()) {
    auto *const entry = pack != nullptr and pack->IsCompParam() ? _FindById(pack->ParamId()) : nullptr;
    if (entry == nullptr) {
      Ice(*comp_pack_elems[0], *_Sm->CurrentScope, "positional comp arguments with no comp pack to take them");
    }
    if (not entry->IsBound()) {
      entry->CompVals.emplace_back(_CompVal{.IsPack = true, .Elems = std::move(comp_pack_elems)});
      entry->IsCompGiven = true;
      _AnyGiven = true;
    }
  }

  if (pack_elems.empty()) { return; }
  auto *const entry = pack != nullptr and pack->IsTypeParam() ? _FindById(pack->ParamId()) : nullptr;
  if (entry == nullptr) {
    Ice(*pack_elems[0], *_Sm->CurrentScope, "positional generic arguments with no type pack to take them");
  }
  if (entry->IsBound()) { return; }
  const auto elems = pack_elems
    | genex::views::transform([](auto const &e) -> TypeAst const* { return e.get(); })
    | genex::to<Vec>();
  entry->TypeVals.emplace_back(_TypeVal{
    .Id = _Sm->CurrentScope->PackIdOfElements(elems), .Written = nullptr, .Site = nullptr, .Elems = pack_elems});
  entry->IsTypeGiven = true;
  _AnyGiven = true;
}

auto GenericSolver::_TakeSelf(
  const bool emit) -> _Entry* {
  // The "Self" entry, made (after every entry made so far, so the
  // solution's order is the order things were given) the first
  // time "Self" is offered; null once it is bound, as an earlier
  // layer outranks a later one.
  if (_Self != nullptr) { return _Self->IsBound() ? nullptr : _Self; }
  auto entry = MakeUnique<_Entry>();
  entry->Param = nullptr;
  entry->Name = static_shared_cast<TypeIdentifierAst>(asts::generate::common_types::SelfType(0));
  entry->Name->StampTypeId(scopes::ParamTypeId(0));
  entry->Emit = emit;
  _Self = entry.get();
  _Entries.emplace_back(std::move(entry));
  return _Self;
}

auto GenericSolver::PinSelf(
  Shared<TypeAst> type, const bool emit) -> void {
  // Held as "Give" holds a type given. The argument is kept with
  // the others given, as a declaration with no parameters answers
  // with what it was given ("_Trivial").
  auto *const entry = _TakeSelf(emit);
  if (entry == nullptr) { return; }
  entry->TypeVals.emplace_back(_TypeVal{.Id = _Sm->CurrentScope->TypeIdOf(*type), .Written = type, .Site = nullptr});
  entry->IsTypeGiven = true;
  _Given.emplace_back(GenericArgumentAst::NewType(entry->Name, std::move(type)));
  _AnyGiven = true;
}

auto GenericSolver::Give(
  GenericSubst const &bindings) -> void {
  // As "Give" does for arguments, each binding under its parameter (by identity), held as the identity it is; a layer
  // given earlier outranks it.
  // The entry is found by the parameter's identity. A binding of a parameter this declaration does not declare is not
  // this solve's, even under a name one of its own shares: "fun ior_[E]" in "sup [T, E] Res[T, E]" declares an "E" of
  // its own, which the receiver's "E" does not bind.
  const auto take = [&](const std::uint64_t pid) -> _Entry* {
    auto *const entry = _FindById(pid);
    if (entry == nullptr or entry->IsBound()) { return nullptr; }
    _AnyGiven = true;
    return entry;
  };
  for (auto const &[pid, id] : bindings.TypeParams) {
    if (id == nullptr) { continue; }
    if (auto *const entry = take(pid); entry != nullptr) {
      entry->TypeVals.emplace_back(_TypeVal{.Id = id, .Written = nullptr, .Site = nullptr});
      entry->IsTypeGiven = true;
    }
  }
  for (auto const &[pid, id] : bindings.CompParams) {
    if (id == nullptr) { continue; }
    if (auto *const entry = take(pid); entry != nullptr) {
      entry->CompVals.emplace_back(_CompValOf(*entry, id, nullptr));
      entry->IsCompGiven = true;
    }
  }
}

auto GenericSolver::Unify(
  Shared<IdentifierAst> const &name, Shared<TypeAst> source, Shared<TypeAst> target) -> void {
  auto eq = MakeUnique<_Equation>();
  eq->Name = name;
  eq->Source = std::move(source);
  eq->Target = std::move(target);
  _Equations.emplace_back(std::move(eq));
}

auto GenericSolver::UnifyPack(
  Shared<IdentifierAst> const &name, Vec<Shared<TypeAst>> const &source_elems, Shared<TypeAst> const &target,
  Shared<TypeAst> site) -> void {
  // Keyed now, as lists: the given elements where the call is (an element naming a bound pack spliced in), the
  // declared pack where the declaration is ("Ts" alone, or what the owner binds it to).
  const auto elems = source_elems
    | genex::views::transform([](auto const &e) -> TypeAst const* { return e.get(); })
    | genex::to<Vec>();
  auto eq = MakeUnique<_Equation>();
  eq->Name = name;
  eq->Source = std::move(site);
  eq->GivenId = _Sm->CurrentScope->PackIdOfElements(elems);
  eq->DeclaredId = _OwnerScope->PackIdOfElements({target.get()});
  _Equations.emplace_back(std::move(eq));
}

auto GenericSolver::ReadDeclaredWith(
  scopes::GenericSubst reading) -> void {
  _DeclaredReading = std::move(reading);
}

auto GenericSolver::KnownBindings(
  GenericParameterGroupAst const &params) const -> Opt<GenericSubst> {
  // What was given, by the parameter each binds (as
  // "BindingsOfArgs" binds over the given layers), packs marked;
  // and "Self" where it is pinned.

  // If nothing has been given, short circuit out of this getter
  // function. No point wasting search work.
  if (not _AnyGiven) { return Nullopt; }

  // Extract the parameter ids for the inputted parameter group,
  // and declare an empty substitution map.
  const auto declared = scopes::ParamsDeclaredBy(params);
  auto out = GenericSubst();

  //
  for (auto const &entry : _Entries) {
    if (not entry->IsBound()) { continue; }
    if (entry.get() == _Self) {
      if (entry->TypeVals.empty()) { continue; }
      if (const auto self_id = _EntryTypeId(*entry); self_id != nullptr) { out.TypeParams.emplace_back(0, self_id); }
      continue;
    }
    const auto pid = _EntryParamId(*entry);
    if (not entry->TypeVals.empty() and genex::contains(declared.TypeParams, pid)) {
      const auto id = _EntryTypeId(*entry);
      if (id == nullptr) { continue; }
      out.TypeParams.emplace_back(pid, id);
    }
    else if (not entry->CompVals.empty() and genex::contains(declared.CompParams, pid)) {
      out.CompParams.emplace_back(pid, _EntryCompId(*entry));
    }
  }
  return out;
}

auto GenericSolver::_EntryParamId(
  _Entry const &entry) const -> std::uint64_t {
  // The parameter an entry's argument is keyed by ("Scope::ArgsIdOf"): the one its name records, else its own.
  if (const auto stamped = entry.Name->StampedTypeId(); stamped != nullptr) {
    auto const &head = scopes::HeaderOf(stamped);
    if (head.Kind == scopes::TypeKey::Tag::TypeParam) {
      return head.TypeParamId;
    }
  }
  return entry.Id;
}

auto GenericSolver::_EntryTypeId(
  _Entry const &entry) const -> scopes::TypeId {
  // Its type's identity; one given as written that does not resolve yet ("Wrap[3_uz]::doubled" read before the block
  // declaring it is attached) keyed as written, what does not resolve spelled ("Scope::PartialTypeIdOf").
  auto const &val = entry.TypeVals[0];
  if (val.Id != nullptr) { return val.Id; }
  return val.Written != nullptr ? _Sm->CurrentScope->PartialTypeIdOf(*val.Written) : nullptr;
}

auto GenericSolver::_EntryCompId(
  _Entry const &entry) const -> scopes::CompId {
  return _ValCompId(entry.CompVals[0]);
}

auto GenericSolver::_ValCompId(
  _CompVal const &val) const -> scopes::CompId {
  // A value's identity; a pack's is its list ("CompKey::OfPack"), an element naming a bound pack spliced in.
  auto const &scope = *_Sm->CurrentScope;
  if (not val.IsPack) { return scope.CompIdOf(*val.Val); }
  if (val.Id != nullptr) { return val.Id; }
  auto elems = Vec<scopes::CompId>();
  for (auto const &elem : val.Elems) {
    const auto id = scope.CompIdOf(*elem);
    if (id == nullptr) { return nullptr; }
    if (scopes::IsPackCompId(id)) { elems.append_range(id->Kids); }
    else { elems.push_back(id); }
  }
  return scopes::InternCompKey(scopes::CompKey::OfPack(std::move(elems)));
}

auto GenericSolver::_CompValOf(
  _Entry const &entry, const scopes::CompId id, Shared<TypeAst> site) const -> _CompVal {
  // A value read back from its identity. A pack's is its list, each element spelled ("Scope::CompAstOf"); an identity
  // that is no list (a template's pack forwarded whole) is its one element.
  auto const &scope = *_Sm->CurrentScope;
  auto out = _CompVal{.Site = std::move(site)};
  if (entry.Param == nullptr or not entry.Param->IsVariadic()) {
    out.Val = scope.CompAstOf(id);
    return out;
  }
  out.IsPack = true;
  if (not scopes::IsPackCompId(id)) {
    out.Elems.emplace_back(scope.CompAstOf(id));
    return out;
  }
  out.Id = id;
  for (const auto kid : id->Kids) { out.Elems.emplace_back(scope.CompAstOf(kid)); }
  return out;
}

auto GenericSolver::_ShownCompVal(
  _CompVal const &val) const -> Shared<ExpressionAst> {
  // What an error shows for a value: a pack as the tuple of its elements ("(..n)").
  if (not val.IsPack) { return val.Val; }
  auto elems = Vec<Unique<ExpressionAst>>();
  for (auto const &elem : val.Elems) { elems.emplace_back(AstClone(elem)); }
  return MakeShared<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
}

auto GenericSolver::_OfferAll(
  scopes::GenericSubst const &inferred, Shared<TypeAst> const &site, FuncRef<bool(_Entry const &)> skip) -> bool {
  // Each parameter of this declaration that a match bound is offered what it was bound to, by "ParamId"; a parameter
  // of anything else the match named (a class's, read through) is not this solve's.
  auto newly_bound = false;
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr or entry->Id == 0 or skip(*entry)) { continue; }
    const auto was_bound = entry->IsBound();
    if (entry->IsTypeParam()) {
      const auto hit = genex::find_if(inferred.TypeParams, [&](auto const &x) { return x.first == entry->Id; });
      if (hit == inferred.TypeParams.end()) { continue; }
      entry->TypeVals.emplace_back(_TypeVal{.Id = hit->second, .Written = nullptr, .Site = site});
    }
    else {
      const auto hit = genex::find_if(inferred.CompParams, [&](auto const &x) { return x.first == entry->Id; });
      if (hit == inferred.CompParams.end()) { continue; }
      entry->CompVals.emplace_back(_CompValOf(*entry, hit->second, site));
    }
    newly_bound = newly_bound or not was_bound;
  }
  return newly_bound;
}

auto GenericSolver::_Match(
  TypeAst const &source, TypeAst const &target) const -> scopes::GenericSubst {
  // Match the type of what was given against the type it was given for, by identity, binding the generics the target
  // names. Conventions are not part of it ("x: &T" given "&Str" binds "T" to "Str"). Either side not keying binds
  // nothing.
  return _MatchIds(_Sm->CurrentScope->TypeIdOf(source), _OwnerScope->TypeIdOf(target));
}

auto GenericSolver::_MatchIds(
  const scopes::TypeId given, scopes::TypeId declared) const -> scopes::GenericSubst {
  // "_Match", by identity: "given" keyed where the call is, "declared" where the declaration is.
  auto const &scope = *_Sm->CurrentScope;
  auto inferred = scopes::GenericSubst();
  if (given == nullptr or declared == nullptr) { return inferred; }
  if (not _DeclaredReading.IsEmpty()) { declared = scopes::SubstituteTypeId(declared, _DeclaredReading); }
  const auto given_id = scopes::BareOf(given);
  declared = scopes::BareOf(declared);
  const auto matched = scopes::UnifyTypeIds(given_id, declared, scope, *_OwnerScope, inferred, true, true);

  // A value can match through a type it is superimposed with, and then that is what the generics are read from: a
  // closure is a "FunMov[(S32,), Bool]" only through its superimposition, so "f: FunMov[(S32,), U]" had nothing to
  // take "U" from. Only a target with a shape to match: a bare "x: T" that failed did so on its constraints, which a
  // supertype would silently dodge ("T: ThreadSafe" bound to a closure's sup).
  auto const &d = scopes::HeaderOf(declared);
  const auto is_bare_param = d.Kind == scopes::TypeKey::Tag::TypeParam;
  if (matched or is_bare_param or not scopes::DoesTypeIdNameAnyGnParams(declared)) { return inferred; }
  auto const *const given_sym = TypeRef::Of(given_id, scope).Symbol;
  if (given_sym == nullptr or given_sym->LinkedScope == nullptr) { return inferred; }
  for (auto const &[cls, cls_scope] : type_members::SuperClsReads(given_sym->LinkedScope->GetSupScopes())) {
    if (cls.Id == nullptr) { continue; }
    auto sup_inferred = scopes::GenericSubst();
    if (scopes::UnifyTypeIds(scopes::BareOf(cls.Id), declared, *cls_scope, *_OwnerScope, sup_inferred, true, true)) {
      return sup_inferred;
    }
  }
  return inferred;
}

auto GenericSolver::_SeedFromOwner() -> void {
  // A parameter the declaration's own scope binds already (an instantiation's, or a block whose parameters stand for
  // it) is read there as what it is bound to, so no declared type names it, and a match never binds it: what it is
  // bound to is its value, unless something was given for it.
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr or entry->Id == 0 or entry->IsBound()) { continue; }
    if (entry->IsTypeParam()) {
      const auto own = scopes::ParamTypeId(entry->Id);
      auto const *const sym = _OwnerScope->FindBoundTypeSymbolById(own);
      const auto bound = sym != nullptr ? _OwnerScope->TypeIdOfSymbol(*sym) : nullptr;
      if (bound != nullptr and bound != own) {
        entry->TypeVals.emplace_back(_TypeVal{.Id = bound, .Written = nullptr, .Site = nullptr});
      }
      continue;
    }
    const auto own = scopes::ParamCompId(entry->Id);
    auto const *const sym = _OwnerScope->FindBoundVarSymbolById(own);
    const auto bound = sym != nullptr ? _OwnerScope->CompIdOfSymbol(*sym) : nullptr;
    if (bound != nullptr and bound != own) {
      entry->CompVals.emplace_back(_CompValOf(*entry, bound, nullptr));
    }
  }
}

auto GenericSolver::_ReadCompValues() -> bool {
  // Comp-to-type inference: a comp argument's value has a type, which binds the type parameters its parameter's
  // declared type names ("A[n=1_uz]" for "[T, cmp n: T]" binds "T" to "USize"). Only fills what nothing else has
  // bound, so a comp value of the wrong type is still reported against its parameter ("_CheckCompArgs"), not as a
  // conflict. An expression is read through its folded value, as its operator call is only analysed on a copy.
  // A literal's type is its suffix, so it can be read as soon as types resolve (a signature or attribute type is
  // written in stage 4). Anything else is only safe to ask once the sup scopes are attached, as "_CheckCompArgs"
  // does: resolving a name earlier caches its type without them.
  auto &sm = *_Sm;
  auto &meta = *_Meta;
  if (meta.CurrentStage < CompilerStage::kResolveDeclarations) { return false; }
  const auto sups_attached = meta.CurrentStage >= CompilerStage::kAttachSupScopes;
  auto newly_bound = false;
  for (auto const &entry : _Entries) {
    if (not entry->IsCompParam() or entry->CompVals.empty() or entry->IsCompValueRead) { continue; }
    auto const &val = entry->CompVals[0];
    if (not val.IsPack and val.Val->To<TypeAst>() != nullptr) { continue; }

    // Only a parameter whose declared type names a type parameter has anything to infer from its value ("cmp n: T");
    // one of a closed type ("cmp n: USize") is not read, so a value that cannot be typed yet ("Self::n", before the
    // "sup" blocks are attached) is not asked to be.
    if (const auto declared = _OwnerScope->TypeIdOf(*entry->Param->CompType);
      declared != nullptr and scopes::ParamsNamedBy(declared).TypeParams.empty() and not declared->HasSelf) {
      entry->IsCompValueRead = true;
      continue;
    }

    // A pack's value is its list, each element of the declared type ("PackSources", "CompElementType").
    const auto sources = val.IsPack ? PackSources(val.Elems, *sm.CurrentScope) : Vec<ExpressionAst*>{val.Val.get()};

    entry->IsCompValueRead = true;
    for (auto *source : sources) {
      if (not sups_attached and source->To<LiteralAst>() == nullptr) { continue; }
      const auto is_operator = comp_generics::IsCompOperator(*source);
      const auto folded = is_operator ? sm.CurrentScope->FoldedCompAstOf(*source) : nullptr;
      if (is_operator and folded == nullptr) { continue; }
      // A copy: a literal's type is its symbol's cached "FqName" node, shared by every use of the type, and the
      // binding is analysed where it is used - early, it would cache a resolution made before the sups exist on all
      // of them.
      const auto source_type = AstCloneShared(
        folded != nullptr ? folded->InferType(&sm, &meta) : CompElementType(*source, sm, meta));
      const auto site = val.Site != nullptr
        ? source_type->WithSourceSpanAt(*val.Site)
        : source_type->WithSourceSpanAt(*source);
      newly_bound = _OfferAll(
        _Match(*source_type, *entry->Param->CompType), site,
        [](auto const &e) { return not e.TypeVals.empty(); }) or newly_bound;
    }
  }
  return newly_bound;
}

auto GenericSolver::_ReadConstraints() -> bool {
  // Constraint-based inference: "[U, F: FunRef[(), U]]" infers "U" from the return type of whichever of the type
  // bound to "F" and its super classes is the "FunRef".
  using errors::SppGenericConstraintError;
  auto &sm = *_Sm;
  auto const &scope = *sm.CurrentScope;
  auto newly_bound = false;
  const auto type_params = _Params->GetTypeParams();
  for (auto const &entry : _Entries) {
    if (not entry->IsTypeParam() or entry->Param->TypeConstraints->Constraints.empty()) { continue; }
    if (entry->TypeVals.empty() or entry->IsConstraintsRead) { continue; }

    // A pack's constraints are its elements', checked one by one ("CheckTypeArgConstraints"): its value is their
    // tuple, which no constraint of theirs is matched against.
    if (entry->Param->IsVariadic()) { continue; }
    entry->IsConstraintsRead = true;
    const auto inferred_id = entry->TypeVals[0].Id;
    if (inferred_id == nullptr) { continue; }

    // The candidates are the type and its super classes, each with the scope its name resolves in: a sup type's
    // name can hold a "Self" (as in "S32 ext Ord[Rhs=Self]"), which only has a symbol inside that sup scope.
    auto const *const concrete_sym = TypeRef::Of(inferred_id, scope).Symbol;
    auto candidates = Vec<Pair<scopes::TypeId, Scope const*>>{};
    if (concrete_sym != nullptr and not concrete_sym->IsGn()) {
      candidates.emplace_back(scope.TypeIdOfSymbol(*concrete_sym), &scope);
      if (concrete_sym->LinkedScope != nullptr) {
        for (auto const &[cls, cls_scope] : type_members::SuperClsReads(concrete_sym->LinkedScope->GetSupScopes())) {
          candidates.emplace_back(cls.Id, cls_scope);
        }
      }
    }

    for (auto const &constraint : entry->Param->TypeConstraints->Constraints) {
      // A constraint that does not key binds nothing and matches nothing; it is reported by "_CheckTypeArgs".
      const auto declared = _OwnerScope->TypeIdOf(*constraint);
      if (declared == nullptr) { continue; }
      auto inferred = scopes::GenericSubst();
      auto matched = false;
      for (auto const &[candidate, candidate_scope] : candidates) {
        if (candidate == nullptr) { continue; }
        inferred = scopes::GenericSubst();
        if (scopes::UnifyTypeIds(
          scopes::BareOf(candidate), scopes::BareOf(declared), *candidate_scope, *_OwnerScope, inferred, true, false)) {
          matched = true;
          break;
        }
      }

      // A constraint that other parameters are inferred through ("U" in "P: FunMov[(T,), Opt[U]]") and that the
      // argument does not fit is reported as a constraint error here, rather than as the dependent parameter being
      // uninferred later, which would hide the cause. One naming no other generics ("P: Copy") is left to the
      // authoritative "_CheckTypeArgs".
      if (not candidates.empty() and not matched) {
        const auto constraint_drives_inference = genex::any_of(
          type_params, [&](auto const *other) {
            return type_predicates::DoesTypeNameAGnParam(*constraint, *other, *_OwnerScope);
          });
        if (constraint_drives_inference) {
          const auto inferred_type = _AstOf(entry->TypeVals[0]);
          Raise<SppGenericConstraintError>(ERR_ARGS(*constraint, *inferred_type));
        }
      }
      // What is inferred through the constraint points where the constrained type came from.
      auto const &val = entry->TypeVals[0];
      newly_bound = _OfferAll(inferred, val.Site != nullptr ? val.Site : val.Written, [](auto const &e) {
        return e.IsBound();
      }) or newly_bound;
    }
  }
  return newly_bound;
}

auto GenericSolver::_SelfForDefault(
  const bool as_value) const -> Shared<TypeAst> {
  // What "Self" stands for in a default, decided in one place for both kinds. What the use pinned "Self" to (a call's
  // receiver) comes first. Otherwise a value named through it ("Self::mo_seq_cst") is looked up in the type the owner
  // is written for, so needs that type; a type argument keeps "Self" as written - it is read where it is used, and an
  // instantiation keys a "Self" argument as parameter 0, open (keyed by meaning, "Vec[Self]" minted "View[T=Self]"
  // without end).
  if (_Self != nullptr and not _Self->TypeVals.empty()) { return _Self->TypeVals[0].Written; }
  if (not as_value) { return nullptr; }
  auto self_type = _OwnerScope->FindEnclosingSelfType(*_Meta);
  return self_type != nullptr and not self_type->IsSelfType() ? self_type : nullptr;
}

auto GenericSolver::_ApplyDefaults() -> void {
  // An optional parameter nothing has bound takes its default, and a pack nothing has bound is empty. A default is
  // written in the declaration's own terms: it is read in
  // the use site's, with everything bound, by "_CrossSubstitute". A type default is keyed where it is written ("Self"
  // as "_SelfForDefault" decides, in the use site's terms already); nothing is keyed before the aliases exist. A comp
  // default is read from its written form ("GenericParameterAst::WrittenCompDefault").
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr or entry->IsBound()) { continue; }
    if (entry->IsTypeParam() and entry->Param->IsOptional()) {
      auto def_type = AstCloneShared(entry->Param->TypeDefault);
      auto def_id = scopes::TypeId(nullptr);
      if (_Meta->CurrentStage >= CompilerStage::kGenTopLvlAliases) {
        if (not def_type->IsSelfType()) {
          def_id = _OwnerScope->TypeIdOf(*def_type);
          entry->IsTypeFromDefault = true;
        }
        else if (const auto self_type = _SelfForDefault(false); self_type != nullptr) {
          def_type = self_type->WithConvention(AstClone(def_type->GetConvention()));
          def_id = _Sm->CurrentScope->TypeIdOf(*def_type);
        }
        else { def_id = _OwnerScope->TypeIdOf(*def_type); }
      }
      auto site = def_id != nullptr ? def_type : nullptr;
      entry->TypeVals.emplace_back(_TypeVal{.Id = def_id, .Written = std::move(def_type), .Site = std::move(site)});
    }
    else if (entry->IsCompParam() and entry->Param->IsOptional()) {
      entry->CompVals.emplace_back(_CompVal{.Val = entry->Param->CompDefault, .Site = nullptr});
      entry->IsCompFromDefault = true;
    }

    // A pack nothing has bound is given nothing: its list is empty,
    // as a variadic function parameter given no arguments is
    // ("f[..Ts]()" called as "f()").
    else if (entry->IsTypeParam() and entry->Param->IsVariadic()) {
      entry->TypeVals.emplace_back(_TypeVal{.Id = scopes::PackIdOf({}), .Written = nullptr, .Site = nullptr});
    }
    else if (entry->IsCompParam() and entry->Param->IsVariadic()) {
      entry->CompVals.emplace_back(
        _CompVal{.IsPack = true, .Id = scopes::InternCompKey(scopes::CompKey::OfPack({}))});
    }
  }
}

auto GenericSolver::_EnforceNoConflicts() const -> void {
  // A parameter reached through several arguments, or through an argument and a constraint, has to be offered the
  // same thing by each: the same type, and the same value (the same "CompId", by identity rather than spelling:
  // "0x2_uz" and "2_uz" are one value). A type that does not key has nothing to compare.
  using errors::SppGenericParameterConflictError;
  for (auto const &entry : _Entries) {
    auto const &types = entry->TypeVals;
    for (auto i = 1uz; i < types.size(); ++i) {
      if (types[0].Id == nullptr or types[i].Id == nullptr or types[0].Id == types[i].Id) { continue; }
      const auto first = _AstOf(types[0]);
      const auto other = _AstOf(types[i]);
      Raise<SppGenericParameterConflictError>(ERR_ARGS(*entry->Name, *first, *other));
    }
    // A comp value read back from its identity is shown where it was read off.
    auto const &comps = entry->CompVals;
    for (auto i = 1uz; i < comps.size(); ++i) {
      if (_ValCompId(comps[i]) == _ValCompId(comps[0])) { continue; }
      const auto first = _ShownCompVal(comps[0]);
      const auto other = _ShownCompVal(comps[i]);
      auto const &first_at = comps[0].Site != nullptr ? static_cast<Ast const&>(*comps[0].Site) : *first;
      auto const &other_at = comps[i].Site != nullptr ? static_cast<Ast const&>(*comps[i].Site) : *other;
      Raise<SppGenericParameterConflictError>(ERR_ARGS(*entry->Name, *first, *other, first_at, other_at));
    }
  }
}

auto GenericSolver::_EnforceAllInferred(
  Ast const &owner) const -> void {
  // Every parameter must be bound to something of its own kind, type parameters reported first.
  using errors::SppGenericParameterNotInferredError;
  for (const auto want_type : {true, false}) {
    for (auto const &entry : _Entries) {
      if (entry->Param == nullptr or entry->IsTypeParam() != want_type) { continue; }
      const auto bound = want_type ? not entry->TypeVals.empty() : not entry->CompVals.empty();
      RaiseIf<SppGenericParameterNotInferredError>(not bound, ERR_ARGS(*entry->Name, owner));
    }
  }
}

auto GenericSolver::_CrossSubstitute() -> void {
  // Read each default, written in the declaration's own terms, in the use site's, with everything else bound (each
  // skipping itself): a type default by identity, so "Vec[T, A=Alloc[T]]" receives the actual "T"; a comp default
  // from its written form, so "m = n + 1_uz" receives the "n" bound with it and "Self::mo_seq_cst" the "Self" the use
  // pinned. What was inferred or given is in the use site's terms already. A literal default names nothing, and
  // nothing is read before the aliases exist.
  //
  // A comp default read is its value when it folds ("n + 1_uz" with "n" bound to "3_uz" is "4_uz"). One still naming a
  // generic is analysed: an operator caches the symbol its left-hand side resolved to, and code generation reads that
  // back, so an unanalysed one reaches stage 11 looking like a namespace access.
  auto &sm = *_Sm;
  auto self_type = Shared<TypeAst>(nullptr);
  auto self_looked_up = false;
  for (auto const &entry : _Entries) {
    const auto rewrites_type = entry->IsTypeFromDefault and entry->TypeVals[0].Id != nullptr;
    const auto rewrites_comp = entry->IsCompFromDefault
      and _Meta->CurrentStage >= CompilerStage::kGenTopLvlAliases
      and entry->Param->WrittenCompDefault->To<LiteralAst>() == nullptr;
    if (not rewrites_type and not rewrites_comp) { continue; }
    auto bindings = _Bindings(entry.get());
    if (rewrites_type) {
      auto &val = entry->TypeVals[0];
      if (not bindings.IsEmpty()) { val.Id = scopes::SubstituteTypeId(val.Id, bindings); }
      continue;
    }

    // "Self" is looked up once, and only for a comp default: it is a symbol lookup, and most uses have none.
    if (not self_looked_up) {
      self_looked_up = true;
      self_type = _SelfForDefault(true);
    }
    if (self_type != nullptr) { scopes::BindSelf(bindings, *self_type, *sm.CurrentScope); }
    auto comp = type_resolution::ReadCompDefault(*entry->Param, std::move(bindings), *_OwnerScope, *sm.CurrentScope);
    if (auto folded = sm.CurrentScope->FoldedCompAstOf(*comp); folded != nullptr) {
      comp = std::move(folded);
    }
    else {
      comp->Stage7_AnalyseSemantics(_Sm, _Meta);
      if (auto folded = comp_generics::FoldCompCall(*comp, _Sm, _Meta); folded != nullptr) { comp = std::move(folded); }
    }
    entry->CompVals.clear();
    entry->CompVals.emplace_back(_CompVal{.Val = std::move(comp), .Site = nullptr});
  }
}

auto GenericSolver::_CheckTypeArgs(scopes::GenericSubst const &bindings) const -> void {
  // Check each type argument against its parameter's constraints, read with every binding substituted in. A pack's
  // constraints hold for each of its elements: each shown as given, else read back from its identity, pointed at what
  // it was inferred from.
  auto args = Vec<Pair<GenericParameterAst const*, Vec<Pair<scopes::TypeId, Shared<TypeAst>>>>>();
  for (auto const &entry : _Entries) {
    if (not entry->IsTypeParam() or entry->TypeVals.empty() or entry->TypeVals[0].Id == nullptr) { continue; }
    if (entry->Param->TypeConstraints->Constraints.empty()) { continue; }
    auto const &val = entry->TypeVals[0];
    auto targets = Vec<Pair<scopes::TypeId, Shared<TypeAst>>>();
    if (not scopes::IsPackTypeId(val.Id)) { targets.emplace_back(val.Id, _AstOf(val)); }
    else {
      auto const &elems = scopes::HeaderOf(val.Id).PackElements;
      for (auto i = 0uz; i < elems.size(); ++i) {
        auto shown = i < val.Elems.size() ? val.Elems[i] : AstCloneShared(_Sm->CurrentScope->TypeAstOf(elems[i]));
        if (val.Site != nullptr and i >= val.Elems.size()) { shown = shown->WithSourceSpanAt(*val.Site); }
        targets.emplace_back(elems[i], std::move(shown));
      }
    }
    args.emplace_back(entry->Param, std::move(targets));
  }
  CheckTypeArgConstraints(args, *_OwnerScope, bindings, *_Sm, *_Meta);
}

auto GenericSolver::_CheckCompArgs(scopes::GenericSubst const &bindings) const -> void {
  // Type-check each comp argument against its parameter's type, with every binding substituted in. The value was
  // given at the use site, so its type is resolved there - and so is the parameter's type, which the substitution has
  // written in the use site's terms (the arguments given there).
  using errors::SppTypeMismatchError;
  using type_compare::TypeEq;
  auto &sm = *_Sm;
  auto &meta = *_Meta;
  for (auto const &entry : _Entries) {
    if (not entry->IsCompParam() or entry->CompVals.empty()) { continue; }
    auto const &val = entry->CompVals[0];
    // A comp expression is typed once the sweep has attached every "sup" scope, as it is analysed then
    // ("GenericArgumentAst::AnalyseCompVal"); a type made during the sweep is checked where it is written.
    const auto needs_sups = val.IsPack
      ? genex::any_of(val.Elems, [](auto const &e) { return comp_generics::NeedsSupScopesToType(*e); })
      : comp_generics::NeedsSupScopesToType(*val.Val);
    if (meta.CurrentStage < CompilerStage::kPreAnalyseSemantics and needs_sups) { continue; }
    const auto p_type = type_resolution::ReadType(
      *entry->Param->CompType, ExprSubst::Across(*_OwnerScope, bindings, *sm.CurrentScope));

    // A pack's value is its list, each element of the declared type ("PackSources", "CompElementType").
    if (val.IsPack) {
      const auto p_ref = TypeRef::Of(*p_type, *sm.CurrentScope);
      for (auto *const elem : PackSources(val.Elems, *sm.CurrentScope)) {
        const auto a_type = CompElementType(*elem, sm, meta);
        RaiseIf<SppTypeMismatchError>(
          not type_compare::Assignable(p_ref, TypeRef::Of(*a_type, *sm.CurrentScope), *sm.CurrentScope, *sm.CurrentScope),
          ERR_ARGS(*entry->Param, *p_type, *elem, *a_type));
      }
      continue;
    }
    const auto raw_a_type = val.Val->InferType(&sm, &meta);
    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(
        TypeRef::Of(*p_type, *sm.CurrentScope), TypeRef::Of(*raw_a_type, *sm.CurrentScope), *sm.CurrentScope,
        *sm.CurrentScope),
      ERR_ARGS(*entry->Param, *p_type, *val.Val, *raw_a_type));
  }
}

auto GenericSolver::Solve(
  Ast const &owner) -> void {
  // A declaration with no parameters of its own has nothing to solve: what was given is the answer, whatever it names.
  if (_Params->Params.empty()) {
    _Trivial = true;
    return;
  }

  // What the declaration's own scope binds already, then the equations. What was given as a type is not offered
  // anything else.
  _SeedFromOwner();
  for (auto const &eq : _Equations) {
    const auto matched = eq->GivenId != nullptr ? _MatchIds(eq->GivenId, eq->DeclaredId) : _Match(*eq->Source, *eq->Target);
    _OfferAll(matched, eq->Source, [](auto const &e) { return e.IsTypeGiven; });
  }

  // Comp values' types and the constraints feed each other, so both run until neither binds anything new.
  while (true) {
    const auto from_comps = _ReadCompValues();
    const auto from_constraints = _ReadConstraints();
    if (not from_comps and not from_constraints) { break; }
  }

  _ApplyDefaults();
  _EnforceNoConflicts();
  _EnforceAllInferred(owner);
  _CrossSubstitute();

  // A comp value is typed once the "sup" scopes are attached; a type's constraints, which name super classes, from the
  // pre-analysis stage, unless a substituted type is being re-read ("SkipSubstitutedConstraintChecks").
  // Both read the solution through one binding of it.
  const auto check_comps = _Meta->CurrentStage >= CompilerStage::kAttachSupScopes;
  const auto check_types = _Meta->CurrentStage >= CompilerStage::kPreAnalyseSemantics
    and not _Meta->SkipSubstitutedConstraintChecks;
  if (not check_comps and not check_types) { return; }
  const auto bindings = _Bindings();
  if (check_comps) { _CheckCompArgs(bindings); }
  if (check_types) { _CheckTypeArgs(bindings); }
}

auto GenericSolver::InferredParams() const -> Vec<std::uint64_t> {
  auto out = Vec<std::uint64_t>();
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr or not entry->IsBound()) { continue; }
    const auto inferred_type = not entry->TypeVals.empty() and not entry->IsTypeGiven and not entry->IsTypeFromDefault;
    const auto inferred_comp = not entry->CompVals.empty() and not entry->IsCompGiven and not entry->IsCompFromDefault;
    if (inferred_type or inferred_comp) { out.emplace_back(_EntryParamId(*entry)); }
  }
  return out;
}

auto GenericSolver::SolvedArgsId() const -> scopes::TypeId {
  // As "Scope::ArgsIdOf" keys a keyword argument: the parameter its name stamps, else the entry's, "Self" as 0; then
  // the value's identity, a comp value keyed where the call is.
  using scopes::TypeKey;

  // Nothing was solved, so the given arguments are the answer as given, keyed as written.
  if (_Trivial) {
    const auto given = _Given | genex::views::transform([](auto const &arg) { return arg.get(); }) | genex::to<Vec>();
    return _Sm->CurrentScope->ArgsIdOf(given, _Params);
  }
  auto key = TypeKey();
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr and not entry->Emit) { continue; }
    if (not entry->IsBound()) { continue; }
    const auto id = entry.get() == _Self ? 0 : _EntryParamId(*entry);
    if (id != 0 or entry.get() == _Self) { key.Push(TypeKey::Tag::Arg, id); }
    else {
      Ice(*entry->Name, *_Sm->CurrentScope, "solved generic argument names no parameter");
    }

    if (not entry->TypeVals.empty()) {
      const auto type_id = _EntryTypeId(*entry);
      if (type_id == nullptr) { return nullptr; }
      key.PushTypePart(type_id);
    }
    else {
      const auto comp = _EntryCompId(*entry);
      if (comp == nullptr) { return nullptr; }
      key.PushCompPart(comp);
    }
  }
  return scopes::InternTypeKey(std::move(key));
}

auto GenericSolver::TakeArgs() -> Vec<Unique<GenericArgumentAst>> {
  // With nothing solved, the given arguments are the answer as they were given.
  if (_Trivial) { return std::move(_Given); }

  // The parameters in declaration order, which is what an instantiation's name is built from, then the names given
  // alongside them that are part of the solution. A type not given is read back from its identity here, recording what
  // it means ("Scope::TypeAstOf"), so it is not analysed again.
  auto out = Vec<Unique<GenericArgumentAst>>();
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr and not entry->Emit) { continue; }

    // A type pack is its elements, positionally: as given, else read back from its list.
    if (not entry->TypeVals.empty() and scopes::IsPackTypeId(entry->TypeVals[0].Id)) {
      auto const &val = entry->TypeVals[0];
      if (not val.Elems.empty()) {
        for (auto const &elem : val.Elems) { out.emplace_back(GenericArgumentAst::NewType(nullptr, elem)); }
        continue;
      }
      for (const auto elem : scopes::HeaderOf(val.Id).PackElements) {
        out.emplace_back(GenericArgumentAst::NewType(nullptr, _Sm->CurrentScope->TypeAstOf(elem)));
      }
      continue;
    }

    if (not entry->TypeVals.empty()) {
      if (entry->IsTypeGiven and entry->TypeVals[0].Written != nullptr) {
        out.emplace_back(GenericArgumentAst::NewType(entry->Name, entry->TypeVals[0].Written));
        continue;
      }
      out.emplace_back(GenericArgumentAst::NewType(entry->Name, _AstOf(entry->TypeVals[0])));
    }
    else if (not entry->CompVals.empty()) {
      // A comp value is cloned where a type is shared: it is analysed in place where the argument is read, and may be
      // the parameter's own default, or the caller's argument. A comp pack is its elements, positionally.
      if (auto const &val = entry->CompVals[0]; val.IsPack) {
        for (auto const &elem : val.Elems) { out.emplace_back(GenericArgumentAst::NewComp(nullptr, AstCloneShared(elem))); }
        continue;
      }
      out.emplace_back(GenericArgumentAst::NewComp(entry->Name, AstCloneShared(entry->CompVals[0].Val)));
    }
  }
  return out;
}

SPP_MOD_END

/// [CHECKED]
auto spp::analyse::utils::generic_inference::NamedGnArgs(
  GenericArgumentGroupAst const &written, GenericParameterGroupAst const &p_group, Scope const *params_scope,
  Ast const &owner, ScopeManager &sm, CompilerMetaData &meta) -> Unique<GenericArgumentGroupAst> {
  // The copy's comp arguments are given their parameters' types
  // before they are named, which analyses the positional ones.
  auto named = AstClone(&written);
  if (params_scope != nullptr) { comp_generics::ExpectCompParamTypes(*named, p_group, *params_scope); }

  // The two kinds are named separately, as a type and a comp
  // parameter cannot share a name, then put in parameter order.
  auto comp_args = Vec<Unique<GenericArgumentAst>>();
  auto type_args = Vec<Unique<GenericArgumentAst>>();
  for (auto &&arg : std::move(named->Args)) {
    (arg->IsCompArg() ? comp_args : type_args).emplace_back(std::move(arg));
  }
  NameCompArgs(comp_args, p_group.GetCompParams(), owner, sm, meta);
  NameTypeArgs(type_args, p_group.GetTypeParams(), owner, sm, meta);
  named->Args = std::move(comp_args);
  named->Args.append_range(genex::views::move(type_args));

  // Into parameter order, by the parameter each argument's name
  // records ("NameArgs" stamped every one), a type pack's elements
  // after them, positional and in order.
  auto elems = Vec<Unique<GenericArgumentAst>>();
  for (auto &arg : named->Args) {
    if (arg->KeywordName() == nullptr) { elems.emplace_back(std::move(arg)); }
  }
  std::erase_if(named->Args, [](auto const &arg) { return arg == nullptr; });
  auto param_index = Map<std::uint64_t, std::size_t>();
  for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) { param_index[p->ParamId()] = i; }
  for (auto const &arg : named->Args) {
    const auto stamped = arg->KeywordName()->StampedTypeId();
    if (stamped == nullptr or not param_index.contains(scopes::HeaderOf(stamped).TypeParamId)) {
      const auto err = Str("generic argument is not named by its parameter's identity");
      Ice(*arg, *sm.CurrentScope, err);
    }
  }
  const auto index_of = [&](GenericArgumentAst const &arg) {
    return param_index.at(scopes::HeaderOf(arg.KeywordName()->StampedTypeId()).TypeParamId);
  };
  named->Args |= genex::actions::sort([&](auto const &a, auto const &b) { return index_of(*a) < index_of(*b); });
  named->Args.append_range(genex::views::move(elems));
  return named;
}
