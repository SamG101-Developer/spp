module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_members;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.function_values;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.statement_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.interner;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::type_members {
  namespace {
    auto FindHeldByValueImpl(
      TypeRef const &ref,
      Scope const &scope,
      std::function<bool(TypeSymbol const &, Scope const &)> const &matches,
      Set<TypeSymbol const*> &seen)
      -> TypeSymbol const* {
      if (ref.Sym == nullptr or ref.IsBorrowed() or ref.IsNever) { return nullptr; }
      auto *const sym = ref.Sym->AsBoundSymbol();
      if (sym == nullptr or sym->IsTypeGeneric()) { return nullptr; }
      if (matches(*sym, scope)) { return sym; }
      if (not seen.insert(sym).second) { return nullptr; }

      // A variant's storage is raw bytes sized for its members,
      // so it is the members themselves that are held.
      if (type_predicates::IsTypeVariant(*sym, scope)) {
        for (auto const &member : type_compare::VariantMembers(TypeRef::OfSym(*sym, scope), scope)) {
          if (auto const *found = FindHeldByValueImpl(member, scope, matches, seen); found != nullptr) { return found; }
        }
        return nullptr;
      }

      if (sym->LinkedScope == nullptr) { return nullptr; }
      for (auto const &part : type_members::GetAllParts(*sym, scope, true)) {
        if (part.Sym == nullptr) { continue; }
        if (auto const *found = FindHeldByValueImpl(
          TypeRef::OfResolved(*part.Sym, part.Where != nullptr ? *part.Where : scope), part.Where != nullptr ? *part.Where : scope,
          matches, seen); found != nullptr) {
          return found;
        }
      }
      return nullptr;
    }

    /** A tuple's or array's number of elements (an array's own binding of "n", however written); none if unknown. */
    auto IndexableLen(
      TypeSymbol const &sym,
      Scope const &scope)
      -> std::optional<std::size_t> {
      if (type_predicates::IsTypeTup(sym, scope)) { return sym.TypeArgTypes().Len(); }
      if (not type_predicates::IsTypeArr(sym, scope)) { return std::nullopt; }
      const auto *const size_val = sym.BoundCompArg("n");
      const auto *const size_lit = size_val != nullptr ? size_val->To<IntegerLiteralAst>() : nullptr;
      if (size_lit == nullptr) { return std::nullopt; }
      return std::stoul(size_lit->Val->TokenData);
    }

    /** The type of a tuple's or array's element "index": per element for a tuple, the one "T" for an array. */
    auto IndexableElem(
      TypeSymbol const &sym,
      Scope const &scope,
      const std::size_t index)
      -> Shared<TypeAst> {
      return sym.TypeArgTypes()[type_predicates::IsTypeArr(sym, scope) ? 0uz : index];
    }

    /// A type's unimplemented abstract methods, with what the answer was read off: the generation it was last known
    /// current at, and the super scopes it was gathered from.
    struct UnimplementedAbstractMethods {
      std::uint64_t Generation = 0;
      Vec<Scope*> Sups;
      Vec<FunctionPrototypeAst const*> Methods;
    };

    auto _UnimplementedAbstractMethodsCache() -> Map<Scope const*, UnimplementedAbstractMethods>& {
      static auto cache = Map<Scope const*, UnimplementedAbstractMethods>();
      return cache;
    }

    auto SupMemberAsMethod(
      Ast const *member)
      -> FunctionPrototypeAst const* {
      if (const auto fn = member->To<FunctionPrototypeAst>(); fn != nullptr) { return fn; }
      if (const auto ext = member->To<SupPrototypeExtensionAst>(); ext != nullptr and ext->Impl != nullptr) {
        const auto final_member = ext->Impl->FinalMember();
        return final_member != nullptr ? final_member->To<FunctionPrototypeAst>() : nullptr;
      }
      return nullptr;
    }

    /**
     * The attributes of a type and everything superimposed on it, as (owning scope, symbol) pairs. Both public
     * attribute walks go through this so that their results line up index for index, which callers rely on. They used
     * to walk separately - one over the symbol table, one over the class prototype's members, and only one of them
     * skipping generic symbols - so a single generic-typed attribute silently desynchronised them.
     * @param cls_sym The symbol of the type whose attributes are wanted.
     * @return One pair per attribute, ordered by the type itself then its super scopes.
     */
    auto CollectAttrSyms(
      TypeSymbol const &cls_sym)
      -> Vec<Pair<Scope*, VariableSymbol*>> {
      auto all_scopes = Vec{cls_sym.LinkedScope};
      all_scopes.AppendRange(cls_sym.LinkedScope->SupScopes());

      auto attrs = Vec<Pair<Scope*, VariableSymbol*>>{};
      for (auto *sup_scope : all_scopes) {
        if (AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
        for (auto *sym : sup_scope->AllVarSymbols(true)) {
          if (sym->Kind != VariableKind::Attribute) { continue; }
          attrs.PushBack(MakePair(sup_scope, sym));
        }
      }
      return attrs;
    }
  }
}

auto spp::analyse::utils::type_members::GetAllParts(
  TypeSymbol const &sym,
  Scope const &scope,
  const bool collapse_arrays)
  -> Vec<TypePart> {
  auto parts = Vec<TypePart>();

  // A tuple and an array hold their parts positionally rather
  // than as attributes, and a destructure of one records each
  // element under its index. The arguments are read off the
  // instantiation's own name, which an alias shares through
  // the scope it links to.
  if (type_predicates::IsTypeCompTimeIndexable(sym, scope)) {
    // A tuple has a part per argument; an array one per element.
    auto elems = IndexableLen(sym, scope).value_or(0uz);
    if (collapse_arrays and type_predicates::IsTypeArr(sym, scope)) { elems = std::min(elems, 1uz); }

    for (auto i = 0uz; i < elems; ++i) {
      const auto elem_type = IndexableElem(sym, scope, i);
      parts.EmplaceBack(
        MakeShared<IdentifierAst>(0uz, std::to_string(i)), i, elem_type, scope.GetTypeSymbol(elem_type.get()),
        &scope);
    }
    return parts;
  }

  // Everything else is its attributes, which carry the scope
  // each one's type resolves in with them.
  auto index = 0uz;
  // Named by the symbol it resolved to where one did, else from its identity: an identity naming "Self" is keyed by the
  // spelling, which would mean another type wherever the part is read.
  for (auto const &[name, attr_ref, attr_scope] : GetAllAttrs(sym)) {
    auto type = attr_ref.Sym != nullptr ? attr_ref.Sym->FqName() : attr_scope->TypeAstOf(attr_ref.Id);
    parts.EmplaceBack(name, index++, std::move(type), attr_ref.Sym, attr_scope);
  }
  return parts;
}

auto spp::analyse::utils::type_members::GetAllAttrs(
  TypeSymbol const &cls_sym)
  -> Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>> {
  auto extended_syms = Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>>{};
  for (auto const &[sup_scope, sym] : CollectAttrSyms(cls_sym)) {
    extended_syms.PushBack({sym->Name, sym->TypeRefIn(*sup_scope), sup_scope});
  }

  return extended_syms;
}

auto spp::analyse::utils::type_members::CheckShadowedCmpAgreesInType(
  CmpStatementAst const &cmp_member,
  Scope &cls_scope,
  Scope const &own_scope,
  ScopeManager const &sm)
  -> void {
  //
  using errors::SppSuperimpositionExtensionCmpStatementInvalidError;

  // Skip this for $Types which are mock types over functions
  // and have unique function overload covering types.
  if (cmp_member.Type->IsCompilerGeneratedType()) { return; }

  // Iterate over every scope that declares the name directly:
  // the type's scope and each of its superimpositions, which
  // is the same walk the ambiguity checks are built on.
  for (auto const &declared : member_lookup::ScopesDeclaringVar(cls_scope, *cmp_member.Name, false)) {
    if (declared.Where == &own_scope) { continue; }

    // A class attribute is a different member reached a different
    // way, not another declaration of this constant, and a method's
    // mock has its own overload rules.
    const auto sym = declared.Symbol;
    if (not sym->IsCompTime() or sym->Kind == VariableKind::Function) { continue; }

    // If the type is inconsistent with the cmp statement being
    // checked then raise an error here.
    RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
      not type_compare::TypeEq(
        sym->TypeRefIn(*declared.Where), TypeRef::Of(*cmp_member.Type, own_scope),
        *declared.Where, own_scope),
      {declared.Where, sm.CurrentScope}, ERR_ARGS(cmp_member, *sym->Name));
  }
}

auto spp::analyse::utils::type_members::ClearUnimplementedAbstractMethodsCache()
  -> void {
  _UnimplementedAbstractMethodsCache().clear();
}

auto spp::analyse::utils::type_members::GetUnimplementedAbstractMethods(
  Scope const &type_scope)
  -> Vec<FunctionPrototypeAst const*> {
  //
  using function_values::SameSignature;

  // Every mention of a type asks this, and the answer is a property of the type rather than of the mention: it is read
  // off the methods of this scope and of the scopes above it, none of which change once the type is in place. The work
  // is a signature comparison of every abstract method against every concrete one, so recomputing it per mention is
  // what makes it one of the most expensive things in analysis.
  //
  // Keyed on the scope. The methods are the scope's own and its super scopes', which are fixed once each is made, so
  // the answer stands for as long as the super scopes do. The structure generation moves whenever any type anywhere
  // gains a super scope (every instantiation does), so when it has moved, the super scopes are compared rather than the
  // answer thrown away.
  auto &cache = _UnimplementedAbstractMethodsCache();
  const auto generation = TypeStructureGeneration();
  const auto hit = cache.find(&type_scope);
  if (hit != cache.end() and hit->second.Generation == generation) { return hit->second.Methods; }
  const auto sups = type_scope.SupScopes();
  if (hit != cache.end() and hit->second.Sups == sups) {
    hit->second.Generation = generation;
    return hit->second.Methods;
  }

  // Skip on functional types because of the awkward difference with the base method having "args: Args" tuple of
  // args, and implementations having their own individual args.
  // Todo: Autopack and check?
  const auto remember = [&](Vec<FunctionPrototypeAst const*> answer) {
    cache[&type_scope] = {generation, sups, answer};
    return answer;
  };

  if (type_scope.TySym != nullptr) {
    if (type_scope.TySym->IsMock()) { return remember({}); }
    if (type_predicates::IsTypeFunc(*type_scope.TySym, type_scope)) { return remember({}); }
  }

  // Gather every method visible on the type, from the type's own scope and from all of its super scopes, each tagged
  // with the scope that resolves the types in its signature.
  auto all_scopes = Vec<Scope const*>{&type_scope};
  all_scopes.AppendRange(sups);

  auto methods = Vec<Pair<Scope const*, FunctionPrototypeAst const*>>();
  for (auto const *scope : all_scopes) {
    if (scope->AstNode == nullptr) { continue; }

    const auto impl = AstAs<ClassPrototypeAst>(scope->AstNode) == nullptr
      ? AstBody(scope->AstNode)
      : Vec<Ast*>{};
    if (impl.IsEmpty()) { continue; }

    for (const auto member : impl) {
      const auto fn = SupMemberAsMethod(member);
      if (fn == nullptr) { continue; }

      const auto method_scope = genex::find_if(
        scope->Children, [member](auto const &child) { return child->AstNode == member; });
      if (method_scope == scope->Children.end()) { continue; }

      methods.EmplaceBack(method_scope->get(), fn);
    }
  }

  // A method stays abstract only while nothing anywhere on the type implements its signature. "SameSignature" answers
  // false for any two methods whose names differ, so only the concrete methods sharing a name with the abstract one can
  // implement it; indexing them by name turns a scan of every method on the type into a scan of its overloads, which
  // for a type with many single-overload methods is the difference between a square and a line.
  auto concrete_by_name = Map<
    spp::utils::InternedId, Vec<Pair<Scope const*, FunctionPrototypeAst const*>>>();
  for (auto const &entry : methods) {
    if (entry.second->AbstractAnnotation != nullptr) { continue; }
    concrete_by_name[entry.second->Name->NameId()].EmplaceBack(entry);
  }

  auto unimplemented = Vec<FunctionPrototypeAst const*>();
  for (auto const &[abs_scope, abs_fn] : methods) {
    if (abs_fn->AbstractAnnotation == nullptr) { continue; }

    const auto candidates = concrete_by_name.find(abs_fn->Name->NameId());
    const auto is_implemented = candidates != concrete_by_name.end()
      and genex::any_of(candidates->second, [&](auto const &other) {
        return SameSignature(*other.second, *other.first, *abs_fn, *abs_scope);
      });

    if (not is_implemented) { unimplemented.EmplaceBack(abs_fn); }
  }

  return remember(std::move(unimplemented));
}

auto spp::analyse::utils::type_members::GetAllAttrAsts(
  TypeSymbol const &cls_sym)
  -> Vec<ClassAttributeAst*> {
  // Driven off the same walk as "GetAllAttrs" so the two line up index for index, then resolved to an ast by name
  // within the scope the symbol came from. Enumerating the prototype's members directly is what let the two lists
  // drift, because the member list has no notion of the generic symbols the other walk skips.
  auto attr_asts = Vec<ClassAttributeAst*>{};
  for (auto const &[sup_scope, sym] : CollectAttrSyms(cls_sym)) {
    const auto cls_proto = sup_scope->AstNode->ToUnchecked<ClassPrototypeAst>();
    auto *found = static_cast<ClassAttributeAst*>(nullptr);
    for (auto const &member : cls_proto->Impl->Members) {
      const auto attr = member->To<ClassAttributeAst>();
      if (attr != nullptr and *attr->Name == *sym->Name) {
        found = attr;
        break;
      }
    }

    // Pushed even when nothing matched, so that a symbol with no written attribute shortens neither list and the
    // index alignment holds regardless.
    attr_asts.EmplaceBack(found);
  }

  return attr_asts;
}

auto spp::analyse::utils::type_members::GetFieldIndexInType(
  TypeSymbol const &type_sym,
  IdentifierAst const &field_name)
  -> std::size_t {
  // A class superimposing "Gen"/"GenOnce"/a "FunXXX" gets that interface's fat-pointer fields prepended ahead of
  // its own declared attributes (see "ClassPrototypeAst::FillLlvmLayout"), so an attribute's declared index has
  // to be shifted past them.
  const auto base = type_members::GetSuperimposedFatPointerFieldCount(type_sym);

  // Get all the attributes on the type.
  const auto all_attrs = GetAllAttrs(type_sym);

  // Find the field index.
  for (auto index = 0uz; index < all_attrs.Len(); ++index) {
    if (*spp::get<0>(all_attrs[index]) == field_name) {
      return base + index;
    }
  }

  return base + all_attrs.Len();
}

auto spp::analyse::utils::type_members::SuperClassTypes(
  TypeSymbol const &sym)
  -> Vec<TypeSymbol*> {
  auto out = Vec<TypeSymbol*>();
  if (sym.LinkedScope == nullptr) { return out; }
  for (auto const *sup_scope : sym.LinkedScope->SupScopes()) {
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    out.EmplaceBack(sup_scope->TySym.get());
  }
  return out;
}

auto spp::analyse::utils::type_members::SuperClassNames(
  Vec<Scope*> const &sup_scopes)
  -> Vec<Pair<Shared<TypeAst>, Scope const*>> {
  auto out = Vec<Pair<Shared<TypeAst>, Scope const*>>();
  for (auto const *sup_scope : sup_scopes) {
    if (sup_scope->TySym == nullptr or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    out.EmplaceBack(sup_scope->TySym->FqName(), sup_scope);
  }
  return out;
}

auto spp::analyse::utils::type_members::GetSuperimposedFatPointerFieldCount(
  TypeSymbol const &type_sym)
  -> std::size_t {
  // "Gen"/"GenOnce" lower to a single opaque llvm coroutine handle
  // (the "llvm.coro.begin" result) rather than a true 2-pointer fat
  // pointer - only the "FunXXX" family is a { fn_ptr, env_ptr } pair.
  for (auto const *sup : SuperClassTypes(type_sym)) {
    if (type_predicates::IsTypeGen(*sup, *type_sym.LinkedScope)) { return 1uz; }
    if (type_predicates::IsTypeFunc(*sup, *type_sym.LinkedScope)) { return 2uz; }
  }
  return 0uz;
}

auto spp::analyse::utils::type_members::FindHeldByValue(
  TypeRef const &ref,
  Scope const &scope,
  std::function<bool(TypeSymbol const &, Scope const &)> const &matches)
  -> TypeSymbol const* {
  auto seen = Set<TypeSymbol const*>();
  return FindHeldByValueImpl(ref, scope, matches, seen);
}

auto spp::analyse::utils::type_members::IsTypeRecursive(
  ClassPrototypeAst const &type,
  ScopeManager const &sm)
  -> Shared<TypeAst> {
  // A type holding itself by value, directly or through anything
  // held by value - another class's attribute, a tuple, an array, a
  // variant or a generic class ("Wrap[Self]"), or its own generic
  // ("A[T]" inside "A") - is infinitely large. The attribute's
  // source type is returned, as this is used for error reporting
  // exclusively.
  auto const &scope = *sm.CurrentScope;
  const auto self_template = type_compare::TemplateOf(*type.GetClsSym(), scope);
  const auto is_self = [&](TypeSymbol const &sym, Scope const &where) { return type_compare::TemplateOf(sym, where) == self_template; };
  for (auto const *attr : type.Impl->Members
       | genex::views::ptr
       | genex::views::cast_dynamic<ClassAttributeAst*>()) {
    if (FindHeldByValue(TypeRef::Of(*attr->Type, scope), scope, is_self) != nullptr) {
      return attr->Source.OriginalType;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::type_members::IsIndexWithinBound(
  const std::size_t index,
  TypeRef const &ref,
  Scope const &scope)
  -> Pair<bool, std::size_t> {
  // A tuple's bound is its number of arguments; an array's is its
  // compile-time "n", always known once resolved.
  // Todo: What about variadic tuples? Per-proto analysis catches this?
  //  Add some unit tests to check.
  using errors::SppInternalCompilerError;
  if (const auto *const sym = ref.KindSym(); sym != nullptr) {
    if (const auto elems = IndexableLen(*sym, scope); elems.has_value()) { return {index < *elems, *elems}; }
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe: the caller has already checked the kind.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(*ref.Sym->FqName(), err_msg));
}

auto spp::analyse::utils::type_members::GetNthTypeOfIndexableType(
  const std::size_t index,
  TypeRef const &ref,
  Scope const &scope)
  -> Shared<TypeAst> {
  using errors::SppInternalCompilerError;
  if (const auto *const sym = ref.KindSym(); sym != nullptr and type_predicates::IsTypeCompTimeIndexable(*sym, scope)) {
    return IndexableElem(*sym, scope, index);
  }

  // Cause an ICE if we reach this state. Should be impossible but
  // just a failsafe: the caller has already checked the kind.
  constexpr auto err_msg = "Non indexable type used in index check";
  Raise<SppInternalCompilerError>(
    {&scope}, ERR_ARGS(*ref.Sym->FqName(), err_msg));
}
