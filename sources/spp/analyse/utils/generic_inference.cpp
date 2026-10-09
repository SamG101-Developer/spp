module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.generic_inference;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.packs;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
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
import spp.utils.interner;
import genex;

namespace spp::analyse::utils::generic_inference {
  namespace {
    /// Reject the first argument name no parameter has, reported
    /// against the first parameter (or the argument itself, when
    /// there are none). The parameter is shown from "params_scope",
    /// where it is written - another file than the use site
    /// ("scope") for any template not declared beside its use.
    auto EnforceGnArgNamesKnown(
      Vec<GenericParameterAst*> const &params, Vec<GenericArgumentAst*> const &args,
      Scope const &params_scope, Scope const *scope) -> void {
      for (auto const *arg : args) {
        const auto known = genex::any_of(params, [arg](auto const *p) { return *p->Name == *arg->KeywordName(); });
        if (known) { continue; }
        Raise<errors::SppArgumentNameInvalidError>(
          {params.IsEmpty() ? scope : &params_scope, scope}, ERR_ARGS(
            params.IsEmpty() ? static_cast<Ast const&>(*arg->KeywordName()) : *params[0], StrView("gn param"),
            *arg->KeywordName(), StrView("gn arg")));
      }
    }

    /// Check each type argument ("args": its parameter, the
    /// identity bound to it, and the type shown for it in an
    /// error) against its parameter's constraints, read where
    /// the parameters are written ("written_scope", which an
    /// unsatisfied one is reported from) with the arguments
    /// bound ("bindings"), from the use site. A pack's constraints
    /// hold for each of its elements.
    auto CheckTypeArgConstraints(
      Vec<Tup<GenericParameterAst const*, TypeId, Shared<TypeAst>>> const &args,
      Scope const &written_scope, GenericSubst const &bindings,
      ScopeManager &sm, meta::CompilerMetaData &meta) -> void {
      using errors::SppGenericConstraintError;
      auto const &scope = *sm.CurrentScope;
      for (auto const &[param, arg, shown] : args) {
        auto const *const arg_sym = TypeRef::Of(arg, scope).Symbol;
        if (arg_sym == nullptr) { continue; }
        auto con_sm = ScopeManager(
          sm.GlobalScope, arg_sym->LinkedScope != nullptr ? arg_sym->LinkedScope : sm.CurrentScope);

        // This parameter's constraints, with the arguments bound.
        auto p_cons = Vec<Shared<TypeAst>>();
        for (auto const &p_con : param->TypeConstraints->Constraints) {
          const auto sub = type_resolution::ReadType(*p_con, ExprSubst::Across(written_scope, bindings, scope));
          {
            // Resolved from the argument's scope, but written, and
            // checked for visibility, where the parameter is declared:
            // a module-private alias there is not visible from, say,
            // a closure argument's (global) scope, and needn't be.
            const auto _meta_guard = meta::MetaGuard(&meta);
            meta.AllowAbstractType = true;
            meta.IgnoreAccessModifierViolations = true;
            sub->Stage7_AnalyseSemantics(&con_sm, &meta);
          }
          p_cons.push_back(sub->WithSourceSpanOf(*p_con));
        }

        // A pack's constraints hold for each of its elements, not for
        // the tuple it is bound to: each shown as given,
        // when the tuple was.
        auto targets = Vec<Pair<scopes::TypeId, Shared<TypeAst>>>();
        if (not param->IsVariadic()) { targets.EmplaceBack(arg, shown); }
        else if (auto const &head = scopes::HeadOf(arg); head.Args != nullptr) {
          const auto shown_elems = packs::TypePackElements(*shown);
          const auto elems = scopes::ArgsOf(head.Args);
          for (auto i = 0uz; i < elems.size(); ++i) {
            if (elems[i].TypeVal == nullptr) { continue; }
            targets.EmplaceBack(
              elems[i].TypeVal, i < shown_elems.Len() ? shown_elems[i] : scope.TypeAstOf(elems[i].TypeVal));
          }
        }

        // Raise an error if any constraint of this argument is
        // not satisfied.
        for (auto const &[target, target_ast] : targets) {
          const auto unsatisfied = type_compare::UnmetConstraint(
            p_cons, TypeRef::Of(target, scope), scopes::IsSelfTypeId(target), scope, scope);
          RaiseIf<SppGenericConstraintError>(
            unsatisfied != nullptr, {&written_scope, sm.CurrentScope},
            ERR_ARGS(*unsatisfied, *target_ast));
        }
      }
    }

    /// Name the positional arguments of one kind after the
    /// parameters they bind ("type_resolution::ParamsBoundByArgs";
    /// "NameTypeArgs", "NameCompArgs"): each is copied under
    /// its parameter's name by "bind", until a trailing variadic
    /// parameter takes the rest as a tuple, built by "pack".
    /// "f[U32, U32]" for "f[..Ts]" is "f[Ts=(U32, U32)]", and
    /// "f[1_u32, 1_u32]" for "f[cmp ..s]" is "f[s=(1_u32, 1_u32)]".
    /// A lone argument naming a pack ("A[Ts]" inside "g[..Ts]")
    /// already is that tuple, so is forwarded as it is ("forwards").
    auto NameArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Scope const &params_scope,
      Ast const &owner, ScopeManager &sm,
      meta::CompilerMetaData &meta,
      Function<void(GenericArgumentAst &, Vec<GenericArgumentAst*> const &, bool)> const &pack,
      Function<void(GenericArgumentAst &, GenericArgumentAst const &)> const &bind) -> void {
      const auto keyword_args = args
        | genex::views::ptr
        | genex::views::filter([](auto const *x) { return x->KeywordName() != nullptr; })
        | genex::to<Vec>();
      EnforceGnArgNamesKnown(params, keyword_args, params_scope, sm.CurrentScope);

      const auto targets = type_resolution::ParamsBoundByArgs(args | genex::views::ptr | genex::to<Vec>(), params);

      const auto _meta_guard = meta::MetaGuard(&meta);
      meta.TypeAnalysisTypeScope = nullptr;

      for (auto i = 0uz; i < args.Len(); ++i) {
        auto const &positional = args[i];
        if (positional->KeywordName() != nullptr) { continue; }
        auto const *const param = targets[i];
        RaiseIf<errors::SppGenericArgumentTooManyError>(
          param == nullptr, {params.IsEmpty() ? sm.CurrentScope : &params_scope, sm.CurrentScope, sm.CurrentScope},
          ERR_ARGS(params.IsEmpty() ? owner : *params[0], owner, *positional));
        auto named = MakeUnique<GenericArgumentAst>(param->Name, nullptr, nullptr, nullptr);

        if (param->IsVariadic()) {
          const auto forwards = i + 1 == args.Len() and packs::DoesArgNameAPack(*positional, *sm.CurrentScope);
          pack(*named, args | genex::views::ptr | genex::views::drop(i) | genex::to<Vec>(), forwards);
          args[i] = std::move(named);
          args |= genex::actions::take(i + 1);
          break;
        }
        bind(*named, *positional);
        args[i] = std::move(named);
      }
    }

    /// "NameArgs" for type arguments. Collected into the pack's
    /// tuple, an argument naming a bound type pack is spread
    /// into that pack's elements ("A[S32, Ts]" with "Ts" bound
    /// to "Tup[Bool, U8]" is "A[S32, Bool, U8]"). Each is
    /// analysed once types resolve.
    auto NameTypeArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params,
      Scope const &params_scope, Ast const &owner, ScopeManager &sm, meta::CompilerMetaData &meta)
      -> void {
      const auto analyse = [&](GenericArgumentAst &named) {
        if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
          named.TypeVal->Stage7_AnalyseSemantics(&sm, &meta);
        }
      };
      const auto pack = [&](GenericArgumentAst &named, Vec<GenericArgumentAst*> const &rest, const bool forwards) {
        if (forwards) { named.TypeVal = AstCloneShared(rest[0]->TypeVal); }
        else {
          auto types = Vec<Shared<TypeAst>>();
          for (auto const *x : rest) {
            if (const auto spread = packs::BoundTypePackElements(*x->TypeVal, *sm.CurrentScope); spread.has_value()) {
              types.AppendRange(*spread);
            }
            else { types.EmplaceBack(AstCloneShared(x->TypeVal)); }
          }
          named.TypeVal = generate::common_types::TupleType(rest[0]->PosStart(), std::move(types));
        }
        analyse(named);
      };
      const auto bind = [&](GenericArgumentAst &named, GenericArgumentAst const &positional) {
        named.TypeVal = AstCloneShared(positional.TypeVal);
        analyse(named);
      };
      NameArgs(args, params, params_scope, owner, sm, meta, pack, bind);
    }

    /// "NameTypeArgs" for comp arguments: a bound comp pack is spread into its elements. A literal or a name is analysed
    /// once types resolve; anything else waits until stage 5 ends, as a comp expression ("n + 1") resolves its
    /// operator through the sup scopes of its operand's type, attached only then. An operator expression is never
    /// analysed here - analysing it consumes it - but by the argument's own "AnalyseCompVal", which folds it or checks
    /// a copy.
    auto NameCompArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params,
      Scope const &params_scope, Ast const &owner, ScopeManager &sm, meta::CompilerMetaData &meta)
      -> void {
      const auto pack = [&](GenericArgumentAst &named, Vec<GenericArgumentAst*> const &rest, const bool forwards) {
        if (forwards) {
          named.CompVal = AstCloneShared(rest[0]->CompVal);
          return;
        }
        auto values = Vec<Unique<ExpressionAst>>();
        for (auto const *x : rest) {
          if (const auto spread = packs::BoundCompPackElements(*x->CompVal, *sm.CurrentScope); spread.has_value()) {
            for (auto const *elem : *spread) { values.EmplaceBack(AstClone(elem)); }
          }
          else { values.EmplaceBack(AstClone(x->CompVal)); }
        }
        named.CompVal = MakeShared<TupleLiteralAst>(nullptr, std::move(values), nullptr);
      };
      const auto bind = [&](GenericArgumentAst &named, GenericArgumentAst const &positional) {
        named.CompVal = AstCloneShared(positional.CompVal);
        auto &val = *named.CompVal;
        const auto stage = meta.CurrentStage;
        if (stage >= meta::CompilerStage::kResolveDeclarations and (not comp_generics::NeedsSupScopesToType(val)
          or (stage >= meta::CompilerStage::kPreAnalyseSemantics and not comp_generics::IsCompOperator(val)))) {
          val.Stage7_AnalyseSemantics(&sm, &meta);
        }
      };
      NameArgs(args, params, params_scope, owner, sm, meta, pack, bind);
    }
  }
}

SPP_MOD_BEGIN
/// A type a parameter is offered. Its identity is what the solver compares, substitutes and checks; it is null only
/// for a type given that does not key (yet), which is passed on as written. "Written" is the type as given, or the
/// default as declared; "Site" what an inferred type, or a default, is read back pointing at. One "Written" with no
/// "Site" is shown as given.
struct spp::analyse::utils::generic_inference::GenericSolver::_TypeVal {
  TypeId Id = nullptr;
  Shared<TypeAst> Written;
  Shared<TypeAst> Site;
};

/// Everything one generic parameter (or a name given alongside the parameters, like a pinned "Self") is offered.
struct spp::analyse::utils::generic_inference::GenericSolver::_Entry {
  /// The parameter, or @c nullptr for a given name that is not one.
  GenericParameterAst const *Param;

  /// The parameter's identity ("ParamId"), which is what a name is found by; 0 for a name that is no parameter.
  std::uint64_t Id = 0;

  /// The parameter's name, for a name that carries no identity (a keyword argument as written, "Self").
  Shared<TypeIdentifierAst> Name;
  Vec<_TypeVal> TypeVals;
  Vec<Shared<ExpressionAst>> CompVals;

  /// Given a type rather than inferred one, so inference neither offers it another nor rewrites it.
  bool IsTypeGiven = false;

  /// Bound to its default, which is written in the declaration's own terms and so may name the others.
  bool IsTypeFromDefault = false;
  bool IsCompFromDefault = false;

  /// Whether a name that is not a parameter is part of the solution.
  bool Emit = false;

  /// Whether the comp value's type, or the bound type's constraints, have been inferred from yet: each is read once,
  /// once there is something to read, which is what makes the solve a worklist rather than a fixed pass order.
  bool IsCompValueRead = false;
  bool IsConstraintsRead = false;

  SPP_ATTR_NODISCARD auto IsBound() const -> bool { return not TypeVals.IsEmpty() or not CompVals.IsEmpty(); }
  SPP_ATTR_NODISCARD auto IsTypeParam() const -> bool { return Param != nullptr and Param->IsTypeParam(); }
  SPP_ATTR_NODISCARD auto IsCompParam() const -> bool { return Param != nullptr and Param->IsCompParam(); }
};

/// What was given (the source) where something is declared (the target), under the name of that declaration.
struct spp::analyse::utils::generic_inference::GenericSolver::_Equation {
  Shared<IdentifierAst> Name;
  Shared<TypeAst> Source;
  Shared<TypeAst> Target;
};

GenericSolver::GenericSolver(
  GenericParameterGroupAst const &params, Scope const &owner_scope, ScopeManager &sm, meta::CompilerMetaData &meta) :
  _Params(&params), _OwnerScope(&owner_scope), _Sm(&sm), _Meta(&meta) {
  for (auto const *param : params.GetAllParams()) {
    auto entry = MakeUnique<_Entry>();
    entry->Param = param;
    entry->Id = param->ParamId();
    entry->Name = dynamic_shared_cast<TypeIdentifierAst>(param->Name);
    _Entries.EmplaceBack(std::move(entry));
  }
}

GenericSolver::~GenericSolver() = default;

auto GenericSolver::_Find(
  TypeIdentifierAst const &name) const -> _Entry* {
  // By identity: a name stamped as one of this declaration's parameters ("TypeParam", or "TypeBound" through a
  // binding of it) is that parameter's entry. A name with no identity (a keyword argument as written, "Self"), or one
  // naming a parameter this declaration does not declare (a "sup" block's, copied into a method's group), is found by
  // its spelling.
  if (const auto id = name.StampedTypeId(); id != nullptr) {
    auto const &head = scopes::HeadOf(id);
    if (head.Kind == scopes::TypeKey::Tag::TypeParam or head.Kind == scopes::TypeKey::Tag::TypeBound) {
      for (auto const &entry : _Entries) {
        if (entry->Id != 0 and entry->Id == head.TypeParamId) { return entry.get(); }
      }
    }
  }
  for (auto const &entry : _Entries) { if (*entry->Name == name) { return entry.get(); } }
  return nullptr;
}

auto GenericSolver::_AstOf(
  _TypeVal const &val) const -> Shared<TypeAst> {
  // A type given, or one that does not key (a default's own copy), as it was written; anything else read back from its
  // identity, pointed at what it was read from (or the default it was declared as), on a copy, as the solution is
  // analysed in place.
  if (val.Id == nullptr or (val.Written != nullptr and val.Site == nullptr)) { return val.Written; }
  const auto type = _Sm->CurrentScope->TypeAstOf(val.Id);
  return val.Site != nullptr ? type->WithSourceSpanOf(*val.Site) : AstCloneShared(type);
}

auto GenericSolver::_Bindings(
  _Entry const *except) const -> scopes::GenericSubst {
  // Every parameter bound so far, by identity, but "except": what a type or value written in the declaration's terms
  // is read with. A comp value is keyed where it was given.
  auto out = scopes::GenericSubst();
  auto const &scope = *_Sm->CurrentScope;
  for (auto const &entry : _Entries) {
    if (entry.get() == except or entry->Param == nullptr or entry->Id == 0) { continue; }
    const auto is_pack = entry->Param->IsVariadic();
    if (entry->IsTypeParam() and not entry->TypeVals.IsEmpty() and entry->TypeVals[0].Id != nullptr) {
      out.TypeParams.emplace_back(entry->Id, entry->TypeVals[0].Id);
      if (is_pack) { out.TypePackParams.push_back(entry->Id); }
    }
    else if (entry->IsCompParam() and not entry->CompVals.IsEmpty()) {
      if (const auto id = scope.CompIdOf(*entry->CompVals[0]); id != nullptr) {
        out.CompParams.emplace_back(entry->Id, id);
        if (is_pack) { out.CompPackParams.push_back(entry->Id); }
      }
    }
  }
  return out;
}

auto GenericSolver::Give(
  Vec<Unique<GenericArgumentAst>> args, const bool emit) -> void {
  // Taken by value, so the caller's list is emptied rather than left holding moved-from arguments.
  using errors::SppInternalCompilerError;
  for (auto &arg : args) {
    if (arg->KeywordName() == nullptr or (not arg->IsTypeArg() and not arg->IsCompArg())) {
      const auto err = "generic argument '" + arg->ToString() + "' is still positional where a binding is expected";
      Raise<SppInternalCompilerError>({_Sm->CurrentScope}, ERR_ARGS(*arg, err));
    }

    // A layer given earlier outranks this one: the first binding offered for a name is the one it keeps.
    const auto name = dynamic_shared_cast<TypeIdentifierAst>(arg->KeywordName());
    auto *entry = _Find(*name);
    if (entry != nullptr and entry->IsBound()) { continue; }
    if (entry == nullptr) {
      auto extra = MakeUnique<_Entry>();
      extra->Param = nullptr;
      extra->Name = name;
      extra->Emit = emit;
      entry = extra.get();
      _Entries.EmplaceBack(std::move(extra));
    }
    if (arg->IsTypeArg()) {
      entry->TypeVals.EmplaceBack(
        _TypeVal{.Id = _Sm->CurrentScope->TypeIdOf(*arg->TypeVal), .Written = arg->TypeVal, .Site = nullptr});
      entry->IsTypeGiven = true;
    }
    else { entry->CompVals.EmplaceBack(arg->CompVal); }
    _Given.EmplaceBack(std::move(arg));
    _AnyGiven = true;
  }
}

auto GenericSolver::Give(
  GenericSubst const &bindings, Scope const &scope) -> void {
  // As "Give" does for arguments, each binding under its parameter's name (by identity where the name records one),
  // held as the identity it is; a layer given earlier outranks it.
  const auto take = [&](TypeIdentifierAst const &name) -> _Entry* {
    auto *entry = _Find(name);
    if (entry != nullptr and entry->IsBound()) { return nullptr; }
    if (entry == nullptr) {
      auto extra = MakeUnique<_Entry>();
      extra->Param = nullptr;
      extra->Name = static_shared_cast<TypeIdentifierAst>(AstCloneShared(&name));
      entry = extra.get();
      _Entries.EmplaceBack(std::move(extra));
    }
    _AnyGiven = true;
    return entry;
  };
  for (auto const &[pid, id] : bindings.TypeParams) {
    auto const *const param = pid != 0 ? scopes::FindGnTypeParamById(pid) : nullptr;
    if (param == nullptr or id == nullptr) { continue; }
    if (auto *const entry = take(*param->Name); entry != nullptr) {
      entry->TypeVals.EmplaceBack(_TypeVal{.Id = id, .Written = nullptr, .Site = nullptr});
      entry->IsTypeGiven = true;
    }
  }
  for (auto const &[pid, id] : bindings.CompParams) {
    auto const *const param = scopes::FindGnCompParamById(pid);
    if (param == nullptr or id == nullptr) { continue; }
    if (auto *const entry = take(*TypeIdentifierAst::FromIdentifier(*param->Name)); entry != nullptr) {
      entry->CompVals.EmplaceBack(scope.CompAstOf(id));
    }
  }
}

auto GenericSolver::Unify(
  Shared<IdentifierAst> const &name, Shared<TypeAst> source, Shared<TypeAst> target) -> void {
  auto eq = MakeUnique<_Equation>();
  eq->Name = name;
  eq->Source = std::move(source);
  eq->Target = std::move(target);
  _Equations.EmplaceBack(std::move(eq));
}

auto GenericSolver::ReadDeclaredWith(
  scopes::GenericSubst reading) -> void {
  _DeclaredReading = std::move(reading);
}

auto GenericSolver::KnownBindings(
  GenericParameterGroupAst const &params) const -> std::optional<GenericSubst> {
  // What was given, by the parameter each binds ("BindByName"'s rule over the given layers), packs marked; and "Self"
  // where it is pinned.
  if (not _AnyGiven) { return std::nullopt; }
  const auto declared = scopes::ParamsDeclaredBy(params);
  auto out = GenericSubst();
  for (auto const &entry : _Entries) {
    if (not entry->IsBound()) { continue; }
    const auto pid = _EntryParamId(*entry);
    if (pid == 0 and entry->Name->ToView() == "Self" and not entry->TypeVals.IsEmpty()) {
      if (const auto self_id = _EntryTypeId(*entry); self_id != nullptr) { out.TypeParams.emplace_back(0, self_id); }
      continue;
    }
    if (not entry->TypeVals.IsEmpty() and genex::contains(declared.TypeParams, pid)) {
      const auto id = _EntryTypeId(*entry);
      if (id == nullptr) { continue; }
      out.TypeParams.emplace_back(pid, id);
      if (auto const *const param = scopes::FindGnTypeParamById(pid); param != nullptr and param->IsVariadic) {
        out.TypePackParams.push_back(pid);
      }
    }
    else if (not entry->CompVals.IsEmpty() and genex::contains(declared.CompParams, pid)) {
      out.CompParams.emplace_back(pid, _Sm->CurrentScope->CompIdOf(*entry->CompVals[0]));
      if (auto const *const param = scopes::FindGnCompParamById(pid); param != nullptr and param->IsVariadic) {
        out.CompPackParams.push_back(pid);
      }
    }
  }
  return out;
}

auto GenericSolver::_EntryParamId(
  _Entry const &entry) const -> std::uint64_t {
  // The parameter an entry's argument is keyed by ("Scope::ArgsIdOf"): the one its name records, else its own.
  if (const auto stamped = entry.Name->StampedTypeId(); stamped != nullptr) {
    auto const &head = scopes::HeadOf(stamped);
    if (head.Kind == scopes::TypeKey::Tag::TypeParam or head.Kind == scopes::TypeKey::Tag::TypeBound) {
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

auto GenericSolver::_OfferAll(
  scopes::GenericSubst const &inferred, Shared<TypeAst> const &site, Function<bool(_Entry const &)> const &skip,
  Function<scopes::TypeId(_Entry const &, scopes::TypeId)> const &adjust_type) -> bool {
  // Each parameter of this declaration that a match bound is offered what it was bound to, by "ParamId"; a parameter
  // of anything else the match named (a class's, read through) is not this solve's.
  auto newly_bound = false;
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr or entry->Id == 0 or skip(*entry)) { continue; }
    const auto was_bound = entry->IsBound();
    if (entry->IsTypeParam()) {
      const auto hit = genex::find_if(inferred.TypeParams, [&](auto const &x) { return x.first == entry->Id; });
      if (hit == inferred.TypeParams.end()) { continue; }
      const auto type = adjust_type ? adjust_type(*entry, hit->second) : hit->second;
      if (type == nullptr) { continue; }
      entry->TypeVals.EmplaceBack(_TypeVal{.Id = type, .Written = nullptr, .Site = site});
    }
    else {
      const auto hit = genex::find_if(inferred.CompParams, [&](auto const &x) { return x.first == entry->Id; });
      if (hit == inferred.CompParams.end()) { continue; }
      entry->CompVals.EmplaceBack(Shared<ExpressionAst>(_Sm->CurrentScope->CompAstOf(hit->second)));
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
  auto const &scope = *_Sm->CurrentScope;
  auto inferred = scopes::GenericSubst();
  const auto given = scope.TypeIdOf(source);
  auto declared = _OwnerScope->TypeIdOf(target);
  if (given == nullptr or declared == nullptr) { return inferred; }
  if (not _DeclaredReading.IsEmpty()) { declared = scopes::SubstituteTypeId(declared, _DeclaredReading); }
  const auto given_id = scopes::BareOf(given);
  declared = scopes::BareOf(declared);
  const auto matched = scopes::UnifyTypeIds(given_id, declared, scope, *_OwnerScope, inferred, true, true);

  // A value can match through a type it is superimposed with, and then that is what the generics are read from: a
  // closure is a "FunMov[(S32,), Bool]" only through its superimposition, so "f: FunMov[(S32,), U]" had nothing to
  // take "U" from. Only a target with a shape to match: a bare "x: T" that failed did so on its constraints, which a
  // supertype would silently dodge ("T: ThreadSafe" bound to a closure's sup).
  auto const &d = scopes::HeadOf(declared);
  const auto is_bare_param = d.Kind == scopes::TypeKey::Tag::TypeParam or d.Kind == scopes::TypeKey::Tag::TypeBound;
  if (matched or is_bare_param or not scopes::DoesTypeIdNameAnyGnParams(declared)) { return inferred; }
  auto const *const given_sym = TypeRef::Of(given_id, scope).Symbol;
  if (given_sym == nullptr or given_sym->LinkedScope == nullptr) { return inferred; }
  for (auto const *sup_scope : given_sym->LinkedScope->GetSupScopes()) {
    if (sup_scope->LinkedTypeSymbol == nullptr) { continue; }
    const auto sup_id = scope.TypeIdOfSymbol(*sup_scope->LinkedTypeSymbol);
    if (sup_id == nullptr) { continue; }
    auto sup_inferred = scopes::GenericSubst();
    if (scopes::UnifyTypeIds(sup_id, declared, scope, *_OwnerScope, sup_inferred, true, true)) {
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
        entry->TypeVals.EmplaceBack(_TypeVal{.Id = bound, .Written = nullptr, .Site = nullptr});
      }
      continue;
    }
    const auto own = scopes::ParamCompId(entry->Id);
    auto const *const sym = _OwnerScope->FindBoundVarSymbolById(own);
    const auto bound = sym != nullptr ? _OwnerScope->CompIdOfSymbol(*sym) : nullptr;
    if (bound != nullptr and bound != own) {
      entry->CompVals.EmplaceBack(Shared<ExpressionAst>(_Sm->CurrentScope->CompAstOf(bound)));
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
  if (meta.CurrentStage < meta::CompilerStage::kResolveDeclarations) { return false; }
  const auto sups_attached = meta.CurrentStage >= meta::CompilerStage::kAttachSupScopes;
  auto newly_bound = false;
  for (auto const &entry : _Entries) {
    if (not entry->IsCompParam() or entry->CompVals.IsEmpty() or entry->IsCompValueRead) { continue; }
    if (entry->CompVals[0]->To<TypeAst>() != nullptr) { continue; }
    auto *const value = entry->CompVals[0].get();

    // Only a parameter whose declared type names a type parameter has anything to infer from its value ("cmp n: T");
    // one of a closed type ("cmp n: USize") is not read, so a value that cannot be typed yet ("Self::n", before the
    // "sup" blocks are attached) is not asked to be.
    if (const auto declared = _OwnerScope->TypeIdOf(*entry->Param->CompType);
      declared != nullptr and scopes::ParamsNamedBy(declared).TypeParams.empty() and not declared->HasSelf) {
      entry->IsCompValueRead = true;
      continue;
    }

    // A variadic parameter's value is the tuple of what was given, each of which is of the declared type; a pack
    // forwarded to it is its elements when bound, and in a template its name is typed as one element already.
    auto sources = Vec<ExpressionAst*>();
    if (not entry->Param->IsVariadic()) { sources.EmplaceBack(value); }
    else if (value->To<TupleLiteralAst>() != nullptr) { sources = packs::CompPackElements(*value); }
    else if (auto spread = packs::BoundCompPackElements(*value, *sm.CurrentScope); spread.has_value()) {
      sources = std::move(*spread);
    }
    else if (packs::DoesCompNameAnUnboundPack(*value, *sm.CurrentScope)) { sources.EmplaceBack(value); }

    entry->IsCompValueRead = true;
    for (auto *source : sources) {
      if (not sups_attached and source->To<LiteralAst>() == nullptr) { continue; }
      const auto is_operator = comp_generics::IsCompOperator(*source);
      const auto folded = is_operator ? sm.CurrentScope->FoldedCompAstOf(*source) : nullptr;
      if (is_operator and folded == nullptr) { continue; }
      // A copy: a literal's type is its symbol's cached "FqName" node, shared by every use of the type, and the
      // binding is analysed where it is used - early, it would cache a resolution made before the sups exist on all
      // of them.
      const auto source_type = AstCloneShared((folded != nullptr ? folded.get() : source)->InferType(&sm, &meta));
      newly_bound = _OfferAll(
        _Match(*source_type, *entry->Param->CompType), source_type,
        [](auto const &e) { return not e.TypeVals.IsEmpty(); }) or newly_bound;
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
    if (not entry->IsTypeParam() or entry->Param->TypeConstraints->Constraints.IsEmpty()) { continue; }
    if (entry->TypeVals.IsEmpty() or entry->IsConstraintsRead) { continue; }
    entry->IsConstraintsRead = true;
    const auto inferred_id = entry->TypeVals[0].Id;
    if (inferred_id == nullptr) { continue; }

    // The candidates are the type and its super classes, each with the scope its name resolves in: a sup type's
    // name can hold a "Self" (as in "S32 ext Ord[Rhs=Self]"), which only has a symbol inside that sup scope.
    auto const *const concrete_sym = TypeRef::Of(inferred_id, scope).Symbol;
    auto candidates = Vec<Pair<scopes::TypeId, Scope const*>>{};
    if (concrete_sym != nullptr and not concrete_sym->IsGn()) {
      candidates.EmplaceBack(scope.TypeIdOfSymbol(*concrete_sym), &scope);
      if (concrete_sym->LinkedScope != nullptr) {
        for (auto const &[sup, sup_scope] : type_members::SuperClsNames(concrete_sym->LinkedScope->GetSupScopes())) {
          candidates.EmplaceBack(sup_scope->TypeIdOf(*sup), sup_scope);
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
      if (not candidates.IsEmpty() and not matched) {
        const auto constraint_drives_inference = genex::any_of(
          type_params, [&](auto const *other) {
            return type_resolution::DoesTypeNameAGnParam(*constraint, *other, *_OwnerScope);
          });
        if (constraint_drives_inference) {
          const auto inferred_type = _AstOf(entry->TypeVals[0]);
          Raise<SppGenericConstraintError>({_OwnerScope, sm.CurrentScope}, ERR_ARGS(*constraint, *inferred_type));
        }
      }
      newly_bound = _OfferAll(inferred, entry->TypeVals[0].Written, [](auto const &e) { return e.IsBound(); })
        or newly_bound;
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
  if (auto const *const pinned = _Find(*generate::common_types::SelfType(0)->ToUnchecked<TypeIdentifierAst>());
    pinned != nullptr and not pinned->TypeVals.IsEmpty()) { return pinned->TypeVals[0].Written; }
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
      if (_Meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases) {
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
      entry->TypeVals.EmplaceBack(_TypeVal{.Id = def_id, .Written = std::move(def_type), .Site = std::move(site)});
    }
    else if (entry->IsCompParam() and entry->Param->IsOptional()) {
      entry->CompVals.EmplaceBack(entry->Param->CompDefault);
      entry->IsCompFromDefault = true;
    }

    // A pack nothing has bound is given nothing: it is empty, the
    // tuple of no elements, as a variadic function parameter given
    // no arguments is ("f[..Ts]()" called as "f()").
    else if (entry->IsTypeParam() and entry->Param->IsVariadic()) {
      auto empty = generate::common_types::TupleType(entry->Param->PosStart(), {});
      const auto empty_id = _Meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases
        ? _OwnerScope->TypeIdOf(*empty)
        : nullptr;
      auto site = empty_id != nullptr ? empty : nullptr;
      entry->TypeVals.EmplaceBack(_TypeVal{.Id = empty_id, .Written = std::move(empty), .Site = std::move(site)});
    }
    else if (entry->IsCompParam() and entry->Param->IsVariadic()) {
      entry->CompVals.EmplaceBack(MakeShared<TupleLiteralAst>(nullptr, Vec<Unique<ExpressionAst>>(), nullptr));
    }
  }
}

auto GenericSolver::_EnforceNoConflicts() const -> void {
  // A parameter reached through several arguments, or through an argument and a constraint, has to be offered the
  // same thing by each: the same type, and the same value (the same "CompId", by identity rather than spelling:
  // "0x2_uz" and "2_uz" are one value). A type that does not key has nothing to compare.
  using errors::SppGenericParameterConflictError;
  auto const &scope = *_Sm->CurrentScope;
  for (auto const &entry : _Entries) {
    // A parameter is shown where it is declared ("_OwnerScope"); a name that is no parameter where it was given.
    const auto scopes = Vec<Scope const*>{entry->Param != nullptr ? _OwnerScope : &scope, &scope};
    auto const &types = entry->TypeVals;
    for (auto i = 1uz; i < types.Len(); ++i) {
      if (types[0].Id == nullptr or types[i].Id == nullptr or types[0].Id == types[i].Id) { continue; }
      const auto first = _AstOf(types[0]);
      const auto other = _AstOf(types[i]);
      Raise<SppGenericParameterConflictError>(scopes, ERR_ARGS(*entry->Name, *first, *other));
    }
    auto const &comps = entry->CompVals;
    for (auto i = 1uz; i < comps.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        scope.CompIdOf(*comps[i]) != scope.CompIdOf(*comps[0]), scopes, ERR_ARGS(*entry->Name, *comps[0], *comps[i]));
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
      const auto bound = want_type ? not entry->TypeVals.IsEmpty() : not entry->CompVals.IsEmpty();
      RaiseIf<SppGenericParameterNotInferredError>(
        not bound, {_Sm->CurrentScope, _OwnerScope}, ERR_ARGS(*entry->Name, owner));
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
      and _Meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases
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
    else { comp->Stage7_AnalyseSemantics(_Sm, _Meta); }
    entry->CompVals.Clear();
    entry->CompVals.EmplaceBack(std::move(comp));
  }
}

auto GenericSolver::_CheckTypeArgs(scopes::GenericSubst const &bindings) const -> void {
  // Check each type argument against its parameter's constraints, read with every binding substituted in.
  auto args = Vec<Tup<GenericParameterAst const*, scopes::TypeId, Shared<TypeAst>>>();
  for (auto const &entry : _Entries) {
    if (not entry->IsTypeParam() or entry->TypeVals.IsEmpty() or entry->TypeVals[0].Id == nullptr) { continue; }
    if (entry->Param->TypeConstraints->Constraints.IsEmpty()) { continue; }
    args.EmplaceBack(entry->Param, entry->TypeVals[0].Id, _AstOf(entry->TypeVals[0]));
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
    if (not entry->IsCompParam() or entry->CompVals.IsEmpty()) { continue; }
    auto *const value = entry->CompVals[0].get();
    // A comp expression is typed once the sweep has attached every "sup" scope, as it is analysed then
    // ("GenericArgumentAst::AnalyseCompVal"); a type made during the sweep is checked where it is written.
    if (meta.CurrentStage < meta::CompilerStage::kPreAnalyseSemantics and comp_generics::NeedsSupScopesToType(*value)) {
      continue;
    }
    const auto raw_a_type = value->InferType(&sm, &meta);
    const auto p_type = type_resolution::ReadType(
      *entry->Param->CompType, ExprSubst::Across(*_OwnerScope, bindings, *sm.CurrentScope));

    // A pack's value is a tuple, each element of which is of the declared type, read off the tuple's identity.
    if (entry->Param->IsVariadic()) {
      const auto p_ref = TypeRef::Of(*p_type, *sm.CurrentScope);
      auto const *const a_sym = TypeRef::Of(*raw_a_type, *sm.CurrentScope).Symbol;
      for (auto const &inner : a_sym != nullptr ? a_sym->TypeArgRefs() : Vec<TypeRef>()) {
        RaiseIf<SppTypeMismatchError>(
          not type_compare::Assignable(p_ref, inner, *sm.CurrentScope, *sm.CurrentScope),
          {_OwnerScope, sm.CurrentScope}, ERR_ARGS(*entry->Param, *p_type, *value, ErrTypeAt(inner, *value)));
      }
      continue;
    }
    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(
        TypeRef::Of(*p_type, *sm.CurrentScope), TypeRef::Of(*raw_a_type, *sm.CurrentScope), *sm.CurrentScope,
        *sm.CurrentScope),
      {_OwnerScope, sm.CurrentScope}, ERR_ARGS(*entry->Param, *p_type, *value, *raw_a_type));
  }
}

auto GenericSolver::Solve(
  Ast const &owner, Shared<IdentifierAst> const &variadic_fn_param) -> void {
  // A declaration with no parameters of its own has nothing to solve: what was given is the answer, whatever it names.
  if (_Params->Params.IsEmpty()) {
    _Trivial = true;
    return;
  }

  // What the declaration's own scope binds already, then the equations. A variadic function parameter is matched
  // against the pack's tuple, so a non-variadic type parameter it names is bound to the element, not the tuple; an
  // empty pack binds no element. What was given as a type is not offered anything else.
  _SeedFromOwner();
  for (auto const &eq : _Equations) {
    const auto is_pack_slot = variadic_fn_param != nullptr and *eq->Name == *variadic_fn_param;
    _OfferAll(
      _Match(*eq->Source, *eq->Target), eq->Source, [](auto const &e) { return e.IsTypeGiven; },
      [is_pack_slot](auto const &e, const scopes::TypeId type) -> scopes::TypeId {
        if (not is_pack_slot or e.Param->IsVariadic()) { return type; }
        auto const &head = scopes::HeadOf(type);
        const auto elements = head.Args != nullptr ? scopes::ArgsOf(head.Args) : std::vector<scopes::TypeIdArg>();
        return elements.empty() ? nullptr : elements[0].TypeVal;
      });
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
  const auto check_comps = _Meta->CurrentStage >= meta::CompilerStage::kAttachSupScopes;
  const auto check_types = _Meta->CurrentStage >= meta::CompilerStage::kPreAnalyseSemantics
    and not _Meta->SkipSubstitutedConstraintChecks;
  if (not check_comps and not check_types) { return; }
  const auto bindings = _Bindings();
  if (check_comps) { _CheckCompArgs(bindings); }
  if (check_types) { _CheckTypeArgs(bindings); }
}

auto GenericSolver::SolvedArgsId() const -> scopes::TypeId {
  // As "Scope::ArgsIdOf" keys a keyword argument: the parameter its name stamps, else the entry's, "Self" as 0, else
  // the name as spelled; then the value's identity, a comp value keyed where the call is.
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
    const auto id = _EntryParamId(*entry);
    if (id != 0) { key.Push(TypeKey::Tag::Arg, id); }
    else if (entry->Name->ToView() == "Self") { key.Push(TypeKey::Tag::Arg, 0); }
    else { key.PushText(TypeKey::Tag::Name, entry->Name->ToView()); }

    if (not entry->TypeVals.IsEmpty()) {
      const auto type_id = _EntryTypeId(*entry);
      if (type_id == nullptr) { return nullptr; }
      key.PushTypePart(type_id);
    }
    else {
      const auto comp = _Sm->CurrentScope->CompIdOf(*entry->CompVals[0]);
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
    if (not entry->TypeVals.IsEmpty()) {
      if (entry->IsTypeGiven and entry->TypeVals[0].Written != nullptr) {
        out.EmplaceBack(GenericArgumentAst::NewType(entry->Name, entry->TypeVals[0].Written));
        continue;
      }
      out.EmplaceBack(GenericArgumentAst::NewType(entry->Name, _AstOf(entry->TypeVals[0])));
    }
    else if (not entry->CompVals.IsEmpty()) {
      // A comp value is cloned where a type is shared: it is analysed in place where the argument is read, and may be
      // the parameter's own default, or the caller's argument.
      out.EmplaceBack(GenericArgumentAst::NewComp(entry->Name, AstCloneShared(entry->CompVals[0])));
    }
  }
  return out;
}

SPP_MOD_END

auto spp::analyse::utils::generic_inference::NamedGnArgs(
  GenericArgumentGroupAst const &written, GenericParameterGroupAst const &p_group, Scope const &params_scope,
  Ast const &owner, ScopeManager &sm,
  meta::CompilerMetaData &meta, const bool is_tuple_owner) -> Unique<GenericArgumentGroupAst> {
  // A tuple's arguments stay positional.
  auto named = AstClone(&written);
  if (is_tuple_owner) { return named; }

  // The two kinds are named separately, as a type and a comp parameter cannot share a name, then put in parameter
  // order.
  auto comp_args = Vec<Unique<GenericArgumentAst>>();
  auto type_args = Vec<Unique<GenericArgumentAst>>();
  for (auto &&arg : std::move(named->Args)) {
    (arg->IsCompArg() ? comp_args : type_args).EmplaceBack(std::move(arg));
  }
  NameCompArgs(comp_args, p_group.GetCompParams(), params_scope, owner, sm, meta);
  NameTypeArgs(type_args, p_group.GetTypeParams(), params_scope, owner, sm, meta);
  named->Args = std::move(comp_args);
  named->Args.AppendRange(std::move(type_args));

  auto param_index = Map<StrView, std::size_t>();
  for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) {
    param_index[p->Name->ToUnchecked<TypeIdentifierAst>()->Name] = i;
  }
  named->Args |= genex::actions::sort([&](auto const &a, auto const &b) {
    return param_index[a->ViewName()] < param_index[b->ViewName()];
  });
  return named;
}
