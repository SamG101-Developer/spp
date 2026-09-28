module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_predicates;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_generics;
import spp.analyse.utils.type_compare;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.identifier_ast;
import spp.asts.parenthesised_expression_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_unary_expression_ast;
import spp.asts.generate.common_types_precompiled;
import spp.utils.algorithms;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::type_predicates {
  namespace {
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

auto spp::analyse::utils::type_predicates::IsTypeGen(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::GEN;
  using generate::common_types_precompiled::GEN_ONCE;
  return type_compare::IsTemplate(sym, *GEN, scope) or type_compare::IsTemplate(sym, *GEN_ONCE, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTup(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::TUP;
  return type_compare::IsTemplate(sym, *TUP, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeArr(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::ARR;
  return type_compare::IsTemplate(sym, *ARR, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVariant(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::VAR;
  return type_compare::IsTemplate(sym, *VAR, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeFunc(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::FUN_MOV;
  using generate::common_types_precompiled::FUN_MUT;
  using generate::common_types_precompiled::FUN_REF;
  return type_compare::IsTemplate(sym, *FUN_MOV, scope) or type_compare::IsTemplate(sym, *FUN_MUT, scope) or type_compare::IsTemplate(sym, *FUN_REF, scope);
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
  return type_compare::IsTemplate(sym, *BOOL, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeVoid(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::VOID;
  return type_compare::IsTemplate(sym, *VOID, scope);
}

auto spp::analyse::utils::type_predicates::IsTypeTry(
  TypeSymbol const &sym,
  Scope const &scope)
  -> bool {
  using generate::common_types_precompiled::TRY;
  return type_compare::IsTemplate(sym, *TRY, scope);
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

namespace spp::analyse::utils::type_predicates {
  namespace {
    /// A comp argument's value is concrete when it is closed - it folds to a literal, as a name bound to a value or
    /// "n + 1_uz" with "n" bound does - and not when it names an unbound generic: "SizedInteger[w=w]" in the
    /// template, or "A[n=(n + 1_uz)]" there, has no value to lower to. Any other kind of expression names no generic.
    auto IsCompValueConcrete(
      ExpressionAst const &val,
      Scope const &scope)
      -> bool {
      if (comp_generics::FoldCompExpr(val, scope) != nullptr) { return true; }
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
