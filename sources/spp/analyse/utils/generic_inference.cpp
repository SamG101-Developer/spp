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
import spp.asts.binary_expression_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.generate.common_types;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;

namespace spp::analyse::utils::generic_inference {
  namespace {
    /// Reject the first argument name no parameter has, reported against the first parameter (or the argument itself,
    /// when there are none).
    auto EnforceGnArgNamesKnown(
      Vec<GenericParameterAst*> const &params, Vec<GenericArgumentAst*> const &args, Scope *scope) -> void {
      for (auto const *arg : args) {
        const auto known = genex::any_of(params, [arg](auto const *p) { return *p->Name == *arg->Name; });
        if (known) { continue; }
        Raise<errors::SppArgumentNameInvalidError>(
          {scope}, ERR_ARGS(params.IsEmpty() ? static_cast<Ast const&>(*arg->Name) : *params[0], StrView("gn param"),
            *arg->Name, StrView("gn arg")));
      }
    }

    /// Name the positional arguments of one kind (type or comp) after the parameters of that kind no keyword argument
    /// names. A trailing variadic parameter takes the rest as a tuple: "f[U32, U32]" for "f[..Ts]" is "f[Ts=(U32,
    /// U32)]", and "f[1_u32, 1_u32]" for "f[cmp ..s]" is "f[s=(1_u32, 1_u32)]". A lone argument naming a pack ("A[Ts]"
    /// inside "g[..Ts]") already is that tuple, so is forwarded as it is.
    auto NameOfKind(
      Vec<Unique<GenericArgumentAst>> &args, Vec<GenericParameterAst*> const &params, const bool comp,
      Ast const &owner, ScopeManager &sm, meta::CompilerMetaData &meta) -> void {
      using generate::common_types::TupleType;
      const auto keyword_args = args
        | genex::views::ptr
        | genex::views::filter([](auto const *x) { return x->Name != nullptr; })
        | genex::to<Vec>();
      EnforceGnArgNamesKnown(params, keyword_args, sm.CurrentScope);

      auto unnamed = params
        | genex::views::filter([&](auto const *p) {
          return not genex::any_of(keyword_args, [p](auto const *a) { return *a->Name == *p->Name; });
        })
        | genex::to<Vec>();
      const auto is_variadic = genex::any_of(params, [](auto const *p) { return p->TokEllipsis != nullptr; });

      const auto _meta_guard = meta::MetaGuard(&meta);
      meta.TypeAnalysisTypeScope = nullptr;

      for (auto i = 0uz; i < args.Len(); ++i) {
        auto const &positional = args[i];
        if (positional->Name != nullptr) { continue; }
        RaiseIf<errors::SppGenericArgumentTooManyError>(
          unnamed.IsEmpty(), {sm.CurrentScope}, ERR_ARGS(params.IsEmpty() ? owner : *params[0], owner, *positional));
        auto named = MakeUnique<GenericArgumentAst>(unnamed.Front()->Name, nullptr, nullptr, nullptr);
        unnamed |= genex::actions::pop_front();

        if (unnamed.IsEmpty() and is_variadic) {
          const auto forwards = i + 1 == args.Len() and packs::NamesPack(*positional, *sm.CurrentScope);
          auto rest = args | genex::views::ptr | genex::views::drop(i) | genex::to<Vec>();
          // Collected into the pack's tuple, an argument naming a bound pack of its own kind is spread into that pack's
          // elements ("A[S32, Ts]" with "Ts" bound to "Tup[Bool, U8]" is "A[S32, Bool, U8]").
          if (comp and forwards) { named->CompVal = AstClone(positional->CompVal); }
          else if (comp) {
            auto values = Vec<Unique<ExpressionAst>>();
            for (auto const *x : rest) {
              if (const auto spread = packs::BoundPackValues(*x->CompVal, *sm.CurrentScope); spread.has_value()) {
                for (auto const *elem : *spread) { values.EmplaceBack(AstClone(elem)); }
              }
              else { values.EmplaceBack(AstClone(x->CompVal)); }
            }
            named->CompVal = MakeUnique<TupleLiteralAst>(nullptr, std::move(values), nullptr);
          }
          else {
            if (forwards) { named->TypeVal = AstClone(positional->TypeVal); }
            else {
              auto types = Vec<Shared<TypeAst>>();
              for (auto const *x : rest) {
                if (const auto spread = packs::BoundPackTypes(*x->TypeVal, *sm.CurrentScope); spread.has_value()) {
                  types.AppendRange(*spread);
                }
                else { types.EmplaceBack(AstCloneShared(x->TypeVal)); }
              }
              named->TypeVal = TupleType(positional->PosStart(), std::move(types));
            }
            if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
              named->TypeVal->Stage7_AnalyseSemantics(&sm, &meta);
            }
          }
          args[i] = std::move(named);
          args |= genex::actions::take(i + 1);
          break;
        }

        named->TypeVal = AstClone(positional->TypeVal);
        named->CompVal = AstClone(positional->CompVal);

        // Anything but a type, literal or name waits until stage 5 ends: a comp expression ("n + 1") resolves its
        // operator through the sup scopes of its operand's type, attached only then. An operator expression is never
        // analysed here - analysing it consumes it - but by the argument's own "AnalyseCompVal", which folds it or
        // checks a copy.
        auto const &val = *named->Value();
        const auto is_plain = val.template To<TypeAst>() != nullptr or val.template To<LiteralAst>() != nullptr
          or val.template To<IdentifierAst>() != nullptr;
        const auto is_operator = val.template To<BinaryExpressionAst>() != nullptr
          or val.template To<ParenthesisedExpressionAst>() != nullptr;
        if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations and (is_plain
          or (meta.CurrentStage >= meta::CompilerStage::kPreAnalyseSemantics and not is_operator))) {
          named->Value()->Stage7_AnalyseSemantics(&sm, &meta);
        }
        args[i] = std::move(named);
      }
    }
  }
}

SPP_MOD_BEGIN
/// Everything one generic parameter (or a name given alongside the parameters, like a pinned "Self") is offered.
struct spp::analyse::utils::generic_inference::GenericSolver::_Entry {
  /// The parameter, or @c nullptr for a given name that is not one.
  GenericParameterAst const *Param;
  Shared<TypeIdentifierAst> Name;
  Vec<Shared<TypeAst>> Types;
  Vec<ExpressionAst*> Comps;

  /// Given a type rather than inferred one, so inference neither offers it another nor rewrites it.
  bool GivenType = false;

  /// Bound to its default, which is written in the declaration's own terms and so may name the others.
  bool CompFromDefault = false;

  /// Whether a name that is not a parameter is part of the solution.
  bool Emit = false;

  /// Whether the comp value's type, or the bound type's constraints, have been inferred from yet: each is read once,
  /// once there is something to read, which is what makes the solve a worklist rather than a fixed pass order.
  bool CompValueRead = false;
  bool ConstraintsRead = false;

  SPP_ATTR_NODISCARD auto IsBound() const -> bool { return not Types.IsEmpty() or not Comps.IsEmpty(); }
  SPP_ATTR_NODISCARD auto IsTypeParam() const -> bool { return Param != nullptr and Param->CompType == nullptr; }
  SPP_ATTR_NODISCARD auto IsCompParam() const -> bool { return Param != nullptr and Param->CompType != nullptr; }
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
    if (arg->Name == nullptr or (arg->TypeVal == nullptr and arg->CompVal == nullptr)) {
      const auto err = "generic argument '" + arg->ToString() + "' is still positional where a binding is expected";
      Raise<SppInternalCompilerError>({_Sm->CurrentScope}, ERR_ARGS(*arg, err));
    }

    // A layer given earlier outranks this one: the first binding offered for a name is the one it keeps.
    const auto name = dynamic_shared_cast<TypeIdentifierAst>(arg->Name);
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
    if (arg->TypeVal != nullptr) {
      entry->Types.EmplaceBack(arg->TypeVal);
      entry->GivenType = true;
    }
    else { entry->Comps.EmplaceBack(arg->CompVal.get()); }
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

auto GenericSolver::KnownArgs() const -> Vec<GenericArgumentAst*> {
  return _Given | genex::views::ptr | genex::to<Vec>();
}

auto GenericSolver::_InferenceMap() const -> type_compare::GenericInferenceMap {
  auto out = type_compare::GenericInferenceMap();
  for (auto const &entry : _Entries) {
    // Lent, not owned: the solver keeps what its entries point into alive for as long as it lives.
    if (not entry->Types.IsEmpty()) { out.emplace(entry->Name, entry->Types[0]); }
    else if (not entry->Comps.IsEmpty()) { out.emplace(entry->Name, Shared<ExpressionAst>(Shared<void>(), entry->Comps[0])); }
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
      auto type = std::static_pointer_cast<TypeAst>(value);
      if (adjust_type) { type = adjust_type(*entry, std::move(type)); }
      if (type == nullptr) { continue; }
      entry->Types.EmplaceBack(std::move(type));
    }
    else {
      // Held for as long as the solver, as the match that found it may have built it or read it from a type made only
      // to match.
      auto comp = value;
      if (auto const *const as_type = value->To<TypeAst>(); as_type != nullptr) {
        comp = Shared<ExpressionAst>(IdentifierAst::FromType(*as_type));
      }
      entry->Comps.EmplaceBack(comp.get());
      _OwnedComps.EmplaceBack(std::move(comp));
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
  const auto source_sym = sm.CurrentScope->GetTypeSymbol(source->WithoutConvention().get());
  if (source_sym == nullptr or source_sym->LinkedScope == nullptr) { return inferred; }
  for (auto const *sup_scope : source_sym->LinkedScope->SupScopes()) {
    if (sup_scope->TySym == nullptr) { continue; }
    auto sup_inferred = type_compare::GenericInferenceMap();
    if (type_compare::RelaxedTypeEq(
      *sup_scope->TySym->FqName(), *target->WithoutConvention(), *sm.CurrentScope, *_OwnerScope, sup_inferred, true)) {
      return sup_inferred;
    }
  }
  return inferred;
}

auto GenericSolver::_InferTypesFromCompValues() -> bool {
  // Comp-to-type inference: a comp argument's value has a type, which binds the type parameters its parameter's
  // declared type names ("A[n=1_uz]" for "[T, cmp n: T]" binds "T" to "USize"). Only fills what nothing else has
  // bound, so a comp value of the wrong type is still reported against its parameter ("_CheckCompArgTypes"), not as a
  // conflict. An expression is read through its folded value, as its operator call is only analysed on a copy.
  // A literal's type is its suffix, so it can be read as soon as types resolve (a signature or attribute type is
  // written in stage 4). Anything else is only safe to ask once the sup scopes are attached, as "_CheckCompArgTypes"
  // does: resolving a name earlier caches its type without them.
  auto &sm = *_Sm;
  auto &meta = *_Meta;
  if (meta.CurrentStage < meta::CompilerStage::kResolveDeclarations) { return false; }
  const auto sups_attached = meta.CurrentStage >= meta::CompilerStage::kAttachSupScopes;
  auto newly_bound = false;
  for (auto const &entry : _Entries) {
    if (not entry->IsCompParam() or entry->Comps.IsEmpty() or entry->CompValueRead) { continue; }
    if (entry->Comps[0]->To<TypeAst>() != nullptr) { continue; }
    auto *const value = entry->Comps[0];

    // A variadic parameter's value is the tuple of what was given, each of which is of the declared type; a pack
    // forwarded to it is its elements when bound, and in a template its name is typed as one element already.
    auto sources = Vec<ExpressionAst*>();
    if (entry->Param->TokEllipsis == nullptr) { sources.EmplaceBack(value); }
    else if (value->To<TupleLiteralAst>() != nullptr) { sources = packs::PackElementValues(*value); }
    else if (auto spread = packs::BoundPackValues(*value, *sm.CurrentScope); spread.has_value()) {
      sources = std::move(*spread);
    }
    else if (packs::IsUnboundCompPackNamed(*value, *sm.CurrentScope)) { sources.EmplaceBack(value); }

    entry->CompValueRead = true;
    for (auto *source : sources) {
      if (not sups_attached and source->To<LiteralAst>() == nullptr) { continue; }
      const auto is_operator = source->To<BinaryExpressionAst>() != nullptr
        or source->To<ParenthesisedExpressionAst>() != nullptr;
      const auto folded = is_operator ? comp_generics::FoldCompExpr(*source, *sm.CurrentScope) : nullptr;
      if (is_operator and folded == nullptr) { continue; }
      // A copy: a literal's type is its symbol's cached "FqName" node, shared by every use of the type, and the
      // binding is analysed where it is used - early, it would cache a resolution made before the sups exist on all
      // of them.
      const auto source_type = AstCloneShared((folded != nullptr ? folded.get() : source)->InferType(&sm, &meta));
      newly_bound = _OfferAll(
        _Match(source_type, entry->Param->CompType), [](auto const &e) { return not e.Types.IsEmpty(); })
        or newly_bound;
    }
  }
  return newly_bound;
}

auto GenericSolver::_InferFromConstraints() -> bool {
  // Constraint-based inference: "[U, F: FunRef[(), U]]" infers "U" from the return type of whichever of the type
  // bound to "F" and its super classes is the "FunRef".
  using errors::SppGenericConstraintError;
  auto &sm = *_Sm;
  auto newly_bound = false;
  const auto type_params = _Params->GetTypeParams();
  for (auto const &entry : _Entries) {
    if (not entry->IsTypeParam() or entry->Param->Constraints->Constraints.IsEmpty()) { continue; }
    if (entry->Types.IsEmpty() or entry->ConstraintsRead) { continue; }
    entry->ConstraintsRead = true;
    auto const inferred_type = entry->Types[0];

    // The candidates are the type and its super classes, each with the scope its name resolves in: a sup type's
    // name can hold a "Self" (as in "S32 ext Ord[Rhs=Self]"), which only has a symbol inside that sup scope.
    const auto concrete_sym = sm.CurrentScope->GetTypeSymbol(inferred_type.get());
    auto candidates = Vec<Pair<Shared<TypeAst>, Scope const*>>{};
    if (concrete_sym != nullptr and not concrete_sym->IsTypeGeneric()) {
      candidates.EmplaceBack(concrete_sym->FqName(), sm.CurrentScope);
      if (concrete_sym->LinkedScope != nullptr) {
        candidates.AppendRange(type_members::SuperClassNames(concrete_sym->LinkedScope->SupScopes()));
      }
    }

    for (auto const &constraint : entry->Param->Constraints->Constraints) {
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
      // authoritative "EnforceGenericConstraintsAllArgs", so a "RelaxedTypeEq" false negative rejects nothing.
      if (not candidates.IsEmpty() and not matched) {
        const auto constraint_drives_inference = genex::any_of(
          type_params, [&](auto const *other) { return constraint->ContainsGenerics(*other); });
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
    pinned != nullptr and not pinned->Types.IsEmpty()) { return pinned->Types[0]; }
  if (not as_value) { return nullptr; }
  auto self_type = _OwnerScope->GetEnclosingSelfType(*_Meta);
  return self_type != nullptr and not self_type->IsSelfType() ? self_type : nullptr;
}

auto GenericSolver::_ApplyTypeDefaults() -> void {
  // An optional type parameter nothing has bound takes its default. The default is read where it is used, so it is
  // recording what it means where it is written, on a copy of its own - "Self" as "_SelfForDefault" decides.
  auto const &meta = *_Meta;
  for (auto const &entry : _Entries) {
    if (not entry->IsTypeParam() or entry->Param->TypeDefault == nullptr or entry->IsBound()) { continue; }
    auto def_type = AstCloneShared(entry->Param->TypeDefault);
    if (meta.CurrentStage >= meta::CompilerStage::kGenTopLvlAliases) {
      if (not def_type->IsSelfType()) { type_resolution::RecordWrittenParts(*def_type, *_OwnerScope); }
      else if (const auto self_type = _SelfForDefault(false); self_type != nullptr) {
        def_type = self_type->WithConvention(AstClone(def_type->GetConvention()));
      }
    }
    entry->Types.EmplaceBack(std::move(def_type));
  }
}

auto GenericSolver::_ApplyCompDefaults() -> void {
  // An optional comp parameter nothing has bound takes its default. A default is written in the callee's own terms,
  // like "Self::mo_seq_cst" or "n + 1", and read back at the use site, where those names mean nothing, so it is
  // translated against everything bound so far - "Self" last, so a "Self" the use pinned outranks the owner's.
  auto &sm = *_Sm;
  auto &meta = *_Meta;
  auto self_arg = Unique<GenericArgumentAst>(nullptr);
  auto self_looked_up = false;
  for (auto const &entry : _Entries) {
    if (not entry->IsCompParam() or entry->Param->CompDefault == nullptr or entry->IsBound()) { continue; }
    auto *default_val = entry->Param->CompDefault.get();

    // Nothing to translate against before the aliases exist. "Self" ("_SelfForDefault") is looked up here rather than
    // up front because it is a symbol lookup, and most uses have no optional comp parameter to make it for.
    if (meta.CurrentStage >= meta::CompilerStage::kGenTopLvlAliases) {
      if (not self_looked_up) {
        self_looked_up = true;
        if (auto self_type = _SelfForDefault(true); self_type != nullptr) {
          self_arg = GenericArgumentAst::NewType(generate::common_types::SelfType(0), std::move(self_type));
        }
      }
      const auto known_group = GenericArgumentGroupAst::FromMap(_InferenceMap());
      auto default_args = known_group->GetAllArgs();
      if (self_arg != nullptr) { default_args.EmplaceBack(self_arg.get()); }

      // A translated default that is closed is its value ("n + 1_uz" with "n" bound to "3_uz" is "4_uz"), which needs no
      // analysis. One still naming a generic has to be analysed: an operator caches the symbol its left-hand side
      // resolved to, and code generation reads that back, so an unanalysed one reaches stage 11 looking like a
      // namespace access. Both translate from the written default: the parameter's own has been analysed, which
      // desugars its operators ("GenericParameterAst::WrittenCompDefault").
      if (not default_args.IsEmpty()) {
        auto substituted = entry->Param->WrittenCompDefault->SubstituteGenericsExpr(default_args);
        if (auto folded = comp_generics::FoldCompExpr(*substituted, *sm.CurrentScope); folded != nullptr) {
          substituted = std::move(folded);
        }
        else { substituted->Stage7_AnalyseSemantics(&sm, &meta); }
        default_val = substituted.get();
        _OwnedComps.EmplaceBack(std::move(substituted));
      }
    }
    entry->Comps.EmplaceBack(default_val);
    entry->CompFromDefault = true;
  }
}

auto GenericSolver::_EnforceNoConflicts() const -> void {
  // A parameter reached through several arguments, or through an argument and a constraint, has to be offered the
  // same thing by each. Types must be the same type ("TypeEq" is identity, so the binding cannot depend on which
  // argument came first). Comp values must be the same value, which is their identity ("CompExprIdentity") rather
  // than their spelling: "0x2_uz" and "2_uz" are one value.
  using errors::SppGenericParameterConflictError;
  using type_compare::TypeEq;
  auto const &scope = *_Sm->CurrentScope;
  const auto identity_of = [&scope](ExpressionAst const &comp) {
    auto out = Str();
    comp_generics::CompExprIdentity(comp, scope, out);
    return out;
  };
  for (auto const &entry : _Entries) {
    auto const &types = entry->Types;
    for (auto i = 1uz; i < types.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        not TypeEq(*types[i], *types[0], scope, scope),
        {_Sm->CurrentScope}, ERR_ARGS(*entry->Name, *types[0], *types[i]));
    }
    auto const &comps = entry->Comps;
    const auto first_comp = comps.IsEmpty() ? Str() : identity_of(*comps[0]);
    for (auto i = 1uz; i < comps.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        identity_of(*comps[i]) != first_comp, {_Sm->CurrentScope}, ERR_ARGS(*entry->Name, *comps[0], *comps[i]));
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
      const auto bound = want_type ? not entry->Types.IsEmpty() : not entry->Comps.IsEmpty();
      RaiseIf<SppGenericParameterNotInferredError>(
        not bound, {_Sm->CurrentScope, _OwnerScope}, ERR_ARGS(*entry->Name, owner));
    }
  }
}

auto GenericSolver::_CrossSubstitute() -> void {
  // Substitute everything bound into each binding the solver made, each skipping itself: an inferred type, so
  // "Vec[T, A=Alloc[T]]" receives the actual "T", and a comp default, which is written in the declaration's own terms,
  // so "m = n + 1_uz" receives the "n" bound after it. A default that names none of the others is left as it is,
  // rather than analysed again.
  for (auto const &entry : _Entries) {
    const auto rewrites_type = not entry->Types.IsEmpty() and not entry->GivenType;
    const auto rewrites_comp = entry->CompFromDefault and entry->Comps.Len() == 1
      and entry->Comps[0]->To<LiteralAst>() == nullptr;
    if (not rewrites_type and not rewrites_comp) { continue; }
    auto others = _InferenceMap();
    others.erase(entry->Name);
    const auto other_group = GenericArgumentGroupAst::FromMap(others);
    if (rewrites_type) {
      const auto bindings = type_resolution::BindArgs(*_Params, other_group->GetAllArgs(), *_Sm->CurrentScope);
      auto type = type_resolution::ReadWith(*entry->Types[0], *_Sm->CurrentScope, bindings, *_Sm->CurrentScope);
      type->Stage7_AnalyseSemantics(_Sm, _Meta);
      entry->Types.Clear();
      entry->Types.EmplaceBack(std::move(type));
      continue;
    }
    // Re-derived from the written default, with everything now bound. Taken only when that closes it to a value.
    auto comp = entry->Param->WrittenCompDefault->SubstituteGenericsExpr(other_group->GetAllArgs());
    auto folded = comp_generics::FoldCompExpr(*comp, *_Sm->CurrentScope);
    if (folded == nullptr) { continue; }
    entry->Comps.Clear();
    entry->Comps.EmplaceBack(folded.get());
    _OwnedComps.EmplaceBack(std::move(folded));
  }
}

auto GenericSolver::_CheckCompArgTypes() const -> void {
  // Type-check each comp argument against its parameter's type, with every binding substituted in. The value was
  // given at the use site, so its type is resolved there - and so is the parameter's type, which the substitution has
  // written in the use site's terms (the arguments given there).
  using errors::SppTypeMismatchError;
  using type_compare::TypeEq;
  auto &sm = *_Sm;
  auto &meta = *_Meta;
  const auto all_group = GenericArgumentGroupAst::FromMap(_InferenceMap());
  const auto bindings = type_resolution::BindArgs(*_Params, all_group->GetAllArgs(), *sm.CurrentScope);
  for (auto const &entry : _Entries) {
    if (not entry->IsCompParam() or entry->Comps.IsEmpty()) { continue; }
    auto *const value = entry->Comps[0];
    const auto raw_a_type = value->InferType(&sm, &meta);
    const auto p_type = type_resolution::ReadWith(*entry->Param->CompType, *_OwnerScope, bindings, *sm.CurrentScope);

    // A pack's value is a tuple, each element of which is of the declared type.
    if (entry->Param->TokEllipsis != nullptr) {
      auto const *const a_sym = sm.CurrentScope->GetTypeSymbol(raw_a_type.get());
      const auto a_type = a_sym != nullptr ? a_sym->FqName() : raw_a_type;
      for (auto const &inner : packs::PackElementTypes(*a_type)) {
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
      _Match(eq->Source, eq->Target), [](auto const &e) { return e.GivenType; },
      [is_pack_slot](auto const &e, Shared<TypeAst> type) -> Shared<TypeAst> {
        if (not is_pack_slot or e.Param->TokEllipsis != nullptr) { return type; }
        const auto elements = packs::PackElementTypes(*type);
        return elements.IsEmpty() ? nullptr : elements[0];
      });
  }

  // Comp values' types and the constraints feed each other, so both run until neither binds anything new.
  while (true) {
    const auto from_comps = _InferTypesFromCompValues();
    const auto from_constraints = _InferFromConstraints();
    if (not from_comps and not from_constraints) { break; }
  }

  _ApplyTypeDefaults();
  _ApplyCompDefaults();
  _EnforceNoConflicts();
  _EnforceAllInferred(owner);
  _CrossSubstitute();
  if (_Meta->CurrentStage >= meta::CompilerStage::kAttachSupScopes) { _CheckCompArgTypes(); }
}

auto GenericSolver::TakeArgs() -> Vec<Unique<GenericArgumentAst>> {
  // With nothing solved, the given arguments are the answer as they were given.
  if (_Trivial) { return std::move(_Given); }

  // The parameters in declaration order, which is what an instantiation's name is built from, then the names given
  // alongside them that are part of the solution.
  auto out = Vec<Unique<GenericArgumentAst>>();
  for (auto const &entry : _Entries) {
    if (entry->Param == nullptr and not entry->Emit) { continue; }
    if (not entry->Types.IsEmpty()) {
      out.EmplaceBack(GenericArgumentAst::NewType(entry->Name, entry->Types[0]));
    }
    else if (not entry->Comps.IsEmpty()) {
      out.EmplaceBack(GenericArgumentAst::NewComp(AstClone(entry->Name), AstClone(entry->Comps[0])));
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
    (arg->CompVal != nullptr ? comp_args : type_args).EmplaceBack(std::move(arg));
  }
  NameOfKind(comp_args, p_group.GetCompParams(), true, owner, sm, meta);
  NameOfKind(type_args, p_group.GetTypeParams(), false, owner, sm, meta);
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

auto spp::analyse::utils::generic_inference::EnforceGenericConstraintsOfParams(
  TypeSymbol const &target, GenericParameterGroupAst const &params,
  ScopeManager &sm, meta::CompilerMetaData &meta) -> void {
  // The block's own parameters stand in as the arguments, so
  // the constraints are checked against what they allow. A
  // constraint that fails is reported where the target declares
  // it.
  EnforceGenericConstraintsAllArgs(
    *target.Type->GnParamGroup, *GenericArgumentGroupAst::FromParams(params),
    *sm.CurrentScope, sm, meta, target.LinkedScope);
}

auto spp::analyse::utils::generic_inference::EnforceGenericConstraintsAllArgs(
  GenericParameterGroupAst const &p_group, GenericArgumentGroupAst const &a_group,
  Scope const &owner_scope, ScopeManager &sm, meta::CompilerMetaData &meta,
  Scope const *const decl_scope) -> void {
  using errors::SppGenericConstraintError;

  // Extract important information.
  auto p_names = p_group.GetTypeParams()
    | genex::views::transform([](auto &&x) { return dynamic_shared_cast<TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  auto p_con_groups = p_group.GetTypeParams()
    | genex::views::transform([](auto &&x) { return x->Constraints->Constraints; })
    | genex::to<Vec>();
  const auto type_args = a_group.GetTypeArgs();
  auto const &written_scope = decl_scope != nullptr ? *decl_scope : owner_scope;
  const auto bindings = type_resolution::BindArgs(p_group, a_group.GetAllArgs(), *sm.CurrentScope);

  // Check that each argument satisfies its constraints.
  for (auto [i, p_name] : p_names | genex::views::enumerate) {
    auto matching = type_args
      | genex::views::filter([&](auto const *a) { return a->ViewName() == p_name->Name; })
      | genex::to<Vec>();
    if (matching.IsEmpty()) { continue; }

    const auto arg_sym = sm.CurrentScope->GetTypeSymbol(matching[0]->TypeVal.get());
    if (arg_sym == nullptr) { continue; }
    const auto con_scope = arg_sym->LinkedScope != nullptr
      ? arg_sym->LinkedScope
      : sm.CurrentScope;
    auto con_sm = ScopeManager(sm.GlobalScope, con_scope);

    // This parameter's constraints, with the arguments bound.
    auto p_cons = Vec<Shared<TypeAst>>();
    for (auto const &p_con : p_con_groups[i]) {
      const auto sub = type_resolution::ReadWith(*p_con, written_scope, bindings, *sm.CurrentScope);
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
    const auto targets = p_group.GetTypeParams()[i]->TokEllipsis != nullptr
      ? packs::PackElementTypes(*matching[0]->TypeVal)
      : Vec<Shared<TypeAst>>{matching[0]->TypeVal};

    // Raise an error if any constraint of this argument is
    // not satisfied.
    for (auto const &target : targets) {
      // Failsafe for some $ClosureTypes, which resolve to nothing.
      auto const *const concrete_sym = sm.CurrentScope->ResolveTypeSymbol(target.get());
      const auto unsatisfied = concrete_sym != nullptr
        ? type_compare::UnmetConstraint(p_cons, *concrete_sym, target->IsSelfType(), owner_scope, *sm.CurrentScope)
        : nullptr;
      RaiseIf<SppGenericConstraintError>(
        unsatisfied != nullptr, {decl_scope != nullptr ? decl_scope : &owner_scope, sm.CurrentScope},
        ERR_ARGS(*unsatisfied, *target));
    }
  }
}

