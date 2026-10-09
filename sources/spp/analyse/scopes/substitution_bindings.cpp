module spp.analyse.scopes.substitution;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.asts.generic_argument_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;

auto spp::analyse::scopes::ParamsDeclaredBy(
  GenericParameterGroupAst const &params) -> TypeIdParams {
  auto out = TypeIdParams();
  for (auto const &param : params.Params) {
    if (const auto pid = param->ParamId(); pid == 0) { continue; }
    else if (param->IsCompParam()) { out.CompParams.push_back(pid); }
    else { out.TypeParams.push_back(pid); }
  }
  return out;
}

auto spp::analyse::scopes::BindByName(
  TypeIdParams const &params, const TypeId args,
  const bool all) -> std::optional<GenericSubst> {
  // Each parameter's argument is the one keyed by its identity ("Scope::ArgsIdOf").
  if (args == nullptr) { return std::nullopt; }
  auto subst = GenericSubst();
  for (const auto pid : params.TypeParams) {
    const auto arg = FindArgOf(args, pid);
    if (not arg.has_value() or arg->TypeVal == nullptr) {
      if (all) { return std::nullopt; }
      continue;
    }
    subst.TypeParams.emplace_back(pid, arg->TypeVal);
    if (auto const *const param = FindGnTypeParamById(pid); param != nullptr and param->IsVariadic) {
      subst.TypePackParams.push_back(pid);
    }
  }
  for (const auto pid : params.CompParams) {
    const auto arg = FindArgOf(args, pid);
    if (not arg.has_value() or arg->CompVal == nullptr) {
      if (all) { return std::nullopt; }
      continue;
    }
    subst.CompParams.emplace_back(pid, arg->CompVal);
    if (auto const *const param = FindGnCompParamById(pid); param != nullptr and param->IsVariadic) {
      subst.CompPackParams.push_back(pid);
    }
  }
  return subst;
}

auto spp::analyse::scopes::BindArgs(
  GenericParameterGroupAst const &params, Vec<GenericArgumentAst*> const &args,
  Scope const &scope) -> GenericSubst {
  return BindByName(ParamsDeclaredBy(params), scope.ArgsIdOf(args, &params), false)
    .value_or(GenericSubst());
}

auto spp::analyse::scopes::BindingsFor(
  GenericSubst const &bindings, GenericParameterGroupAst const &params)
  -> GenericSubst {
  const auto declared = [&](const std::uint64_t pid) {
    return pid != 0 and genex::any_of(params.Params, [pid](auto const &p) { return p->ParamId() == pid; });
  };
  auto out = GenericSubst();
  for (auto const &[pid, id] : bindings.TypeParams) { if (declared(pid)) { out.TypeParams.emplace_back(pid, id); } }
  for (auto const &[pid, id] : bindings.CompParams) { if (declared(pid)) { out.CompParams.emplace_back(pid, id); } }
  for (const auto pid : bindings.TypePackParams) { if (declared(pid)) { out.TypePackParams.push_back(pid); } }
  for (const auto pid : bindings.CompPackParams) { if (declared(pid)) { out.CompPackParams.push_back(pid); } }
  return out;
}

auto spp::analyse::scopes::BindSelf(
  GenericSubst &subst, TypeAst const &self_type,
  Scope const &scope) -> void {
  if (const auto self_id = scope.TypeIdOf(self_type); self_id != nullptr) { subst.TypeParams.emplace_back(0, self_id); }
}

auto spp::analyse::scopes::InstanceBindings(
  TypeRef const &inst) -> GenericSubst {
  if (inst.Id == nullptr or HeadOf(inst.Id).Kind != TypeKey::Tag::Inst) { return {}; }
  auto const *const tmpl = inst.Template();
  if (tmpl == nullptr or tmpl->GnParams() == nullptr) { return {}; }
  const auto params = ParamsDeclaredBy(*tmpl->GnParams());
  return BindByName(params, HeadOf(inst.Id).Args, false).value_or(GenericSubst());
}
