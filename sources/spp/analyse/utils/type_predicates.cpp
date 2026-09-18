module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_predicates;
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
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
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
import spp.asts.parenthesised_expression_ast;
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

namespace spp::analyse::utils::type_predicates {
  namespace {
    auto GetAttrTypes(
      const ClassPrototypeAst *cls_proto,
      const Scope *cls_scope,
      Vec<Pair<TypeSymbol*, ClassAttributeAst*>> &attr_symbols)
      -> void {
      // Get all attribute types, without recursion errors (this will
      // be handled elsewhere, so assume it has been checked already).
      for (auto const &member : cls_proto->Impl->Members
           | genex::views::ptr
           | genex::views::cast_dynamic<ClassAttributeAst*>) {
        auto type_sym = cls_scope->GetTypeSymbol(member->Type.get());
        if (genex::contains(attr_symbols, type_sym, [](auto &&x) { return x.first; })) { continue; }
        if (type_sym->IsTypeGeneric()) { continue; }

        attr_symbols.EmplaceBack(type_sym, member);
        GetAttrTypes(type_sym->Type, type_sym->LinkedScope, attr_symbols);
      }
    }

  }
}

auto spp::analyse::utils::type_predicates::NamesSelfType(
  TypeAst const &type)
  -> bool {
  return type.AnyPart([](TypeIdentifierAst const &part) { return part.Name == "Self"; });
}

auto spp::analyse::utils::type_predicates::IsTupSymbol(
  TypeSymbol const &sym)
  -> bool {
  // Compared against the precompiled name rather than through a scope, because the symbol's own qualified name is
  // already the answer: an alias for the tuple resolves to "std::tuple::Tup" just as the type itself does.
  using generate::common_types_precompiled::TUP;
  const auto as_unary = dynamic_shared_cast<TypeUnaryExpressionAst>(sym.FqName()->WithoutGenerics());
  return as_unary != nullptr and *as_unary == *TUP->ToUnchecked<TypeUnaryExpressionAst>();
}

auto spp::analyse::utils::type_predicates::TemplateOf(
  TypeSymbol const &sym,
  Scope const &scope)
  -> TypeSymbol* {
  // Followed until nothing changes, capped against a cycle.
  auto *s = const_cast<TypeSymbol*>(&sym);
  for (auto step = 0; step < 8; ++step) {
    auto *next = s;
    if (s->Kind == TypeKind::GenericParam and s->ParamId != 0) {
      if (auto *const bound = scope.Canon(*s); bound != nullptr) { next = bound; }
    }
    else if ((s->Kind == TypeKind::GenericArg or s->IsSelf()) and s->LinkedScope != nullptr
      and s->LinkedScope->TySym != nullptr) {
      next = s->LinkedScope->TySym.get();
    }
    else if (s->Alias != nullptr and s->Alias->Resolved != nullptr) {
      if (auto *const target = scope.GetTypeSymbol(s->Alias->Resolved->WithoutGenerics().get()); target != nullptr) {
        next = target;
      }
    }
    if (next == s) { break; }
    s = next;
  }
  return s->InstanceOf != nullptr ? s->InstanceOf : s;
}

auto spp::analyse::utils::type_predicates::IsTemplate(
  TypeSymbol const &sym,
  TypeAst const &tmpl,
  Scope const &scope)
  -> bool {
  auto const *tmpl_sym = scope.GetTypeSymbol(&tmpl);
  return tmpl_sym != nullptr and TemplateOf(sym, scope) == TemplateOf(*tmpl_sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeGen(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::GEN;
  using generate::common_types_precompiled::GEN_ONCE;
  return IsTemplate(sym, *GEN, scope) or IsTemplate(sym, *GEN_ONCE, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTup(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::TUP;
  return IsTemplate(sym, *TUP, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeArr(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::ARR;
  return IsTemplate(sym, *ARR, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVariant(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::VAR;
  return IsTemplate(sym, *VAR, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeFunc(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::FUN_MOV;
  using generate::common_types_precompiled::FUN_MUT;
  using generate::common_types_precompiled::FUN_REF;
  return IsTemplate(sym, *FUN_MOV, scope) or IsTemplate(sym, *FUN_MUT, scope) or IsTemplate(sym, *FUN_REF, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeCompTimeIndexable(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  return IsTypeTup(sym, scope) or IsTypeArr(sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeBool(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::BOOL;
  return IsTemplate(sym, *BOOL, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVoid(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::VOID;
  return IsTemplate(sym, *VOID, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTry(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::TRY;
  return IsTemplate(sym, *TRY, scope);
}

// The kind checks for a resolved type, as a value is held: a borrow is none of the kinds, as "TypeEq" against a template
// never matched one, except that a borrowed variant is still a variant; "!" is only itself; and a "$" mock is a function
// value, as "TypeEq" matches it against the function types it superimposes.

auto spp::analyse::utils::type_predicates::IsTypeGen(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeGen(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTup(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeTup(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeArr(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeArr(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVariant(
  TypeRef const &ref,
  Scope const &scope)
  -> bool {
  const auto sym = ref.IsNever ? nullptr : ref.Sym;
  return sym != nullptr and IsTypeVariant(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeFunc(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and (sym->IsMock() or IsTypeFunc(*sym, scope));
}

auto spp::analyse::utils::type_predicates::IsTypeCompTimeIndexable(
  TypeRef const &ref,
  Scope const &scope)
  -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeCompTimeIndexable(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeBool(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeBool(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVoid(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeVoid(*sym, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTry(TypeRef const &ref, Scope const &scope) -> bool {
  const auto sym = ref.KindSym();
  return sym != nullptr and IsTypeTry(*sym, scope);
}

auto spp::analyse::utils::type_predicates::GetSuperimposedFatPointerFieldCount(
  TypeSymbol const &type_sym)
  -> std::size_t {
  if (type_sym.LinkedScope == nullptr) { return 0uz; }

  // "Gen"/"GenOnce" lower to a single opaque llvm coroutine handle
  // (the "llvm.coro.begin" result) rather than a true 2-pointer fat
  // pointer - only the "FunXXX" family is a { fn_ptr, env_ptr } pair.
  for (auto const *sup_scope : type_sym.LinkedScope->SupScopes()) {
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    if (IsTypeGen(*sup_scope->TySym, *type_sym.LinkedScope)) { return 1uz; }
    if (IsTypeFunc(*sup_scope->TySym, *type_sym.LinkedScope)) { return 2uz; }
  }
  return 0uz;
}

namespace spp::analyse::utils::type_predicates {
  namespace {
    /// A comp argument's value is concrete when it is closed - it folds to a literal, as a name bound to a value or
    /// "n + 1_uz" with "n" bound does - and not when it names an unbound generic: "SizedInteger[w=w]" in the
    /// template, or "A[n=(n + 1_uz)]" there, has no value to lower to. Any other kind of expression names no generic.
    auto IsCompValueConcrete(
      ExpressionAst const &val,
      Scope const &scope)
      -> bool {
      if (cmp_utils::FoldCompExpr(val, scope) != nullptr) { return true; }
      return val.To<IdentifierAst>() == nullptr and val.To<BinaryExpressionAst>() == nullptr
        and val.To<ParenthesisedExpressionAst>() == nullptr;
    }
  }
}

auto spp::analyse::utils::type_predicates::IsTypeFullyConcrete(
  TypeAst const &type,
  Scope const &scope)
  -> bool {
  // Only a name that positively resolves to an unbound parameter counts against the type. Such a symbol is found but
  // carries no prototype - it is linked to the dummy scope "GenericParameterAst::Stage2_GenTopLvlScopes" makes for
  // it - where a parameter bound to a real type carries that type's prototype.
  //
  // A name that resolves to nothing at all is a different situation and is deliberately not treated as a parameter: a
  // nested argument is looked up in the instantiation's own scope, which need not have every type its arguments were
  // written in terms of in view, and reading "not found" as "still generic" would refuse perfectly good instantiations.
  const auto stripped = type.WithoutGenerics()->WithoutConvention();
  const auto sym = scope.GetTypeSymbol(stripped.get());
  if (type.IsSelfType()) { return false; }
  if (sym != nullptr and sym->Type == nullptr) { return false; }

  // Then every argument, recursively. Recursion terminates because a written type is a finite tree; it is the
  // arguments that carry the parameters, and a type like "NonNull[T=T]" is only distinguishable from "NonNull[T=U8]"
  // by looking at them.
  for (auto const &gn_arg : type.LastTypePart()->GnArgGroup->Args) {
    if (gn_arg->TypeVal != nullptr) {
      if (not IsTypeFullyConcrete(*gn_arg->TypeVal, scope)) { return false; }
      continue;
    }

    if (gn_arg->CompVal != nullptr) {
      if (not IsCompValueConcrete(*gn_arg->CompVal, scope)) { return false; }
    }
  }
  return true;
}

auto spp::analyse::utils::type_predicates::IsTypeRecursive(
  ClassPrototypeAst const &type,
  ScopeManager const &sm)
  -> Shared<TypeAst> {
  // Get the attribute types recursively from the class prototype,
  // and check for a match with the class prototype. Use the source
  // type as this function is used for error reporting exclusively.
  auto attr_info = Vec<Pair<TypeSymbol*, ClassAttributeAst*>>{};
  GetAttrTypes(&type, sm.CurrentScope, attr_info);
  for (auto const &[attr_type_sym, attr_ast] : attr_info) {
    if (attr_type_sym == type.GetClsSym().get()) {
      return attr_ast->Source.OriginalType;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::type_predicates::IsTypeBorrowed(
  TypeAst const &type,
  ScopeManager const &sm,
  const bool deep)
  -> bool {
  // Check that either this type, or any inner types for variants,
  // are "&" or "&mut". Start with short-circuits on the type given,
  // which might contain an "&"/"&mut" unary operator.
  if (type.GetConvention() != nullptr) { return true; }
  if (type.IsSelfType()) { return false; }

  // A variant is borrowed when any member is: the members are flattened through nested variants already.
  if (deep and IsTypeVariant(TypeRef::OfHead(type, *sm.CurrentScope), *sm.CurrentScope)) {
    for (auto const &member : type_compare::VariantMembers(TypeRef::Of(type, *sm.CurrentScope), *sm.CurrentScope)) {
      if (member.IsBorrowed()) { return true; }
    }
  }

  // No borrowing of any nature discovered => non borrowable type.
  // Checked all depths.
  return false;
}

auto spp::analyse::utils::type_predicates::IsIndexWithinBound(
  const std::size_t index,
  TypeRef const &ref,
  Scope const &scope)
  -> Pair<bool, std::size_t> {
  // For tuples, count the number of generic arguments. This is the
  // number of arguments in the tuple. amd the upper bound.
  // Todo: What about variadic tuples? Per-proto analysis catches this?
  //  Add some unit tests to check.
  using errors::SppInternalCompilerError;
  if (IsTypeTup(ref, scope)) {
    const auto elems = ref.Sym->TypeArgTypes().Len();
    return {index < elems, elems};
  }

  // For arrays, check the size argument. This is the compile time
  // generic argument "n" that is always known / resolved. The size
  // is the instantiation's own binding of "n", however its argument
  // was written ("n + 1_uz").
  if (IsTypeArr(ref, scope)) {
    const auto *const size_val = ref.Sym->BoundCompArg("n");
    if (const auto *const size_lit = size_val != nullptr ? size_val->To<IntegerLiteralAst>() : nullptr) {
      const auto elems = std::stoul(size_lit->Val->TokenData);
      return {index < elems, elems};
    }
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe: the caller has already checked the kind.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(*ref.Sym->FqName(), err_msg));
}

auto spp::analyse::utils::type_predicates::GetNthTypeOfIndexableType(
  const std::size_t index,
  TypeRef const &ref,
  Scope const &scope)
  -> Shared<TypeAst> {
  // For tuples, return the nth generic argument. This can be
  // different per element.
  using errors::SppInternalCompilerError;
  if (IsTypeTup(ref, scope)) {
    return ref.Sym->TypeArgTypes()[index];
  }

  // For arrays, return the element type. This is always the same
  // per element.
  if (IsTypeArr(ref, scope)) {
    return ref.Sym->TypeArgType("T");
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe: the caller has already checked the kind.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(*ref.Sym->FqName(), err_msg));
}

auto spp::analyse::utils::type_predicates::AreGenericArgsConcrete(
  Vec<Unique<GenericArgumentAst>> const &args,
  Scope const &scope)
  -> bool {
  return genex::all_of(args | genex::views::ptr, [&](auto const *arg) {
    if (arg->TypeVal != nullptr) { return IsTypeFullyConcrete(*arg->TypeVal, scope); }
    if (arg->CompVal != nullptr) { return IsCompValueConcrete(*arg->CompVal, scope); }
    return true;
  });
}
