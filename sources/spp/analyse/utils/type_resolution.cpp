module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_resolution;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
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
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.utils.interner;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::type_resolution {
  namespace {
    /// "ReadType", null where the type does not resolve.
    auto ReadTypeOrNull(
      TypeAst const &written, ExprSubst const &sub) -> Shared<TypeAst> {
      const auto id = SubstituteTypeId(sub.Written->TypeIdOf(written), sub.Bindings);
      const auto out = id != nullptr ? sub.Reading->TypeAstOf(id) : nullptr;
      return out != nullptr ? out->WithSourceSpanOf(written) : nullptr;
    }
  }
}

/// [CHECKED]
auto spp::analyse::utils::type_resolution::AnalyseWrittenType(
  TypeAst const &written, ScopeManager &sm, meta::CompilerMetaData &meta,
  const SelfPolicy self) -> Shared<TypeAst> {
  // Based on the "self policy", extract the "Self" from the
  // current scope. Substituting with Self=nullptr is a no-op
  // so safe to fallthrough.
  const auto self_type = self == SelfPolicy::kSubstitute
    ? sm.CurrentScope->FindEnclosingSelfType(meta)
    : nullptr;

  // Substitute the Self type's legitimate type in, and if a
  // substitution was not done (ie "written" did not contained
  // "Self"), then call the stage 7 analyse (substitution also
  // runs the analysis), for other callers.
  auto t = self_type::SubstituteSelf(written, self_type.get(), *sm.CurrentScope, &sm, &meta);
  if (self_type == nullptr or not type_predicates::DoesTypeNameSelf(written)) {
    t->Stage7_AnalyseSemantics(&sm, &meta);
  }
  return t;
}

/// [CHECKED]
auto spp::analyse::utils::type_resolution::AnalyseWrittenComp(
  ExpressionAst const &written,
  Scope const &scope) -> Unique<ExpressionAst> {
  // Try to fold the written expression ast. If it folded, then
  // it has also been analysed (like type's Self substitution),
  // so return the folded comp ast.
  if (auto folded = scope.FoldedCompAstOf(written); folded != nullptr) { return folded; }

  // If the comp expression is an identifier, then get the var
  // symbol it represents, and get that symbols "bound comp val",
  // which is pre-stored on the symbol.
  const auto id = written.To<IdentifierAst>();
  const auto var = id != nullptr ? scope.FindVarSymbol(id) : nullptr;
  const auto bound = var != nullptr ? var->BoundCompVal() : nullptr;
  return bound != nullptr ? AstClone(bound) : nullptr;
}

/// [CHECKED]
auto spp::analyse::utils::type_resolution::ParamsBoundByArgs(
  Vec<GenericArgumentAst*> const &args,
  Vec<GenericParameterAst*> const &params) -> Vec<GenericParameterAst*> {
  // Whether a keyword argument (of either kind) names the
  // parameter, which no positional argument then binds.
  const auto taken_by_keyword = [&args](GenericParameterAst const *param) {
    return genex::any_of(args, [param](auto const *arg) {
      return arg->KeywordName() != nullptr and *arg->KeywordName() == *param->Name;
    });
  };

  // One cursor per kind ([0] type, [1] comp): a positional
  // argument binds the next parameter of its own kind.
  auto next = std::array<std::size_t, 2>{0, 0};
  auto out = Vec<GenericParameterAst*>();
  for (const auto arg : args) {
    if (arg->KeywordName() != nullptr) {
      const auto it = genex::find_if(params, [arg](auto const *param) { return *param->Name == *arg->KeywordName(); });
      out.EmplaceBack(it != params.end() ? *it : nullptr);
      continue;
    }

    // A reference into "next", so advancing it moves that kind's
    // cursor. The cursor is one of the two slots in "next", for
    // type vs comp.
    const auto comp = arg->IsCompArg();
    auto &cursor = next[comp ? 1 : 0];
    while (cursor < params.Len() and (params[cursor]->IsCompParam() != comp or taken_by_keyword(params[cursor]))) {
      ++cursor;
    }
    out.EmplaceBack(cursor < params.Len() ? params[cursor] : nullptr);
    if (cursor < params.Len() and not params[cursor]->IsVariadic()) { ++cursor; }
  }
  return out;
}

auto spp::analyse::utils::type_resolution::ReadType(
  TypeAst const &written, ExprSubst const &sub) -> Shared<TypeAst> {
  auto out = ReadTypeOrNull(written, sub);
  return out != nullptr ? out : AstCloneShared(&written);
}

auto spp::analyse::utils::type_resolution::ReadTypeDefault(
  GenericParameterAst const &param, GenericSubst bindings,
  Scope const &written_in, Scope const &read_in) -> Shared<TypeAst> {
  if (param.TypeDefault == nullptr) { return nullptr; }
  return ReadTypeOrNull(*param.TypeDefault, ExprSubst::Across(written_in, std::move(bindings), read_in));
}

auto spp::analyse::utils::type_resolution::ReadCompDefault(
  GenericParameterAst const &param, GenericSubst bindings,
  Scope const &written_in, Scope const &read_in) -> Shared<ExpressionAst> {
  if (param.WrittenCompDefault == nullptr) { return nullptr; }
  return ReadComp(*param.WrittenCompDefault, ExprSubst::Across(written_in, std::move(bindings), read_in));
}

namespace {
  /// A comp value read by its identity alone; null when it has an opaque part, or none.
  auto ReadCompId(
    ExpressionAst const &written, ExprSubst const &sub)
    -> spp::Shared<ExpressionAst> {
    namespace scopes = spp::analyse::scopes;
    const auto id = sub.Written->CompIdOf(written);
    auto const *const node = id;
    const auto opaque = [](CompKey const &part) { return part.Kind == CompKey::Part::Opaque; };
    if (node == nullptr or node->Any(opaque)) { return nullptr; }
    const auto rewritten = SubstituteCompId(id, sub.Bindings);
    return rewritten != nullptr ? sub.Reading->CompAstOf(rewritten) : nullptr;
  }
}

auto spp::analyse::utils::type_resolution::ReadComp(
  ExpressionAst const &written, ExprSubst const &sub) -> Shared<ExpressionAst> {
  if (auto value = ReadCompId(written, sub); value != nullptr) { return value; }
  if (written.To<IdentifierAst>() != nullptr) { return AstCloneShared(&written); }
  return written.ReadExpr(sub);
}

auto spp::analyse::utils::type_resolution::DoesTypeNameAGnParam(
  TypeAst const &type, GenericParameterAst const &param,
  Scope const &scope) -> bool {
  // The parameter's identity, given when declared ("GenericParameterAst::Stage2_GenTopLvlScopes"); each part is the parameter (or a
  // binding of it) it resolves to where read. Walked as written, not read off the type's identity: an alias over a
  // template not made yet ("SizedIntegerUnsigned[w]") is keyed with its arguments spelled, so its identity names no
  // parameter that the written arguments do.
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

auto spp::analyse::utils::type_resolution::StampType(
  TypeAst const &type, TypeSymbol const &sym) -> void {
  // A bare generic template is not a type yet (its arguments are filled in where it is analysed), and neither is a
  // "use" of one ("Str" through the prelude's "use std::string::Str").
  if (type.StampedTypeId() != nullptr or sym.IsBareTemplate() or sym.UseTarget()->IsBareTemplate()) { return; }
  if (sym.IsGn() and sym.ParamId() == 0) { return; }

  // An identity with an unresolved part ("A[n=Buf::n]" before "Buf::n" can be read) is no identity yet: stamped, every
  // later read would key that, rather than what it resolves to by then.
  const auto id = NameTypeIdOf(sym);
  if (id == nullptr or id->HasUnresolved) { return; }
  type.StampTypeId(id);
}

auto spp::analyse::utils::type_resolution::StampComp(
  IdentifierAst const &name, VariableSymbol const &sym) -> void {
  if (name.StampedCompId() != nullptr or not sym.IsGn()) { return; }
  if (const auto param = sym.ParamId(); param != 0) { name.StampCompId(scopes::ParamCompId(param)); }
}

auto spp::analyse::utils::type_resolution::StampTypeParts(
  TypeAst const &type, Scope const &scope) -> void {
  // A nested name ("A::B") is recorded on its left-hand side, as the part walkers read it.
  if (auto const *const postfix = type.To<TypePostfixExpressionAst>(); postfix != nullptr) {
    StampTypeParts(*postfix->Lhs, scope);
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
    if (tmpl != nullptr and not tmpl->IsGn()) { part->StampTemplateId(scopes::NameTypeIdOf(*tmpl)); }
  }
  else if (auto const *const sym = scope.FindTypeSymbol(&type); sym != nullptr) {
    StampType(*part, *sym);
  }

  // Its arguments are written here too: each type whole, each comp value's names and the types it names constants
  // through.
  for (auto const &arg : part->GnArgGroup->Args) {
    if (arg->IsTypeArg()) {
      StampTypeParts(*arg->TypeVal, scope);
      continue;
    }
    static_cast<void>(type_predicates::AnyCompPart(
      *arg->CompVal,
      [&scope](IdentifierAst const &name) {
        if (auto const *const sym = scope.FindVarSymbol(&name); sym != nullptr) { StampComp(name, *sym); }
        return false;
      },
      [&scope](TypeAst const &owner) {
        StampTypeParts(owner, scope);
        return false;
      }));
  }
}

auto spp::analyse::utils::type_resolution::StampPrecompiledTypes(
  Scope const &global) -> void {
  // Only the concrete ones: a template ("Tup", "Var") is not a
  // type until its arguments are filled, so records nothing.
  using namespace asts::generate::common_types_precompiled;
  for (auto const &type : {
         BOOL, VOID, NEVER, COPY, DROP, THREAD_SAFE, CHAR, STR_VIEW,
         S8, S16, S32, S64, S128, S256, SSIZE,
         U8, U16, U32, U64, U128, U256, USIZE,
         F8, F16, F32, F64, F128
       }) {
    if (type != nullptr) { StampTypeParts(*type, global); }
  }

  // The templates a reader with no scope (a substitution)
  // compares against, read from "TEMPLATE_SYMBOLS".
  for (auto const &tmpl : {TUP, VAR}) {
    if (tmpl != nullptr) {
      static_cast<void>(scopes::PrecompiledTemplate(*tmpl, global));
    }
  }
}

auto spp::analyse::utils::type_resolution::StampCompParts(
  ExpressionAst const &value, Scope const &scope) -> void {
  static_cast<void>(type_predicates::AnyCompPart(
    value,
    [&scope](IdentifierAst const &name) {
      if (auto const *const sym = scope.FindVarSymbol(&name); sym != nullptr) { StampComp(name, *sym); }
      return false;
    },
    [&scope](TypeAst const &owner) {
      StampTypeParts(owner, scope);
      return false;
    }));
}

auto spp::analyse::utils::type_resolution::RecordedArgsFor(
  GenericArgumentGroupAst const &args, GenericParameterGroupAst const &params,
  Scope const &scope, Scope const &decl_scope) -> Vec<Unique<GenericArgumentAst>> {
  auto out = Vec<Unique<GenericArgumentAst>>();
  if (params.GetVariadicParam() != nullptr) { return out; }

  // Each argument names its parameter as the solver names it ("ParamsBoundByArgs").
  const auto all_args = args.GetAllArgs();
  const auto all_params = params.GetAllParams();
  const auto targets = ParamsBoundByArgs(all_args, all_params);
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
    auto bindings = scopes::BindArgs(params, so_far, scope);
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
