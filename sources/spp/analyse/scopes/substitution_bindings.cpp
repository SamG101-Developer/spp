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

auto spp::analyse::scopes::BindArgs(
  GenericParameterGroupAst const &params, Vec<GenericArgumentAst*> const &args,
  Scope const &scope) -> GenericSubst {
  const auto declared = ParamsDeclaredBy(params);
  return BindingsOfArgs(scope.ArgsIdOf(args, &params), &declared);
}

/// [CHECKED]
auto spp::analyse::scopes::BindingsOfArgs(
  const TypeId args, TypeIdParams const *only) -> GenericSubst {
  // Start with an empty generic substitution map. A null "args"
  // binds nothing ("ArgsOf" and "FindArgOf" find nothing in it).
  auto subst = GenericSubst();

  // Bind one named argument under its parameter (a pack's is its
  // list).
  const auto bind = [&subst](TypeIdArg const &arg) {
    if (arg.TypeVal != nullptr) { subst.TypeParams.emplace_back(arg.Slot, arg.TypeVal); }
    else if (arg.CompVal != nullptr) { subst.CompParams.emplace_back(arg.Slot, arg.CompVal); }
  };

  // Every named argument, in the order they are keyed.
  if (only == nullptr) {
    for (auto const &arg : ArgsOf(args)) {
      if (arg.Named) { bind(arg); }
    }
    return subst;
  }

  // Only the listed parameters, in their order: each one's
  // argument is the one keyed by its identity.
  for (const auto pid : only->TypeParams) {
    if (const auto arg = FindArgOf(args, pid); arg.has_value() and arg->TypeVal != nullptr) { bind(*arg); }
  }
  for (const auto pid : only->CompParams) {
    if (const auto arg = FindArgOf(args, pid); arg.has_value() and arg->CompVal != nullptr) { bind(*arg); }
  }
  return subst;
}

auto spp::analyse::scopes::ArgsIdOfBindings(
  GenericSubst const &bindings, Scope const &scope) -> TypeId {
  auto key = TypeKey();
  for (auto const &[pid, id] : bindings.TypeParams) {
    if (pid == 0 or id == nullptr) { continue; }
    key.Push(TypeKey::Tag::Arg, pid);
    key.PushTypePart(scope.ReadIn(id));
  }
  for (auto const &[pid, id] : bindings.CompParams) {
    if (id == nullptr) { continue; }
    key.Push(TypeKey::Tag::Arg, pid);
    key.PushCompPart(scope.ReadCompIn(id));
  }
  return key.Words.empty() ? nullptr : InternTypeKey(std::move(key));
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
  return out;
}

auto spp::analyse::scopes::BindSelf(
  GenericSubst &subst, TypeAst const &self_type,
  Scope const &scope) -> void {
  if (const auto self_id = scope.TypeIdOf(self_type); self_id != nullptr) { subst.TypeParams.emplace_back(0, self_id); }
}

auto spp::analyse::scopes::SubstituteSelfId(
  const TypeId type, const TypeId self) -> TypeId {
  if (type == nullptr or self == nullptr or not type->HasSelf) { return type; }
  auto subst = GenericSubst();
  subst.TypeParams.emplace_back(0, self);
  return SubstituteTypeId(type, subst);
}

auto spp::analyse::scopes::InstanceBindings(
  TypeRef const &inst) -> GenericSubst {
  if (inst.Id == nullptr or HeaderOf(inst.Id).Kind != TypeKey::Tag::Inst) { return {}; }
  auto const *const tmpl = inst.Template();
  if (tmpl == nullptr or tmpl->GnParams() == nullptr) { return {}; }
  const auto params = ParamsDeclaredBy(*tmpl->GnParams());
  return BindingsOfArgs(HeaderOf(inst.Id).Args, &params);
}
