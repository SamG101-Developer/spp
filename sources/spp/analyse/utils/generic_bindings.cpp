module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

#define MAKE_VARIADIC_COMP_ARGS(What)                                              \
  What                                                                             \
    | genex::views::move                                                           \
    | genex::views::drop(i)                                                        \
    | genex::views::transform([](auto &&x) { return asts::AstClone(x->CompVal); }) \
    | genex::to<Vec>();

#define MAKE_VARIADIC_TYPE_ARGS(What)                                                    \
  What                                                                                   \
    | genex::views::move                                                                 \
    | genex::views::drop(i)                                                              \
    | genex::views::transform([](auto &&x) { return asts::AstCloneShared(x->TypeVal); }) \
    | genex::to<Vec>();

module spp.analyse.utils.generic_bindings;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
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
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import genex;

namespace spp::analyse::utils::generic_bindings {
  namespace {
    /// Check that we are never left with uninferred generic
    /// arguments. Every generic parameter must have an inferable
    /// argument.
    auto EnforceNoUninferredGnArgs(
      Vec<Shared<TypeIdentifierAst>> const &p_names,
      Vec<Shared<TypeIdentifierAst>> const &i_names,
      Scope const &owner_scope, Shared<Ast> const &owner,
      ScopeManager &sm) -> void {
      using errors::SppGenericParameterNotInferredError;

      // Check for uninferred arguments.
      const auto uninferred_params = p_names
        | genex::views::not_in(i_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

      RaiseIf<SppGenericParameterNotInferredError>(
        not uninferred_params.IsEmpty(), {sm.CurrentScope, &owner_scope},
        ERR_ARGS(*uninferred_params[0], *owner));
    }

    /// Reject a keyword argument whose name is not one of the parameters.
    auto EnforceNoInvalidGnArgs(
      Vec<GenericParameterAst*> const &params,
      Vec<GenericArgumentAst*> const &named_args,
      ScopeManager &sm)
      -> void {
      // Shortcut; when there are no named arguments then there
      // is nothing to enforce for comparisons between values for
      // the same binding name.
      using errors::SppArgumentNameInvalidError;
      if (named_args.IsEmpty()) { return; }

      // Check for invalid argument names against parameter names.
      // An "invalid" generic argument name is for example "T=S32"
      // where "T" doesn't appear in the generic parameter list.
      for (auto &&a : named_args) {
        auto found = false;
        for (auto &&p : params) {
          found = *a->Name == *p->Name;
          if (found) { break; }
        }

        // No match was found, so raise the invalid argument error
        // in context of generic arguments vs generic parameters.
        RaiseIf<SppArgumentNameInvalidError>(
          not found, {sm.CurrentScope},
          ERR_ARGS(*params[0], "gn param", *a, "gn arg"));
      }
    }

    /// Turn every positional argument of one kind into the
    /// keyword argument it stands for, by pairing it with
    /// the next parameter that has not already been named.
    /// A trailing variadic parameter consumes whatever is
    /// left, as a tuple.
    auto NameGnArgsImpl(
      GenericArgumentGroupAst &a_group, Vec<GenericParameterAst*> const &params,
      const bool comp, Ast const &owner, ScopeManager &sm,
      meta::CompilerMetaData &meta) -> void {
      using generate::common_types::TupleType;
      using errors::SppGenericArgumentTooManyError;

      // Validate the named arguments against the parameters. This
      // weeds out anything that'd mess up the naming scheme below.
      EnforceNoInvalidGnArgs(
        params, a_group.GetKeywordArgs(), sm);

      // Get the names of the keyword arguments, all of the one kind
      // being named.
      auto a_names = a_group.GetKeywordArgs()
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();

      // Get the names of the leftover parameters. These are the
      // parameters that don't already have args bound to them.
      auto p_names = params
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

      // Check for the existence of a variadic parameter. This
      // is needed to activate the variadic arg swallowing steps.
      const auto is_variadic = genex::any_of(params, [](auto *p) {
        return p->TokEllipsis != nullptr;
      });

      const auto _meta_guard = meta::MetaGuard(&meta);
      meta.TypeAnalysisTypeScope = nullptr;

      for (auto [i, positional_arg] : a_group.GetPositionalArgs() | genex::views::enumerate) {
        // If we sl have arguments to name, but there are no
        // parameters left, then there were too many arguments
        // provided.
        RaiseIf<SppGenericArgumentTooManyError>(
          p_names.IsEmpty(), {sm.CurrentScope},
          ERR_ARGS(params.IsEmpty() ? owner : *params[0], owner, *positional_arg));

        // Create the keyword argument from the positional argument
        // (no value is set yet).
        auto kw_arg = MakeUnique<GenericArgumentAst>(
          p_names.Front(), nullptr, nullptr, nullptr);
        p_names |= genex::actions::pop_front();

        // The variadic parameter requires a tuple of the remaining
        // arguments.
        if (p_names.IsEmpty() and is_variadic) {
          // Variadic check: map comp arguments "func[1_u32, 1_u32]"
          // for "func[cmp ..s]" to "func[ts = (1_u32, 1_u32)]". Uses
          // the tuple literal ast.
          if (comp) {
            auto elems = MAKE_VARIADIC_COMP_ARGS(a_group.Args);
            auto tuple = MakeUnique<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
            kw_arg->CompVal = std::move(tuple);
          }

          // Variadic check: map type arguments "func[U32, U32]"
          // for "func[..Ts]" to "func[Ts = (U32, U32)]". Uses the
          // tuple type + analysis for generic implementation.
          else {
            // A lone argument naming a pack ("A[Ts]" inside
            // "g[..Ts]") is already the tuple, so is forwarded.
            if (i + 1 == a_group.Args.Len()) {
              const auto &val = positional_arg->TypeVal;
              const auto sym = sm.CurrentScope->GetTypeSymbol(val->WithoutGenerics().get(), false);
              if (sym != nullptr and sym->IsTypeGeneric() and sym->IsVariadic) {
                kw_arg->TypeVal = AstClone(val);
                if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
                  kw_arg->TypeVal->Stage7_AnalyseSemantics(&sm, &meta);
                }
                a_group.Args[i] = std::move(kw_arg);
                break;
              }
            }
            auto elems = MAKE_VARIADIC_TYPE_ARGS(a_group.Args);
            auto tuple = TupleType(positional_arg->PosStart(), std::move(elems));
            if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
              tuple->Stage7_AnalyseSemantics(&sm, &meta);
            }
            kw_arg->TypeVal = std::move(tuple);
          }

          a_group.Args[i] = std::move(kw_arg);
          a_group.Args |= genex::actions::take(i + 1);
          break;
        }

        // Otherwise, attach the single argument convention and
        // value.
        kw_arg->TypeVal = AstClone(positional_arg->TypeVal);
        kw_arg->CompVal = AstClone(positional_arg->CompVal);

        // Anything but a type, literal or name waits until stage
        // 5 ends: a comp expression ("n + 1") resolves its operator
        // through the sup scopes of its operand's type, attached
        // only then. An operator expression is never analysed here
        // - analysing it consumes it - but by the argument's own
        // "AnalyseCompVal", which folds it or checks a copy.
        auto const &val = *kw_arg->Value();
        const auto is_plain = val.template To<TypeAst>() != nullptr or val.template To<LiteralAst>() != nullptr
          or val.template To<IdentifierAst>() != nullptr;
        const auto is_operator = val.template To<BinaryExpressionAst>() != nullptr
          or val.template To<ParenthesisedExpressionAst>() != nullptr;
        if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations and (is_plain
          or (meta.CurrentStage >= meta::CompilerStage::kPreAnalyseSemantics and not is_operator))) {
          kw_arg->Value()->Stage7_AnalyseSemantics(&sm, &meta);
        }
        a_group.Args[i] = std::move(kw_arg);
      }
    }

    auto CollectDirectInferences(
      Shared<TypeAst> const &source_type,
      Shared<TypeAst> const &target_type,
      Shared<IdentifierAst> const &target_name,
      Vec<Shared<TypeIdentifierAst>> const &type_a_names,
      Vec<Shared<TypeIdentifierAst>> const &type_p_names,
      Vec<Shared<TypeIdentifierAst>> const &variadic_type_p_names,
      Vec<Shared<TypeIdentifierAst>> const &comp_p_names,
      Shared<IdentifierAst> const &variadic_fn_param_name,
      Scope const &owner_scope,
      ScopeManager const &sm,
      GenericBindingSet &bindings)
      -> void {
      //
      auto temp_gs = type_compare::GenericInferenceMap();
      type_compare::RelaxedTypeEq(
        *source_type->WithoutConvention(),
        *target_type->WithoutConvention(),
        *sm.CurrentScope, owner_scope, temp_gs, true);

      const auto is_variadic_param_slot =
        variadic_fn_param_name != nullptr
        and target_name != nullptr
        and *target_name == *variadic_fn_param_name;

      for (auto const &[inferred_name, inferred_val] : temp_gs) {
        if (genex::contains(type_a_names, *inferred_name, genex::meta::deref)) { continue; }

        auto *typed = inferred_val->To<TypeAst>();
        const auto declared_type = genex::contains(type_p_names, *inferred_name, genex::meta::deref);
        const auto declared_comp = genex::contains(comp_p_names, *inferred_name, genex::meta::deref);

        if (declared_type) {
          if (typed == nullptr) { continue; }
          auto shared = typed->shared_from_this();
          if (is_variadic_param_slot and not
            genex::contains(variadic_type_p_names, *inferred_name, genex::meta::deref)) {
            auto const &inner = shared->LastTypePart()->GnArgGroup->Args[0];
            shared = inner->TypeVal;
          }
          bindings.Add(inferred_name, std::move(shared));
        }
        else if (declared_comp) {
          bindings.Add(inferred_name, inferred_val);
        }
      }
    }

    /// Constraint-based inference: "[U, F: FunRef[(), U]]"
    /// infers "U" from the return type of whichever of the
    /// type bound to "F" and its super classes is the "FunRef".
    auto InferFromConstraints(
      Vec<GenericParameterAst*> const &type_params,
      Vec<Shared<TypeIdentifierAst>> const &type_p_names,
      Vec<Shared<TypeIdentifierAst>> const &comp_p_names,
      Scope const &owner_scope,
      ScopeManager &sm,
      GenericBindingSet &bindings) -> void {
      using errors::SppGenericConstraintError;
      for (const auto param : type_params) {
        if (param->Constraints->Constraints.IsEmpty()) { continue; }
        const auto cast_name = dynamic_shared_cast<TypeIdentifierAst>(param->Name);

        // Find the inferred concrete type for this param.
        const auto inferred_type = bindings.Resolved(cast_name.get()).Type;
        if (inferred_type == nullptr) { continue; }

        // Build the candidate list from the type and all
        // subtypes. Each candidate carries the scope its
        // name resolves in: a sup type's name can hold a
        // "Self" (as in "S32 ext Ord[Rhs=Self]"), which
        // only has a symbol inside that sup scope, not in
        // the calling scope.
        const auto concrete_sym = sm.CurrentScope->GetTypeSymbol(inferred_type.get());
        auto candidates = Vec<Pair<Shared<TypeAst>, Scope const*>>{};
        if (concrete_sym != nullptr and not concrete_sym->IsTypeGeneric()) {
          candidates.EmplaceBack(concrete_sym->FqName(), sm.CurrentScope);
          if (concrete_sym->LinkedScope != nullptr) {
            for (auto const *sup_scope : concrete_sym->LinkedScope->SupScopes()) {
              if (AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
              candidates.EmplaceBack(sup_scope->TySym->FqName(), sup_scope);
            }
          }
        }

        for (auto const &constraint : param->Constraints->Constraints) {
          // Try each candidate in order and stop at the first
          // match.
          auto temp_gs = type_compare::GenericInferenceMap();
          auto matched = false;
          for (auto const &[candidate, candidate_scope] : candidates) {
            temp_gs.clear();
            if (type_compare::RelaxedTypeEq(
              *candidate->WithoutConvention(),
              *constraint->WithoutConvention(),
              *candidate_scope, owner_scope, temp_gs, true, false)) {
              matched = true;
              break;
            }
          }

          // Niche constraint error that needs to be added here, otherwise we get misleading errors from
          // fallthrough. The parameter had a concrete inferred type (so candidates were available), but none of
          // them satisfied this constraint. If the constraint is what other generic parameters are inferred
          // through (eg the "U" in "P: FunMov[(T,), Opt[U]]"), then the supplied argument simply does not fit the
          // constraint's shape. Surface that as a constraint error now, rather than letting the dependent
          // parameter fall through and fail later with a misleading "generic parameter not inferred" error that
          // hides the real cause. Constraints that reference no other generics (eg "P: Copy") are left to the
          // authoritative TypeEq-based EnforceGenericConstraintsAllArgs check, to avoid any RelaxedTypeEq
          // false-negative rejecting a valid call here. TODO
          if (not candidates.IsEmpty() and not matched) {
            const auto constraint_drives_inference = genex::any_of(
              type_params, [&](auto const *other) { return constraint->ContainsGenerics(*other); });
            RaiseIf<SppGenericConstraintError>(
              constraint_drives_inference,
              {&owner_scope, sm.CurrentScope}, ERR_ARGS(*constraint, *inferred_type));
          }

          for (auto const &[inferred_name, inferred_val] : temp_gs) {
            // Skip names already bound to a type, to avoid
            // duplicate entries.
            if (bindings.Contains(inferred_name.get())) { continue; }

            if (genex::contains(type_p_names, *inferred_name, genex::meta::deref)) {
              auto *typed = inferred_val->To<TypeAst>();
              if (typed == nullptr) { continue; }
              bindings.Add(inferred_name, typed->shared_from_this());
            }
            else if (genex::contains(comp_p_names, *inferred_name, genex::meta::deref)) {
              bindings.Add(inferred_name, inferred_val);
            }
          }
        }
      }
    }

    /// Bind each optional type parameter nothing else has bound
    /// to its default. The default is read where it is used, so
    /// it is stamped with what it means where it is written, on
    /// a copy of its own, as the binding is analysed where it is
    /// used. "Self" is the one name never stamped by meaning, so
    /// a default of "Self" is named from the scope declaring it
    /// instead.
    auto ApplyTypeDefaults(
      Vec<GenericParameterAst*> const &type_params, Scope const &owner_scope,
      meta::CompilerMetaData const &meta, GenericBindingSet &bindings) -> void {
      // Go through each type parameter, looking for a default
      // value on it.
      for (const auto opt_param : type_params) {
        if (opt_param->TypeDefault == nullptr) { continue; }

        // Get the name of the optional parameter. Check if it
        // has been bound yet or not. We only want to consider
        // the unbound once.
        const auto cast_name = dynamic_shared_cast<TypeIdentifierAst>(opt_param->Name);
        if (bindings.Contains(cast_name.get())) { continue; }
        auto def_type = AstCloneShared(opt_param->TypeDefault);

        // If possible, set the default's type to the "Self"
        // symbol's lookup. Otherwise, stamp the default type
        // based on the owner scope.
        if (meta.CurrentStage >= meta::CompilerStage::kGenTopLvlAliases) {
          if (const auto self_sym = def_type->IsSelfType() ? owner_scope.GetTypeSymbol(def_type.get()) : nullptr;
            self_sym != nullptr) {
            def_type = self_sym->FqName()->WithConvention(AstClone(def_type->GetConvention()));
          }
          else {
            type_utils::StampWrittenParts(*def_type, owner_scope);
          }
        }

        // Add the binding in with the genuine default type, for
        // the owner scope.
        bindings.Add(cast_name, std::move(def_type));
      }
    }

    /// Bind each optional generic comp parameter that hasn't
    /// been given a value. A default is written in the callee's
    /// own terms, like "Self::mo_seq_cst" or "n + 1", and read
    /// back at the call site, where those names mean nothing.
    /// So it must be translated in the same way as the types
    /// are, against everything bound so far. Must return the
    /// vector in order to keep unique pointers alive where the
    /// bindings point to the raw pointers.
    auto ApplyCompDefaults(
      Vec<GenericParameterAst*> const &comp_params, Scope const &owner_scope,
      ScopeManager &sm, meta::CompilerMetaData &meta, GenericBindingSet &bindings)
      -> Vec<Shared<ExpressionAst>> {
      auto substituted_comp_defaults = Vec<Shared<ExpressionAst>>();
      auto owner_self_arg = Unique<GenericArgumentAst>(nullptr);
      auto owner_self_looked_up = false;

      // Go through each comp parameter, looking for a default
      // value on it.
      for (const auto opt_param : comp_params) {
        if (opt_param->CompDefault == nullptr) { continue; }

        // Get the name of the optional parameter. Check if it
        // has been bound yet or not. We only want to consider
        // the unbound once.
        const auto cast_name = dynamic_shared_cast<TypeIdentifierAst>(opt_param->Name);
        if (bindings.Contains(cast_name.get())) { continue; }
        const auto default_val = opt_param->CompDefault.get();

        // Nothing to translate against before the aliases exist.
        // The lookup for "Self" is made here rather than up front
        // because it is a symbol lookup, and most calls have no
        // optional comp parameter to make it for.
        if (meta.CurrentStage >= meta::CompilerStage::kGenTopLvlAliases) {
          if (not owner_self_looked_up) {
            owner_self_looked_up = true;
            if (auto self_type = owner_scope.GetEnclosingSelfType(meta);
              self_type != nullptr and not self_type->IsSelfType()) {
              owner_self_arg = GenericArgumentAst::NewType(
                generate::common_types::SelfType(0), std::move(self_type));
            }
          }

          // What is already bound comes first, so a "Self" the call
          // itself pinned outranks the one the owner declares.
          const auto known_group = GenericArgumentGroupAst::FromMap(bindings.ToInferenceMap());
          auto default_args = known_group->GetAllArgs();
          if (owner_self_arg != nullptr) { default_args.EmplaceBack(owner_self_arg.get()); }

          if (not default_args.IsEmpty()) {
            // The rewritten clone has to be analysed: an operator
            // caches the symbol its left-hand side resolved to, and
            // code generation reads that back, so an unanalysed one
            // reaches stage 11 looking like a namespace access.
            auto substituted = default_val->SubstituteGenericsExpr(default_args);
            substituted->Stage7_AnalyseSemantics(&sm, &meta);
            default_val = substituted.get();
            substituted_comp_defaults.EmplaceBack(std::move(substituted));
          }
        }

        bindings.Add(cast_name, default_val);
      }
      return substituted_comp_defaults;
    }

    /// Substitute everything bound into each inferred type
    /// binding, each skipping itself. This allows for types
    /// like "Vec[T, A=Alloc[T]]" to receive the actual "T"
    /// value, and also type/comp cross-references. Todo:
    /// does this handle comp injecting? Looks to only be type.
    auto CrossSubstituteBindings(
      Vec<Shared<TypeIdentifierAst>> const &type_a_names, ScopeManager &sm,
      meta::CompilerMetaData &meta, GenericBindingSet &bindings) -> void {
      // Iterate through the type names, looking for cross
      // application possibilities.
      for (auto const &type_name : bindings.TypeNames()) {
        if (genex::contains(type_a_names, *type_name, genex::meta::deref)) { continue; }

        // Substitute through everything else that is known,
        // skipping this name to avoid cycles.
        auto other_unified = bindings.ToInferenceMap();
        other_unified.erase(type_name);
        const auto other_group = GenericArgumentGroupAst::FromMap(other_unified);

        auto t = bindings.Resolved(type_name.get()).Type->SubstituteGenerics(other_group->GetAllArgs());
        t->Stage7_AnalyseSemantics(&sm, &meta);
        bindings.Replace(type_name.get(), std::move(t));
      }
    }

    /// Type-check each comp argument against its parameter's
    /// type, with every binding substituted in. Run once the
    /// arguments are back on the group.
    auto CheckCompArgTypes(
      Vec<GenericParameterAst*> const &comp_params, Scope const &owner_scope,
      ScopeManager &sm, meta::CompilerMetaData &meta, GenericBindingSet const &bindings) -> void {
      using errors::SppTypeMismatchError;
      using type_compare::TypeEq;
      const auto all_final_group = GenericArgumentGroupAst::FromMap(bindings.ToInferenceMap());
      const auto all_final_args = all_final_group->GetAllArgs();

      // Walk the parameters, not the bindings: the two used to
      // be sorted into the same order and zipped, which only
      // held while every comp parameter had a binding.
      for (auto *param : comp_params) {
        auto *inferred_val = bindings.Resolved(
          dynamic_shared_cast<TypeIdentifierAst>(param->Name).get()).Comp;
        if (inferred_val == nullptr) { continue; }
        const auto raw_a_type = inferred_val->InferType(&sm, &meta);
        auto a_type = owner_scope.GetTypeSymbol(raw_a_type.get())->FqName();
        auto p_type = param->CompType->SubstituteGenerics(all_final_args);

        if (param->TokEllipsis != nullptr) {
          for (auto const &inner : a_type->LastTypePart()->GnArgGroup->Args
               | genex::views::ptr
               | genex::views::filter([](auto *g) { return g->Name == nullptr and g->TypeVal != nullptr; })
               | genex::views::transform([](auto *g) { return g->TypeVal; })
               | genex::to<Vec>()) {
            RaiseIf<SppTypeMismatchError>(
              not TypeEq(*p_type, *inner, owner_scope, *sm.CurrentScope),
              {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *inferred_val, *inner));
          }
          break;
        }
        RaiseIf<SppTypeMismatchError>(
          not TypeEq(*p_type, *a_type, owner_scope, *sm.CurrentScope),
          {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *inferred_val, *raw_a_type));
      }
    }
  }
}

auto spp::analyse::utils::generic_bindings::NameGnArgs(
  GenericArgumentGroupAst &a_group, GenericParameterGroupAst const &p_group,
  Ast const &owner, ScopeManager &sm, meta::CompilerMetaData &meta,
  const bool is_tuple_owner) -> void {
  // Special case for tuples to prevent an infinite recursion.
  if (is_tuple_owner) { return; }

  // Split the arguments by moving off the group (will re-add
  // back after).
  const auto comp_args = GenericArgumentGroupAst::NewEmpty();
  const auto type_args = GenericArgumentGroupAst::NewEmpty();
  for (auto &&arg : a_group.Args) {
    (arg->CompVal != nullptr ? comp_args : type_args)->Args.push_back(std::move(arg));
  }
  a_group.Args.Clear();

  const auto comp_params = p_group.GetCompParams();
  const auto type_params = p_group.GetTypeParams();

  // Name the two kinds of arguments separately. This is fine
  // as there is no cross-over between names for comp params/args
  // vs type params/args.
  NameGnArgsImpl(*comp_args, comp_params, true, owner, sm, meta);
  NameGnArgsImpl(*type_args, type_params, false, owner, sm, meta);

  // Recombine the named arguments back into the original argument
  // group.
  a_group.Args.AppendRange(std::move(comp_args->Args));
  a_group.Args.AppendRange(std::move(type_args->Args));

  // Build index map once for O(n). This maximizes the efficiency of
  // the sorting. Apply the sorting to the arguments to keep them in
  // order of the parameters.
  auto param_index = Map<StrView, std::size_t>();
  for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) {
    param_index[p->Name->ToUnchecked<TypeIdentifierAst>()->Name] = i;
  }

  a_group.Args |= genex::actions::sort([&](auto const &a, auto const &b) {
    return param_index[a->ViewName()] < param_index[b->ViewName()];
  });
}

SPP_MOD_BEGIN
auto GenericBinding::IsType() const -> bool {
  return Type != nullptr;
}

auto GenericBinding::IsBound() const -> bool {
  return Type != nullptr or Comp != nullptr;
}

GenericBindingSet::GenericBindingSet() = default;

auto GenericBindingSet::FromNamedArgs(
  GenericArgumentGroupAst &a_group, ScopeManager &sm) -> GenericBindingSet {
  using errors::SppInternalCompilerError;

  // Build the generic binding set by moving all the generic
  // arguments off of the argument group, and clearing the
  // argument group's storage.
  auto out = GenericBindingSet();
  for (auto &&arg : std::move(a_group.Args)) {
    out._OwnedArgs.EmplaceBack(std::move(arg));
  }
  a_group.Args.Clear();

  // For each value in the binding set, properly register the
  // value for the binding. These registrations could get modified
  // by the inference afterwards, and checked for consistency.
  for (auto const &arg : out._OwnedArgs) {
    if (arg->Name != nullptr and arg->CompVal != nullptr) {
      out.Add(dynamic_shared_cast<TypeIdentifierAst>(arg->Name), arg->CompVal.get());
    }
    else if (arg->Name != nullptr and arg->TypeVal != nullptr) {
      out.Add(dynamic_shared_cast<TypeIdentifierAst>(arg->Name), arg->TypeVal);
    }
    else {
      const auto err = "generic argument '" + arg->ToString() + "' is still positional where a binding is expected";
      Raise<SppInternalCompilerError>({sm.CurrentScope}, ERR_ARGS(*arg, err));
    }
  }
  return out;
}

auto GenericBindingSet::Args() const -> Vec<GenericArgumentAst*> {
  // Get raw pointer for each generic argument.
  return _OwnedArgs | genex::views::ptr | genex::to<Vec>();
}

GenericBindingSet::~GenericBindingSet() = default;

auto GenericBindingSet::Add(
  Shared<TypeIdentifierAst> const &name, Shared<TypeAst> value) -> void {
  // Register a value for a generic type argument.
  _Table[name].Types.EmplaceBack(std::move(value));
}

auto GenericBindingSet::Add(
  Shared<TypeIdentifierAst> const &name, ExpressionAst *value) -> void {
  // Register a value for a generic comp argument.
  _Table[name].Comps.EmplaceBack(value);
}

auto GenericBindingSet::Replace(
  TypeIdentifierAst const *name, Shared<TypeAst> value) -> void {
  // The candidates have already been reconciled by this
  // point, so the replacement is the whole binding.
  for (auto &[k, v] : _Table) {
    if (*k != *name) { continue; }
    v.Types.Clear();
    v.Types.EmplaceBack(std::move(value));
    return;
  }
}

auto GenericBindingSet::Contains(
  TypeIdentifierAst const *name) const -> bool {
  // Check for the name existing in the table, by deref
  // comparison, and that values of either kind exist for
  // it. A type and a comp parameter cannot share a name.
  return genex::any_of(
    _Table,
    [name](auto const &e) { return *e.first == *name and not(e.second.Types.IsEmpty() and e.second.Comps.IsEmpty()); });
}

auto GenericBindingSet::Resolved(
  TypeIdentifierAst const *name) const -> GenericBinding {
  // The first candidate wins; "EnforceNoConflicts" is
  // what establishes the rest agree with it.
  for (auto const &[k, v] : _Table) {
    if (*k != *name) { continue; }
    if (not v.Types.IsEmpty()) { return GenericBinding{.Type = v.Types[0], .Comp = nullptr}; }
    if (not v.Comps.IsEmpty()) { return GenericBinding{.Type = nullptr, .Comp = v.Comps[0]}; }
  }
  return GenericBinding{.Type = nullptr, .Comp = nullptr};
}

auto GenericBindingSet::TypeNames() const
  -> Vec<Shared<TypeIdentifierAst>> {
  // Get all the types in the bindings map that have values
  // assigned to them.
  auto out = Vec<Shared<TypeIdentifierAst>>();
  for (auto const &[k, v] : _Table) {
    if (not v.Types.IsEmpty()) { out.EmplaceBack(k); }
  }
  return out;
}

auto GenericBindingSet::CompNames() const
  -> Vec<Shared<TypeIdentifierAst>> {
  // Get all the comps in the bindings map that have
  // values assigned to them.
  auto out = Vec<Shared<TypeIdentifierAst>>();
  for (auto const &[k, v] : _Table) {
    if (not v.Comps.IsEmpty()) { out.EmplaceBack(k); }
  }
  return out;
}

auto GenericBindingSet::EnforceNoConflicts(
  ScopeManager &sm) const -> void {
  using errors::SppGenericParameterConflictError;
  using type_compare::TypeEq;

  // A parameter reached through multiple arguments, or
  // through an arguments and constraints, has to be
  // offered the same thing by both.
  for (auto const &entry : _Table) {
    auto const &name = entry.first;
    auto const &types = entry.second.Types;
    auto const &comps = entry.second.Comps;

    // Use the TypeEq equality checker for types, to
    // ensure they are consistent.
    // Todo: a type inferred off a literal ("v=1") is generated, so its block reads "<generated code>"; point at the
    //  argument it came from (TestSelfTypePositionsGeneric.test_invalid_self_class_attribute_generic_argument_...).
    for (auto i = 1uz; i < types.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        not TypeEq(*types[i], *types[0], *sm.CurrentScope, *sm.CurrentScope),
        {sm.CurrentScope}, ERR_ARGS(*name, *types[0], *types[i]));
    }

    // Expression for comp args can just use the normal
    // `==`, because each expression ast implements its
    // exact comparison methodology.
    for (auto i = 1uz; i < comps.Len(); ++i) {
      RaiseIf<SppGenericParameterConflictError>(
        *comps[i] != *comps[0],
        {sm.CurrentScope}, ERR_ARGS(*name, *comps[0], *comps[i]));
    }
  }
}

auto GenericBindingSet::ToInferenceMap() const
  -> type_compare::GenericInferenceMap {
  auto out = type_compare::GenericInferenceMap();
  for (auto const &[name, candidates] : _Table) {
    if (not candidates.Types.IsEmpty()) { out.emplace(name, candidates.Types[0].get()); }
    else if (not candidates.Comps.IsEmpty()) { out.emplace(name, candidates.Comps[0]); }
  }
  return out;
}

auto GenericBindingSet::ToArgs(
  GenericParameterGroupAst const &p_group) const -> Vec<Unique<GenericArgumentAst>> {
  auto out = Vec<Unique<GenericArgumentAst>>();

  // Walking the parameters rather than the table puts
  // the arguments in declaration order, which is what
  // the instantiation's name is built from.
  for (auto const *param : p_group.GetAllParams()) {
    const auto p_name = dynamic_shared_cast<TypeIdentifierAst>(param->Name);
    const auto bound = Resolved(p_name.get());
    if (not bound.IsBound()) { continue; }

    if (bound.IsType()) {
      out.EmplaceBack(GenericArgumentAst::NewType(p_name, bound.Type));
      continue;
    }

    // A comp argument given a type is a value named by
    // that type ("Self::mo_seq_cst"), so it is carried
    // as the identifier form rather than as a type.
    // Todo: is this first block correct?
    if (const auto *as_type = bound.Comp->To<TypeAst>(); as_type != nullptr) {
      out.EmplaceBack(GenericArgumentAst::NewComp(
        AstClone(p_name), IdentifierAst::FromType(*as_type)));
    }
    else {
      out.EmplaceBack(GenericArgumentAst::NewComp(
        AstClone(p_name), AstClone(bound.Comp)));
    }
  }

  return out;
}

SPP_MOD_END

auto spp::analyse::utils::generic_bindings::EnforceGenericConstraintsOfParams(
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

auto spp::analyse::utils::generic_bindings::EnforceGenericConstraintsAllArgs(
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
  const auto all_args = a_group.GetAllArgs();
  const auto type_args = a_group.GetTypeArgs();

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

    // Cross apply the inferred arguments into this parameter's
    // constraints.
    auto p_cons = Vec<Shared<TypeAst>>();
    for (auto const &p_con : p_con_groups[i]) {
      const auto sub = p_con->SubstituteGenerics(all_args);
      {
        const auto _meta_guard = meta::MetaGuard(&meta);
        meta.AllowAbstractType = true;
        sub->Stage7_AnalyseSemantics(&con_sm, &meta);
      }
      p_cons.push_back(sub->WithSourceSpanOf(*p_con));
    }

    // Handle variadic constraint checks. Todo: Expand docs
    auto targets = Vec<TypeAst const*>();
    if (p_group.GetTypeParams()[i]->TokEllipsis != nullptr) {
      for (auto const *elem : matching[0]->TypeVal->LastTypePart()->GnArgGroup->GetTypeArgs()) {
        targets.EmplaceBack(elem->TypeVal.get());
      }
    }
    else {
      targets.EmplaceBack(matching[0]->TypeVal.get());
    }

    // Raise an error if any constraint of this argument is
    // not satisfied.
    for (auto const *target : targets) {
      const auto unsatisfied = type_compare::EnforceGenericConstraintsOneArg(
        p_cons, *target, owner_scope, *sm.CurrentScope);
      RaiseIf<SppGenericConstraintError>(
        unsatisfied != nullptr, {decl_scope != nullptr ? decl_scope : &owner_scope, sm.CurrentScope},
        ERR_ARGS(*unsatisfied, *target));
    }
  }
}

auto spp::analyse::utils::generic_bindings::InferGnArgs(
  GenericParameterGroupAst const &p_group, GenericArgumentGroupAst &a_group,
  Shared<InferenceSourceMap> infer_source, Shared<InferenceTargetMap> infer_target,
  Shared<Ast> const &owner, Scope const &owner_scope,
  Shared<IdentifierAst> const &variadic_fn_param_name,
  const bool is_tuple_owner, ScopeManager &sm,
  meta::CompilerMetaData &meta) -> void {
  meta.InferSource = MakeShared<meta::GenericInferenceBindings>();
  meta.InferTarget = MakeShared<meta::GenericInferenceBindings>();

  if (is_tuple_owner or p_group.Params.IsEmpty()) { return; }

  // Separate param lists and extract names and constraint
  // groups.
  const auto type_params = p_group.GetTypeParams();
  const auto comp_params = p_group.GetCompParams();

  const auto type_p_names = type_params
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  const auto variadic_type_p_names = type_params
    | genex::views::filter([](auto *x) { return x->TokEllipsis != nullptr; })
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  const auto comp_p_names = comp_params
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();

  // Every candidate a parameter is offered goes into one
  // binding set. The written arguments go in first, so
  // they are the ones a later candidate has to agree with.
  auto bindings = GenericBindingSet::FromNamedArgs(a_group, sm);
  const auto type_a_names = bindings.Args()
    | genex::views::filter([](auto const *x) { return x->Name != nullptr and x->TypeVal != nullptr; })
    | genex::views::transform([](auto const *x) { return dynamic_shared_cast<TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();

  // First inference comes from the infer source and target
  // maps.
  for (auto const &[target_name, target_type] : *infer_target) {
    if (not infer_source->contains(target_name)) { continue; }
    CollectDirectInferences(
      infer_source->at(target_name), target_type, target_name, type_a_names, type_p_names, variadic_type_p_names,
      comp_p_names, variadic_fn_param_name, owner_scope, sm, bindings);
  }

  InferFromConstraints(type_params, type_p_names, comp_p_names, owner_scope, sm, bindings);
  ApplyTypeDefaults(type_params, owner_scope, meta, bindings);

  // Kept for as long as "bindings" is read: it points into these translated defaults.
  const auto _comp_defaults = ApplyCompDefaults(
    comp_params, owner_scope, sm, meta, bindings);

  // Validate there are no conflicting candidates or
  // uninferred required params.
  bindings.EnforceNoConflicts(sm);
  EnforceNoUninferredGnArgs(type_p_names, bindings.TypeNames(), owner_scope, owner, sm);
  EnforceNoUninferredGnArgs(comp_p_names, bindings.CompNames(), owner_scope, owner, sm);
  CrossSubstituteBindings(type_a_names, sm, meta, bindings);

  // Emit the final arg list, in parameter declaration
  // order.
  a_group.Args = bindings.ToArgs(p_group);
  if (meta.CurrentStage >= meta::CompilerStage::kAttachSupScopes) {
    CheckCompArgTypes(comp_params, owner_scope, sm, meta, bindings);
  }
}
