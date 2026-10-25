module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_members;
import spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.member_lookup;
import spp.analyse.utils.overload_resolution;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.convention_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.integer_literal_ast;
import spp.asts.statement_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.utils.algorithms;
import spp.utils.ice;
import spp.utils.interner;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::type_members {
  namespace {
    auto FindHeldByValueImpl(
      TypeRef const &ref,
      Scope const &scope,
      FuncRef<bool(TypeRef const &)> matches,
      Set<TypeSymbol const*> &seen)
      -> TypeRef {
      if (ref.Symbol == nullptr or ref.IsBorrowed() or ref.IsNever) { return TypeRef(); }
      auto *const sym = ref.Symbol->AsBound();
      if (sym == nullptr or sym->IsGn()) { return TypeRef(); }
      if (matches(ref)) { return ref; }
      if (not seen.insert(sym).second) { return TypeRef(); }

      // A variant's storage is raw bytes sized for its members,
      // so it is the members themselves that are held.
      if (type_predicates::IsTypeVariant(ref, scope)) {
        for (auto const &member : type_members::VariantMemberRefs(ref, scope)) {
          if (auto found = FindHeldByValueImpl(member, scope, matches, seen); found.Symbol != nullptr) { return found; }
        }
        return TypeRef();
      }

      if (sym->LinkedScope == nullptr) { return TypeRef(); }
      for (auto const &part : type_members::GetAllParts(ref, scope, true)) {
        // The part's type itself, by value: a part bound to a borrow holds the borrowed type no more than "&T" does.
        auto const &where = part.Where != nullptr ? *part.Where : scope;
        if (auto found = FindHeldByValueImpl(part.Ref.WithoutConvention(), where, matches, seen);
          found.Symbol != nullptr) {
          return found;
        }
      }
      return TypeRef();
    }

    /** A tuple's or array's number of elements (an array's own binding of "n", however written); none if unknown. */
    auto IndexableLen(
      TypeRef const &ref,
      Scope const &scope)
      -> Opt<std::size_t> {
      if (type_predicates::IsTypeTuple(ref, scope)) { return ref.Symbol->TypeArgRefs().size(); }
      if (not type_predicates::IsTypeArray(ref, scope)) { return Nullopt; }
      const auto size = U64Of(ref.Symbol->CompArgId("n"));
      return size.has_value() ? Opt(static_cast<std::size_t>(*size)) : Nullopt;
    }

    /** The type of a tuple's or array's element "index": per element for a tuple, the one "T" for an array; read off
     * the instance's identity ("TypeSymbol::TypeArgRefs"). No type past the end. */
    auto IndexableElem(
      TypeRef const &ref,
      Scope const &scope,
      const std::size_t index)
      -> TypeRef {
      const auto args = ref.Symbol->TypeArgRefs();
      const auto at = type_predicates::IsTypeArray(ref, scope) ? 0uz : index;
      return at < args.size() ? args[at] : TypeRef();
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
    SPP_ATTR_HOT auto CollectAttrSymbols(
      TypeSymbol const &cls_sym)
      -> Vec<Pair<Scope*, VariableSymbol*>> {
      auto all_scopes = Vec{cls_sym.LinkedScope};
      all_scopes.append_range(cls_sym.LinkedScope->GetSupScopes());

      auto attrs = Vec<Pair<Scope*, VariableSymbol*>>{};
      for (auto *sup_scope : all_scopes) {
        if (AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
        for (auto *sym : sup_scope->GetAllVarSymbols(true)) {
          if (sym->Kind != VariableKind::Attribute) { continue; }
          attrs.push_back(MakePair(sup_scope, sym));
        }
      }
      return attrs;
    }

    /// A resolved type named as its symbol names it, under its
    /// convention: how a variant's members are listed.
    auto AsType(
      TypeRef const &ref)
      -> Shared<TypeAst> {
      auto name = ref.Symbol->FqName();
      return ref.Conv != ConventionTag::MOV ? name->WithConvention(ConventionAstOf(ref.Conv)) : name;
    }

    /// For a variant type, flatten any sub-variants into the
    /// single variant type list, and remove any duplicates too
    /// (flattening can introduce duplicates, or they might be
    /// pre-existing).
    auto FlattenVariants(
      TypeAst const &variants, Scope const &scope) -> Vec<Pair<Shared<TypeAst>, TypeRef>> {
      // Create the list of types that will be the final returned
      // flattened, deduplicated list.
      auto out = Vec<Pair<Shared<TypeAst>, TypeRef>>();

      // The adder function first checks that the type doesn't
      // exist within the output list, before adding it in.
      const auto add_unique = [&](Shared<TypeAst> const &member, TypeRef const &ref) {
        const auto same = [&](auto const &x) { return type_compare::TypeEq(ref, x.second, scope, scope); };
        if (genex::any_of(out, same)) { return; }
        out.emplace_back(member, ref);
      };

      // Get the variant member's types from the tuple that is
      // the "..Variants" generic parameter's argument. Add or
      // flatten each type into the resulting list.
      for (const auto arg : variants.LastTypePart()->GnArgGroup->GetTypeArgs()) {
        const auto ref = TypeRef::Of(*arg->TypeVal, scope);
        const auto nested = VariantMemberRefs(ref, scope);
        if (nested.empty()) { add_unique(arg->TypeVal, ref); }
        else { for (auto const &m : nested) { add_unique(AsType(m), m); } }
      }

      // Return the new filled flat vector.
      return out;
    }
  }
}

auto spp::analyse::utils::type_members::GetAllParts(
  TypeRef const &ref,
  Scope const &scope,
  const bool collapse_arrays)
  -> Vec<TypePart> {
  auto parts = Vec<TypePart>();
  if (ref.Symbol == nullptr) { return parts; }
  const auto value = ref.WithoutConvention();

  // A tuple and an array hold their parts positionally rather
  // than as attributes, and a destructure of one records each
  // element under its index. The arguments are read off the
  // instantiation's own name, which an alias shares through
  // the scope it links to.
  if (type_predicates::IsTypeCompTimeIndexable(value, scope)) {
    // A tuple has a part per argument; an array one per element.
    auto elems = IndexableLen(value, scope).value_or(0uz);
    if (collapse_arrays and type_predicates::IsTypeArray(value, scope)) { elems = std::min(elems, 1uz); }

    for (auto i = 0uz; i < elems; ++i) {
      auto elem_ref = IndexableElem(value, scope, i);
      auto elem_type = elem_ref.AstIn(scope);
      parts.emplace_back(MakeShared<IdentifierAst>(0uz, std::to_string(i)), i, std::move(elem_type), elem_ref, &scope);
    }
    return parts;
  }

  // Everything else is its attributes, which carry the scope
  // each one's type resolves in with them.
  auto index = 0uz;
  // Named by the symbol it resolved to where one did, else from its identity: an identity naming "Self" is keyed by the
  // spelling, which would mean another type wherever the part is read.
  for (auto const &[name, attr_ref, attr_scope] : GetAllAttrs(*ref.Symbol->AsBound())) {
    auto type = attr_ref.AstIn(*attr_scope);
    parts.emplace_back(name, index++, std::move(type), attr_ref, attr_scope);
  }
  return parts;
}

auto spp::analyse::utils::type_members::GetAllAttrs(
  TypeSymbol const &cls_sym)
  -> Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>> {
  auto extended_syms = Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>>{};
  for (auto const &[sup_scope, sym] : CollectAttrSymbols(cls_sym)) {
    extended_syms.push_back({sym->Name, sym->TypeRefIn(*sup_scope), sup_scope});
  }

  return extended_syms;
}

auto spp::analyse::utils::type_members::CheckShadowedCmpAgreesInType(
  CmpStatementAst const &cmp_member,
  Scope &cls_scope,
  Scope const &own_scope)
  -> void {
  //
  using errors::SppSuperimpositionExtensionCmpStatementInvalidError;

  // Skip this for $Types which are mock types over functions
  // and have unique function overload covering types.
  if (cmp_member.Type->IsCompilerGeneratedType()) { return; }

  // Iterate over every scope that declares the name directly:
  // the type's scope and each of its superimpositions, which
  // is the same walk the ambiguity checks are built on.
  for (auto const &declared : member_lookup::ScopesDeclaringVar(cls_scope, *cmp_member.Name)) {
    if (declared.Where == &own_scope) { continue; }

    // A class attribute is a different member reached a different
    // way, not another declaration of this constant, and a method's
    // mock has its own overload rules.
    const auto sym = declared.Symbol;
    if (not sym->IsCompTime() or sym->Kind == VariableKind::FnMock) { continue; }

    // If the type is inconsistent with the cmp statement being
    // checked then raise an error here.
    RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
      not type_compare::TypeEq(
        sym->TypeRefIn(*declared.Where), TypeRef::Of(*cmp_member.Type, own_scope),
        *declared.Where, own_scope),
      ERR_ARGS(cmp_member, *sym->Name));
  }
}

auto spp::analyse::utils::type_members::ClearUnimplementedAbstractMethodsCache()
  -> void {
  _UnimplementedAbstractMethodsCache().clear();
}

auto spp::analyse::utils::type_members::GetUnimplementedAbstractMethods(
  Scope const &type_scope)
  -> Vec<FunctionPrototypeAst const*> {
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
  const auto sups = type_scope.GetSupScopes();
  if (hit != cache.end() and genex::equals(hit->second.Sups, sups)) {
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

  if (type_scope.LinkedTypeSymbol != nullptr) {
    if (type_predicates::IsTypeFunction(TypeRef::ForKindCheck(type_scope), type_scope)) { return remember({}); }
  }

  // A method stays abstract only while nothing nearer the type implements its signature: an abstract method is
  // unimplemented when the type's method table still holds it ("overload_resolution::MethodsOf", which applies overrides
  // as a call reaching the method would), so the two never disagree. Read once per name an abstract method has.
  auto names = Vec<IdentifierAst const*>();
  for (auto const *scope : sups) {
    if (scope->AstNode == nullptr or AstAs<ClassPrototypeAst>(scope->AstNode) != nullptr) { continue; }
    for (const auto member : AstBody(scope->AstNode)) {
      const auto fn = SupMemberAsMethod(member);
      if (fn == nullptr or fn->AbstractAnnotation == nullptr) { continue; }
      if (genex::none_of(names, [&](auto const *n) { return n->NameId() == fn->Name->NameId(); })) {
        names.emplace_back(fn->Name.get());
      }
    }
  }

  auto unimplemented = Vec<FunctionPrototypeAst const*>();
  for (auto const *name : names) {
    for (auto const &entry : overload_resolution::MethodsOf(type_scope, *name)) {
      if (entry.Proto->AbstractAnnotation != nullptr) { unimplemented.emplace_back(entry.Proto); }
    }
  }

  return remember(std::move(unimplemented));
}

auto spp::analyse::utils::type_members::IsLeftUnimplementedByAnError(
  Scope const &type_scope,
  FunctionPrototypeAst const &abs_fn)
  -> bool {
  // Only a recovering compile carries on past a failed block,
  // so only it reaches a use of the type the block left abstract.
  // A block's instances share its ast, which is what the failure
  // was recorded against.
  if (not errors::diagnostic_sink::IsEnabled()) { return false; }
  for (auto const *sup_scope : type_scope.GetSupScopes()) {
    const auto ext = AstAs<SupPrototypeExtensionAst>(sup_scope->AstNode);
    if (ext == nullptr or not errors::diagnostic_sink::IsPoisoned(ext)) { continue; }
    for (const auto member : AstBody(sup_scope->AstNode)) {
      const auto fn = SupMemberAsMethod(member);
      if (fn != nullptr and *fn->Name == *abs_fn.Name) { return true; }
    }
  }
  return false;
}

auto spp::analyse::utils::type_members::AttrIndicesOf(
  Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>> const &attrs, IdentifierAst const &name)
  -> Vec<std::size_t> {
  auto out = Vec<std::size_t>();
  for (auto i = 0uz; i < attrs.size(); ++i) {
    if (*spp::get<0>(attrs[i]) == name) { out.emplace_back(i); }
  }
  return out;
}

auto spp::analyse::utils::type_members::AttrAstOf(
  Scope const &attr_scope, IdentifierAst const &name)
  -> ClassAttributeAst* {
  // Found by name in the class the scope is, where the symbol came from.
  for (auto const &member : attr_scope.AstNode->As<ClassPrototypeAst>()->Impl->Members) {
    if (const auto attr = member->To<ClassAttributeAst>(); attr != nullptr and *attr->Name == name) { return attr; }
  }
  return nullptr;
}

auto spp::analyse::utils::type_members::GetFieldIndexInType(
  TypeSymbol const &type_sym,
  IdentifierAst const &field_name)
  -> std::size_t {
  // A class superimposing "Gen"/"GenOnce"/a "FunXXX" gets
  // that interface's fat-pointer fields prepended ahead of
  // its own declared attributes, so an attribute's declared
  // index has to be shifted past them.
  const auto base = GetSuperimposedFatPointerFieldCount(type_sym);

  // The first attribute so named; past the last when none is (a name access to a shared one is ambiguous, so
  // the first is the one).
  const auto all_attrs = GetAllAttrs(type_sym);
  const auto indices = AttrIndicesOf(all_attrs, field_name);
  return base + (indices.empty() ? all_attrs.size() : indices.front());
}

auto spp::analyse::utils::type_members::SuperClsRefs(
  TypeRef const &ref,
  Scope const &scope)
  -> Vec<TypeRef> {
  auto out = Vec<TypeRef>();
  if (ref.Symbol == nullptr or ref.Symbol->LinkedScope == nullptr) { return out; }
  for (auto const *sup_scope : SuperClsScopes(ref.Symbol->LinkedScope->GetSupScopes())) {
    out.emplace_back(TypeRef::Of(*sup_scope->LinkedTypeSymbol, scope));
  }
  return out;
}

auto spp::analyse::utils::type_members::SuperClsScopes(
  Vec<Scope*> const &sup_scopes)
  -> Vec<Scope const*> {
  auto out = Vec<Scope const*>();
  for (auto const *sup_scope : sup_scopes) {
    if (sup_scope->LinkedTypeSymbol == nullptr
      or asts::AstAs<ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
    out.emplace_back(sup_scope);
  }
  return out;
}

auto spp::analyse::utils::type_members::SuperClsReads(
  Vec<Scope*> const &sup_scopes)
  -> Vec<Pair<TypeRef, Scope const*>> {
  auto out = Vec<Pair<TypeRef, Scope const*>>();
  for (auto const *cls_scope : SuperClsScopes(sup_scopes)) {
    out.emplace_back(TypeRef::Of(*cls_scope->LinkedTypeSymbol, *cls_scope), cls_scope);
  }
  return out;
}

auto spp::analyse::utils::type_members::GetSuperimposedFatPointerFieldCount(
  TypeSymbol const &type_sym)
  -> std::size_t {
  // "Gen"/"GenOnce" lower to a single opaque llvm coroutine handle
  // (the "llvm.coro.begin" result) rather than a true 2-pointer fat
  // pointer - only the "FunXXX" family is a { fn_ptr, env_ptr } pair.
  if (type_sym.LinkedScope == nullptr) { return 0uz; }
  auto const &scope = *type_sym.LinkedScope;
  for (auto const &sup : SuperClsRefs(TypeRef::ForKindCheck(type_sym, scope), scope)) {
    if (type_predicates::IsTypeGenerator(sup, scope)) { return 1uz; }
    if (type_predicates::IsTypeFunction(sup, scope)) { return 2uz; }
  }
  return 0uz;
}

auto spp::analyse::utils::type_members::FindHeldByValue(
  TypeRef const &ref,
  Scope const &scope,
  FuncRef<bool(TypeRef const &)> matches)
  -> TypeRef {
  auto seen = Set<TypeSymbol const*>();
  return FindHeldByValueImpl(ref, scope, matches, seen);
}

auto spp::analyse::utils::type_members::FindRecursiveAttrType(
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
  auto const *const self_template = TypeRef::ForKindCheck(*type.GetClsSymbol(), scope).Template();
  const auto is_self = [&](TypeRef const &held) { return held.Template() == self_template; };
  for (auto const *attr : type.Impl->Members
       | genex::views::ptr
       | genex::views::cast_dynamic<ClassAttributeAst*>()) {
    if (FindHeldByValue(TypeRef::Of(*attr->Type, scope), scope, is_self).Symbol != nullptr) {
      return attr->Source.OriginalType;
    }
  }
  return nullptr;
}

auto spp::analyse::utils::type_members::IndexBoundOf(
  TypeRef const &ref,
  Scope const &scope)
  -> std::size_t {
  // A tuple's bound is its number of arguments; an array's is its
  // compile-time "n", always known once resolved.
  // Todo: What about variadic tuples? Per-proto analysis catches this?
  //  Add some unit tests to check.
  if (const auto elems = IndexableLen(ref, scope); elems.has_value()) { return *elems; }

  // The caller has already checked the kind, so anything else
  // is a compiler bug.
  constexpr auto err_msg = "Non indexable type used in index check";
  Ice(*ref.Symbol->FqName(), scope, err_msg);
}

auto spp::analyse::utils::type_members::GetNthTypeOfIndexableType(
  const std::size_t index,
  TypeRef const &ref,
  Scope const &scope)
  -> TypeRef {
  if (type_predicates::IsTypeCompTimeIndexable(ref, scope)) {
    return IndexableElem(ref, scope, index);
  }

  // The caller has already checked the kind, so anything else
  // is a compiler bug.
  constexpr auto err_msg = "Non indexable type used in index check";
  Ice(*ref.Symbol->FqName(), scope, err_msg);
}

auto spp::analyse::utils::type_members::OrderVariantMembers(
  Vec<Pair<Shared<TypeAst>, TypeSymbol const*>> members)
  -> Vec<Shared<TypeAst>> {
  auto keyed = Vec<Pair<Str, Shared<TypeAst>>>();
  for (auto &[type, sym] : members) {
    keyed.emplace_back(sym != nullptr ? sym->FqName()->ToString() : type->ToString(), std::move(type));
  }
  genex::actions::stable_sort(keyed, {}, [](auto const &k) -> Str const& { return k.first; });
  auto out = Vec<Shared<TypeAst>>();
  for (auto &[_, type] : keyed) { out.emplace_back(std::move(type)); }
  return out;
}

/// [CHECKED]
auto spp::analyse::utils::type_members::VariantMembers(
  TypeAst const &type, Scope const &scope) -> Vec<Shared<TypeAst>> {
  // A type based overload of the variant member detection,
  // which flattens and deduplicates.
  const auto variants = type.LastTypePart()->GnArgGroup->At("Variants");
  if (variants != nullptr and variants->IsTypeArg()) {
    return FlattenVariants(*variants->TypeVal, scope)
      | genex::views::transform([](auto const &x) { return x.first; })
      | genex::to<Vec>();
  }

  return VariantMemberRefs(TypeRef::Of(type, scope), scope)
    | genex::views::transform([](auto const &x) { return AsType(x); })
    | genex::to<Vec>();
}

/// [CHECKED]
auto spp::analyse::utils::type_members::VariantMemberRefs(
  TypeRef const &ref, Scope const &scope) -> Vec<TypeRef> {
  // Never types won't have variant members so guard against
  // that at the start.
  const auto sym = ref.IsNever ? nullptr : ref.Symbol;
  if (sym == nullptr) { return {}; }

  // Its identity lists its members, flattened and in order,
  // each what it means; anything else is not a variant.
  if (ref.Id == nullptr or scopes::HeadOf(ref.Id).Kind != TypeKey::Tag::Variant) { return {}; }
  auto members = Vec<TypeRef>();
  // Read straight off each member's identity, one per member:
  // a member's index is its tag.
  for (const auto member : scopes::HeadOf(ref.Id).VariantMembers) { members.emplace_back(TypeRef::Of(member, scope)); }
  return members;
}
