module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.cmp_utils;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.convention_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.integer_literal_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.statement_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.utils.ast_utils;
import spp.asts.utils.visibility;
import spp.utils.algorithms;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.strings;
import genex;
import std;

auto spp::analyse::utils::type_utils::GetFunctionalType(
  TypeAst const &type,
  Scope const &scope)
  -> Shared<const TypeAst> {
  //
  const auto type_sym = scope.GetTypeSymbol(&type);

  // Callable quick-fix to use the constrained callable type
  // rather than the genuine one for memory-analysis reasons;
  // constraint of FunMov but passed as FunMut needs to still
  // use the FunMov overload.
  for (auto const &constraint : type_sym->GenericConstraints) {
    if (type_predicates::IsTypeFunc(TypeRef::OfHead(*constraint, scope), scope)) { return constraint; }
  }

  // Check the type itself and all its supertypes (a type
  // superimposing a function type is also callable).
  if (not type.IsCompilerGeneratedType() and type_predicates::IsTypeFunc(TypeRef::OfHead(type, scope), scope)) {
    return type.shared_from_this();
  }
  for (auto const *sup_scope : type_sym->LinkedScope->SupScopes()) {
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    if (type_predicates::IsTypeFunc(*sup_scope->TySym, scope)) { return sup_scope->TySym->FqName(); }
  }

  return nullptr;
}

auto spp::analyse::utils::type_utils::GetGenAndYieldTypes(
  TypeRef const &ref,
  Scope const &scope,
  ExpressionAst const &expr,
  std::function<Shared<TypeAst>()> const &spell,
  StrView what,
  const bool raise)
  -> Tup<TypeSymbol*, Shared<TypeAst>, bool> {
  //
  using generate::common_types_precompiled::GEN_ONCE;
  using errors::SppExpressionNotGeneratorError;
  using errors::SppExpressionAmbiguousGeneratorError;

  // A generic type is deliberately *not* rejected here: a parameter constrained to "Gen[T]" is a generator, its
  // "LinkedScope" carries the constraint's super types, and "Iterator::concat" relies on that. The sibling guards in
  // "GetTryType" and "GetFwdTypes" bail out on generics instead, which is the inconsistency their shared Todo is
  // about - so only the lookup itself is guarded.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests. Reconcile with the "IsTypeGeneric()" early-outs in "GetTryType"/"GetFwdTypes" at the same time.
  const auto type_sym = ref.Sym;
  if (type_sym == nullptr) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionNotGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return {nullptr, nullptr, false};
  }

  // Search the type itself, then its super classes by symbol, for a direct generator type.
  auto generator_candidates = Vec<TypeSymbol*>();
  if (type_predicates::IsTypeGen(ref, scope)) { generator_candidates.EmplaceBack(type_sym); }
  for (auto const *sup_scope : type_sym->LinkedScope->SupScopes()) {
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    if (type_predicates::IsTypeGen(*sup_scope->TySym, scope)) {
      generator_candidates.EmplaceBack(sup_scope->TySym.get());
    }
  }

  // If there are no Gen or GenOnce super types, then the
  // generator and yield type cannot be obtained, so either
  // throw an error or return nullptr.
  if (generator_candidates.IsEmpty()) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionNotGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return {nullptr, nullptr, false};
  }

  // If there are more than 1 Gen or GenOnce super types, then
  // the generator and yield types would be ambiguous, so either
  // throw an error or return nullptr.
  if (generator_candidates.Len() > 1) {
    if (raise) {
      const auto type = spell();
      Raise<SppExpressionAmbiguousGeneratorError>({&scope}, ERR_ARGS(expr, *type, what));
    }
    return {nullptr, nullptr, false};
  }

  // Extract the generator and yield type from the candidates.
  // Accessing [0] is safe as we have already done the validation
  // beforehand.
  auto *const generator_sym = generator_candidates[0];
  auto yield_type = generator_sym->TypeArgType("Yield");
  auto is_once = type_predicates::IsTemplate(*generator_sym, *GEN_ONCE, scope);

  // Return all the information about the generator type.
  return {generator_sym, yield_type, is_once};
}

auto spp::analyse::utils::type_utils::GetTryType(
  TypeRef const &ref,
  ExpressionAst const &expr,
  std::function<Shared<TypeAst>()> const &spell,
  ScopeManager const &sm,
  StrView what,
  const bool raise)
  -> TypeSymbol* {
  // Generic types are not Try types, so return nullptr.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests.
  const auto type_sym = ref.Sym;
  if (type_sym == nullptr or type_sym->IsTypeGeneric()) { return nullptr; }

  // Search the type itself, then its super classes by symbol, for a direct try type.
  auto try_type_candidates = Vec<TypeSymbol*>();
  if (type_predicates::IsTypeTry(ref, *sm.CurrentScope)) { try_type_candidates.EmplaceBack(type_sym); }
  for (auto const *sup_scope : type_sym->LinkedScope->SupScopes()) {
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    if (type_predicates::IsTypeTry(*sup_scope->TySym, *sm.CurrentScope)) {
      try_type_candidates.EmplaceBack(sup_scope->TySym.get());
    }
  }

  // If there are no Try super types, then the try type cannot
  // be obtained, so either throw an error or return nullptr.
  if (try_type_candidates.IsEmpty()) {
    if (raise) {
      const auto type = spell();
      Raise<errors::SppExpressionNotTryError>({sm.CurrentScope}, ERR_ARGS(expr, *type));
    }
    return nullptr;
  }

  // If there are more than 1 Try super types, then the Try
  // type would be ambiguous, so either throw an error or
  // return nullptr.
  if (try_type_candidates.Len() > 1) {
    if (raise) {
      const auto type = spell();
      Raise<errors::SppExpressionAmbiguousTryError>({sm.CurrentScope}, ERR_ARGS(expr, *type, what));
    }
    return nullptr;
  }

  // Extract the Try type and return it.
  return try_type_candidates[0];
}

auto spp::analyse::utils::type_utils::GetFwdTypes(
  TypeSymbol const &sym,
  Scope const &scope)
  -> Pair<TypeSymbol*, TypeSymbol*> {
  //
  using generate::common_types_precompiled::FWD_MUT;
  using generate::common_types_precompiled::FWD_REF;

  // Generic types do not have forward types, so return nullptr.
  if (sym.IsTypeGeneric() or sym.LinkedScope == nullptr) { return {nullptr, nullptr}; }

  // Find the first FwdRef and first FwdMut super type in a single pass.
  auto *fwd_ref_sym = static_cast<TypeSymbol*>(nullptr);
  auto *fwd_mut_sym = static_cast<TypeSymbol*>(nullptr);
  const auto consider = [&](TypeSymbol *candidate) {
    if (fwd_ref_sym == nullptr and type_predicates::IsTemplate(*candidate, *FWD_REF, scope)) {
      fwd_ref_sym = candidate;
    }
    else if (fwd_mut_sym == nullptr and type_predicates::IsTemplate(*candidate, *FWD_MUT, scope)) {
      fwd_mut_sym = candidate;
    }
  };

  // Search the type and its super classes for whichever marker is still missing.
  consider(const_cast<TypeSymbol*>(&sym));
  for (auto const *sup_scope : sym.LinkedScope->SupScopes()) {
    if (fwd_ref_sym != nullptr and fwd_mut_sym != nullptr) { break; }
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    consider(sup_scope->TySym.get());
  }

  return {fwd_ref_sym, fwd_mut_sym};
}

auto spp::analyse::utils::type_utils::BuildFwdCall(
  ExpressionAst const &receiver,
  TypeRef const &receiver_ref,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Unique<PostfixExpressionAst> {
  // A type forwards by superimposing "FwdRef" or "FwdMut", whose coroutines are "fwd_ref" and "fwd_mut".
  if (receiver_ref.Sym == nullptr) { return nullptr; }
  const auto [fwd_ref_type, fwd_mut_type] = GetFwdTypes(*receiver_ref.Sym, *sm->CurrentScope);
  if (fwd_ref_type == nullptr and fwd_mut_type == nullptr) { return nullptr; }

  // Which of the two is taken follows the receiver's own convention: a value borrowed mutably forwards to a mutable
  // borrow of what it points at. Preferring the immutable one unconditionally turned a "&mut" receiver into a "&"
  // yield, which is what a mutable forward was for in the first place. Fall back to whichever exists when the
  // preferred one does not.
  const auto wants_mut = receiver_ref.Conv == ConventionTag::MUT and fwd_mut_type != nullptr;

  // Build "<receiver>.fwd_ref()". The forwarding coroutines return a "GenOnce", so the call resumes itself and the
  // expression evaluates to the borrow of the forwarded-to value.
  auto field_name = MakeUnique<IdentifierAst>(
    receiver.PosStart(), wants_mut or fwd_ref_type == nullptr ? "fwd_mut" : "fwd_ref");
  auto field = MakeUnique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
  auto member_access = MakeUnique<PostfixExpressionAst>(AstClone(&receiver), std::move(field));
  auto func_call = MakeUnique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
  auto fwd_call = MakeUnique<PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  // Analyse the built call, so that it can be inferred from and generated like any other analysed expression. The
  // receiver is analysed a second time here (it is a clone of an already analysed expression), which is what the
  // other operators that map themselves onto a method call do too.
  fwd_call->Stage7_AnalyseSemantics(sm, meta);
  return fwd_call;
}

auto spp::analyse::utils::type_utils::GetTypeSymOrError(
  Scope const &scope,
  TypeIdentifierAst const &type_part,
  ScopeManager const &sm)
  -> TypeSymbol* {
  //
  using expr_utils::RaiseMissingTypeIdentifierAndClosestOptions;

  // Get the type part's symbol, and raise an error if it doesn't exist.
  const auto type_sym = scope.GetTypeSymbol(&type_part, false);
  if (type_sym == nullptr) {
    RaiseMissingTypeIdentifierAndClosestOptions(type_part, scope.AllTypeSymbols(), sm);
  }

  // Return the found type symbol.
  return type_sym;
}

auto spp::analyse::utils::type_utils::GetNsScopeOrError(
  Scope const &scope,
  IdentifierAst const &ns,
  ScopeManager const &sm)
  -> Scope* {
  //
  using expr_utils::RaiseMissingIdentifierAndClosestOptions;

  // If the namespace does not exist, raise an error.
  const auto ns_sym = scope.GetNsSymbol(&ns);
  if (ns_sym == nullptr) {
    RaiseMissingIdentifierAndClosestOptions(ns, {}, scope.AllNsSymbols(), sm);
  }

  // Return the found namespace scope.
  return ns_sym->LinkedScope;
}

auto spp::analyse::utils::type_utils::RecursiveAliasSearch(
  TypeStatementAst const &alias_stmt,
  const bool from_use_stmt,
  Scope *tracking_scope,
  ScopeManager *sm,
  meta::CompilerMetaData *meta)
  -> Tup<Shared<TypeAst>, Shared<GenericParameterGroupAst>, Scope*> {
  //
  using generic_bindings::NameGnArgs;

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
      expr_utils::RaiseMissingTypeIdentifierAndClosestOptions(
        *ty.LastTypePart(), tracking_scope->AllTypeSymbols(), *sm);
    }
    return sym;
  };

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

auto spp::analyse::utils::type_utils::SubstituteSelfType(
  TypeAst const &type,
  Scope const &scope,
  meta::CompilerMetaData const &meta,
  bool *const substituted)
  -> Shared<TypeAst> {
  // Substitute "Self" with the concrete enclosing type, if there is one and the type names it.
  const auto true_self_type = scope.GetEnclosingSelfType(meta);
  if (true_self_type == nullptr or not type_predicates::NamesSelfType(type)) { return AstClone(&type); }
  if (substituted != nullptr) { *substituted = true; }
  return SubstituteSelfTypeWith(type, *true_self_type);
}

auto spp::analyse::utils::type_utils::SubstituteSelfTypeAndAnalyse(
  TypeAst const &type,
  Scope const &scope,
  ScopeManager &sm,
  meta::CompilerMetaData &meta,
  bool *const substituted)
  -> Shared<TypeAst> {
  auto replaced = false;
  auto t = SubstituteSelfType(type, scope, meta, &replaced);
  if (substituted != nullptr) { *substituted = replaced; }

  // Only a type that actually had a "Self" replaced is analysed here. One that did not is handed back as the plain
  // clone it is, so that this does not analyse a written type at a point its owner has not chosen to - and so that a
  // "Self" left standing for want of an enclosing type is reported by whoever does analyse it.
  if (not replaced) { return t; }

  const auto _meta_guard = meta::MetaGuard(&meta);
  meta.AllowAbstractType = true;
  t->Stage7_AnalyseSemantics(&sm, &meta);
  return t;
}

auto spp::analyse::utils::type_utils::SubstituteSelfTypeWith(
  TypeAst const &type,
  TypeAst const &replacement)
  -> Shared<TypeAst> {
  using generate::common_types::SelfType;

  // If "Self" is not present, return a plain clone.
  if (not type_predicates::NamesSelfType(type)) { return AstClone(&type); }

  const auto g = GenericArgumentAst::NewType(
    SelfType(0), AstClone(&replacement));
  const auto args = Vec<GenericArgumentAst*>{g.get()};
  return type.SubstituteGenerics(args);
}

auto spp::analyse::utils::type_utils::ResolveWrittenType(
  TypeAst const &written,
  ScopeManager &sm,
  meta::CompilerMetaData &meta,
  const SelfPolicy self)
  -> Shared<TypeAst> {
  // A type that had "Self" replaced is analysed as it is replaced ("SubstituteSelfTypeAndAnalyse"); any other here.
  auto substituted = false;
  auto t = self == SelfPolicy::kSubstitute
    ? SubstituteSelfTypeAndAnalyse(written, *sm.CurrentScope, sm, meta, &substituted)
    : AstClone(&written);
  if (not substituted) { t->Stage7_AnalyseSemantics(&sm, &meta); }

  return sm.CurrentScope->GetTypeSymbol(t.get())->FqName()
    ->WithConvention(AstClone(written.GetConvention()))
    ->WithSourceSpanOf(written);
}

auto spp::analyse::utils::type_utils::StampWrittenParts(
  TypeAst const &type,
  Scope const &scope)
  -> void {
  // Every part, nested arguments included. Comp arguments are stamped with the comp parameters they name.
  static_cast<void>(type.AnyPart([&scope](TypeIdentifierAst const &part) {
    for (auto const *comp_arg : part.GnArgGroup->GetCompArgs()) {
      if (comp_arg->CompVal != nullptr) { cmp_utils::StampCompGenerics(*comp_arg->CompVal, scope); }
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
