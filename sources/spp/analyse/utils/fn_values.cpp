module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.fn_values;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.marker_sups;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.sup_blocks;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.expression_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.identifier_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.utils.ptr;
import spp.utils.types;
import genex;

/// [CHECKED]
auto spp::analyse::utils::fn_values::FnBlockOf(
  Scope const &scope) -> Pair<SupPrototypeExtensionAst*, FunctionPrototypeAst*> {
  // Get the sup-ext node in the scope, and inspect its body.
  // Select the 1st child (the actual function from pre-
  // processing), and return it with the ext block.
  const auto ext = AstAs<SupPrototypeExtensionAst>(scope.AstNode);
  const auto body = ext != nullptr ? AstBody(ext) : Vec<Ast*>();
  const auto proto = not body.empty() ? body[0]->To<FunctionPrototypeAst>() : nullptr;
  return {proto != nullptr ? ext : nullptr, proto};
}

auto spp::analyse::utils::fn_values::GetFnValueName(
  TypeRef const &type) -> Pair<Shared<IdentifierAst>, Scope const*> {
  // Only a function's own "$" mock names a function, and only held by value: a borrow of one is not that function.
  // A closure's mock is unnamed by definition (its function type is attached directly), and has no block to walk.
  const auto mock_sym = type.KindSymbol();
  if (mock_sym == nullptr or mock_sym->Kind != TypeKind::FnMock) { return {nullptr, nullptr}; }
  if (mock_sym->LinkedScope == nullptr) { return {nullptr, nullptr}; }

  // Iterate the scopes on the function type ie $Type,
  // extracting the function prototype out of it. Return
  // a match. Always only 1 sup-ext for the $Types, so
  // the name and scope are always correct here.
  for (auto const *ext_scope : mock_sym->LinkedScope->DirectSupScopes) {
    const auto proto = FnBlockOf(*ext_scope).second;
    if (proto == nullptr) { continue; }

    // A method's overloads can be spread over several "sup" blocks
    // of its owner, so a non-generic owner's class scope is named,
    // which gathers the overloads from all of them.
    const auto block = ext_scope->Parent;
    const auto block_node = block->AstNode;
    if (sup_blocks::IsSupBlock(block_node)) {
      const auto owner_sym = block->FindHeadSymbol(*AstName(block_node));
      if (owner_sym != nullptr and not owner_sym->IsGn() and owner_sym->LinkedScope != nullptr
        and owner_sym->Type != nullptr and owner_sym->Type->GnParamGroup->Params.empty()) {
        return {proto->Name, owner_sym->LinkedScope};
      }
    }
    return {proto->Name, block};
  }

  // Failsafe against no matches (doubt this will ever
  // be reached even, but c++ type safety).
  return {nullptr, nullptr};
}

auto spp::analyse::utils::fn_values::MatchFnValue(
  TypeRef const &mock, TypeRef const &func,
  Scope const &func_scope) -> Opt<FnValueMatch> {
  //
  using type_compare::TypeEq;

  // The target has to be a function type held by value, and the
  // value a named function's mock.
  if (func.Symbol == nullptr or func.IsBorrowed() or func.Symbol->IsMock()
    or not type_predicates::IsTypeFunction(func, func_scope)
    or GetFnValueName(mock).first == nullptr) {
    return Nullopt;
  }

  // The mock carries the superimposed function type of each overload.
  const auto mock_sym = mock.Symbol;
  const auto func_type_ast = func.Symbol->FqName();
  auto const &func_type = *func_type_ast;

  // Each overload attaches its own "sup $F ext FunXxx { fun ... }"
  // block to the mock. Its function type is compared along with the
  // ones above it, as a "FunRef" is also a "FunMut" and a "FunMov".
  auto match = Opt<FnValueMatch>();
  for (auto const *ext_scope : mock_sym->LinkedScope->DirectSupScopes) {
    const auto [ext, proto] = FnBlockOf(*ext_scope);
    const auto kind_sym = proto != nullptr
      ? ext_scope->FindHeadSymbol(*ext->SuperCls)
      : nullptr;
    if (kind_sym == nullptr or kind_sym->LinkedScope == nullptr) { continue; }

    // Checks whether the function types are compatible; so a FunRef
    // cannot be used where a FunMov is needed (generics aside); but
    // a FunMov can be used in place of a FunRef (constraint).
    const auto target_kind = func_scope.FindHeadSymbol(func_type);
    if (target_kind == nullptr) { continue; }
    const auto target_tmpl = TypeRef::ForKindCheck(*target_kind, func_scope).Template();
    if (not type_predicates::IsOrExtendsTemplate(*kind_sym, *ext_scope, target_tmpl)) { continue; }

    const auto own_generics = ext->SuperCls->LastTypePart()->GnArgGroup.get();
    const auto target_generics = func_type.WithoutConvention()->LastTypePart()->GnArgGroup.get();
    const auto own_args = own_generics->At("Args");
    const auto target_args = target_generics->At("Args");
    const auto own_out = own_generics->At("Out");
    const auto target_out = target_generics->At("Out");
    if (own_args == nullptr or own_out == nullptr or target_args == nullptr or target_out == nullptr) { continue; }

    // A generic overload binds its own generics off the target,
    // by identity, which is why its own signature is the pattern.
    // Either way the signature is then checked exactly.
    auto const &own_params = proto->GnParamGroup->Params;
    auto inferred = scopes::GenericSubst();
    if (not own_params.empty()) {
      const auto unify = [&](TypeAst const &target, TypeAst const &own) {
        const auto t = func_scope.TypeIdOf(target);
        const auto o = ext_scope->TypeIdOf(own);
        return t != nullptr and o != nullptr and scopes::UnifyTypeIds(t, o, func_scope, *ext_scope, inferred);
      };
      if (not unify(*target_args->TypeVal, *own_args->TypeVal) or not unify(*target_out->TypeVal, *own_out->TypeVal)) {
        continue;
      }
    }

    // Only its own generics, each of which has to be bound.
    const auto bindings = scopes::BindingsFor(inferred, *proto->GnParamGroup);
    if (bindings.TypeParams.size() + bindings.CompParams.size() != own_params.size()) { continue; }

    // Its own signature with what it inferred bound, read where the target is.
    const auto read = [&](TypeAst const &own) { return type_resolution::ReadType(own, ExprSubst::Across(*ext_scope, bindings, func_scope)); };
    const auto assignable = [&](TypeAst const &own, TypeAst const &target) {
      return type_compare::Assignable(
        TypeRef::Of(*read(own), func_scope), TypeRef::Of(target, func_scope), func_scope, func_scope);
    };
    if (not assignable(*own_args->TypeVal, *target_args->TypeVal) or not assignable(*own_out->TypeVal, *target_out->TypeVal)) {
      continue;
    }

    // A non-generic overload wins outright; a generic one only if
    // none does.
    auto found = FnValueMatch{
      .Proto = proto, .FnScope = ext_scope, .Bindings = bindings};
    if (own_params.empty()) { return found; }
    if (not match.has_value()) { match = std::move(found); }
  }
  return match;
}

auto spp::analyse::utils::fn_values::InstantiateFnValue(
  TypeRef const &value, TypeRef const &target,
  ScopeManager *sm, meta::CompilerMetaData *meta) -> void {
  const auto match = MatchFnValue(value, target, *sm->CurrentScope);
  const auto args_id = match.has_value() ? scopes::ArgsIdOfBindings(match->Bindings, *sm->CurrentScope) : nullptr;
  if (args_id == nullptr) { return; }
  auto tm = sm->NewManagerAt(const_cast<Scope*>(match->FnScope));
  monomorphization::RequireFnInstance(match->Proto, match->FnScope, args_id, &tm, meta);
}

auto spp::analyse::utils::fn_values::FindFnValue(
  TypeRef const &value, TypeRef const &target,
  ScopeManager const &sm) -> FunctionPrototypeAst* {
  const auto match = MatchFnValue(value, target, *sm.CurrentScope);
  if (not match.has_value()) { return nullptr; }
  const auto args_id = scopes::ArgsIdOfBindings(match->Bindings, *sm.CurrentScope);
  if (args_id == nullptr) { return match->Proto; }
  return monomorphization::FindRequiredFnInstance(match->Proto, args_id);
}

/// [CHECKED]
auto spp::analyse::utils::fn_values::IndirectFnTypeOf(
  ExpressionAst &expr, ScopeManager &sm,
  meta::CompilerMetaData *meta) -> Shared<const TypeAst> {
  // Get the type of the expression, then find its functional
  // type. The functional type is the "FunRef|FunMut|FunMov" the
  // expression is or superimposes - a generic gets its one from
  // the constraints - and is null for anything not callable,
  // which the caller reports as "no valid signatures".

  // A parameter declared against a generic is called through
  // the interface its constraint promised, recorded on the
  // symbol when the instantiation was made. Read before the
  // type, which by then is whatever the generic was substituted
  // with.
  if (const auto sym = sm.CurrentScope->FindVarSymbolOutermost(expr).first;
    sym != nullptr and sym->IndirectFnType != nullptr) {
    return sym->IndirectFnType;
  }

  // Otherwise, infer the type of the expression, find its
  // functional superimpositions, and get the function type
  // on the relevant sup block.
  const auto expr_type = expr.InferType(&sm, meta);
  const auto fn_sup = marker_sups::FindFnSup(*expr_type, *sm.CurrentScope);
  return fn_sup.Symbol != nullptr ? fn_sup.AstIn(*sm.CurrentScope) : nullptr;
}
