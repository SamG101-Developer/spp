module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.aliases;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.generic_inference;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_predicates;
import spp.asts.class_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;
import genex;
import std;

auto spp::analyse::utils::aliases::StatementTarget(
  TypeStatementAst const &alias_stmt,
  const bool from_use_stmt,
  Scope *tracking_scope,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*, TypeSymbol*> {
  const auto filter_params = [](GenericParameterGroupAst const &pg, GenericArgumentGroupAst const &ag) {
    auto out = GenericParameterGroupAst::NewEmptyShared();
    const auto name_of = [](auto const *param) { return param->Name->LastTypePart()->Name.c_str(); };
    for (auto const &param : pg.Params) {
      if (ag.At(name_of(param.get())) == nullptr) { out->Params.EmplaceBack(AstClone(param)); }
    }
    return out;
  };

  // Each head along the alias chain, raising on a bad identifier
  // rather than crashing on it.
  const auto lookup = [&](TypeAst const &ty) {
    return member_lookup::FindTypeSymbolOrError(*tracking_scope, *ty.WithoutGns(), *sm);
  };

  // An alias named inside its own target's arguments ("type X =
  // Vec[X]") expands forever, just as "type X = X" does, and
  // used to overflow the stack in the instance lookup. Compared
  // by symbol, read where the statement is written: a part that
  // is the alias itself.
  if (not from_use_stmt) {
    auto const *const self_sym = sm->CurrentScope->FindHeadSymbol(*alias_stmt.NewType);
    const auto names_self = [&](TypeIdentifierAst const &part) {
      return self_sym != nullptr and sm->CurrentScope->FindHeadSymbol(part) == self_sym;
    };
    for (auto const &arg : alias_stmt.OldType->LastTypePart()->GnArgGroup->Args) {
      RaiseIf<errors::SppTypeAliasCyclicError>(
        arg->IsTypeArg() and type_predicates::AnyTypePart(*arg->TypeVal, names_self),
        {sm->CurrentScope}, ERR_ARGS(alias_stmt, *arg));
    }
  }

  // The immediate target, and its symbol.
  auto old_type = alias_stmt.OldType;
  auto old_sym = lookup(*old_type);

  // If this is a use statement to a class, then grab its
  // generics and return immediately. For example, use
  // Vec::Vec => type Vec[T, A: ... = ...] = Vec::Vec[T=T, A=A]
  if (from_use_stmt and old_sym->Alias == nullptr) {
    auto generic_params = old_sym->Type->GnParamGroup;
    old_type = old_type->WithGns(GenericArgumentGroupAst::FromParams(*generic_params));
    return {old_type, generic_params, old_sym->LinkedScope, old_sym};
  }

  // The target as written, its arguments named by the parameters of what it names. Nothing is substituted: an alias
  // of an alias records the one it names, and is read through it by identity ("Scope::TypeIdOf" keys an alias as its
  // target), flattened once its statement is resolved ("TypeStatementAst::Stage4_ResolveDeclarations"). Parameters of
  // a target "type" alias that are not given here are this alias's too, passed straight on ("use std::result::Res").
  // A "use" passes its arguments straight to what it names (whose own parameters a "use" only adopts at its own
  // stage 3), so the arguments are named by, and only a "type" alias at the end of the uses carries, what that is.
  auto const *const carrier = old_sym->UseTarget();
  const auto is_tuple = type_predicates::IsTypeTuple(
    TypeRef::ForKindCheck(*carrier, *sm->CurrentScope), *sm->CurrentScope);
  auto const *const params_scope = carrier->GnParamsScope();
  auto named = generic_inference::NamedGnArgs(
    *old_type->LastTypePart()->GnArgGroup,
    carrier->GnParams() != nullptr ? *carrier->GnParams() : *GenericParameterGroupAst::NewEmpty(),
    params_scope != nullptr ? *params_scope : *sm->CurrentScope, *old_type, *sm, *meta, is_tuple);
  auto attach = carrier->Alias != nullptr and not carrier->Alias->IsFromUseStmt
    ? filter_params(*carrier->Alias->Params, *named)
    : GenericParameterGroupAst::NewEmptyShared();
  for (auto &&arg : GenericArgumentGroupAst::FromParams(*attach)->Args) { named->Args.EmplaceBack(std::move(arg)); }
  old_type = old_type->WithGns(std::move(named));

  // The chain is followed only to reject a cycle and to find the class at its end, which the alias links and whose
  // scope its instantiations are attached in. Statements are recorded rather than symbols, because a statement is
  // what the source wrote and so what the error can point at; the starting one is seeded so that a self-alias
  // ("type A = A") is caught on its first step.
  auto followed_aliases = Vec{&alias_stmt};
  auto *final_sym = old_sym;
  while (final_sym->Alias != nullptr) {
    // The alias closing the cycle is shown from where it is
    // declared, which can be another file than this one.
    auto const *const closing_scope = final_sym->Alias->DeclaredIn();
    RaiseIf<errors::SppTypeAliasCyclicError>(
      genex::contains(followed_aliases, final_sym->Alias->Stmt),
      {sm->CurrentScope, closing_scope != nullptr ? closing_scope : sm->CurrentScope},
      ERR_ARGS(alias_stmt, *final_sym->Alias->Stmt));
    followed_aliases.EmplaceBack(final_sym->Alias->Stmt);
    tracking_scope = final_sym->ScopeDefinedIn;
    final_sym = lookup(*final_sym->Alias->Written);
  }
  return {old_type, attach, final_sym->ScopeDefinedIn, final_sym};
}

auto spp::analyse::utils::aliases::InstanceTargetOf(
  TypeSymbol const &alias, const scopes::TypeId id,
  Scope const &scope) -> Shared<TypeAst> {
  // Filed under the class it names (a "use" of the class), the identity is the target already.
  if (id == nullptr or scopes::HeadOf(id).Kind != scopes::TypeKey::Tag::Inst) { return nullptr; }
  if (scopes::HeadOf(id).Symbol() != &alias) { return scope.TypeAstOf(id); }

  // A "use" stands for what it names under the same arguments: the identity re-headed onto that. One whose name
  // does not lead past itself is read as any alias, off its recorded target.
  auto *const used = alias.Alias->IsFromUseStmt ? alias.UseTarget() : nullptr;
  if (used != nullptr and used != &alias) {
    const auto reheaded = scopes::InstanceIdOfArgs(*used, scopes::HeadOf(id).Args);
    return used->Alias != nullptr ? InstanceTargetOf(*used, reheaded, scope) : scope.TypeAstOf(reheaded);
  }

  // Its target, by identity, with its parameters bound to the instantiation's arguments ("AliasTargetId").
  const auto target = alias.AliasTargetId(scopes::HeadOf(id).Args);
  return target != nullptr ? scope.TypeAstOf(target) : nullptr;
}
