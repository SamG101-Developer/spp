module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.generic_inference;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
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
    /// Reject the first argument name no parameter has, reported against the first parameter (or the argument itself,
    /// when there are none).
    auto EnforceGnArgNamesKnown(
      Vec<GenericParameterAst*> const &params, Vec<GenericArgumentAst*> const &args, Scope *scope) -> void {
      for (auto const *arg : args) {
        const auto known = genex::any_of(params, [arg](auto const *p) { return *p->Name == *arg->TypeName(); });
        if (known) { continue; }
        Raise<errors::SppArgumentNameInvalidError>(
          {scope}, ERR_ARGS(
            params.IsEmpty() ? static_cast<Ast const&>(*arg->TypeName()) : *params[0], StrView("gn param"),
            *arg->TypeName(), StrView("gn arg")));
      }
    }

    /// Check each type argument against its parameter's constraints, read where the parameters are written
    /// ("written_scope", which an unsatisfied one is reported from) with the arguments bound ("bindings"), from the use
    /// site. A pack's constraints hold for each of its elements.
    auto CheckTypeArgConstraints(
      GenericParameterGroupAst const &p_group, GenericArgumentGroupAst const &a_group, Scope const &written_scope,
      scopes::GenericSubst const &bindings, ScopeManager &sm, meta::CompilerMetaData &meta) -> void {
      using errors::SppGenericConstraintError;

      // Extract important information.
      auto p_names = p_group.GetTypeParams()
        | genex::views::transform([](auto &&x) { return dynamic_shared_cast<TypeIdentifierAst>(x->Name); })
        | genex::to<Vec>();
      auto p_con_groups = p_group.GetTypeParams()
        | genex::views::transform([](auto &&x) { return x->TypeConstraints->Constraints; })
        | genex::to<Vec>();
      const auto type_args = a_group.GetTypeArgs();

      // Check that each argument satisfies its constraints.
      for (auto [i, p_name] : p_names | genex::views::enumerate) {
        auto matching = type_args
          | genex::views::filter([&](auto const *a) { return a->ViewName() == p_name->Name; })
          | genex::to<Vec>();
        if (matching.IsEmpty()) { continue; }

        const auto arg_sym = sm.CurrentScope->FindTypeSymbol(matching[0]->TypeVal.get());
        if (arg_sym == nullptr) { continue; }
        const auto con_scope = arg_sym->LinkedScope != nullptr
          ? arg_sym->LinkedScope
          : sm.CurrentScope;
        auto con_sm = ScopeManager(sm.GlobalScope, con_scope);

        // This parameter's constraints, with the arguments bound.
        auto p_cons = Vec<Shared<TypeAst>>();
        for (auto const &p_con : p_con_groups[i]) {
          const auto sub = type_resolution::ReadType(
            *p_con, ExprSubst::Across(written_scope, bindings, *sm.CurrentScope));
          {
            // Resolved from the argument's scope, but written, and
            // checked for visibility, where the parameter is declared:
            // a module-private alias there is not visible from, say, a
            // closure argument's (global) scope, and needn't be.
            const auto _meta_guard = meta::MetaGuard(&meta);
            meta.AllowAbstractType = true;
            meta.IgnoreAccessModifierViolations = true;
            sub->Stage7_AnalyseSemantics(&con_sm, &meta);
          }
          p_cons.push_back(sub->WithSourceSpanOf(*p_con));
        }

        // A pack's constraints hold for each of its elements, not for
        // the tuple it is bound to.
        const auto targets = p_group.GetTypeParams()[i]->IsVariadic()
          ? packs::TypePackElements(*matching[0]->TypeVal)
          : Vec<Shared<TypeAst>>{matching[0]->TypeVal};

        // Raise an error if any constraint of this argument is
        // not satisfied.
        for (auto const &target : targets) {
          const auto unsatisfied = type_compare::UnmetConstraint(
            p_cons, TypeRef::Of(*target, *sm.CurrentScope), target->IsSelfType(), *sm.CurrentScope, *sm.CurrentScope);
          RaiseIf<SppGenericConstraintError>(
            unsatisfied != nullptr, {&written_scope, sm.CurrentScope},
            ERR_ARGS(*unsatisfied, *target));
        }
      }
    }

    /// Name the positional arguments of one kind after the parameters they bind ("type_resolution::ParamsOfArgs";
    /// "NameTypeArgs", "NameCompArgs"): each is copied under its parameter's name by "bind", until a trailing variadic
    /// parameter takes the rest as a tuple, built by "pack". "f[U32, U32]" for "f[..Ts]" is "f[Ts=(U32, U32)]", and
    /// "f[1_u32, 1_u32]" for "f[cmp ..s]" is "f[s=(1_u32, 1_u32)]". A lone argument naming a pack ("A[Ts]" inside
    /// "g[..Ts]") already is that tuple, so is forwarded as it is ("forwards").
    auto NameArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Ast const &owner, ScopeManager &sm,
      meta::CompilerMetaData &meta,
      Function<void(GenericArgumentAst &, Vec<GenericArgumentAst*> const &, bool)> const &pack,
      Function<void(GenericArgumentAst &, GenericArgumentAst const &)> const &bind) -> void {
      const auto keyword_args = args
        | genex::views::ptr
        | genex::views::filter([](auto const *x) { return x->TypeName() != nullptr; })
        | genex::to<Vec>();
      EnforceGnArgNamesKnown(params, keyword_args, sm.CurrentScope);

      const auto targets = type_resolution::ParamsOfArgs(args | genex::views::ptr | genex::to<Vec>(), params);

      const auto _meta_guard = meta::MetaGuard(&meta);
      meta.TypeAnalysisTypeScope = nullptr;

      for (auto i = 0uz; i < args.Len(); ++i) {
        auto const &positional = args[i];
        if (positional->TypeName() != nullptr) { continue; }
        auto const *const param = targets[i];
        RaiseIf<errors::SppGenericArgumentTooManyError>(
          param == nullptr, {sm.CurrentScope}, ERR_ARGS(params.IsEmpty() ? owner : *params[0], owner, *positional));
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

    /// "NameArgs" for type arguments. Collected into the pack's tuple, an argument naming a bound type pack is spread
    /// into that pack's elements ("A[S32, Ts]" with "Ts" bound to "Tup[Bool, U8]" is "A[S32, Bool, U8]"). Each is
    /// analysed once types resolve.
    auto NameTypeArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Ast const &owner, ScopeManager &sm,
      meta::CompilerMetaData &meta) -> void {
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
      NameArgs(args, params, owner, sm, meta, pack, bind);
    }

    /// "NameTypeArgs" for comp arguments: a bound comp pack is spread into its elements. A literal or a name is analysed
    /// once types resolve; anything else waits until stage 5 ends, as a comp expression ("n + 1") resolves its
    /// operator through the sup scopes of its operand's type, attached only then. An operator expression is never
    /// analysed here - analysing it consumes it - but by the argument's own "AnalyseCompVal", which folds it or checks
    /// a copy.
    auto NameCompArgs(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, Ast const &owner, ScopeManager &sm,
      meta::CompilerMetaData &meta) -> void {
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
        if (stage >= meta::CompilerStage::kResolveDeclarations and (not comp_generics::IsCompExpression(val)
          or (stage >= meta::CompilerStage::kPreAnalyseSemantics and not comp_generics::IsCompOperator(val)))) {
          val.Stage7_AnalyseSemantics(&sm, &meta);
        }
      };
      NameArgs(args, params, owner, sm, meta, pack, bind);
    }
  }
}

SPP_MOD_BEGIN
/// Everything one generic parameter (or a name given alongside the parameters, like a pinned "Self") is offered.
struct spp::analyse::utils::generic_inference::GenericSolver::_Entry {
  /// The parameter, or @c nullptr for a given name that is not one.
  GenericParameterAst const *Param;
  Shared<TypeIdentifierAst> Name;
  Vec<Shared<TypeAst>> TypeVals;
  Vec<Shared<ExpressionAst>> CompVals;

  /// Given a type rather than inferred one, so inference neither offers it another nor rewrites it.
  bool IsTypeGiven = false;

  /// Bound to its default, which is written in the declaration's own terms and so may name the others.
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
    entry->Name = dynamic_shared_cast<TypeIdentifierAst>(param->Name);
    _Entries.EmplaceBack(std::move(entry));
  }
}

GenericSolver::~GenericSolver() = default;

auto GenericSolver::_Find(
  TypeIdentifierAst const &name) const -> _Entry* {
  for (auto const &entry : _Entries) { if (*entry->Name == name) { return entry.get(); } }
  return nullptr;
}

auto GenericSolver::Give(
  Vec<Unique<GenericArgumentAst>> args, const bool emit) -> void {
  // Taken by value, so the caller's list is emptied rather than left holding moved-from arguments.
  using errors::SppInternalCompilerError;
  for (auto &arg : args) {
    if (arg->TypeName() == nullptr or (not arg->IsTypeArg() and not arg->IsCompArg())) {
      const auto err = "generic argument '" + arg->ToString() + "' is still positional where a binding is expected";
      Raise<SppInternalCompilerError>({_Sm->CurrentScope}, ERR_ARGS(*arg, err));
    }

    // A layer given earlier outranks this one: the first binding offered for a name is the one it keeps.
    const auto name = dynamic_shared_cast<TypeIdentifierAst>(arg->TypeName());
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
      entry->TypeVals.EmplaceBack(arg->TypeVal);
      entry->IsTypeGiven = true;
    }
    else { entry->CompVals.EmplaceBack(arg->CompVal); }
    _Given.EmplaceBack(std::move(arg));
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

auto GenericSolver::GetKnownArgs() const -> Vec<GenericArgumentAst*> {
  return _Given | genex::views::ptr | genex::to<Vec>();
}

auto GenericSolver::_InferenceMap() const -> type_compare::GenericInferenceMap {
  auto out = type_compare::GenericInferenceMap();
  for (auto const &entry : _Entries) {
    if (not entry->TypeVals.IsEmpty()) { out.emplace(entry->Name, entry->TypeVals[0]); }
    else if (not entry->CompVals.IsEmpty()) { out.emplace(entry->Name, entry->CompVals[0]); }
  }
  return out;
}

auto GenericSolver::_OfferAll(
  type_compare::GenericInferenceMap const &inferred, Function<bool(_Entry const &)> const &skip,
  Function<Shared<TypeAst>(_Entry const &, Shared<TypeAst>)> const &adjust_type) -> bool {
  // A comp value offered to a type parameter is not a binding for it; nor is anything offered to a name the group does
  // not declare. A type offered to a comp parameter is a value named through that type ("Self::mo_seq_cst"), taken in
  // its identifier form, so every comp binding the solver holds is an expression, whatever reads it.
  auto newly_bound = false;
  for (auto const &[name, value] : inferred) {
    auto *const entry = _Find(*name);
    if (entry == nullptr or entry->Param == nullptr or skip(*entry)) { continue; }
    const auto was_bound = entry->IsBound();
    if (entry->IsTypeParam()) {
      if (value->To<TypeAst>() == nullptr) { continue; }
      auto type = static_shared_cast<TypeAst>(value);
      if (adjust_type) { type = adjust_type(*entry, std::move(type)); }
      if (type == nullptr) { continue; }
      entry->TypeVals.EmplaceBack(std::move(type));
    }
    else {
      auto const *const as_type = value->To<TypeAst>();
      entry->CompVals.EmplaceBack(as_type != nullptr ? Shared<ExpressionAst>(IdentifierAst::FromType(*as_type)) : value);
    }
    newly_bound = newly_bound or not was_bound;
  }
  return newly_bound;
}

auto GenericSolver::_Match(
  Shared<TypeAst> const &source, Shared<TypeAst> const &target) const -> type_compare::GenericInferenceMap {
  // Match the type of what was given against the type it was given for, binding the generics the target names.
  auto &sm = *_Sm;
  auto inferred = type_compare::GenericInferenceMap();
  const auto matched = type_compare::RelaxedTypeEq(
    *source->WithoutConvention(), *target->WithoutConvention(), *sm.CurrentScope, *_OwnerScope, inferred, true);

  // A value can match through a type it is superimposed with, and then that is what the generics are read from: a
  // closure is a "FunMov[(S32,), Bool]" only through its superimposition, so "f: FunMov[(S32,), U]" had nothing to
  // take "U" from. Only a target with a shape to match: a bare "x: T" that failed did so on its constraints, which a
  // supertype would silently dodge ("T: ThreadSafe" bound to a closure's sup).
  const auto target_has_args = not target->WithoutConvention()->LastTypePart()->GnArgGroup->Args.IsEmpty();
  if (matched or not target_has_args) { return inferred; }
  const auto source_sym = sm.CurrentScope->FindTypeSymbol(source->WithoutConvention().get());
  if (source_sym == nullptr or source_sym->LinkedScope == nullptr) { return inferred; }
  for (auto const *sup_scope : source_sym->LinkedScope->GetSupScopes()) {
    if (sup_scope->LinkedTypeSymbol == nullptr) { continue; }
    auto sup_inferred = type_compare::GenericInferenceMap();
    if (type_compare::RelaxedTypeEq(
      *sup_scope->LinkedTypeSymbol->FqName(), *target->WithoutConvention(), *sm.CurrentScope, *_OwnerScope,
      sup_inferred, true)) {
      return sup_inferred;
    }
  }
  return inferred;
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
      declared != nullptr and scopes::ParamsOf(declared).TypeParams.empty() and not declared->HasSelf) {
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
      const auto folded = is_operator ? comp_generics::FoldCompExpr(*source, *sm.CurrentScope) : nullptr;
      if (is_operator and folded == nullptr) { continue; }
      // A copy: a literal's type is its symbol's cached "FqName" node, shared by every use of the type, and the
      // binding is analysed where it is used - early, it would cache a resolution made before the sups exist on all
      // of them.
      const auto source_type = AstCloneShared((folded != nullptr ? folded.get() : source)->InferType(&sm, &meta));
      newly_bound = _OfferAll(
        _Match(source_type, entry->Param->CompType), [](auto const &e) { return not e.TypeVals.IsEmpty(); })
        or newly_bound;
    }
  }
  return newly_bound;
}

auto GenericSolver::_ReadConstraints() -> bool {
  // Constraint-based inference: "[U, F: FunRef[(), U]]" infers "U" from the return type of whichever of the type
  // bound to "F" and its super classes is the "FunRef".
  using errors::SppGenericConstraintError;
  auto &sm = *_Sm;
  auto newly_bound = false;
  const auto type_params = _Params->GetTypeParams();
  for (auto const &entry : _Entries) {
    if (not entry->IsTypeParam() or entry->Param->TypeConstraints->Constraints.IsEmpty()) { continue; }
    if (entry->TypeVals.IsEmpty() or entry->IsConstraintsRead) { continue; }
    entry->IsConstraintsRead = true;
    auto const inferred_type = entry->TypeVals[0];

    // The candidates are the type and its super classes, each with the scope its name resolves in: a sup type's
    // name can hold a "Self" (as in "S32 ext Ord[Rhs=Self]"), which only has a symbol inside that sup scope.
    const auto concrete_sym = sm.CurrentScope->FindTypeSymbol(inferred_type.get());
    auto candidates = Vec<Pair<Shared<TypeAst>, Scope const*>>{};
    if (concrete_sym != nullptr and not concrete_sym->IsGn()) {
      candidates.EmplaceBack(concrete_sym->FqName(), sm.CurrentScope);
      if (concrete_sym->LinkedScope != nullptr) {
        candidates.AppendRange(type_members::SuperClsNames(concrete_sym->LinkedScope->GetSupScopes()));
      }
    }

    for (auto const &constraint : entry->Param->TypeConstraints->Constraints) {
      auto inferred = type_compare::GenericInferenceMap();
      auto matched = false;
      for (auto const &[candidate, candidate_scope] : candidates) {
        inferred.clear();
        if (type_compare::RelaxedTypeEq(
          *candidate->WithoutConvention(), *constraint->WithoutConvention(),
          *candidate_scope, *_OwnerScope, inferred, true, false)) {
          matched = true;
          break;
        }
      }

      // A constraint that other parameters are inferred through ("U" in "P: FunMov[(T,), Opt[U]]") and that the
      // argument does not fit is reported as a constraint error here, rather than as the dependent parameter being
      // uninferred later, which would hide the cause. One naming no other generics ("P: Copy") is left to the
      // authoritative "_CheckTypeArgs", so a "RelaxedTypeEq" false negative rejects nothing.
      if (not candidates.IsEmpty() and not matched) {
        const auto constraint_drives_inference = genex::any_of(
          type_params, [&](auto const *other) {
            return type_resolution::DoesTypeNameAGnParam(*constraint, *other, *_OwnerScope);
          });
        RaiseIf<SppGenericConstraintError>(
          constraint_drives_inference, {_OwnerScope, sm.CurrentScope}, ERR_ARGS(*constraint, *inferred_type));
      }
      newly_bound = _OfferAll(inferred, [](auto const &e) { return e.IsBound(); }) or newly_bound;
    }
  }
  return newly_bound;
}

auto GenericSolver::_SelfForDefault(
  const bool as_value) const -> Shared<TypeAst> {
  // What "Self" stands for in a default, decided in one place for both kinds. What the use pinned "Self" to (a call's
  // receiver) comes first. Otherwise a value named through it ("Self::mo_seq_cst") is looked up in the type the owner
  // is written for, so needs that type; a type argument keeps "Self" as written - it is read where it is used, and an
  // instantiation keys a "Self" argument by its spelling (keyed by meaning, "Vec[Self]" minted "View[T=Self]" without
  // end).
  if (auto const *const pinned = _Find(*generate::common_types::SelfType(0)->ToUnchecked<TypeIdentifierAst>());
    pinned != nullptr and not pinned->TypeVals.IsEmpty()) { return pinned->TypeVals[0]; }
  if (not as_value) { return nullptr; }
  auto self_type = _OwnerScope->FindEnclosingSelfType(*_Meta);
  return self_type != nullptr and not self_type->IsSelfType() ? self_type : nullptr;
}

auto GenericSolver::_ApplyDefaults() -> void {
  // An optional parameter nothing has bound takes its default, as written in the declaration's own terms: it is read in
  // the use site's, with everything bound, by "_CrossSubstitute". A type default records what it means where it is
  // written, on a copy of its own ("Self" as "_SelfForDefault" decides); a comp default is read from its written form
  // ("GenericParameterAst::WrittenCompDefault").
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr or entry->IsBound()) { continue; }
    if (entry->IsTypeParam() and entry->Param->IsOptional()) {
      auto def_type = AstCloneShared(entry->Param->TypeDefault);
      if (_Meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases) {
        if (not def_type->IsSelfType()) { type_resolution::RecordTypeParts(*def_type, *_OwnerScope); }
        else if (const auto self_type = _SelfForDefault(false); self_type != nullptr) {
          def_type = self_type->WithConvention(AstClone(def_type->GetConvention()));
        }
      }
      entry->TypeVals.EmplaceBack(std::move(def_type));
    }
    else if (entry->IsCompParam() and entry->Param->IsOptional()) {
      entry->CompVals.EmplaceBack(entry->Param->CompDefault);
      entry->IsCompFromDefault = true;
    }
  }
}

auto GenericSolver::_EnforceNoConflicts() const -> void {
  // A parameter reached through several arguments, or through an argument and a constraint, has to be offered the
  // same thing by each. Types must be the same type ("TypeEq" is identity, so the binding cannot depend on which
  // argument came first). Comp values must be the same value ("CompEq", by identity rather than
  // spelling): "0x2_uz" and "2_uz" are one value.
  using errors::SppGenericParameterConflictError;
  using type_compare::TypeEq;
  using type_compare::CompEq;
  auto const &scope = *_Sm->CurrentScope;
  for (auto const &entry : _Entries) {
    auto const &types = entry->TypeVals;
    for (auto i = 1uz; i < types.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        not TypeEq(*types[i], *types[0], scope, scope),
        {_Sm->CurrentScope}, ERR_ARGS(*entry->Name, *types[0], *types[i]));
    }
    auto const &comps = entry->CompVals;
    for (auto i = 1uz; i < comps.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        not CompEq(*comps[i], *comps[0], scope, scope),
        {_Sm->CurrentScope}, ERR_ARGS(*entry->Name, *comps[0], *comps[i]));
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
  // Read each binding the solver made in the declaration's own terms in the use site's, with everything else bound
  // (each skipping itself): an inferred or default type, so "Vec[T, A=Alloc[T]]" receives the actual "T", and a comp
  // default, so "m = n + 1_uz" receives the "n" bound with it and "Self::mo_seq_cst" the "Self" the use pinned. A
  // literal default names nothing, and nothing is read before the aliases exist.
  //
  // A comp default read is its value when it folds ("n + 1_uz" with "n" bound to "3_uz" is "4_uz"). One still naming a
  // generic is analysed: an operator caches the symbol its left-hand side resolved to, and code generation reads that
  // back, so an unanalysed one reaches stage 11 looking like a namespace access.
  auto &sm = *_Sm;
  auto self_type = Shared<TypeAst>(nullptr);
  auto self_looked_up = false;
  for (auto const &entry : _Entries) {
    const auto rewrites_type = not entry->TypeVals.IsEmpty() and not entry->IsTypeGiven;
    const auto rewrites_comp = entry->IsCompFromDefault
      and _Meta->CurrentStage >= meta::CompilerStage::kGenTopLvlAliases
      and entry->Param->WrittenCompDefault->To<LiteralAst>() == nullptr;
    if (not rewrites_type and not rewrites_comp) { continue; }
    auto others = _InferenceMap();
    others.erase(entry->Name);
    auto bindings = type_resolution::BindInferred(others, *_Params, *sm.CurrentScope);
    if (rewrites_type) {
      auto type = type_resolution::ReadType(*entry->TypeVals[0], ExprSubst::In(*sm.CurrentScope, bindings));
      type->Stage7_AnalyseSemantics(_Sm, _Meta);
      entry->TypeVals.Clear();
      entry->TypeVals.EmplaceBack(std::move(type));
      continue;
    }

    // "Self" is looked up once, and only for a comp default: it is a symbol lookup, and most uses have none.
    if (not self_looked_up) {
      self_looked_up = true;
      self_type = _SelfForDefault(true);
    }
    if (self_type != nullptr) { type_resolution::BindSelf(bindings, *self_type, *sm.CurrentScope); }
    auto comp = type_resolution::ReadCompDefault(*entry->Param, std::move(bindings), *_OwnerScope, *sm.CurrentScope);
    if (auto folded = comp_generics::FoldCompExpr(*comp, *sm.CurrentScope); folded != nullptr) {
      comp = std::move(folded);
    }
    else { comp->Stage7_AnalyseSemantics(_Sm, _Meta); }
    entry->CompVals.Clear();
    entry->CompVals.EmplaceBack(std::move(comp));
  }
}

auto GenericSolver::_CheckTypeArgs(scopes::GenericSubst const &bindings) const -> void {
  // Check each type argument against its parameter's constraints, read with every binding substituted in.
  CheckTypeArgConstraints(
    *_Params, *GenericArgumentGroupAst::FromMap(_InferenceMap()), *_OwnerScope, bindings, *_Sm, *_Meta);
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
    if (meta.CurrentStage < meta::CompilerStage::kPreAnalyseSemantics and comp_generics::IsCompExpression(*value)) {
      continue;
    }
    const auto raw_a_type = value->InferType(&sm, &meta);
    const auto p_type = type_resolution::ReadType(
      *entry->Param->CompType, ExprSubst::Across(*_OwnerScope, bindings, *sm.CurrentScope));

    // A pack's value is a tuple, each element of which is of the declared type.
    if (entry->Param->IsVariadic()) {
      auto const *const a_sym = sm.CurrentScope->FindTypeSymbol(raw_a_type.get());
      const auto a_type = a_sym != nullptr ? a_sym->FqName() : raw_a_type;
      for (auto const &inner : packs::TypePackElements(*a_type)) {
        RaiseIf<SppTypeMismatchError>(
          not type_compare::Assignable(*p_type, *inner, *sm.CurrentScope, *sm.CurrentScope),
          {_OwnerScope, sm.CurrentScope}, ERR_ARGS(*entry->Param, *p_type, *value, *inner));
      }
      continue;
    }
    RaiseIf<SppTypeMismatchError>(
      not type_compare::Assignable(*p_type, *raw_a_type, *sm.CurrentScope, *sm.CurrentScope),
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

  // The equations. A variadic function parameter is matched against the pack's tuple, so a non-variadic type
  // parameter it names is bound to the element, not the tuple; an empty pack binds no element. What was given as a
  // type is not offered anything else.
  for (auto const &eq : _Equations) {
    const auto is_pack_slot = variadic_fn_param != nullptr and *eq->Name == *variadic_fn_param;
    _OfferAll(
      _Match(eq->Source, eq->Target), [](auto const &e) { return e.IsTypeGiven; },
      [is_pack_slot](auto const &e, Shared<TypeAst> type) -> Shared<TypeAst> {
        if (not is_pack_slot or e.Param->IsVariadic()) { return type; }
        const auto elements = packs::TypePackElements(*type);
        return elements.IsEmpty() ? nullptr : elements[0];
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
  const auto bindings = type_resolution::BindInferred(_InferenceMap(), *_Params, *_Sm->CurrentScope);
  if (check_comps) { _CheckCompArgs(bindings); }
  if (check_types) { _CheckTypeArgs(bindings); }
}

auto GenericSolver::TakeArgs() -> Vec<Unique<GenericArgumentAst>> {
  // With nothing solved, the given arguments are the answer as they were given.
  if (_Trivial) { return std::move(_Given); }

  // The parameters in declaration order, which is what an instantiation's name is built from, then the names given
  // alongside them that are part of the solution.
  auto out = Vec<Unique<GenericArgumentAst>>();
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr and not entry->Emit) { continue; }
    if (not entry->TypeVals.IsEmpty()) {
      out.EmplaceBack(GenericArgumentAst::NewType(entry->Name, entry->TypeVals[0]));
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
  GenericArgumentGroupAst const &written, GenericParameterGroupAst const &p_group, Ast const &owner, ScopeManager &sm,
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
  NameCompArgs(comp_args, p_group.GetCompParams(), owner, sm, meta);
  NameTypeArgs(type_args, p_group.GetTypeParams(), owner, sm, meta);
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

auto spp::analyse::utils::generic_inference::EnforceGnConstraintsOfParams(
  TypeSymbol const &target, GenericParameterGroupAst const &params,
  ScopeManager &sm, meta::CompilerMetaData &meta) -> void {
  // The block's own parameters stand in as the arguments, so
  // the constraints are checked against what they allow. A
  // constraint that fails is reported where the target declares
  // it.
  const auto args = GenericArgumentGroupAst::FromParams(params);
  const auto bindings = type_resolution::BindArgs(*target.Type->GnParamGroup, args->GetAllArgs(), *sm.CurrentScope);
  CheckTypeArgConstraints(
    *target.Type->GnParamGroup, *args, target.LinkedScope != nullptr ? *target.LinkedScope : *sm.CurrentScope, bindings,
    sm, meta);
}
