module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

#define MAKE_VARIADIC_COMP_ARGS(What)                                                                         \
  What                                                                                                        \
    | genex::views::move                                                                                      \
    | genex::views::drop(i)                                                                                   \
    | genex::views::transform([](auto &&x) { return asts::AstClone(x->template To<GenericArgType>()->Val); }) \
    | genex::to<Vec>();

#define MAKE_VARIADIC_TYPE_ARGS(What)                                                                               \
  What                                                                                                              \
    | genex::views::move                                                                                            \
    | genex::views::drop(i)                                                                                         \
    | genex::views::transform([](auto &&x) { return asts::AstCloneShared(x->template To<GenericArgType>()->Val); }) \
    | genex::to<Vec>();

module spp.analyse.utils.generic_bindings;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.generic_parameter_type_optional_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.identifier_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import genex;

namespace spp::analyse::utils::generic_bindings {
  namespace {
    auto EnforceNoUninferredGnArgs(
      Vec<Shared<asts::TypeIdentifierAst>> const &p_names,
      Vec<Shared<asts::TypeIdentifierAst>> const &i_names,
      scopes::Scope const &owner_scope,
      Shared<asts::Ast> const &owner,
      scopes::ScopeManager &sm)
      -> void {
      //
      using errors::SppGenericParameterNotInferredError;

      // Check for uninferred arguments.
      const auto uninferred_params = p_names
        | genex::views::not_in(i_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();

      RaiseIf<SppGenericParameterNotInferredError>(
        not uninferred_params.IsEmpty(), {sm.CurrentScope, &owner_scope},
        ERR_ARGS(*uninferred_params[0], *owner));
    }

    /**
     * Reject a keyword argument whose name is not one of the parameters.
     */
    template <typename GenericArgType, typename GenericParamType>
    auto EnforceNoInvalidGnArgs(
      Vec<asts::GenericParameterAst*> const &params,
      Vec<asts::GenericArgumentAst*> const &named_args,
      scopes::ScopeManager &sm)
      -> void {
      // Shortcut; when there are no named arguments then there
      // is nothing to enforce for comparisons between values for
      // the same binding name.
      using errors::SppArgumentNameInvalidError;
      using GenericKeywordArgType = asts::detail::make_keyword_arg_t<GenericArgType>;
      if (named_args.IsEmpty()) { return; }

      // Check for invalid argument names against parameter names.
      // An "invalid" generic argument name is for example "T=S32"
      // where "T" doesn't appear in the generic parameter list.
      for (auto &&a : named_args | genex::views::cast_dynamic<GenericKeywordArgType*>) {
        auto found = false;
        for (auto &&p : params | genex::views::cast_dynamic<GenericParamType*>) {
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

    /**
     * Turn every positional argument of one kind into the keyword argument it stands for, by pairing it with the
     * next parameter that has not already been named. A trailing variadic parameter consumes whatever is left, as a
     * tuple.
     */
    template <typename GenericArgType, typename GenericParamType, typename GenericParamTypeVariadicAst>
    auto NameGnArgsImpl(
      asts::GenericArgumentGroupAst &a_group,
      Vec<asts::GenericParameterAst*> const &params,
      asts::Ast const &owner,
      scopes::ScopeManager &sm,
      asts::meta::CompilerMetaData &meta)
      -> void {
      //
      using asts::generate::common_types::TupleType;
      using errors::SppGenericArgumentTooManyError;
      using GenericKeywordArgType = asts::detail::make_keyword_arg_t<GenericArgType>;

      // Validate the named arguments against the parameters. This
      // weeds out anything that'd mess up the naming scheme below.
      EnforceNoInvalidGnArgs<GenericArgType, GenericParamType>(
        params, a_group.GetAllArgs(), sm);

      // Get the names of the keyword arguments. As this is either
      // doing only type or comp, pre-cast then use the name.
      auto a_names = a_group.GetKeywordArgs()
        | genex::views::cast_dynamic<GenericKeywordArgType*>()
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
        return p->template To<GenericParamTypeVariadicAst>() != nullptr;
      });

      const auto _meta_guard = asts::meta::MetaGuard(&meta);
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
        auto kw_arg = MakeUnique<GenericKeywordArgType>(
          p_names.Front(), nullptr, nullptr);
        p_names |= genex::actions::pop_front();

        // The variadic parameter requires a tuple of the remaining
        // arguments.
        if (p_names.IsEmpty() and is_variadic) {
          // Variadic check: map comp arguments "func[1_u32, 1_u32]"
          // for "func[cmp ..s]" to "func[ts = (1_u32, 1_u32)]". Uses
          // the tuple literal ast.
          if constexpr (std::same_as<GenericParamType, asts::GenericParameterCompAst>) {
            auto elems = MAKE_VARIADIC_COMP_ARGS(a_group.Args);
            auto tuple = MakeUnique<asts::TupleLiteralAst>(nullptr, std::move(elems), nullptr);
            kw_arg->Val = std::move(tuple);
          }

          // Variadic check: map type arguments "func[U32, U32]"
          // for "func[..Ts]" to "func[Ts = (U32, U32)]". Uses the
          // tuple type + analysis for generic implementation.
          else {
            auto elems = MAKE_VARIADIC_TYPE_ARGS(a_group.Args);
            auto tuple = TupleType(positional_arg->PosStart(), std::move(elems));
            if (meta.CurrentStage >= asts::meta::CompilerStage::kQualifyTypes) { tuple->Stage7_AnalyseSemantics(&sm, &meta); }
            kw_arg->Val = std::move(tuple);
          }

          a_group.Args[i] = std::move(kw_arg);
          a_group.Args |= genex::actions::take(i + 1);
          break;
        }

        // Otherwise, attach the single argument convention and
        // value.
        kw_arg->Val = asts::AstClone(positional_arg->To<GenericArgType>()->Val);
        if (meta.CurrentStage >= asts::meta::CompilerStage::kQualifyTypes) { kw_arg->Val->Stage7_AnalyseSemantics(&sm, &meta); }
        a_group.Args[i] = std::move(kw_arg);
      }
    }

    auto CollectDirectInferences(
      Shared<asts::TypeAst> const &source_type,
      Shared<asts::TypeAst> const &target_type,
      Shared<asts::IdentifierAst> const &target_name,
      Vec<Shared<asts::TypeIdentifierAst>> const &type_p_names,
      Vec<Shared<asts::TypeIdentifierAst>> const &variadic_type_p_names,
      Vec<Shared<asts::TypeIdentifierAst>> const &comp_p_names,
      Shared<asts::IdentifierAst> const &variadic_fn_param_name,
      scopes::Scope const &owner_scope,
      scopes::ScopeManager &sm,
      GenericBindingSet &bindings)
      -> void {
      //
      auto temp_gs = spp::analyse::utils::type_compare::GenericInferenceMap();
      spp::analyse::utils::type_compare::RelaxedTypeEq(
        *source_type->WithoutConvention(),
        *target_type->WithoutConvention(),
        *sm.CurrentScope, owner_scope, temp_gs, true);

      const auto is_variadic_param_slot =
        variadic_fn_param_name != nullptr
        and target_name != nullptr
        and *target_name == *variadic_fn_param_name;

      for (auto const &[inferred_name, inferred_val] : temp_gs) {
        auto *typed = inferred_val->To<asts::TypeAst>();
        const auto declared_type = genex::contains(type_p_names, *inferred_name, genex::meta::deref);
        const auto declared_comp = genex::contains(comp_p_names, *inferred_name, genex::meta::deref);

        if (declared_type) {
          if (typed == nullptr) { continue; }
          auto shared = typed->shared_from_this();
          if (is_variadic_param_slot and not genex::contains(variadic_type_p_names, *inferred_name, genex::meta::deref)) {
            auto const &inner = shared->LastTypePart()->GnArgGroup->Args[0];
            shared = inner->ToUnchecked<asts::GenericArgumentTypeAst>()->Val;
          }
          bindings.Add(inferred_name, std::move(shared));
        }
        else if (declared_comp) {
          bindings.Add(inferred_name, inferred_val);
        }
      }
    }
  }
}

auto spp::analyse::utils::generic_bindings::WithoutSelfBindingGenerics(
  Shared<asts::TypeAst> const &type)
  -> Shared<asts::TypeAst> {
  // An empty argument list is not a self-binding one - there is nothing to strip, and nothing wrong with it.
  auto const &args = type->LastTypePart()->GnArgGroup->Args;
  if (args.IsEmpty()) { return type; }
  if (not genex::all_of(args, [](auto const &a) { return BindsToItself(*a); })) { return type; }
  return type->WithoutGenerics()->WithConvention(asts::AstClone(type->GetConvention()));
}

auto spp::analyse::utils::generic_bindings::BindsToItself(
  asts::GenericArgumentAst const &arg)
  -> bool {
  // Compare the argument name against the value it
  // contains for textual equality.
  if (const auto *type_arg = arg.To<asts::GenericArgumentTypeAst>(); type_arg != nullptr) {
    return type_arg->Val->ToString() == arg.ViewName();
  }
  if (const auto *comp_arg = arg.To<asts::GenericArgumentCompAst>(); comp_arg != nullptr) {
    return comp_arg->Val->ToString() == arg.ViewName();
  }
  return false;
}

auto spp::analyse::utils::generic_bindings::NameGnArgs(
  asts::GenericArgumentGroupAst &a_group,
  asts::GenericParameterGroupAst const &p_group,
  asts::Ast const &owner,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData &meta,
  const bool is_tuple_owner)
  -> void {
  // Special case for tuples to prevent an infinite recursion.
  if (is_tuple_owner) { return; }

  // Split the arguments by moving off the group (will re-add
  // back after).
  const auto comp_args = asts::GenericArgumentGroupAst::NewEmpty();
  const auto type_args = asts::GenericArgumentGroupAst::NewEmpty();
  for (auto &&arg : a_group.Args) {
    (arg->To<asts::GenericArgumentCompAst>() ? comp_args : type_args)->Args.push_back(std::move(arg));
  }
  a_group.Args.Clear();

  // Copy the raw pointer vectors from the splits. Cast into
  // the raw `GenericParameterAst` for re-combination later.
  const auto comp_params = [&] {
    auto raw = p_group.GetCompParams();
    return Vec<asts::GenericParameterAst*>(raw.begin(), raw.end());
  }();

  const auto type_params = [&] {
    auto raw = p_group.GetTypeParams();
    return Vec<asts::GenericParameterAst*>(raw.begin(), raw.end());
  }();

  // Name the two kinds of arguments separately. This is fine
  // as there is no cross-over between names for comp params/args
  // vs type params/args.
  NameGnArgsImpl<
    asts::GenericArgumentCompAst,
    asts::GenericParameterCompAst,
    asts::GenericParameterCompVariadicAst>(
    *comp_args, comp_params, owner, sm, meta);

  NameGnArgsImpl<
    asts::GenericArgumentTypeAst,
    asts::GenericParameterTypeAst,
    asts::GenericParameterTypeVariadicAst>(
    *type_args, type_params, owner, sm, meta);

  // Recombine the named arguments back into the original argument
  // group.
  a_group.Args.AppendRange(std::move(comp_args->Args));
  a_group.Args.AppendRange(std::move(type_args->Args));

  // Build index map once for O(n). This maximizes the efficiency of
  // the sorting. Apply the sorting to the arguments to keep them in
  // order of the parameters.
  auto param_index = Map<StrView, std::size_t>();
  for (auto [i, p] : p_group.GetAllParams() | genex::views::enumerate) {
    param_index[p->Name->ToUnchecked<asts::TypeIdentifierAst>()->Name] = i;
  }

  a_group.Args |= genex::actions::sort([&](auto const &a, auto const &b) {
    return param_index[a->ViewName()] < param_index[b->ViewName()];
  });
}

SPP_MOD_BEGIN

auto spp::analyse::utils::generic_bindings::GenericBinding::IsType() const
  -> bool {
  return Type != nullptr;
}

auto spp::analyse::utils::generic_bindings::GenericBinding::IsBound() const
  -> bool {
  return Type != nullptr or Comp != nullptr;
}

spp::analyse::utils::generic_bindings::GenericBindingSet::GenericBindingSet() = default;

auto spp::analyse::utils::generic_bindings::GenericBindingSet::FromNamedArgs(
  asts::GenericArgumentGroupAst &a_group,
  scopes::ScopeManager &sm)
  -> GenericBindingSet {
  //
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
    if (auto const *comp_arg = arg->To<asts::GenericArgumentCompKeywordAst>(); comp_arg != nullptr) {
      out.Add(dynamic_shared_cast<asts::TypeIdentifierAst>(comp_arg->Name), comp_arg->Val.get());
    }
    else if (auto const *type_arg = arg->To<asts::GenericArgumentTypeKeywordAst>(); type_arg != nullptr) {
      out.Add(dynamic_shared_cast<asts::TypeIdentifierAst>(type_arg->Name), type_arg->Val);
    }
    else {
      const auto err = "generic argument '" + arg->ToString() + "' is still positional where a binding is expected";
      Raise<SppInternalCompilerError>({sm.CurrentScope}, ERR_ARGS(*arg, err));
    }
  }
  return out;
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::Args() const
  -> Vec<asts::GenericArgumentAst*> {
  // Get raw pointer for each generic argument.
  return _OwnedArgs | genex::views::ptr | genex::to<Vec>();
}

spp::analyse::utils::generic_bindings::GenericBindingSet::~GenericBindingSet() = default;

auto spp::analyse::utils::generic_bindings::GenericBindingSet::Add(
  Shared<asts::TypeIdentifierAst> const &name,
  Shared<asts::TypeAst> value)
  -> void {
  // Register a value for a generic type argument.
  _Table[name].Types.EmplaceBack(std::move(value));
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::Add(
  Shared<asts::TypeIdentifierAst> const &name,
  asts::ExpressionAst *value)
  -> void {
  // Register a value for a generic comp argument.
  _Table[name].Comps.EmplaceBack(value);
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::Replace(
  asts::TypeIdentifierAst const *name,
  Shared<asts::TypeAst> value)
  -> void {
  // The candidates have already been reconciled by this
  // point, so the replacement is the whole binding.
  for (auto &[k, v] : _Table) {
    if (not(*k == *name)) { continue; }
    v.Types.Clear();
    v.Types.EmplaceBack(std::move(value));
    return;
  }
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::ContainsType(
  asts::TypeIdentifierAst const *name) const
  -> bool {
  // Check for the type existing in the table, by deref
  // comparison, and that values exist for that name.
  return genex::any_of(
    _Table,
    [name](auto const &e) { return *e.first == *name and not e.second.Types.IsEmpty(); });
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::ContainsComp(
  asts::TypeIdentifierAst const *name) const
  -> bool {
  // Check for the comp existing in the table, by deref
  // comparison, and that values exist for that name.
  return genex::any_of(
    _Table,
    [name](auto const &e) { return *e.first == *name and not e.second.Comps.IsEmpty(); });
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::Resolved(
  asts::TypeIdentifierAst const *name) const
  -> GenericBinding {
  // The first candidate wins; "EnforceNoConflicts" is
  // what establishes the rest agree with it.
  for (auto const &[k, v] : _Table) {
    if (not(*k == *name)) { continue; }
    if (not v.Types.IsEmpty()) { return GenericBinding{.Type = v.Types[0], .Comp = nullptr}; }
    if (not v.Comps.IsEmpty()) { return GenericBinding{.Type = nullptr, .Comp = v.Comps[0]}; }
  }
  return GenericBinding{.Type = nullptr, .Comp = nullptr};
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::TypeNames() const
  -> Vec<Shared<asts::TypeIdentifierAst>> {
  // Get all the types in the bindings map that have
  // values assigned to them.
  auto out = Vec<Shared<asts::TypeIdentifierAst>>();
  for (auto const &[k, v] : _Table) {
    if (not v.Types.IsEmpty()) { out.EmplaceBack(k); }
  }
  return out;
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::CompNames() const
  -> Vec<Shared<asts::TypeIdentifierAst>> {
  // Get all the comps in the bindings map that have
  // values assigned to them.
  auto out = Vec<Shared<asts::TypeIdentifierAst>>();
  for (auto const &[k, v] : _Table) {
    if (not v.Comps.IsEmpty()) { out.EmplaceBack(k); }
  }
  return out;
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::EnforceNoConflicts(
  scopes::ScopeManager &sm) const
  -> void {
  //
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

auto spp::analyse::utils::generic_bindings::GenericBindingSet::ToInferenceMap() const
  -> type_compare::GenericInferenceMap {
  auto out = type_compare::GenericInferenceMap();
  for (auto const &[name, candidates] : _Table) {
    if (not candidates.Types.IsEmpty()) { out.emplace(name, candidates.Types[0].get()); }
    else if (not candidates.Comps.IsEmpty()) { out.emplace(name, candidates.Comps[0]); }
  }
  return out;
}

auto spp::analyse::utils::generic_bindings::GenericBindingSet::ToArgs(
  asts::GenericParameterGroupAst const &p_group) const
  -> Vec<Unique<asts::GenericArgumentAst>> {
  auto out = Vec<Unique<asts::GenericArgumentAst>>();

  // Walking the parameters rather than the table puts
  // the arguments in declaration order, which is what
  // the instantiation's name is built from.
  for (auto const *param : p_group.GetAllParams()) {
    const auto p_name = dynamic_shared_cast<asts::TypeIdentifierAst>(param->Name);
    const auto bound = Resolved(p_name.get());
    if (not bound.IsBound()) { continue; }

    if (bound.IsType()) {
      out.EmplaceBack(MakeUnique<asts::GenericArgumentTypeKeywordAst>(p_name, nullptr, bound.Type));
      continue;
    }

    // A comp argument given a type is a value named by
    // that type ("Self::mo_seq_cst"), so it is carried
    // as the identifier form rather than as a type.
    // Todo: is this first block correct?
    if (const auto *as_type = bound.Comp->To<asts::TypeAst>(); as_type != nullptr) {
      out.EmplaceBack(MakeUnique<asts::GenericArgumentCompKeywordAst>(
        asts::AstClone(p_name), nullptr, asts::IdentifierAst::FromType(*as_type)));
    }
    else {
      out.EmplaceBack(MakeUnique<asts::GenericArgumentCompKeywordAst>(
        asts::AstClone(p_name), nullptr, asts::AstClone(bound.Comp)));
    }
  }

  return out;
}

SPP_MOD_END

auto spp::analyse::utils::generic_bindings::EnforceGenericConstraintsAllArgs(
  asts::GenericParameterGroupAst const &p_group,
  asts::GenericArgumentGroupAst const &a_group,
  scopes::Scope const &owner_scope,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData &meta)
  -> void {
  using errors::SppGenericConstraintError;

  // Extract important information.
  auto p_names = p_group.GetTypeParams()
    | genex::views::transform([](auto &&x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
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

    const auto arg_sym = sm.CurrentScope->GetTypeSymbol(matching[0]->Val.get());
    if (arg_sym == nullptr) { continue; }
    const auto con_scope = arg_sym->LinkedScope != nullptr
      ? arg_sym->LinkedScope
      : sm.CurrentScope;
    auto con_sm = scopes::ScopeManager(sm.GlobalScope, con_scope);

    // Cross apply the inferred arguments into this
    // parameter's constraints.
    auto p_cons = Vec<Shared<asts::TypeAst>>();
    for (auto p_con : p_con_groups[i]) {
      auto def_type_raw = p_con->WithoutGenerics();
      if (auto def_val_type_sym = owner_scope.GetTypeSymbol(def_type_raw.get()); def_val_type_sym != nullptr and meta.
        CurrentStage >= asts::meta::CompilerStage::kGenTopLvlAliases) {
        auto temp = def_val_type_sym->FqName();
        temp = temp->WithGenerics(asts::AstClone(p_con->LastTypePart()->GnArgGroup));
        p_con = std::move(temp);
      }

      auto sub = p_con->SubstituteGenerics(all_args);
      {
        const auto _meta_guard = asts::meta::MetaGuard(&meta);
        meta.AllowAbstractType = true;
        sub->Stage7_AnalyseSemantics(&con_sm, &meta);
      }
      p_cons.push_back(std::move(sub));
    }

    // Handle variadic constraint checks. Todo: Expand docs
    auto targets = Vec<asts::TypeAst const*>();
    if (p_group.GetTypeParams()[i]->To<asts::GenericParameterTypeVariadicAst>() != nullptr) {
      for (auto const *elem : matching[0]->Val->LastTypePart()->GnArgGroup->GetTypeArgs()) {
        targets.EmplaceBack(elem->Val.get());
      }
    }
    else {
      targets.EmplaceBack(matching[0]->Val.get());
    }

    // Raise an error if any constraint of this argument is
    // not satisfied.
    for (auto const *target : targets) {
      const auto unsatisfied = type_compare::EnforceGenericConstraintsOneArg(
        p_cons, *target, owner_scope, *sm.CurrentScope);
      RaiseIf<SppGenericConstraintError>(
        unsatisfied != nullptr, {&owner_scope, sm.CurrentScope},
        ERR_ARGS(*unsatisfied, *target));
    }
  }
}

auto spp::analyse::utils::generic_bindings::InferGnArgs(
  asts::GenericParameterGroupAst const &p_group,
  asts::GenericArgumentGroupAst &a_group,
  Shared<InferenceSourceMap> infer_source,
  Shared<InferenceTargetMap> infer_target,
  Shared<asts::Ast> const &owner,
  scopes::Scope const &owner_scope,
  Shared<asts::IdentifierAst> const &variadic_fn_param_name,
  const bool is_tuple_owner,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData &meta)
  -> void {
  using errors::SppGenericConstraintError;
  using errors::SppTypeMismatchError;
  using type_compare::TypeEq;

  meta.InferSource = MakeShared<asts::meta::GenericInferenceBindings>();
  meta.InferTarget = MakeShared<asts::meta::GenericInferenceBindings>();

  if (is_tuple_owner or p_group.Params.IsEmpty()) { return; }

  // Separate param lists and extract names and constraint
  // groups.
  const auto type_params = p_group.GetTypeParams();
  const auto comp_params = p_group.GetCompParams();

  auto type_p_names = type_params
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  auto variadic_type_p_names = type_params
    | genex::views::filter([](auto *x) { return x->template To<asts::GenericParameterTypeVariadicAst>() != nullptr; })
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();
  auto comp_p_names = comp_params
    | genex::views::transform([](auto *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();

  // Every candidate a parameter is offered goes into one
  // binding set. The written arguments go in first, so
  // they are the ones a later candidate has to agree with.
  auto bindings = generic_bindings::GenericBindingSet::FromNamedArgs(a_group, sm);
  auto type_a_names = bindings.Args()
    | genex::views::cast_dynamic<asts::GenericArgumentTypeKeywordAst*>()
    | genex::views::transform([](auto const *x) { return dynamic_shared_cast<asts::TypeIdentifierAst>(x->Name); })
    | genex::to<Vec>();

  // First inference comes from the infer source and target
  // maps.
  for (auto const &[target_name, target_type] : *infer_target) {
    if (not infer_source->contains(target_name)) { continue; }
    CollectDirectInferences(
      infer_source->at(target_name), target_type, target_name, type_p_names, variadic_type_p_names, comp_p_names,
      variadic_fn_param_name, owner_scope, sm, bindings);
  }

  // Next is constraint based inference, where for example
  // "[U, F: FunRef[(), U]]" can infer "U" from the return
  // type of whatever subtype of "F" is "FunRef", and "U"
  // is extractable as a generic argument.
  {
    for (auto *param : type_params) {
      if (param->Constraints->Constraints.IsEmpty()) { continue; }
      const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(param->Name);

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
      auto candidates = Vec<Pair<Shared<asts::TypeAst>, scopes::Scope const*>>{};
      if (concrete_sym != nullptr and not concrete_sym->IsGeneric) {
        candidates.EmplaceBack(concrete_sym->FqName(), sm.CurrentScope);
        if (concrete_sym->LinkedScope != nullptr) {
          for (auto const *sup_scope : concrete_sym->LinkedScope->SupScopes()) {
            if (sup_scope->AstNode->To<asts::ClassPrototypeAst>() == nullptr) { continue; }
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
            {sm.CurrentScope, &owner_scope}, ERR_ARGS(*constraint, *inferred_type));
        }

        for (auto const &[inferred_name, inferred_val] : temp_gs) {
          // Skip names already bound to a type, to avoid
          // duplicate entries.
          if (bindings.ContainsType(inferred_name.get())) { continue; }

          if (genex::contains(type_p_names, *inferred_name, genex::meta::deref)) {
            auto *typed = inferred_val->To<asts::TypeAst>();
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

  // Apply optional defaults for still unknown type params.
  // Type params may need qualification.
  for (auto *opt_param : type_params | genex::views::cast_dynamic<asts::GenericParameterTypeOptionalAst*>()) {
    const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
    if (bindings.ContainsType(cast_name.get())) { continue; }
    auto def_type = opt_param->DefaultVal;
    auto def_type_raw = def_type->WithoutGenerics();
    if (auto def_sym = owner_scope.GetTypeSymbol(def_type_raw.get()); def_sym != nullptr and meta.CurrentStage >= asts::meta::CompilerStage::kGenTopLvlAliases) {
      auto temp = def_sym->FqName()->WithConvention(asts::AstClone(def_type->GetConvention()));
      if (not type_predicates::IsTypeSelf(*def_type)) {
        temp = temp->WithGenerics(asts::AstClone(def_type->LastTypePart()->GnArgGroup));
      }
      def_type = std::move(temp);
    }
    bindings.Add(cast_name, std::move(def_type));
  }

  // Apply optional defaults for still unknown comp params.
  // A default is an expression written in the callee's own
  // terms - "Self::mo_seq_cst", "n + 1" - and is read back
  // here, where those names mean nothing: this is the call
  // site's scope, and "Self" names nothing at all in it. So
  // it is translated the same way the prototype's types are,
  // against everything bound so far plus the "Self" its
  // owner resolves to. Don't remove the "substituted" list,
  // as it holds owned versions of the raw pointers this puts
  // into "bindings".
  auto substituted_comp_defaults = Vec<Shared<asts::ExpressionAst>>();
  auto owner_self_arg = Unique<asts::GenericArgumentTypeKeywordAst>(nullptr);
  auto owner_self_looked_up = false;

  for (auto *opt_param : comp_params | genex::views::cast_dynamic<asts::GenericParameterCompOptionalAst*>()) {
    const auto cast_name = dynamic_shared_cast<asts::TypeIdentifierAst>(opt_param->Name);
    if (bindings.ContainsComp(cast_name.get())) { continue; }

    auto *default_val = opt_param->DefaultVal.get();

    // Nothing to translate against before the aliases exist.
    // The lookup for "Self" is made here rather than up front
    // because it is a symbol lookup, and most calls have no
    // optional comp parameter to make it for.
    if (meta.CurrentStage >= asts::meta::CompilerStage::kGenTopLvlAliases) {
      if (not owner_self_looked_up) {
        owner_self_looked_up = true;
        if (auto self_type = owner_scope.GetEnclosingSelfType(meta);
          self_type != nullptr and not self_type->IsSelfType()) {
          owner_self_arg = MakeUnique<asts::GenericArgumentTypeKeywordAst>(
            asts::generate::common_types::SelfType(0), nullptr, std::move(self_type));
        }
      }

      // What is already bound comes first, so a "Self" the call
      // itself pinned outranks the one the owner declares.
      const auto known_group = asts::GenericArgumentGroupAst::FromMap(bindings.ToInferenceMap());
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

  // Validate there are no conflicting candidates or
  // uninferred required params.
  bindings.EnforceNoConflicts(sm);
  EnforceNoUninferredGnArgs(type_p_names, bindings.TypeNames(), owner_scope, owner, sm);
  EnforceNoUninferredGnArgs(comp_p_names, bindings.CompNames(), owner_scope, owner, sm);

  // Cross-apply: substitute all known values (type +
  // comp together) into each type param's resolved
  // type. Handles "Vec[T, A=Alloc[T]]" style defaults
  // and type<->comp cross-substitution.
  for (auto const &type_name : bindings.TypeNames()) {
    if (genex::contains(type_a_names, *type_name, genex::meta::deref)) { continue; }

    // Substitute through everything else that is known,
    // skipping this name to avoid cycles.
    auto other_unified = bindings.ToInferenceMap();
    other_unified.erase(type_name);
    const auto other_group = asts::GenericArgumentGroupAst::FromMap(other_unified);

    auto t = bindings.Resolved(type_name.get()).Type->SubstituteGenerics(other_group->GetAllArgs());
    t->Stage7_AnalyseSemantics(&sm, &meta);
    bindings.Replace(type_name.get(), std::move(t));
  }

  // Emit the final arg list, in parameter declaration
  // order.
  a_group.Args = bindings.ToArgs(p_group);

  // Cmp argument type-checking (semantic stage only).
  // Done after args are restored onto a_group so that
  // the following issue is solved: when analysing
  // "SizedIntegerSigned[32_u32]", "32_u32" is inferred
  // and checked. But as it is inferred, the generics
  // were missing, because this function temporarily
  // removes them. So we only analyse AFTER they are
  // re-added having been checked.
  if (meta.CurrentStage >= asts::meta::CompilerStage::kAttachSupScopes) {
    const auto all_final_group = asts::GenericArgumentGroupAst::FromMap(bindings.ToInferenceMap());
    const auto all_final_args = all_final_group->GetAllArgs();

    // Walk the parameters, not the bindings: the two
    // used to be sorted into the same order and zipped,
    // which only held while every comp parameter had a binding.
    for (auto *param : comp_params) {
      auto *inferred_val = bindings.Resolved(
        dynamic_shared_cast<asts::TypeIdentifierAst>(param->Name).get()).Comp;
      if (inferred_val == nullptr) { continue; }
      auto a_type = owner_scope.GetTypeSymbol(inferred_val->InferType(&sm, &meta).get())->FqName();
      auto p_type = param->Type->SubstituteGenerics(all_final_args);

      if (param->To<asts::GenericParameterCompVariadicAst>()) {
        for (auto const &inner : a_type->LastTypePart()->GnArgGroup->Args
             | genex::views::ptr
             | genex::views::cast_dynamic<asts::GenericArgumentTypePositionalAst*>()
             | genex::views::transform([](auto *g) { return g->Val; })
             | genex::to<Vec>()) {
          RaiseIf<SppTypeMismatchError>(
            not TypeEq(*p_type, *inner, owner_scope, *sm.CurrentScope),
            {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *inferred_val, *inner));
        }
        break;
      }
      auto raw_a_type = inferred_val->InferType(&sm, &meta);
      RaiseIf<SppTypeMismatchError>(
        not TypeEq(*p_type, *a_type, owner_scope, *sm.CurrentScope),
        {&owner_scope, sm.CurrentScope}, ERR_ARGS(*param, *p_type, *inferred_val, *raw_a_type));
    }
  }
}
