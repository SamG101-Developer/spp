module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_resolution;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.instance_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.generic_inference;
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
import spp.asts.type_postfix_expression_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.utils.ast_utils;
import spp.utils.interner;
import spp.utils.ptr;
import genex;
import std;

auto spp::analyse::utils::type_resolution::AnalyseWrittenType(
  TypeAst const &written, ScopeManager &sm, meta::CompilerMetaData &meta,
  const SelfPolicy self) -> Shared<TypeAst> {
  // "Self" replaced where asked, which analyses what it replaced; anything else is analysed here.
  const auto self_type = self == SelfPolicy::kSubstitute ? sm.CurrentScope->FindEnclosingSelfType(meta) : nullptr;
  auto t = self_type::SubstituteSelf(written, self_type.get(), &sm, &meta);
  if (self_type == nullptr or not type_predicates::DoesTypeNameSelf(written)) {
    t->Stage7_AnalyseSemantics(&sm, &meta);
  }
  return t;
}

auto spp::analyse::utils::type_resolution::AnalyseWrittenComp(
  ExpressionAst const &written,
  Scope const &scope)
  -> Unique<ExpressionAst> {
  if (auto folded = comp_generics::FoldCompExpr(written, scope); folded != nullptr) { return folded; }
  auto const *const id = written.To<IdentifierAst>();
  auto const *const var = id != nullptr ? scope.FindVarSymbol(id) : nullptr;
  auto const *const bound = var != nullptr ? var->BoundCompVal() : nullptr;
  return bound != nullptr ? AstClone(bound) : nullptr;
}



auto spp::analyse::utils::type_resolution::BindArgs(
  GenericParameterGroupAst const &params,
  Vec<GenericArgumentAst*> const &args,
  Scope const &scope)
  -> scopes::GenericSubst {
  return scopes::BindByName(scopes::ParamsOfGroup(params), scope.ArgsIdOf(args), false)
    .value_or(scopes::GenericSubst());
}

auto spp::analyse::utils::type_resolution::ParamsOfArgs(
  Vec<GenericArgumentAst*> const &args,
  Vec<GenericParameterAst*> const &params)
  -> Vec<GenericParameterAst*> {
  const auto named = [&args](GenericParameterAst const *param) {
    return genex::any_of(args, [param](auto const *arg) {
      return arg->TypeName() != nullptr and *arg->TypeName() == *param->Name;
    });
  };
  auto next = std::array<std::size_t, 2>{0, 0};
  auto out = Vec<GenericParameterAst*>();
  for (auto const *arg : args) {
    if (arg->TypeName() != nullptr) {
      const auto it = genex::find_if(params, [arg](auto const *param) { return *param->Name == *arg->TypeName(); });
      out.EmplaceBack(it != params.end() ? *it : nullptr);
      continue;
    }
    const auto comp = arg->IsCompArg();
    auto &i = next[comp ? 1 : 0];
    while (i < params.Len() and (params[i]->IsCompParam() != comp or named(params[i]))) { ++i; }
    out.EmplaceBack(i < params.Len() ? params[i] : nullptr);
    if (i < params.Len() and not params[i]->IsVariadic()) { ++i; }
  }
  return out;
}

auto spp::analyse::utils::type_resolution::BindInferred(
  type_compare::GenericInferenceMap const &inferred,
  GenericParameterGroupAst const &params,
  Scope const &scope)
  -> scopes::GenericSubst {
  auto subst = scopes::GenericSubst();
  for (auto const &param : params.Params) {
    const auto pid = param->ParamId();
    const auto hit = inferred.find(dynamic_shared_cast<TypeIdentifierAst>(param->Name));
    if (pid == 0 or hit == inferred.end()) { continue; }
    const auto is_pack = param->IsVariadic();
    if (param->IsCompParam()) {
      if (const auto id = scope.CompIdOf(*hit->second); id != 0) {
        subst.CompParams.emplace_back(pid, id);
        if (is_pack) { subst.CompPackParams.push_back(pid); }
      }
      continue;
    }
    auto const *const type = hit->second->To<TypeAst>();
    if (const auto id = type != nullptr ? scope.TypeIdOf(*type) : nullptr; id != nullptr) {
      subst.TypeParams.emplace_back(pid, id);
      if (is_pack) { subst.TypePackParams.push_back(pid); }
    }
  }
  return subst;
}

auto spp::analyse::utils::type_resolution::BindSelf(
  scopes::GenericSubst &subst,
  TypeAst const &self_type,
  Scope const &scope)
  -> void {
  if (const auto self_id = scope.TypeIdOf(self_type); self_id != nullptr) { subst.TypeParams.emplace_back(0, self_id); }
}

auto spp::analyse::utils::type_resolution::InstanceBindings(
  TypeRef const &inst)
  -> scopes::GenericSubst {
  if (inst.Id == nullptr or scopes::HeadOf(inst.Id).Kind != scopes::InstanceKey::Tag::Inst) { return {}; }
  auto const *const tmpl = inst.Template();
  if (tmpl == nullptr or tmpl->GnParams() == nullptr) { return {}; }
  const auto params = scopes::ParamsOfGroup(*tmpl->GnParams());
  return scopes::BindByName(params, scopes::HeadOf(inst.Id).Args, false).value_or(scopes::GenericSubst());
}

namespace {
  /// "ReadType", null where the type does not resolve.
  auto ReadTypeOrNull(
    spp::asts::TypeAst const &written,
    spp::analyse::scopes::ExprSubst const &sub)
    -> spp::Shared<spp::asts::TypeAst> {
    const auto id = spp::analyse::scopes::SubstituteTypeId(sub.Written->TypeIdOf(written), sub.Bindings);
    auto out = id != nullptr ? sub.Reading->TypeAstOf(id) : nullptr;
    return out != nullptr ? out->WithSourceSpanOf(written) : nullptr;
  }
}

auto spp::analyse::utils::type_resolution::ReadType(
  TypeAst const &written,
  scopes::ExprSubst const &sub)
  -> Shared<TypeAst> {
  auto out = ReadTypeOrNull(written, sub);
  return out != nullptr ? out : AstCloneShared(&written);
}

auto spp::analyse::utils::type_resolution::ReadTypeDefault(
  GenericParameterAst const &param,
  scopes::GenericSubst bindings,
  Scope const &written_in,
  Scope const &read_in)
  -> Shared<TypeAst> {
  if (param.TypeDefault == nullptr) { return nullptr; }
  return ReadTypeOrNull(*param.TypeDefault, ExprSubst::Across(written_in, std::move(bindings), read_in));
}

auto spp::analyse::utils::type_resolution::ReadCompDefault(
  GenericParameterAst const &param,
  scopes::GenericSubst bindings,
  Scope const &written_in,
  Scope const &read_in)
  -> Shared<ExpressionAst> {
  if (param.WrittenCompDefault == nullptr) { return nullptr; }
  return ReadComp(*param.WrittenCompDefault, ExprSubst::Across(written_in, std::move(bindings), read_in));
}

namespace {
  /// A comp value read by its identity alone; null when it has an opaque part, or none.
  auto ReadCompId(
    spp::asts::ExpressionAst const &written,
    spp::analyse::scopes::ExprSubst const &sub)
    -> spp::Shared<spp::asts::ExpressionAst> {
    namespace scopes = spp::analyse::scopes;
    const auto id = sub.Written->CompIdOf(written);
    auto const *const node = scopes::CompNodeOf(id);
    const auto opaque = [](scopes::CompNode const &part) { return part.Kind == scopes::CompNode::Part::Opaque; };
    if (node == nullptr or node->Any(opaque)) { return nullptr; }
    const auto rewritten = scopes::SubstituteCompId(id, sub.Bindings);
    return rewritten != 0 ? sub.Reading->CompAstOf(rewritten) : nullptr;
  }
}

auto spp::analyse::utils::type_resolution::ReadComp(
  ExpressionAst const &written,
  scopes::ExprSubst const &sub)
  -> Shared<ExpressionAst> {
  if (auto value = ReadCompId(written, sub); value != nullptr) { return value; }
  if (written.To<IdentifierAst>() != nullptr) { return AstCloneShared(&written); }
  return written.ReadExpr(sub);
}

auto spp::analyse::utils::type_resolution::DoesTypeNameAGnParam(
  TypeAst const &type,
  GenericParameterAst const &param,
  Scope const &scope)
  -> bool {
  // The parameter's identity, given when declared ("GenericParameterAst::Stage2_GenTopLvlScopes"); each part is the parameter (or a
  // binding of it) it resolves to where read.
  const auto type_param = param.IsTypeParam() ? param.ParamId() : 0;
  const auto comp_param = param.IsCompParam() ? param.ParamId() : 0;
  return type_predicates::AnyTypePart(
    type,
    [&](TypeIdentifierAst const &part) {
      auto const *const sym = type_param != 0 ? scope.FindTypeSymbol(&part) : nullptr;
      return sym != nullptr and sym->ParamId() == type_param;
    },
    [&](IdentifierAst const &name) {
      auto const *const sym = comp_param != 0 ? scope.FindVarSymbol(&name) : nullptr;
      return sym != nullptr and sym->ParamId() == comp_param;
    });
}

auto spp::analyse::utils::type_resolution::RecordWrittenType(
  TypeAst const &type,
  TypeSymbol const &sym)
  -> void {
  // A bare generic template is not a type yet (its arguments are filled in where it is analysed), and neither is a
  // "use" of one ("Str" through the prelude's "use std::string::Str").
  if (type.WrittenTypeId() != nullptr or sym.IsBareTemplate() or sym.UseTarget()->IsBareTemplate()) { return; }
  if (sym.IsGn() and sym.ParamId() == 0) { return; }

  // An identity with an unresolved part ("A[n=Buf::n]" before "Buf::n" can be read) is no identity yet: stamped, every
  // later read would key that, rather than what it resolves to by then.
  const auto id = scopes::WrittenTypeIdOf(sym);
  if (id == nullptr or id->HasUnresolved) { return; }
  type.SetWrittenTypeId(id);
}

auto spp::analyse::utils::type_resolution::RecordWrittenComp(
  IdentifierAst const &name,
  VariableSymbol const &sym)
  -> void {
  if (name.WrittenCompParamId() != 0 or not sym.IsGn()) { return; }
  if (const auto param = sym.ParamId(); param != 0) { name.SetWrittenCompParamId(param); }
}

auto spp::analyse::utils::type_resolution::RecordTypeParts(
  TypeAst const &type,
  Scope const &scope)
  -> void {
  // A nested name ("A::B") is recorded on its left-hand side, as the part walkers read it.
  if (auto const *const postfix = type.To<TypePostfixExpressionAst>(); postfix != nullptr) {
    RecordTypeParts(*postfix->Lhs, scope);
    return;
  }

  // The name is looked up whole, so a namespace before it ("std::number::U8") is where it is found, not this scope
  // ("U8" here can be a "use" of it, or nothing).
  auto const *const part = type.LastTypePart();
  if (part == nullptr) { return; }
  if (not part->GnArgGroup->Args.IsEmpty()) {
    // A name written with arguments has the template it instantiates at its head, whatever the arguments become. A
    // "use" of the template is an alias here, private to this module, so the head is the template it names.
    auto const *tmpl = scope.FindHeadSymbol(type);
    if (tmpl != nullptr) { tmpl = tmpl->UseTarget(); }
    if (tmpl != nullptr and not tmpl->IsGn()) { part->SetWrittenTemplateId(scopes::WrittenTypeIdOf(*tmpl)); }
  }
  else if (auto const *const sym = scope.FindTypeSymbol(&type); sym != nullptr) {
    RecordWrittenType(*part, *sym);
  }

  // Its arguments are written here too: each type whole, each comp value's names and the types it names constants
  // through.
  for (auto const &arg : part->GnArgGroup->Args) {
    if (arg->IsTypeArg()) {
      RecordTypeParts(*arg->TypeVal, scope);
      continue;
    }
    static_cast<void>(type_predicates::AnyCompPart(
      *arg->CompVal,
      [&scope](IdentifierAst const &name) {
        if (auto const *const sym = scope.FindVarSymbol(&name); sym != nullptr) { RecordWrittenComp(name, *sym); }
        return false;
      },
      [&scope](TypeAst const &owner) {
        RecordTypeParts(owner, scope);
        return false;
      }));
  }
}

auto spp::analyse::utils::type_resolution::RecordCompParts(
  ExpressionAst const &value,
  Scope const &scope)
  -> void {
  static_cast<void>(type_predicates::AnyCompPart(
    value,
    [&scope](IdentifierAst const &name) {
      if (auto const *const sym = scope.FindVarSymbol(&name); sym != nullptr) { RecordWrittenComp(name, *sym); }
      return false;
    },
    [&scope](TypeAst const &owner) {
      RecordTypeParts(owner, scope);
      return false;
    }));
}

auto spp::analyse::utils::type_resolution::RecordedArgsFor(
  GenericArgumentGroupAst const &args,
  GenericParameterGroupAst const &params,
  Scope const &scope,
  Scope const &decl_scope)
  -> Vec<Unique<GenericArgumentAst>> {
  auto out = Vec<Unique<GenericArgumentAst>>();
  if (params.GetVariadicParam() != nullptr) { return out; }

  // Each argument names its parameter as the solver names it ("ParamsOfArgs").
  const auto all_args = args.GetAllArgs();
  const auto all_params = params.GetAllParams();
  const auto targets = ParamsOfArgs(all_args, all_params);
  for (auto *const param : all_params) {
    const auto at = genex::find(targets, param);
    auto const *const given = at != targets.end() ? all_args[static_cast<std::size_t>(at - targets.begin())] : nullptr;
    if (given != nullptr and given->IsTypeArg()) {
      out.EmplaceBack(GenericArgumentAst::NewType(param->Name, given->TypeVal));
      continue;
    }
    if (given != nullptr and given->IsCompArg()) {
      out.EmplaceBack(GenericArgumentAst::NewComp(param->Name, AstCloneShared(given->CompVal)));
      continue;
    }

    // A default, read with the arguments before it bound.
    if (not param->IsOptional()) { continue; }
    const auto so_far = out | genex::views::ptr | genex::to<Vec>();
    auto bindings = BindArgs(params, so_far, scope);
    if (param->IsTypeParam()) {
      auto type = ReadTypeDefault(*param, std::move(bindings), decl_scope, scope);
      if (type == nullptr) { return {}; }
      out.EmplaceBack(GenericArgumentAst::NewType(param->Name, std::move(type)));
    }
    else {
      out.EmplaceBack(GenericArgumentAst::NewComp(
        param->Name, ReadCompDefault(*param, std::move(bindings), decl_scope, scope)));
    }
  }
  return out;
}
