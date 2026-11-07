module;
#include <spp/macros.hpp>
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

use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeId);
use_alias(spp::analyse::scopes, TypeSymbol);
use_alias(spp::asts, GenericArgumentGroupAst);
use_alias(spp::asts, GenericParameterGroupAst);
use_alias(spp::asts, TypeAst);
use_alias(spp::asts, TypeStatementAst);
use_alias(spp::asts::meta, CompilerMetaData);

/// [CHECKED]
auto spp::analyse::utils::aliases::StatementTarget(
  TypeStatementAst const &alias_stmt, const bool from_use_stmt,
  Scope const *tracking_scope, ScopeManager *sm, CompilerMetaData *meta)
  -> AliasTarget {
  //
  const auto filter_params = [](
    GenericParameterGroupAst const &pg,
    GenericArgumentGroupAst const &ag) {
    auto out = GenericParameterGroupAst::NewEmptyShared();
    const auto name_of = [](auto const *param) { return param->Name->LastTypePart()->Name.c_str(); };
    for (auto const &param : pg.Params) {
      if (ag.At(name_of(param.get())) == nullptr) { out->Params.emplace_back(AstClone(param)); }
    }
    return out;
  };

  // Each head along the alias chain, raising on a bad identifier
  // rather than crashing on it.
  const auto lookup = [&](TypeAst const &ty) {
    return member_lookup::FindTypeSymbolOrError(*tracking_scope, *ty.WithoutGns());
  };

  // If any of the direct target's generic arguments name the new type itself,
  // then there is a cycle. This prevents "type X = Vec[X]" or "type X = X" from
  // infinitely expanding.
  if (not from_use_stmt) {
    const auto self_sym = sm->CurrentScope->FindHeadSymbol(*alias_stmt.NewType);
    const auto names_self = [&](auto const &part) {
      if (self_sym == nullptr) { return false; }
      return sm->CurrentScope->FindHeadSymbol(part) == self_sym;
    };

    auto const &args = alias_stmt.OldType->LastTypePart()->GnArgGroup->Args;
    for (auto const &arg : args) {
      RaiseIf<errors::SppTypeAliasCyclicError>(
        arg->IsTypeArg() and type_predicates::AnyTypePart(*arg->TypeVal, names_self),
        ERR_ARGS(alias_stmt, *arg));
    }
  }

  // The immediate target, and its symbol, read in the statement's own scope,
  // where its parameters are declared, so a target that is one of them
  // ("type Id[T] = T") is found as it is in any other position.
  auto old_type = alias_stmt.OldType;
  const auto old_sym = member_lookup::FindTypeSymbolOrError(
    *sm->CurrentScope, *old_type->WithoutGns());

  // If this is a use statement to a class, then grab its generics and return
  // immediately. For example, "use std::vector::Vec" gets "[T, A]".
  if (from_use_stmt and old_sym->Alias == nullptr) {
    const auto generic_params = old_sym->Type->GnParamGroup;
    old_type = old_type->WithGns(
      GenericArgumentGroupAst::FromParams(*generic_params));
    return {old_type, generic_params, old_sym->LinkedScope, old_sym};
  }

  // Shift through the "use" (not "type") aliases using "UseTarget", until a non
  // "use" type is reached. If the next type in the chain is from a "type"
  // statement, the generics to attach are from the "type" statement. Add the
  // "attach" to "named" and stick them onto the old type.
  const auto carrier = old_sym->UseTarget();
  auto named = generic_inference::NamedGnArgs(
    *old_type->LastTypePart()->GnArgGroup,
    carrier->GnParams() != nullptr ? *carrier->GnParams() : *GenericParameterGroupAst::NewEmpty(),
    carrier->LinkedScope, *old_type, *sm, *meta);
  const auto attach = carrier->Alias != nullptr and not carrier->Alias->IsFromUseStmt
    ? filter_params(*carrier->Alias->Params, *named)
    : GenericParameterGroupAst::NewEmptyShared();
  for (auto &&arg : GenericArgumentGroupAst::FromParams(*attach)->Args) {
    named->Args.emplace_back(std::move(arg));
  }
  old_type = old_type->WithGns(std::move(named));

  // The chain is followed only to reject a cycle and to find the class at its
  // end, which the alias links and whose scope its instantiations are attached
  // in. Can use pointer comparisons as types can't be declared twice.
  auto followed_aliases = Vec{&alias_stmt};
  auto final_sym = old_sym;
  while (final_sym->Alias != nullptr) {
    RaiseIf<errors::SppTypeAliasCyclicError>(
      genex::contains(followed_aliases, final_sym->Alias->Stmt),
      ERR_ARGS(alias_stmt, *final_sym->Alias->Stmt));
    followed_aliases.emplace_back(final_sym->Alias->Stmt);
    tracking_scope = final_sym->ScopeDefinedIn;
    final_sym = lookup(*final_sym->Alias->Written);
  }

  // Return the old type (with the generic arguments it might need), the generic
  // parameters to add to the new "type" statement, and some scope/symbol info.
  return {
    .DirectTarget = old_type,
    .AttachGenerics = attach,
    .TrackingScope = final_sym->ScopeDefinedIn,
    .ResolvedSym = final_sym
  };
}

auto spp::analyse::utils::aliases::InstanceTargetIdOf(
  TypeSymbol const &alias, const TypeId id) -> TypeId {
  // Filed under the class it names (a "use" of the class), the identity is the
  // target already.
  if (id == nullptr) { return nullptr; }
  if (scopes::HeaderOf(id).Kind != scopes::TypeKey::Tag::Inst) { return nullptr; }
  if (scopes::HeaderOf(id).Symbol() != &alias) { return id; }

  // A "use" stands for what it names under the same arguments: the identity
  // re-headed onto that. One whose name does not lead past itself is read as
  // any alias, off its recorded target.
  const auto used = alias.Alias->IsFromUseStmt ? alias.UseTarget() : nullptr;
  if (used != nullptr and used != &alias) {
    const auto reheaded = scopes::InstanceIdOfArgs(*used, scopes::HeaderOf(id).Args);
    return used->Alias != nullptr ? InstanceTargetIdOf(*used, reheaded) : reheaded;
  }

  // Its target, by identity, with its parameters bound to the instantiation's
  // arguments ("AliasTargetId").
  return alias.AliasTargetId(scopes::HeaderOf(id).Args);
}
