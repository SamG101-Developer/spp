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
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_variadic_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.generate.common_types;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import genex;

namespace spp::analyse::utils::generic_bindings {
  namespace {
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

      meta.Save();
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
            if (meta.CurrentStage > 5) { tuple->Stage7_AnalyseSemantics(&sm, &meta); }
            kw_arg->Val = std::move(tuple);
          }

          a_group.Args[i] = std::move(kw_arg);
          a_group.Args |= genex::actions::take(i + 1);
          break;
        }

        // Otherwise, attach the single argument convention and
        // value.
        kw_arg->Val = asts::AstClone(positional_arg->To<GenericArgType>()->Val);
        if (meta.CurrentStage > 5) { kw_arg->Val->Stage7_AnalyseSemantics(&sm, &meta); }
        a_group.Args[i] = std::move(kw_arg);
      }

      meta.Restore();
    }
  }
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
  const auto comp_params = ( {
    auto raw = p_group.GetCompParams();
    Vec<asts::GenericParameterAst*>(raw.begin(), raw.end());
  });

  const auto type_params = ( {
    auto raw = p_group.GetTypeParams();
    Vec<asts::GenericParameterAst*>(raw.begin(), raw.end());
  });

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
  std::ranges::move(comp_args->Args, std::back_inserter(a_group.Args));
  std::ranges::move(type_args->Args, std::back_inserter(a_group.Args));

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
  using type_utils::TypeEq;

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
  -> type_utils::GenericInferenceMap {
  auto out = type_utils::GenericInferenceMap();
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
