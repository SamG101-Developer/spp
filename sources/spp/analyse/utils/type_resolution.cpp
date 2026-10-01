module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_resolution;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.statement_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import spp.utils.interner;
import spp.utils.ptr;
import genex;
import std;

auto spp::analyse::utils::type_resolution::ThroughAlias(
  TypeAst const &type,
  Scope const &scope)
  -> Shared<const TypeAst> {
  // Only a written alias head is looked through, to the name of what the type resolves to ("TypeRef" never answers
  // with an alias). A copy is handed back: a symbol's name is shared by every use of it.
  const auto head = scope.GetTypeSymbol(type.WithoutGenerics().get());
  const auto full = TypeRef::Of(type, scope).Sym;
  if (head == nullptr or head->Alias == nullptr or full == nullptr) { return type.shared_from_this(); }
  return asts::AstCloneShared(full->FqName());
}

auto spp::analyse::utils::type_resolution::AliasStatementTarget(
  TypeStatementAst const &alias_stmt,
  const bool from_use_stmt,
  Scope *tracking_scope,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*, TypeSymbol*> {
  // How to extract generic parameters from a type symbol: alias, then type, otherwise none (generic).
  const auto NO_PARAMS = GenericParameterGroupAst::NewEmpty();
  const auto extract_params = [&NO_PARAMS](TypeSymbol const &ts) {
    return ts.Alias
      ? ts.Alias->Params.get()
      : ts.Type
      ? ts.Type->GnParamGroup.get()
      : NO_PARAMS.get();
  };

  const auto filter_params = [](GenericParameterGroupAst const &pg, GenericArgumentGroupAst const &ag) {
    auto out = GenericParameterGroupAst::NewEmptyShared();
    const auto name_of = [](auto const *param) { return param->Name->LastTypePart()->Name.c_str(); };
    for (auto const &param : pg.Params) {
      if (ag.At(name_of(param.get())) == nullptr) { out->Params.EmplaceBack(AstClone(param)); }
    }
    return out;
  };

  // Consistent lookup function used multiple times
  // throughout this function, preventing crashing on
  // bad identifiers throughout the alias chain.
  const auto lookup = [&](TypeAst const &ty) {
    const auto sym = tracking_scope->GetTypeSymbol(ty.WithoutGenerics().get());
    if (sym == nullptr) {
      member_lookup::RaiseMissingTypeIdentifierAndClosestOptions(
        *ty.LastTypePart(), tracking_scope->AllTypeSymbols(), *sm);
    }
    return sym;
  };

  // An alias named inside its own target's arguments ("type X =
  // Vec[X]") expands forever, just as "type X = X" does, and used
  // to overflow the stack in the instance lookup.
  if (not from_use_stmt) {
    const auto alias_name = alias_stmt.NewType->Name;
    for (auto const &arg : alias_stmt.OldType->LastTypePart()->GnArgGroup->Args) {
      RaiseIf<errors::SppTypeAliasCyclicError>(
        arg->TypeVal != nullptr and arg->TypeVal->AnyPart(
          [&alias_name](TypeIdentifierAst const &part) { return part.Name == alias_name; }),
        {sm->CurrentScope}, ERR_ARGS(alias_stmt, *arg));
    }
  }

  // The immediate target, and its symbol.
  auto old_type = alias_stmt.OldType;
  auto old_sym = lookup(*old_type);

  // If this is a use statement to a class, then grab its generics and return immediately.
  // For example, use Vec::Vec => type Vec[T, A: ... = ...] = Vec::Vec[T=T, A=A]
  if (from_use_stmt and old_sym->Alias == nullptr) {
    auto generic_params = old_sym->Type->GnParamGroup;
    old_type = old_type->WithGenerics(GenericArgumentGroupAst::FromParams(*generic_params));
    return {old_type, generic_params, old_sym->LinkedScope, old_sym};
  }

  // The target as written, its arguments named by the parameters of what it names. Nothing is substituted: an alias
  // of an alias records the one it names, and is read through it by identity ("Scope::TypeIdOf" keys an alias as its
  // target), flattened once its statement is resolved ("TypeStatementAst::Stage4_ResolveDeclarations"). Parameters of
  // a target "type" alias that are not given here are this alias's too, passed straight on ("use std::result::Res").
  // A "use" passes its arguments straight to what it names (whose own parameters a "use" only adopts at its own
  // stage 3), so the arguments are named by, and only a "type" alias at the end of the uses carries, what that is.
  auto const *carrier = old_sym;
  while (carrier->Alias != nullptr and carrier->Alias->FromUseStmt) {
    auto const *const used = carrier->UseTarget();
    if (used == nullptr or used == carrier) { break; }
    carrier = used;
  }
  const auto is_tuple = type_predicates::IsTypeTup(*carrier, *sm->CurrentScope);
  auto named = generic_inference::NamedGnArgs(
    *old_type->LastTypePart()->GnArgGroup, *extract_params(*carrier), *old_type, *sm, *meta, is_tuple);
  auto attach = carrier->Alias != nullptr and not carrier->Alias->FromUseStmt
    ? filter_params(*carrier->Alias->Params, *named)
    : GenericParameterGroupAst::NewEmptyShared();
  for (auto &&arg : GenericArgumentGroupAst::FromParams(*attach)->Args) { named->Args.EmplaceBack(std::move(arg)); }
  old_type = old_type->WithGenerics(std::move(named));

  // The chain is followed only to reject a cycle and to find the class at its end, which the alias links and whose
  // scope its instantiations are attached in. Statements are recorded rather than symbols, because a statement is
  // what the source wrote and so what the error can point at; the starting one is seeded so that a self-alias
  // ("type A = A") is caught on its first step.
  auto followed_aliases = Vec{&alias_stmt};
  auto *final_sym = old_sym;
  while (final_sym->Alias != nullptr) {
    RaiseIf<errors::SppTypeAliasCyclicError>(
      genex::contains(followed_aliases, final_sym->Alias->Stmt),
      {sm->CurrentScope}, ERR_ARGS(alias_stmt, *final_sym->Alias->Stmt));
    followed_aliases.EmplaceBack(final_sym->Alias->Stmt);
    tracking_scope = final_sym->ScopeDefinedIn;
    final_sym = lookup(*final_sym->Alias->Written);
  }
  return {old_type, attach, final_sym->ScopeDefinedIn, final_sym};
}

auto spp::analyse::utils::type_resolution::ResolveWrittenType(
  TypeAst const &written,
  ScopeManager &sm,
  meta::CompilerMetaData &meta,
  const SelfPolicy self)
  -> Shared<TypeAst> {
  // A type that had "Self" replaced is analysed as it is replaced ("SubstituteSelfTypeAndAnalyse"); any other here.
  auto substituted = false;
  auto t = self == SelfPolicy::kSubstitute
    ? self_type::SubstituteSelfTypeAndAnalyse(written, *sm.CurrentScope, sm, meta, &substituted)
    : AstClone(&written);
  if (not substituted) { t->Stage7_AnalyseSemantics(&sm, &meta); }
  return t;
}

auto spp::analyse::utils::type_resolution::ParamsOfGroup(
  GenericParameterGroupAst const &params)
  -> scopes::TypeIdParams {
  auto out = scopes::TypeIdParams();
  for (auto const &param : params.Params) {
    const auto written = param->Name->LastTypePart()->Written();
    if (const auto pid = scopes::ParamIdOf(written); pid != 0) { out.Types.push_back(pid); }
    else if (const auto cpid = scopes::CompParamIdOf(written); cpid != 0) {
      out.Comps.push_back(scopes::CompParamText(cpid));
    }
  }
  return out;
}

auto spp::analyse::utils::type_resolution::BindByName(
  scopes::TypeIdParams const &params,
  const scopes::TypeId args,
  const bool all)
  -> std::optional<scopes::TypeSubst> {
  if (args == nullptr) { return std::nullopt; }
  const auto arg_list = scopes::ArgsOf(args);
  const auto arg_named = [&arg_list](StrView name) -> scopes::TypeIdArg const* {
    const auto id = static_cast<std::uint64_t>(spp::utils::Intern(name));
    const auto it = genex::find_if(arg_list, [id](auto const &arg) { return arg.Named and arg.Name == id; });
    return it != arg_list.end() ? &*it : nullptr;
  };
  auto subst = scopes::TypeSubst();
  for (const auto pid : params.Types) {
    auto const *const param = scopes::GenericParamOf(pid);
    auto const *const arg = param != nullptr ? arg_named(param->Name->ToView()) : nullptr;
    if (arg == nullptr or arg->Type == nullptr) {
      if (all) { return std::nullopt; }
      continue;
    }
    subst.Types.emplace_back(pid, arg->Type);
    if (param->IsVariadic) { subst.TypePacks.push_back(pid); }
  }
  for (const auto comp : params.Comps) {
    auto const *const param = scopes::GenericCompParamOf(scopes::CompParamIdOfText(comp));
    auto const *const arg = param != nullptr ? arg_named(param->Name->Val) : nullptr;
    if (arg == nullptr or arg->Comp == 0) {
      if (all) { return std::nullopt; }
      continue;
    }
    subst.Comps.emplace_back(comp, arg->Comp);
    if (param->IsVariadic) { subst.CompPacks.push_back(comp); }
  }
  return subst;
}

auto spp::analyse::utils::type_resolution::BindArgs(
  GenericParameterGroupAst const &params,
  Vec<GenericArgumentAst*> const &args,
  Scope const &scope)
  -> scopes::TypeSubst {
  return BindByName(ParamsOfGroup(params), scope.InstanceIdentityKey(args), false).value_or(scopes::TypeSubst());
}

auto spp::analyse::utils::type_resolution::ReadWith(
  TypeAst const &written,
  Scope const &written_scope,
  scopes::TypeSubst const &subst,
  Scope const &scope)
  -> Shared<TypeAst> {
  const auto id = scopes::SubstituteTypeId(written_scope.TypeIdOf(written), subst);
  auto out = id != nullptr ? scope.TypeAstOf(id) : nullptr;
  if (out == nullptr) { return AstCloneShared(&written); }
  return out->WithSourceSpanOf(written);
}

auto spp::analyse::utils::type_resolution::ReadInto(
  TypeAst const &written,
  Scope const &scope,
  scopes::TypeSubst const &also)
  -> Shared<TypeAst> {
  return ReadWith(written, scope, also, scope);
}

auto spp::analyse::utils::type_resolution::RecordWrittenParts(
  TypeAst const &type,
  Scope const &scope)
  -> void {
  // Every part, nested arguments included. Comp arguments record the comp parameters they name.
  static_cast<void>(type.AnyPart([&scope](TypeIdentifierAst const &part) {
    for (auto const *comp_arg : part.GnArgGroup->GetCompArgs()) {
      if (comp_arg->CompVal != nullptr) { comp_generics::RecordCompGenerics(*comp_arg->CompVal, scope); }
    }

    // A name written with arguments has the template it instantiates at its head, whatever the arguments become. A
    // "use" of the template is an alias here, private to this module, so the head is the template it names.
    if (not part.GnArgGroup->Args.IsEmpty()) {
      // Followed through what each "use" names, not to the class at the end of the chain: a "use" of a "type" alias
      // ("SizedIntegerUnsigned[w]") stops at that alias, whose own parameters are the ones the arguments bind; its
      // target's are more ("SizedInteger[w, signed]").
      auto *tmpl = scope.GetTypeSymbol(part.WithoutGenerics().get());
      if (tmpl != nullptr) { tmpl = tmpl->UseTarget(); }
      if (tmpl != nullptr and not tmpl->IsTypeGeneric()) { part.SetTemplateWritten(Scope::WrittenIdOf(*tmpl)); }
      return false;
    }

    // A plain name records its identity when it means the same from anywhere: a parameter or a closed class - directly, or through
    // an alias of one, as a "use" of a class makes. Anything else still depends on the scope asking.
    if (part.Written() != nullptr) { return false; }
    auto *const sym = TypeRef::Of(part, scope).Sym;
    if (sym != nullptr and (
      (sym->Kind == TypeKind::GenericParam and sym->ParamId != 0)
      or (sym->Kind == TypeKind::Class and sym->IsConcrete and sym->Alias == nullptr))) {
      part.SetWritten(Scope::WrittenIdOf(*sym));
    }
    return false;
  }));
}
