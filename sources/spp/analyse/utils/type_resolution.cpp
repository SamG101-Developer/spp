module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_resolution;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.arg_naming;
import spp.analyse.utils.comp_generics;
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
import spp.utils.ptr;
import genex;
import std;

auto spp::analyse::utils::type_resolution::ThroughAlias(
  TypeAst const &type,
  Scope const &scope)
  -> Shared<const TypeAst> {
  // Only a written alias head is looked through. What the whole
  // type resolves to is the target instance, or, where the symbol
  // is still the alias, its recorded target. A copy is handed back:
  // a symbol's name is shared by every use of it.
  const auto head = TypeRef::OfHead(type, scope).Sym;
  if (head == nullptr or head->Kind != TypeKind::Alias) { return type.shared_from_this(); }
  const auto full = TypeRef::Of(type, scope).Sym;
  if (full == nullptr) { return type.shared_from_this(); }
  if (full->Kind == TypeKind::Alias) {
    return full->Alias != nullptr and full->Alias->Resolved != nullptr ? full->Alias->Resolved : type.shared_from_this();
  }
  return asts::AstCloneShared(full->FqName());
}

auto spp::analyse::utils::type_resolution::RecursiveAliasSearch(
  TypeStatementAst const &alias_stmt,
  const bool from_use_stmt,
  Scope *tracking_scope,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*> {
  //
  using arg_naming::NameGnArgs;

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

  // Get the next type in the search, and its symbol.
  auto old_type = alias_stmt.OldType;
  auto old_sym = lookup(*old_type);

  // If this is a use statement to a class, then grab its generics and return immediately.
  // For example, use Vec::Vec => type Vec[T, A: ... = ...] = Vec::Vec[T=T, A=A]
  if (from_use_stmt and old_sym->Alias == nullptr) {
    auto generic_params = old_sym->Type->GnParamGroup;
    old_type = old_type->WithGenerics(GenericArgumentGroupAst::FromParams(*generic_params));
    return {old_type, generic_params, old_sym->LinkedScope};
  }

  // Empty at first: the arguments written here are this target's own, bound by its parameters, not substituted into it.
  const auto generic_args = GenericArgumentGroupAst::NewEmpty();
  auto final_generic_params = GenericParameterGroupAst::NewEmptyShared();
  tracking_scope = old_sym->ScopeDefinedIn;

  // The walk below has no base case beyond "the next name is not an alias", so an alias that leads back to one
  // already being followed is followed forever. Statements are what is recorded rather than symbols, because a
  // statement is what the source wrote and so is what the error can point at; the starting one is seeded so that a
  // self-alias ("type A = A") is caught on its first step rather than on its second.
  auto followed_aliases = Vec{&alias_stmt};

  // Whether "old_type" has had the arguments bound so far substituted in, which a type must have exactly once.
  auto bound = false;

  while (true) {
    // A "use" alias declares no parameters of its own - it renames a type without reshaping it - so arguments
    // written at the use site belong to whatever it names rather than being bound here. Every other alias binds
    // them to the parameters it declares, which is what naming and substituting them does.
    const auto passes_generics_through = old_sym->Alias != nullptr and old_sym->Alias->FromUseStmt;

    if (not passes_generics_through) {
      const auto is_tuple = type_predicates::IsTupSymbol(*old_sym);
      NameGnArgs(*old_type->LastTypePart()->GnArgGroup, *extract_params(*old_sym), *old_type, *sm, *meta, is_tuple);
      if (old_sym->Alias) {
        final_generic_params = filter_params(*old_sym->Alias->Params, *old_type->LastTypePart()->GnArgGroup);
      }
      old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs());
      if (not is_tuple) { *generic_args += *old_type->LastTypePart()->GnArgGroup; }
      bound = true;
    }
    tracking_scope = old_sym->ScopeDefinedIn;

    if (old_sym->Alias == nullptr) { break; }
    RaiseIf<errors::SppTypeAliasCyclicError>(
      genex::contains(followed_aliases, old_sym->Alias->Stmt),
      {sm->CurrentScope}, ERR_ARGS(alias_stmt, *old_sym->Alias->Stmt));
    followed_aliases.EmplaceBack(old_sym->Alias->Stmt);

    // Follow the alias, handing the arguments straight on when it is one that passes them through.
    auto const *carried = old_type->LastTypePart()->GnArgGroup.get();
    const auto carries_generics = passes_generics_through and not carried->Args.IsEmpty();
    old_type = carries_generics
      ? old_sym->Alias->Written->WithGenerics(AstClone(carried))
      : old_sym->Alias->Written;
    old_sym = lookup(*old_type);
    bound = false;

    // Arguments just handed on still have to be bound by whatever received them, so the walk goes round once more
    // even when that is a class rather than another alias.
    if (old_sym->Alias == nullptr and not carries_generics) { break; }
  }

  old_type = lookup(*old_type)->FqName()->WithGenerics(AstClone(old_type->LastTypePart()->GnArgGroup));

  auto &temp = *old_type->LastTypePart()->GnArgGroup;
  NameGnArgs(
    temp, *extract_params(*old_sym), *old_type, *sm, *meta, type_predicates::IsTupSymbol(*old_sym));
  // Not again once bound: the last arguments bound are this type's own, and substituting a type's arguments into itself
  // re-binds the ones naming a parameter spelled like its target's ("Single[Arr[T, n], A]" became
  // "Single[Arr[Arr[T, n], n], A]").
  if (not bound) { old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs()); }
  return {old_type, final_generic_params, tracking_scope};
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

  return sm.CurrentScope->GetTypeSymbol(t.get())->FqName()
    ->WithConvention(AstClone(written.GetConvention()))
    ->WithSourceSpanOf(written);
}

auto spp::analyse::utils::type_resolution::StampWrittenParts(
  TypeAst const &type,
  Scope const &scope)
  -> void {
  // Every part, nested arguments included. Comp arguments are stamped with the comp parameters they name.
  static_cast<void>(type.AnyPart([&scope](TypeIdentifierAst const &part) {
    for (auto const *comp_arg : part.GnArgGroup->GetCompArgs()) {
      if (comp_arg->CompVal != nullptr) { comp_generics::StampCompGenerics(*comp_arg->CompVal, scope); }
    }

    // A name written with arguments has the template it instantiates at its head, whatever the arguments become. A
    // "use" of the template is an alias here, private to this module, so the head is the template it names.
    if (not part.GnArgGroup->Args.IsEmpty()) {
      // Followed through what each "use" names, not to the class at the end of the chain: a "use" of a "type" alias
      // ("SizedIntegerUnsigned[w]") stops at that alias, whose own parameters are the ones the arguments bind; its
      // target's are more ("SizedInteger[w, signed]").
      auto *tmpl = scope.GetTypeSymbol(part.WithoutGenerics().get());
      for (auto step = 0; step < 8 and tmpl != nullptr and tmpl->Alias != nullptr and tmpl->Alias->FromUseStmt
        and tmpl->Alias->DeclScope != nullptr and tmpl->Alias->Written != nullptr; ++step) {
        tmpl = tmpl->Alias->DeclScope->GetTypeSymbol(tmpl->Alias->Written->WithoutGenerics().get());
      }
      if (tmpl != nullptr and not tmpl->IsTypeGeneric()) { part.SetTemplateStamp(tmpl); }
      return false;
    }

    // A plain name is stamped when it means the same from anywhere: a parameter or a closed class - directly, or through
    // an alias of one, as a "use" of a class makes. Anything else still depends on the scope asking.
    if (part.Stamp() != nullptr) { return false; }
    auto *sym = scope.GetTypeSymbol(&part);
    if (sym != nullptr and sym->Kind == TypeKind::Alias and sym->Alias != nullptr
      and sym->Alias->DeclScope != nullptr) {
      sym = sym->Alias->DeclScope->GetTypeSymbol(sym->Alias->Resolved.get());
    }
    if (sym != nullptr and (
      (sym->Kind == TypeKind::GenericParam and sym->ParamId != 0)
      or (sym->Kind == TypeKind::Class and sym->IsConcrete and sym->Alias == nullptr))) {
      part.SetStamp(sym);
    }
    return false;
  }));
}
