module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_predicates;
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

namespace spp::analyse::utils::type_predicates {
  namespace {
    auto GetAttrTypes(
      const asts::ClassPrototypeAst *cls_proto,
      const scopes::Scope *cls_scope,
      Vec<Pair<scopes::TypeSymbol*, asts::ClassAttributeAst*>> &attr_symbols)
      -> void {
      // Get all attribute types, without recursion errors (this will
      // be handled elsewhere, so assume it has been checked already).
      for (auto const &member : cls_proto->Impl->Members
           | genex::views::ptr
           | genex::views::cast_dynamic<asts::ClassAttributeAst*>) {
        auto type_sym = cls_scope->GetTypeSymbol(member->Type.get());
        if (genex::contains(attr_symbols, type_sym, [](auto &&x) { return x.first; })) { continue; }
        if (type_sym->IsGeneric) { continue; }

        attr_symbols.EmplaceBack(type_sym, member);
        GetAttrTypes(type_sym->Type, type_sym->LinkedScope, attr_symbols);
      }
    }
  }
}

auto spp::analyse::utils::type_predicates::IsTypeTry(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::try::Try[Ok, Err]".
  using asts::generate::common_types_precompiled::TRY;

  return type_compare::TypeEq(*type.WithoutGenerics(), *TRY, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeCompTimeIndexable(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // The only two types that can be indexed at compile time are the
  // tuple type, and the array type.
  return
    IsTypeTup(*type.WithoutGenerics(), scope) or IsTypeArr(*type.WithoutGenerics(), scope);
}

auto spp::analyse::utils::type_predicates::IsTypeArr(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::array::Arr[T, n]". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::ARR;
  return type_compare::TypeEq(*type.WithoutGenerics(), *ARR, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTup(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "Tup::Tup[Ts...]". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::TUP;
  return type_compare::TypeEq(*type.WithoutGenerics(), *TUP, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTupSymbol(
  scopes::TypeSymbol const &sym)
  -> bool {
  // Compared against the precompiled name rather than through a scope, because the symbol's own qualified name is
  // already the answer: an alias for the tuple resolves to "std::tuple::Tup" just as the type itself does.
  using asts::generate::common_types_precompiled::TUP;
  const auto as_unary = dynamic_shared_cast<asts::TypeUnaryExpressionAst>(sym.FqName()->WithoutGenerics());
  return as_unary != nullptr and *as_unary == *TUP->ToUnchecked<asts::TypeUnaryExpressionAst>();
}

auto spp::analyse::utils::type_predicates::IsTypeVariant(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::variant::Variant[Ts...]". This
  // only considers the type directly, not any supertypes. It does
  // a "remove convention" first. Todo: Conv for others?
  using asts::generate::common_types_precompiled::VAR;
  return type_compare::TypeEq(*type.WithoutConvention()->WithoutGenerics(), *VAR, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeBool(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::bool::Bool". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::BOOL;
  return type_compare::TypeEq(type, *BOOL, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeGen(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::generator::Gen[T]" or
  // "std::generator::GenOnce[T]". This only considers the
  // type directly, not any supertypes.
  using asts::generate::common_types_precompiled::GEN;
  using asts::generate::common_types_precompiled::GEN_ONCE;

  return
    type_compare::TypeEq(*type.WithoutGenerics(), *GEN, scope, scope) or
    type_compare::TypeEq(*type.WithoutGenerics(), *GEN_ONCE, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVoid(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::void::Void". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::VOID;
  return type_compare::TypeEq(type, *VOID, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeNever(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against "std::never::Never". This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::NEVER;
  return type_compare::TypeEq(type, *NEVER, scope, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeSelf(
  asts::TypeAst const &type)
  -> bool {
  // Check for a string match to "Self".
  const auto type_identifier = type.To<asts::TypeIdentifierAst>();
  return type_identifier != nullptr and type_identifier->Name == "Self";
}

auto spp::analyse::utils::type_predicates::IsTypeFunc(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Check the type against one of the following three targets:
  // `std::function::FunRef|FunMut|FunMov[Args, Out]`. This only
  // considers the type directly, not any supertypes.
  using asts::generate::common_types_precompiled::FUN_MOV;
  using asts::generate::common_types_precompiled::FUN_MUT;
  using asts::generate::common_types_precompiled::FUN_REF;
  return
    type_compare::TypeEq(*type.WithoutGenerics(), *FUN_MOV, scope, scope) or
    type_compare::TypeEq(*type.WithoutGenerics(), *FUN_MUT, scope, scope) or
    type_compare::TypeEq(*type.WithoutGenerics(), *FUN_REF, scope, scope);
}

auto spp::analyse::utils::type_predicates::GetSuperimposedFatPointerFieldCount(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> std::size_t {
  const auto type_sym = scope.GetTypeSymbol(&type);
  if (type_sym == nullptr or type_sym->LinkedScope == nullptr) { return 0uz; }

  // "Gen"/"GenOnce" lower to a single opaque llvm coroutine handle
  // (the "llvm.coro.begin" result) rather than a true 2-pointer fat
  // pointer - only the "FunXXX" family is a { fn_ptr, env_ptr } pair.
  for (auto const &sup_type : type_sym->LinkedScope->SupTypes()) {
    if (IsTypeGen(*sup_type, *type_sym->LinkedScope)) { return 1uz; }
    if (IsTypeFunc(*sup_type, *type_sym->LinkedScope)) { return 2uz; }
  }
  return 0uz;
}

auto spp::analyse::utils::type_predicates::IsTypeFullyConcrete(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> bool {
  // Only a name that positively resolves to an unbound parameter counts against the type. Such a symbol is found but
  // carries no prototype - it is linked to the dummy scope "GenericParameterTypeAst::Stage2_GenTopLvlScopes" makes for
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
    if (const auto type_arg = gn_arg->To<asts::GenericArgumentTypeAst>(); type_arg != nullptr) {
      if (not IsTypeFullyConcrete(*type_arg->Val, scope)) { return false; }
      continue;
    }

    // A comp argument still written as a name is a parameter rather than a value: "SizedInteger[w=w]" is the template
    // and "SizedInteger[w=32]" is the instantiation, and only the second has a width to lower to (see the
    // "kSizedIntegerParts" case in "RegisterLlvmTypeInfo", which gives up on anything that is not a literal). A name
    // that stood for a value would have been rewritten to that value when the instantiation was built.
    if (const auto comp_arg = gn_arg->To<asts::GenericArgumentCompAst>(); comp_arg != nullptr) {
      if (comp_arg->Val->To<asts::IdentifierAst>() != nullptr) { return false; }
    }
  }
  return true;
}

auto spp::analyse::utils::type_predicates::IsTypeRecursive(
  asts::ClassPrototypeAst const &type,
  scopes::ScopeManager const &sm)
  -> Shared<asts::TypeAst> {
  // Get the attribute types recursively from the class prototype,
  // and check for a match with the class prototype. Use the source
  // type as this function is used for error reporting exclusively.
  auto attr_info = Vec<Pair<scopes::TypeSymbol*, asts::ClassAttributeAst*>>{};
  GetAttrTypes(&type, sm.CurrentScope, attr_info);
  for (auto const &[attr_type_sym, attr_ast] : attr_info) {
    if (attr_type_sym == type.GetClsSym().get()) {
      return attr_ast->Source.OriginalType;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::type_predicates::IsTypeBorrowed(
  asts::TypeAst const &type,
  scopes::ScopeManager const &sm,
  const bool deep)
  -> bool {
  // Check that either this type, or any inner types for variants,
  // are "&" or "&mut". Start with short-circuits on the type given,
  // which might contain an "&"/"&mut" unary operator.
  using asts::generate::common_types_precompiled::VAR;
  if (type.GetConvention() != nullptr) { return true; }
  if (type.IsSelfType()) { return false; }

  // Check the inner types for variant types. Reuse this function
  // recursively to reach any depth type, and check for a possible
  // borrow.
  if (deep and type_compare::TypeEq(*type.WithoutGenerics(), *VAR, *sm.CurrentScope, *sm.CurrentScope, false)) {
    for (auto const &inner_type : type_compare::DedupVariableInnerTypes(type, *sm.CurrentScope)) {
      if (IsTypeBorrowed(*inner_type, sm, deep)) { return true; }
    }
  }

  // No borrowing of any nature discovered => non borrowable type.
  // Checked all depths.
  return false;
}

auto spp::analyse::utils::type_predicates::IsIndexWithinBound(
  const std::size_t index,
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Pair<bool, std::size_t> {
  // For tuples, count the number of generic arguments. This is the
  // number of arguments in the tuple. amd the upper bound.
  // Todo: What about variadic tuples? Per-proto analysis catches this?
  //  Add some unit tests to check.
  using errors::SppInternalCompilerError;
  if (IsTypeTup(type, scope)) {
    const auto elems = type.LastTypePart()->GnArgGroup->Args.Len();
    return {index < elems, elems};
  }

  // For arrays, check the size argument. This is the compile time
  // generic argument "n" that is always known / resolved.
  if (IsTypeArr(type, scope)) {
    const auto size_arg = type.LastTypePart()->GnArgGroup->CompAt("n");
    const auto size_arg_cast = size_arg->Val->To<asts::IntegerLiteralAst>();
    const auto elems = std::stoul(size_arg_cast->Val->TokenData);
    return {index < elems, elems};
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(type, err_msg));
}

auto spp::analyse::utils::type_predicates::GetNthTypeOfIndexableType(
  const std::size_t index,
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Shared<asts::TypeAst> {
  // For tuples, return the nth generic argument. This can be
  // different per element.
  using errors::SppInternalCompilerError;
  if (IsTypeTup(type, scope)) {
    return type.LastTypePart()->GnArgGroup->GetTypeArgs()[index]->Val;
  }

  // For arrays, return the element type. This is always the same
  // per element.
  if (IsTypeArr(type, scope)) {
    return type.LastTypePart()->GnArgGroup->GetTypeArgs()[0]->Val;
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(type, err_msg));
}
