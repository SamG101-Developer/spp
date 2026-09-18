module;
#include <spp/analyse/macros.hpp>
module spp.analyse.utils.type_members;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.func_utils;
import spp.analyse.utils.generic_bindings;
import spp.analyse.utils.mem_info_utils;
import spp.analyse.utils.mem_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_predicates;
import spp.asts.annotation_ast;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.class_attribute_ast;
import spp.asts.class_implementation_ast;
import spp.asts.class_member_ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
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

namespace spp::analyse::utils::type_members {
  namespace {
    auto _UnimplementedAbstractMethodsCache() -> Map<
      Scope const*,
      Pair<std::uint64_t, Vec<FunctionPrototypeAst const*>>>& {
      static auto cache = Map<
        Scope const*,
        Pair<std::uint64_t, Vec<FunctionPrototypeAst const*>>>();
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
    const auto args = sym.TypeArgTypes();
    const auto is_arr = type_predicates::IsTypeArr(sym, scope);

    // A tuple has a part per argument; an array one per element, its length being its own binding of "n".
    auto elems = args.Len();
    if (is_arr) {
      const auto *const size_val = sym.BoundCompArg("n");
      const auto *const size_lit = size_val != nullptr ? size_val->To<IntegerLiteralAst>() : nullptr;
      elems = size_lit != nullptr ? std::stoul(size_lit->Val->TokenData) : 0uz;
      if (collapse_arrays) { elems = std::min(elems, 1uz); }
    }

    for (auto i = 0uz; i < elems; ++i) {
      const auto elem_type = args[is_arr ? 0uz : i];
      parts.EmplaceBack(
        MakeShared<IdentifierAst>(0uz, std::to_string(i)), i, elem_type, scope.GetTypeSymbol(elem_type.get()),
        &scope);
    }
    return parts;
  }

  // Everything else is its attributes, which carry the scope
  // each one's type resolves in with them.
  auto index = 0uz;
  for (auto const &[name, attr_sym, attr_scope] : GetAllAttrs(sym)) {
    parts.EmplaceBack(name, index++, attr_sym->FqName(), attr_sym, attr_scope);
  }
  return parts;
}

auto spp::analyse::utils::type_members::GetAllAttrs(
  TypeSymbol const &cls_sym)
  -> Vec<Tup<Shared<IdentifierAst>, TypeSymbol*, Scope*>> {
  auto extended_syms = Vec<Tup<Shared<IdentifierAst>, TypeSymbol*, Scope*>>{};
  for (auto const &[sup_scope, sym] : CollectAttrSyms(cls_sym)) {
    extended_syms.PushBack({sym->Name, sym->TypeRefIn(*sup_scope).Sym, sup_scope});
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
  for (auto const &declared : expr_utils::ScopesDeclaringVar(cls_scope, *cmp_member.Name, false)) {
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
        *declared.Where, own_scope, false),
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
  using func_utils::SameSignature;

  // Every mention of a type asks this, and the answer is a property of the type rather than of the mention: it is read
  // off the methods of this scope and of the scopes above it, none of which change once the type is in place. The work
  // is a signature comparison of every abstract method against every concrete one, so recomputing it per mention is
  // what makes it one of the most expensive things in analysis.
  //
  // Keyed on the scope, and retired whenever the shape of the scope tree changes - which covers both a scope being
  // re-parented and a type gaining or losing the super scopes its inherited methods come from, since both bump the
  // linkage generation.
  auto &cache = _UnimplementedAbstractMethodsCache();
  const auto generation = TypeStructureGeneration();
  if (const auto hit = cache.find(&type_scope); hit != cache.end() and hit->second.first == generation) {
    return hit->second.second;
  }

  // Skip on functional types because of the awkward difference with the base method having "args: Args" tuple of
  // args, and implementations having their own individual args.
  // Todo: Autopack and check?
  const auto remember = [&](Vec<FunctionPrototypeAst const*> answer) {
    cache[&type_scope] = {generation, answer};
    return answer;
  };

  if (type_scope.TySym != nullptr) {
    if (type_scope.TySym->IsMock()) { return remember({}); }
    if (type_predicates::IsTypeFunc(*type_scope.TySym, type_scope)) { return remember({}); }
  }

  // Gather every method visible on the type, from the type's own scope and from all of its super scopes, each tagged
  // with the scope that resolves the types in its signature.
  auto all_scopes = Vec<Scope const*>{&type_scope};
  all_scopes.AppendRange(type_scope.SupScopes());

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
  const auto base = type_predicates::GetSuperimposedFatPointerFieldCount(type_sym);

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
