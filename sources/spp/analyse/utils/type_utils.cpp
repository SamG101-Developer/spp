module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
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
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_comp_keyword_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_type_keyword_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_optional_ast;
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
import spp.lex.lexer;
import spp.parse.parser_spp;
import spp.parse.errors.parser_error;
import spp.utils.algorithms;
import spp.utils.interner;
import spp.utils.ptr;
import spp.utils.strings;
import genex;
import std;

auto spp::analyse::utils::type_utils::GetFunctionalType(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Shared<const asts::TypeAst> {
  //
  const auto type_sym = scope.GetTypeSymbol(&type);

  // Callable quick-fix to use the constrained callable type
  // rather than the genuine one for memory-analysis reasons;
  // constraint of FunMov but passed as FunMut needs to still
  // use the FunMov overload.
  for (auto const &constraint : type_sym->GenericConstraints) {
    if (type_predicates::IsTypeFunc(*constraint, scope)) { return constraint; }
  }

  // Check the type itself and all its supertypes (a type
  // superimposing a function type is also callable).
  auto sup_types = Vec<Shared<const asts::TypeAst>>();
  if (not type.IsCompilerGeneratedType()) { sup_types.EmplaceBack(type.shared_from_this()); }
  sup_types.AppendRange(type_sym->LinkedScope->SupTypes());
  for (auto const &sup_type : sup_types) {
    if (type_predicates::IsTypeFunc(*sup_type, scope)) { return sup_type; }
  }

  return nullptr;
}

auto spp::analyse::utils::type_utils::GetGenAndYieldTypes(
  asts::TypeAst const &type,
  scopes::Scope const &scope,
  asts::ExpressionAst const &expr,
  StrView what,
  const bool raise)
  -> Tup<Shared<const asts::TypeAst>, Shared<asts::TypeAst>, bool> {
  //
  using asts::generate::common_types_precompiled::GEN_ONCE;
  using errors::SppExpressionNotGeneratorError;
  using errors::SppExpressionAmbiguousGeneratorError;

  // A generic type is deliberately *not* rejected here: a parameter constrained to "Gen[T]" is a generator, its
  // "LinkedScope" carries the constraint's super types, and "Iterator::concat" relies on that. The sibling guards in
  // "GetTryType" and "GetFwdTypes" bail out on generics instead, which is the inconsistency their shared Todo is
  // about - so only the lookup itself is guarded.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests. Reconcile with the "IsGeneric" early-outs in "GetTryType"/"GetFwdTypes" at the same time.
  const auto type_sym = scope.GetTypeSymbol(&type);
  if (type_sym == nullptr) {
    RaiseIf<SppExpressionNotGeneratorError>(raise, {&scope}, ERR_ARGS(expr, type, what));
    return {nullptr, nullptr, false};
  }

  // Discover the supertypes and add the current type to it.
  auto sup_types = Vec{type.shared_from_this()};
  sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

  // Search through the supertypes for a direct generator type.
  // Simple comparison check against the Gen and GenOnce types.
  const auto generator_type_candidates = sup_types
    | genex::views::filter([&](auto const &sup_type) { return type_predicates::IsTypeGen(*sup_type, scope); })
    | genex::to<Vec>();

  // If there are no Gen or GenOnce super types, then the
  // generator and yield type cannot be obtained, so either
  // throw an error or return nullptr.
  if (generator_type_candidates.IsEmpty()) {
    RaiseIf<SppExpressionNotGeneratorError>(
      raise, {&scope}, ERR_ARGS(expr, type, what));
    return {nullptr, nullptr, false};
  }

  // If there are more than 1 Gen or GenOnce super types, then
  // the generator and yield types would be ambiguous, so either
  // throw an error or return nullptr.
  if (generator_type_candidates.Len() > 1) {
    RaiseIf<SppExpressionAmbiguousGeneratorError>(
      raise, {&scope}, ERR_ARGS(expr, type, what));
    return {nullptr, nullptr, false};
  }

  // Extract the generator and yield type from the candidates.
  // Accessing [0] is safe as we have already done the validation
  // beforehand.
  auto generator_type = generator_type_candidates[0];
  auto yield_type = generator_type->LastTypePart()->GnArgGroup->TypeAt("Yield")->Val;
  auto is_once = type_compare::TypeEq(
    *GEN_ONCE, *generator_type->WithoutGenerics(), scope, scope);

  // Return all the information about the generator type.
  return {generator_type, yield_type, is_once};
}

auto spp::analyse::utils::type_utils::GetTryType(
  asts::TypeAst const &type,
  asts::ExpressionAst const &expr,
  scopes::ScopeManager const &sm,
  StrView what,
  const bool raise)
  -> Shared<const asts::TypeAst> {
  // Generic types are not Try types, so return nullptr.
  // Todo: Like Copy, can we rely on constraints here? Add
  //  unit tests.
  const auto type_sym = sm.CurrentScope->GetTypeSymbol(&type);
  if (type_sym->IsGeneric) { return nullptr; }

  // Discover the supertypes and add the current type to it.
  auto sup_types = Vec{type.shared_from_this()};
  sup_types.AppendRange(type_sym->LinkedScope->SupTypes());

  // Search through the supertypes for a direct try type.
  // Simple comparison check against the Try types.
  const auto try_type_candidates = sup_types
    | genex::views::filter([&sm](auto &&sup_type) { return type_predicates::IsTypeTry(*sup_type, *sm.CurrentScope); })
    | genex::to<Vec>();

  // If there are no Try super types, then the try type cannot
  // be obtained, so either throw an error or return nullptr.
  if (try_type_candidates.IsEmpty()) {
    RaiseIf<errors::SppExpressionNotTryError>(
      raise, {sm.CurrentScope}, ERR_ARGS(expr, type));
    return nullptr;
  }

  // If there are more than 1 Try super types, then the Try
  // type would be ambiguous, so either throw an error or
  // return nullptr.
  if (try_type_candidates.Len() > 1) {
    RaiseIf<errors::SppExpressionAmbiguousTryError>(
      raise, {sm.CurrentScope}, ERR_ARGS(expr, type, what));
    return nullptr;
  }

  // Extract the Try type and return it.
  return try_type_candidates[0];
}

auto spp::analyse::utils::type_utils::GetFwdTypes(
  asts::TypeAst const &type,
  scopes::ScopeManager const &sm)
  -> Pair<Shared<asts::TypeAst>, Shared<asts::TypeAst>> {
  //
  using asts::generate::common_types_precompiled::FWD_MUT;
  using asts::generate::common_types_precompiled::FWD_REF;

  // Generic types do not have forward types, so return nullptr.
  const auto type_sym = sm.CurrentScope->GetTypeSymbol(&type);
  if (type_sym->IsGeneric) { return {nullptr, nullptr}; }

  // Find the first FwdRef and first FwdMut super type in a single pass.
  auto fwd_ref_type = Shared<asts::TypeAst>(nullptr);
  auto fwd_mut_type = Shared<asts::TypeAst>(nullptr);
  const auto consider = [&](Shared<asts::TypeAst> const &candidate) {
    const auto bare = candidate->WithoutGenerics();
    if (fwd_ref_type == nullptr and type_compare::TypeEq(*bare, *FWD_REF, *sm.CurrentScope, *sm.CurrentScope)) {
      fwd_ref_type = candidate;
    }
    else if (fwd_mut_type == nullptr and type_compare::TypeEq(*bare, *FWD_MUT, *sm.CurrentScope, *sm.CurrentScope)) {
      fwd_mut_type = candidate;
    }
  };

  // Search the types for whichever marker is still missing.
  consider(type.WithoutGenerics());
  for (auto const &sup_type : type_sym->LinkedScope->SupTypes()) {
    if (fwd_ref_type != nullptr and fwd_mut_type != nullptr) { break; }
    consider(sup_type);
  }

  return {fwd_ref_type, fwd_mut_type};
}

auto spp::analyse::utils::type_utils::BuildFwdCall(
  asts::ExpressionAst const &receiver,
  asts::TypeAst const &receiver_type,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Unique<asts::PostfixExpressionAst> {
  // A type forwards by superimposing "FwdRef" or "FwdMut", whose coroutines are "fwd_ref" and "fwd_mut".
  const auto [fwd_ref_type, fwd_mut_type] = GetFwdTypes(receiver_type, *sm);
  if (fwd_ref_type == nullptr and fwd_mut_type == nullptr) { return nullptr; }

  // Which of the two is taken follows the receiver's own convention: a value borrowed mutably forwards to a mutable
  // borrow of what it points at. Preferring the immutable one unconditionally turned a "&mut" receiver into a "&"
  // yield, which is what a mutable forward was for in the first place. Fall back to whichever exists when the
  // preferred one does not.
  const auto conv = receiver_type.GetConvention();
  const auto wants_mut = conv != nullptr and *conv == asts::ConventionTag::MUT and fwd_mut_type != nullptr;

  // Build "<receiver>.fwd_ref()". The forwarding coroutines return a "GenOnce", so the call resumes itself and the
  // expression evaluates to the borrow of the forwarded-to value.
  auto field_name = MakeUnique<asts::IdentifierAst>(
    receiver.PosStart(), wants_mut or fwd_ref_type == nullptr ? "fwd_mut" : "fwd_ref");
  auto field = MakeUnique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
  auto member_access = MakeUnique<asts::PostfixExpressionAst>(asts::AstClone(&receiver), std::move(field));
  auto func_call = MakeUnique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
  auto fwd_call = MakeUnique<asts::PostfixExpressionAst>(std::move(member_access), std::move(func_call));

  // Analyse the built call, so that it can be inferred from and generated like any other analysed expression. The
  // receiver is analysed a second time here (it is a clone of an already analysed expression), which is what the
  // other operators that map themselves onto a method call do too.
  fwd_call->Stage7_AnalyseSemantics(sm, meta);
  return fwd_call;
}

auto spp::analyse::utils::type_utils::GetTypeSymOrError(
  scopes::Scope const &scope,
  asts::TypeIdentifierAst const &type_part,
  scopes::ScopeManager const &sm)
  -> scopes::TypeSymbol* {
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
  scopes::Scope const &scope,
  asts::IdentifierAst const &ns,
  scopes::ScopeManager const &sm)
  -> scopes::Scope* {
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
  asts::TypeStatementAst const &alias_stmt,
  const bool from_use_stmt,
  scopes::Scope *tracking_scope,
  scopes::ScopeManager *sm,
  asts::meta::CompilerMetaData *meta)
  -> Tup<Shared<asts::TypeAst>, Shared<asts::GenericParameterGroupAst>, scopes::Scope*> {
  //
  using generic_bindings::NameGnArgs;

  // How to extract generic parameters from a type symbol: alias, then type, otherwise none (generic).
  const auto NO_PARAMS = asts::GenericParameterGroupAst::NewEmpty();
  const auto extract_params = [&NO_PARAMS](scopes::TypeSymbol const &ts) {
    return ts.Alias
      ? ts.Alias->Params.get()
      : ts.Type
      ? ts.Type->GnParamGroup.get()
      : NO_PARAMS.get();
  };

  const auto filter_params = [](asts::GenericParameterGroupAst const &pg, asts::GenericArgumentGroupAst const &ag) {
    auto out = asts::GenericParameterGroupAst::NewEmptyShared();
    for (auto const &param : pg.GetTypeParams()) {
      if (not genex::any_of(ag.GetTypeKeywordArgs(), [&](auto const *arg) { return *arg->Name == *param->Name; })) {
        out->Params.EmplaceBack(asts::AstClone(param));
      }
    }
    for (auto const &param : pg.GetCompParams()) {
      if (not genex::any_of(ag.GetCompKeywordArgs(), [&](auto const *arg) { return *arg->Name == *param->Name; })) {
        out->Params.EmplaceBack(asts::AstClone(param));
      }
    }
    return out;
  };

  // Get the next type in the search, and its symbol.
  auto old_type = alias_stmt.OldType;
  auto old_sym = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics().get());

  // If this is a use statement to a class, then grab its generics and return immediately.
  // For example, use Vec::Vec => type Vec[T, A: ... = ...] = Vec::Vec[T=T, A=A]
  if (from_use_stmt and old_sym->Alias == nullptr) {
    auto generic_params = old_sym->Type->GnParamGroup;
    old_type = old_type->WithGenerics(asts::GenericArgumentGroupAst::FromParams(*generic_params));
    return {old_type, generic_params, old_sym->LinkedScope};
  }

  const auto generic_args = asts::AstClone(old_type->LastTypePart()->GnArgGroup.get());
  auto final_generic_params = asts::GenericParameterGroupAst::NewEmptyShared();
  tracking_scope = old_sym->ScopeDefinedIn;

  // The walk below has no base case beyond "the next name is not an alias", so an alias that leads back to one
  // already being followed is followed forever. Statements are what is recorded rather than symbols, because a
  // statement is what the source wrote and so is what the error can point at; the starting one is seeded so that a
  // self-alias ("type A = A") is caught on its first step rather than on its second.
  auto followed_aliases = Vec{&alias_stmt};

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
      ? old_sym->Alias->Written->WithGenerics(asts::AstClone(carried))
      : old_sym->Alias->Written;
    old_sym = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics().get());

    // Arguments just handed on still have to be bound by whatever received them, so the walk goes round once more
    // even when that is a class rather than another alias.
    if (old_sym->Alias == nullptr and not carries_generics) { break; }
  }

  old_type = tracking_scope->GetTypeSymbol(old_type->WithoutGenerics().get())->FqName()->WithGenerics(
    AstClone(old_type->LastTypePart()->GnArgGroup));

  auto &temp = *old_type->LastTypePart()->GnArgGroup;
  NameGnArgs(
    temp, *extract_params(*old_sym), *old_type, *sm, *meta, type_predicates::IsTupSymbol(*old_sym));
  old_type = old_type->SubstituteGenerics(generic_args->GetAllArgs());
  return {old_type, final_generic_params, tracking_scope};
}

auto spp::analyse::utils::type_utils::ResolveAndSubstituteSelfType(
  asts::TypeAst const &type,
  scopes::Scope const &scope,
  scopes::ScopeManager &sm,
  asts::meta::CompilerMetaData &meta)
  -> Shared<asts::TypeAst> {
  // Todo: always clone here? performance hit i think.
  using asts::generate::common_types::SelfType;
  const auto true_self_type = scope.GetEnclosingSelfType(meta);
  if (true_self_type == nullptr) { return AstClone(&type); }

  // If "Self" is not present, return a plain clone.
  if (not type.AnyPart([](asts::TypeIdentifierAst const &part) { return part.Name == "Self"; })) {
    return AstClone(&type);
  }

  // Substitute "Self" with the concrete enclosing type.
  const auto g = MakeUnique<asts::GenericArgumentTypeKeywordAst>(SelfType(0), nullptr, true_self_type);
  const auto args = Vec<asts::GenericArgumentAst*>{g.get()};

  auto t = type.SubstituteGenerics(args);
  const auto _meta_guard = asts::meta::MetaGuard(&meta);
  meta.AllowAbstractType = true;
  t->Stage7_AnalyseSemantics(&sm, &meta);
  return t;
}
