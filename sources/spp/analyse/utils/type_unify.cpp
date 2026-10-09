module spp.analyse.utils.type_unify;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_compare;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import genex;
import std;

namespace {
  namespace sc = spp::analyse::scopes;
  using spp::asts::ConventionTag;

  /// What every step of one unification reads: the two sides'
  /// scopes, the bindings made so far, and what is checked.
  struct Ctx {
    Scope const &Given;
    Scope const &Declared;
    GenericSubst &Bindings;
    bool CheckVariant;
    bool CheckConstraints;
  };

  /// "type_compare::ConventionEq" on a key's convention words (0 is none, a move): the same, or "&mut" given for "&".
  auto ConventionWordEq(const std::uint64_t given, const std::uint64_t declared) -> bool {
    const auto g = static_cast<ConventionTag>(given);
    const auto d = static_cast<ConventionTag>(declared);
    if (g == d) { return true; }
    if (g == ConventionTag::MOV or d == ConventionTag::MOV) { return false; }
    return not(g == ConventionTag::MUT and d == ConventionTag::REF);
  }

  /// A type parameter binds "given", the same type each time it is named ("Pair[T, T]"), and, where asked, one its
  /// constraints allow (one that names no type has nothing to check). Bound even when they do not: the match fails,
  /// but what it bound is still read ("GenericSolver::_Match" on a bare parameter, whose constraints are reported
  /// later, once everything is bound).
  auto BindType(const std::uint64_t param_id, const sc::TypeId given, Ctx &ctx) -> bool {
    for (auto const &[p, bound] : ctx.Bindings.TypeParams) { if (p == param_id) { return bound == given; } }
    auto const *const param = sc::FindGnTypeParamById(param_id);
    ctx.Bindings.TypeParams.emplace_back(param_id, given);
    if (param != nullptr and param->IsVariadic) { ctx.Bindings.TypePackParams.push_back(param_id); }
    if (not ctx.CheckConstraints or param == nullptr or param->TypeConstraints.IsEmpty()) { return true; }

    // A pack's constraints hold for each of its elements, not for the tuple they are gathered into
    // ("type_compare::PackConstraintsSatisfied"): an empty pack has none to check.
    const auto holds = [&](const sc::TypeId type) {
      const auto ref = sc::TypeRef::Of(type, ctx.Given);
      return ref.Symbol == nullptr or spp::analyse::utils::type_compare::UnmetConstraint(
        param->TypeConstraints, ref, false, ctx.Declared, ctx.Given) == nullptr;
    };
    if (not param->IsVariadic) { return holds(given); }
    auto const &head = sc::HeadOf(given);
    if (head.Args == nullptr) { return true; }
    return genex::all_of(sc::ArgsOf(head.Args), [&](auto const &arg) {
      return arg.TypeVal == nullptr or holds(arg.TypeVal);
    });
  }

  /// "BindType" for a comp parameter: the same value each time it is named ("P2[n, n]").
  auto BindComp(const std::uint64_t param_id, const sc::CompId given, sc::GenericSubst &bindings) -> bool {
    for (auto const &[p, bound] : bindings.CompParams) { if (p == param_id) { return bound == given; } }
    bindings.CompParams.emplace_back(param_id, given);
    if (auto const *const param = sc::FindGnCompParamById(param_id); param != nullptr and param->IsVariadic) {
      bindings.CompPackParams.push_back(param_id);
    }
    return true;
  }

  /// The variadic type parameter a declared argument is, if it is one: a pack the rest is gathered into.
  auto TypePackParamOf(sc::TypeId declared) -> std::uint64_t {
    if (declared == nullptr) { return 0; }
    auto const &head = sc::HeadOf(declared);
    if (head.Kind != sc::TypeKey::Tag::TypeParam or head.Conv != 0 or head.TypeParamId == 0) { return 0; }
    auto const *const param = sc::FindGnTypeParamById(head.TypeParamId);
    return param != nullptr and param->IsVariadic ? head.TypeParamId : 0;
  }

  /// "TypePackParamOf" for a comp value.
  auto CompPackParamOf(sc::CompId declared) -> std::uint64_t {
    if (declared == nullptr or declared->Kind != sc::CompKey::Part::Param) { return 0; }
    auto const *const param = sc::FindGnCompParamById(declared->ParamId);
    return param != nullptr and param->IsVariadic ? declared->ParamId : 0;
  }

  /// The tuple a gathered type pack is bound to: "Tup" over the types, by position.
  auto TupleIdOf(std::vector<sc::TypeId> const &elems, sc::Scope const &scope) -> sc::TypeId {
    auto *const tup = scope.FindTypeSymbol(spp::asts::generate::common_types_precompiled::TUP.get());
    if (tup == nullptr) { return nullptr; }
    if (elems.empty()) { return sc::TypeRef::Of(*tup, scope).Id; }
    auto key = sc::TypeKey();
    for (auto i = 0uz; i < elems.size(); ++i) {
      if (elems[i] == nullptr) { return nullptr; }
      key.Push(sc::TypeKey::Tag::Pos, i);
      key.PushTypePart(elems[i]);
    }
    return sc::InstanceIdOfArgs(*tup, sc::InternTypeKey(std::move(key)));
  }

  auto UnifyType(sc::TypeId given, sc::TypeId declared, Ctx &ctx) -> bool;

  /// The arguments of two instances of one template: each declared one against the given one for the same parameter,
  /// or at the same position; a trailing variadic parameter takes what is left, as a tuple (or comp pack). A bare
  /// template given ("Vec" in its own body) stands for its own parameters.
  auto UnifyArgs(
    sc::TypeIdHead const &given, sc::TypeIdHead const &declared, sc::TypeSymbol const &tmpl, Ctx &ctx) -> bool {
    const auto declared_args = declared.Args != nullptr ? sc::ArgsOf(declared.Args) : std::vector<sc::TypeIdArg>();
    const auto bare = given.Kind == sc::TypeKey::Tag::Symbol;
    const auto given_args = not bare and given.Args != nullptr ? sc::ArgsOf(given.Args) : std::vector<sc::TypeIdArg>();
    const auto is_variadic = tmpl.Type != nullptr and tmpl.Type->GnParamGroup->GetVariadicParam() != nullptr;

    // A pattern with no arguments takes any instance of its class, of a fixed number of parameters.
    if (declared_args.empty()) { return not is_variadic or given_args.empty(); }

    auto positional = 0uz;
    for (auto i = 0uz; i < declared_args.size(); ++i) {
      auto const &d = declared_args[i];

      // For a parameter: the given argument for the same one; a bare template's own parameter is that parameter.
      if (d.Named and not d.Spelled) {
        if (bare) {
          if (d.TypeVal != nullptr and not UnifyType(sc::ParamTypeId(d.Slot), d.TypeVal, ctx)) { return false; }
          if (d.CompVal != nullptr and not spp::analyse::utils::type_unify::UnifyCompIds(
            sc::ParamCompId(d.Slot), d.CompVal, ctx.Bindings)) { return false; }
          continue;
        }
        const auto g = sc::FindArgOf(given.Args, d.Slot);
        if (not g.has_value()) { return false; }
        if (d.TypeVal != nullptr and (g->TypeVal == nullptr or not UnifyType(g->TypeVal, d.TypeVal, ctx))) { return false; }
        if (d.CompVal != nullptr and (g->CompVal == nullptr or not spp::analyse::utils::type_unify::UnifyCompIds(
          g->CompVal, d.CompVal, ctx.Bindings))) { return false; }
        continue;
      }

      // By position: the given positional arguments in order. A trailing pack takes the rest.
      auto rest = std::vector<sc::TypeIdArg>();
      for (auto j = 0uz; j < given_args.size(); ++j) {
        if (given_args[j].Named) { continue; }
        rest.push_back(given_args[j]);
      }
      const auto last = i + 1 == declared_args.size();
      if (last and d.TypeVal != nullptr) {
        if (const auto pack = TypePackParamOf(d.TypeVal); pack != 0) {
          auto elems = std::vector<sc::TypeId>();
          for (auto j = positional; j < rest.size(); ++j) {
            if (rest[j].TypeVal == nullptr) { return false; }
            elems.push_back(rest[j].TypeVal);
          }
          return BindType(pack, TupleIdOf(elems, ctx.Given), ctx);
        }
      }
      if (last and d.CompVal != nullptr) {
        if (const auto pack = CompPackParamOf(d.CompVal); pack != 0) {
          auto elems = std::vector<sc::CompKey const*>();
          for (auto j = positional; j < rest.size(); ++j) {
            if (rest[j].CompVal == nullptr) { return false; }
            elems.push_back(rest[j].CompVal);
          }
          return BindComp(pack, sc::InternCompKey(sc::CompKey::OfPack(std::move(elems))), ctx.Bindings);
        }
      }
      if (positional >= rest.size()) { return false; }
      auto const &g = rest[positional++];
      if (d.TypeVal != nullptr and (g.TypeVal == nullptr or not UnifyType(g.TypeVal, d.TypeVal, ctx))) { return false; }
      if (d.CompVal != nullptr and (g.CompVal == nullptr or not spp::analyse::utils::type_unify::UnifyCompIds(
        g.CompVal, d.CompVal, ctx.Bindings))) { return false; }
      if (last and is_variadic and positional != rest.size()) { return false; }
    }
    return true;
  }

  auto UnifyType(sc::TypeId given, sc::TypeId declared, Ctx &ctx) -> bool {
    if (given == nullptr or declared == nullptr) { return false; }
    auto const &d = sc::HeadOf(declared);

    // A parameter of the pattern binds what is opposite it: as given ("T" opposite "&Str" is "&Str"), or, written with
    // a convention ("&T"), what that convention is over ("&T" opposite "&Str" is "Str"). "Self" (0) is not one: it is
    // the enclosing type, which the pattern does not bind.
    if ((d.Kind == sc::TypeKey::Tag::TypeParam and d.TypeParamId != 0) or d.Kind == sc::TypeKey::Tag::TypeBound) {
      if (d.Conv == 0) { return BindType(d.TypeParamId, given, ctx); }
      return ConventionWordEq(sc::HeadOf(given).Conv, d.Conv) and BindType(d.TypeParamId, sc::BareOf(given), ctx);
    }

    // The same type fits itself ("Self" opposite "Self", a closed type opposite itself); one naming parameters is
    // still walked, so each binds itself.
    if (given == declared and not sc::DoesTypeIdNameAnyGnParams(declared)) { return true; }
    auto const &g = sc::HeadOf(given);
    if (not ConventionWordEq(g.Conv, d.Conv)) { return false; }

    // A variant pattern takes any of its members.
    if (ctx.CheckVariant and d.Kind == sc::TypeKey::Tag::Variant) {
      for (const auto member : d.Members) {
        auto saved = ctx.Bindings;
        if (UnifyType(sc::BareOf(given), member, ctx)) { return true; }
        ctx.Bindings = std::move(saved);
      }
    }

    // Otherwise the same class, its arguments matched.
    auto const *const g_sym = g.Symbol();
    auto const *const d_sym = d.Symbol();
    if (g_sym == nullptr or d_sym == nullptr or g_sym->Type != d_sym->Type) { return false; }
    if (d.Kind == sc::TypeKey::Tag::Variant and g.Kind == sc::TypeKey::Tag::Variant) {
      // A pack among the pattern's members ("Var[Variants]", "Var[None, Rest]") takes the members the others leave, as
      // a tuple in their canonical order: members are a set, so each other one takes whichever given member fits it.
      const auto pack = genex::find_if(d.Members, [](const sc::TypeId m) { return TypePackParamOf(m) != 0; });
      if (pack != d.Members.end()) {
        auto used = std::vector<bool>(g.Members.size(), false);
        for (const auto member : d.Members) {
          if (member == *pack) { continue; }
          auto found = false;
          for (auto i = 0uz; i < g.Members.size() and not found; ++i) {
            if (used[i]) { continue; }
            auto saved = ctx.Bindings;
            if (UnifyType(g.Members[i], member, ctx)) { used[i] = found = true; }
            else { ctx.Bindings = std::move(saved); }
          }
          if (not found) { return false; }
        }
        auto rest = std::vector<sc::TypeId>();
        for (auto i = 0uz; i < g.Members.size(); ++i) { if (not used[i]) { rest.push_back(g.Members[i]); } }
        return BindType(TypePackParamOf(*pack), TupleIdOf(rest, ctx.Given), ctx);
      }
      if (g.Members.size() != d.Members.size()) { return false; }
      for (auto i = 0uz; i < d.Members.size(); ++i) {
        if (not UnifyType(g.Members[i], d.Members[i], ctx)) { return false; }
      }
      return true;
    }
    return UnifyArgs(g, d, *d_sym, ctx);
  }
}

auto spp::analyse::utils::type_unify::UnifyTypeIds(
  const scopes::TypeId given,
  const scopes::TypeId declared,
  Scope const &given_scope,
  Scope const &declared_scope,
  scopes::GenericSubst &bindings,
  const bool check_variant,
  const bool check_constraints)
  -> bool {
  auto ctx = Ctx{
    .Given = given_scope, .Declared = declared_scope, .Bindings = bindings, .CheckVariant = check_variant,
    .CheckConstraints = check_constraints
  };
  return UnifyType(given, declared, ctx);
}

auto spp::analyse::utils::type_unify::UnifyCompIds(
  const scopes::CompId given,
  const scopes::CompId declared,
  scopes::GenericSubst &bindings)
  -> bool {
  if (given == nullptr or declared == nullptr) { return false; }
  if (declared->Kind == scopes::CompKey::Part::Param) { return BindComp(declared->ParamId, given, bindings); }

  // Two packs element by element; a trailing variadic parameter of the pattern takes the rest as a pack of its own.
  if (declared->Kind == scopes::CompKey::Part::Pack and given->Kind == scopes::CompKey::Part::Pack) {
    auto const &d = declared->Kids;
    auto const &g = given->Kids;
    const auto pack = d.empty() ? 0 : CompPackParamOf(d.back());
    const auto fixed = pack != 0 ? d.size() - 1 : d.size();
    if (pack != 0 ? g.size() < fixed : g.size() != fixed) { return false; }
    for (auto i = 0uz; i < fixed; ++i) { if (not UnifyCompIds(g[i], d[i], bindings)) { return false; } }
    if (pack == 0) { return true; }
    auto rest = std::vector<scopes::CompKey const*>(g.begin() + static_cast<std::ptrdiff_t>(fixed), g.end());
    return BindComp(pack, scopes::InternCompKey(scopes::CompKey::OfPack(std::move(rest))), bindings);
  }
  return given == declared;
}
