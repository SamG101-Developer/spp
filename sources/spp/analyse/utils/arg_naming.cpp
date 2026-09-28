module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.arg_naming;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.packs;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.expression_ast;
import spp.asts.function_call_argument_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_keyword_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.literal_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;

namespace spp::analyse::utils::arg_naming {
  namespace {
    /// Reject the first argument name no parameter has, reported
    /// against the first parameter (or the argument itself, when
    /// there are none). "what" is the kind of argument, "fn" or
    /// "gn". Names compare by value, whatever ast holds them.
    template <typename P, typename A>
    auto EnforceNamesKnown(
      Vec<P> const &p_names,
      Vec<A> const &a_names,
      Ast const *param_ctx,
      const StrView what,
      Scope *scope)
      -> void {
      const auto invalid = a_names
        | genex::views::not_in(p_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
      if (invalid.IsEmpty()) { return; }
      const auto p_what = Str(what) + " param";
      const auto a_what = Str(what) + " arg";
      Raise<errors::SppArgumentNameInvalidError>(
        {scope}, ERR_ARGS(param_ctx != nullptr ? *param_ctx : *invalid[0], StrView(p_what), *invalid[0], StrView(a_what)));
    }

    /// The parameters, in order, that no keyword argument names.
    template <typename P, typename A>
    auto UnnamedParams(
      Vec<P> const &p_names,
      Vec<A> const &a_names)
      -> Vec<P> {
      return p_names
        | genex::views::not_in(a_names, genex::meta::deref, genex::meta::deref)
        | genex::to<Vec>();
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
      // An "invalid" generic argument name is for example "T=S32"
      // where "T" doesn't appear in the generic parameter list.
      const auto a_names = a_group.GetKeywordArgs()
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();
      const auto all_p_names = params
        | genex::views::transform([](auto *x) { return x->Name; })
        | genex::to<Vec>();
      EnforceNamesKnown(all_p_names, a_names, params.IsEmpty() ? nullptr : params[0], "gn", sm.CurrentScope);

      // The leftover parameters, which don't already have args
      // bound to them, are what the positional arguments name.
      auto p_names = UnnamedParams(all_p_names, a_names);

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

        // The variadic parameter takes a tuple of the remaining
        // arguments: "f[U32, U32]" for "f[..Ts]" is "f[Ts=(U32, U32)]",
        // and "f[1_u32, 1_u32]" for "f[cmp ..s]" is "f[s=(1_u32,
        // 1_u32)]". A lone argument naming a pack ("A[Ts]" inside
        // "g[..Ts]", "A[n]" inside "g[cmp ..n: Bool]") already is
        // that tuple, so is forwarded as it is.
        if (p_names.IsEmpty() and is_variadic) {
          const auto forwards = i + 1 == a_group.Args.Len() and packs::NamesPack(*positional_arg, *sm.CurrentScope);
          auto remaining = a_group.Args | genex::views::ptr | genex::views::drop(i) | genex::to<Vec>();
          if (comp and forwards) { kw_arg->CompVal = AstClone(positional_arg->CompVal); }
          else if (comp) {
            kw_arg->CompVal = MakeUnique<TupleLiteralAst>(nullptr, remaining
              | genex::views::transform([](auto *x) { return AstClone(x->CompVal); })
              | genex::to<Vec>(), nullptr);
          }
          else {
            if (forwards) { kw_arg->TypeVal = AstClone(positional_arg->TypeVal); }
            else {
              kw_arg->TypeVal = TupleType(positional_arg->PosStart(), remaining
                | genex::views::transform([](auto *x) { return AstCloneShared(x->TypeVal); })
                | genex::to<Vec>());
            }
            if (meta.CurrentStage >= meta::CompilerStage::kResolveDeclarations) {
              kw_arg->TypeVal->Stage7_AnalyseSemantics(&sm, &meta);
            }
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
  }
}

auto spp::analyse::utils::arg_naming::NameGnArgs(
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

auto spp::analyse::utils::arg_naming::NameFnArgs(
  FunctionCallArgumentGroupAst &a_group, FunctionParameterGroupAst const &p_group,
  ScopeManager &sm, meta::CompilerMetaData *const meta,
  Vec<GenericArgumentAst*> const &generic_args, Scope *const callee_scope)
  -> void {
  // Validate the named arguments against the parameters.
  const auto a_names = a_group.GetKeywordArgs()
    | genex::views::transform([](auto *x) { return x->Name.get(); })
    | genex::to<Vec>();
  const auto all_p_names = p_group.GetAllParams()
    | genex::views::transform([](auto *x) { return x->ExtractName(); })
    | genex::to<Vec>();
  EnforceFnArgNamesKnown(all_p_names, a_names, p_group, sm);

  // The leftover parameters are what the positional arguments name.
  auto p_names = UnnamedParams(all_p_names, a_names);

  // Check for the existence of a variadic parameter.
  const auto is_variadic = p_group.GetVariadicParams() != nullptr;

  for (auto [i, positional_arg] : a_group.GetPositionalArgs() | genex::views::enumerate) {
    // Create the keyword argument from the positional argument. It
    // is named after the parameter, but placed where the argument
    // was written, as the parameter's own name is in the callee.
    auto kw_arg = MakeUnique<FunctionCallArgumentKeywordAst>(
      MakeShared<IdentifierAst>(positional_arg->PosStart(), Str(p_names.Front()->Val)), nullptr, nullptr, nullptr);
    p_names |= genex::actions::pop_front();

    // The variadic parameter requires a tuple of the remaining arguments.
    // Todo: The pack drops each argument's convention, because a tuple literal has no way to hold a borrow (borrows
    //  are second-class, so "(&x, 1)" is a syntax error). That makes "Ts" infer as "Tup[S32]" where the callee sees
    //  "&S32", which is what stops "async a(&x)" resolving against "F: FunMov[(Ts), T]" - the mock's own function
    //  type keeps the convention. It also means the pack is never memory-checked, so a moved-from argument passed
    //  variadically twice goes unreported.
    if (p_names.IsEmpty() and is_variadic) {
      auto elems = a_group.Args
        | genex::views::move
        | genex::views::drop(i)
        | genex::views::transform([](auto &&x) { return asts::AstClone(x->Val); })
        | genex::to<Vec>();
      kw_arg->Val = MakeUnique<TupleLiteralAst>(nullptr, std::move(elems), nullptr);
      a_group.Args[i] = std::move(kw_arg);
      a_group.Args |= genex::actions::take(i + 1);
      break;
    }

    // Otherwise, attach the single argument convention and value.
    kw_arg->Conv = std::move(positional_arg->Conv);
    kw_arg->SetSelfType(positional_arg->GetSelfType());
    kw_arg->Val = std::move(positional_arg->Val);
    a_group.Args[i] = std::move(kw_arg);
  }

  // Put the arguments into the parameters' own order, materialising
  // a default value for every optional parameter the call left out.
  // Ordering by parameter is needed for LLVM to do an ordinal match
  // despite S++ operating with keyword-matching.
  auto ordered_args = Vec<Unique<FunctionCallArgumentAst>>();
  for (auto const *param : p_group.GetAllParams()) {
    const auto param_name = param->ExtractName();

    auto matched = false;
    for (auto &&arg : a_group.Args) {
      const auto kw_arg = arg != nullptr
        ? arg->To<FunctionCallArgumentKeywordAst>()
        : nullptr;

      if (kw_arg == nullptr or kw_arg->Name->Val != param_name->Val) { continue; }
      ordered_args.EmplaceBack(std::move(arg));
      matched = true;
      break;
    }
    if (matched) { continue; }

    // A variadic parameter the call gave nothing to must receive
    // an empty pack. This is because if we have a variadic generic
    // for a variadic function parameter: `fun f[..Ts](..a: Ts)` -
    // then no argument leaves `Ts` as "uninferred". Instead, force
    // `Tup[]`.
    if (param->To<FunctionParameterVariadicAst>() != nullptr) {
      auto empty_pack = MakeUnique<TupleLiteralAst>(
        nullptr, Vec<Unique<ExpressionAst>>(), nullptr);
      ordered_args.EmplaceBack(MakeUnique<FunctionCallArgumentKeywordAst>(
        param_name, nullptr, nullptr, std::move(empty_pack)));
      continue;
    }

    // Leftover optional parameters inject their argument into the
    // call site (unlike Python, which executes once for all func
    // calls).
    const auto optional_param = param->To<FunctionParameterOptionalAst>();
    if (optional_param == nullptr or optional_param->DefaultVal == nullptr) { continue; }

    // Translate the default out of the callee's terms as it is
    // materialised. The parameter's own type is substituted when
    // the instantiation's prototype is built, and its default is
    // an expression, so it needs the expression-level walk for
    // the same reason - otherwise "alloc: A = A()" arrives here
    // as an "A()" the caller has no "A" for.
    auto const &written = optional_param->Source.OriginalDefaultVal != nullptr
      ? optional_param->Source.OriginalDefaultVal
      : optional_param->DefaultVal;
    auto default_val = generic_args.IsEmpty()
      ? AstClone(optional_param->DefaultVal)
      : AstClone(written->SubstituteGenericsExpr(generic_args));

    // Analyse the substitution where the default was written.
    if (not generic_args.IsEmpty() and meta != nullptr) {
      const auto outer_scope = sm.CurrentScope;
      if (callee_scope != nullptr) { sm.CurrentScope = callee_scope; }
      default_val->Stage7_AnalyseSemantics(&sm, meta);
      sm.CurrentScope = outer_scope;
    }

    ordered_args.EmplaceBack(MakeUnique<FunctionCallArgumentKeywordAst>(
      param_name, nullptr, nullptr, std::move(default_val)));
  }
  a_group.Args = std::move(ordered_args);
}

auto spp::analyse::utils::arg_naming::EnforceFnArgNamesKnown(
  Vec<Shared<IdentifierAst>> const &param_names,
  Vec<IdentifierAst*> const &arg_names,
  FunctionParameterGroupAst const &params,
  ScopeManager const &sm)
  -> void {
  auto const *const param_ctx = params.Params.IsEmpty() ? nullptr : static_cast<Ast const*>(params.Params[0].get());
  EnforceNamesKnown(param_names, arg_names, param_ctx, "fn", sm.CurrentScope);
}
