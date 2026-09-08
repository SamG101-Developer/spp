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

namespace spp::analyse::utils::type_members {
  namespace {
    auto _UnimplementedAbstractMethodsCache() -> Map<
      scopes::Scope const*,
      Pair<std::uint64_t, Vec<asts::FunctionPrototypeAst const*>>>& {
      static auto cache = Map<
        scopes::Scope const*,
        Pair<std::uint64_t, Vec<asts::FunctionPrototypeAst const*>>>();
      return cache;
    }

    auto SupMemberAsMethod(
      asts::Ast const *member)
      -> asts::FunctionPrototypeAst const* {
      if (const auto fn = member->To<asts::FunctionPrototypeAst>(); fn != nullptr) { return fn; }
      if (const auto ext = member->To<asts::SupPrototypeExtensionAst>(); ext != nullptr and ext->Impl != nullptr) {
        const auto final_member = ext->Impl->FinalMember();
        return final_member != nullptr ? final_member->To<asts::FunctionPrototypeAst>() : nullptr;
      }
      return nullptr;
    }

    /**
     * The attributes of a type and everything superimposed on it, as (owning scope, symbol) pairs. Both public
     * attribute walks go through this so that their results line up index for index, which callers rely on. They used
     * to walk separately - one over the symbol table, one over the class prototype's members, and only one of them
     * skipping generic symbols - so a single generic-typed attribute silently desynchronised them.
     * @param type The type whose attributes are wanted.
     * @param scope The scope, used to resolve @p type to its symbol.
     * @return One pair per attribute, ordered by the type itself then its super scopes.
     */
    auto CollectAttrSyms(
      asts::TypeAst const &type,
      scopes::Scope const &scope)
      -> Vec<Pair<scopes::Scope*, scopes::VariableSymbol*>> {
      const auto cls_sym = scope.GetTypeSymbol(&type);
      auto all_scopes = Vec{cls_sym->LinkedScope};
      all_scopes.AppendRange(cls_sym->LinkedScope->SupScopes());

      auto attrs = Vec<Pair<scopes::Scope*, scopes::VariableSymbol*>>{};
      for (auto *sup_scope : all_scopes) {
        if (AstAs<asts::ClassPrototypeAst>(sup_scope->AstNode) == nullptr) { continue; }
        for (auto *sym : sup_scope->AllVarSymbols(true)) {
          if (sym->IsGeneric) { continue; }
          attrs.PushBack(MakePair(sup_scope, sym));
        }
      }
      return attrs;
    }
  }
}

auto spp::analyse::utils::type_members::GetAllAttrs(
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Vec<Tup<Shared<asts::IdentifierAst>, scopes::TypeSymbol*, scopes::Scope*>> {
  auto extended_syms = Vec<Tup<Shared<asts::IdentifierAst>, scopes::TypeSymbol*, scopes::Scope*>>{};
  for (auto const &[sup_scope, sym] : CollectAttrSyms(type, scope)) {
    extended_syms.PushBack({sym->Name, sup_scope->GetTypeSymbol(sym->Type.get()), sup_scope});
  }

  return extended_syms;
}

auto spp::analyse::utils::type_members::CheckShadowedCmpAgreesInType(
  asts::CmpStatementAst const &cmp_member,
  scopes::Scope &cls_scope,
  scopes::Scope const &own_scope,
  scopes::ScopeManager const &sm)
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
    // way, not another declaration of this constant.
    const auto sym = declared.Symbol;
    if (sym->MemInfo->AstCompTime == nullptr) { continue; }
    if (sym->Type->IsCompilerGeneratedType()) { continue; }

    // If the type is inconsistent with the cmp statement being
    // checked then raise an error here.
    RaiseIf<SppSuperimpositionExtensionCmpStatementInvalidError>(
      not type_compare::TypeEq(*sym->Type, *cmp_member.Type, *declared.Where, own_scope, false),
      {declared.Where, sm.CurrentScope}, ERR_ARGS(cmp_member, *sym->Name));
  }
}

auto spp::analyse::utils::type_members::ClearUnimplementedAbstractMethodsCache()
  -> void {
  _UnimplementedAbstractMethodsCache().clear();
}

auto spp::analyse::utils::type_members::GetUnimplementedAbstractMethods(
  scopes::Scope const &type_scope)
  -> Vec<asts::FunctionPrototypeAst const*> {
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
  const auto generation = scopes::TypeStructureGeneration();
  if (const auto hit = cache.find(&type_scope); hit != cache.end() and hit->second.first == generation) {
    return hit->second.second;
  }

  // Skip on functional types because of the awkward difference with the base method having "args: Args" tuple of
  // args, and implementations having their own individual args.
  // Todo: Autopack and check?
  const auto remember = [&](Vec<asts::FunctionPrototypeAst const*> answer) {
    cache[&type_scope] = {generation, answer};
    return answer;
  };

  if (type_scope.TySym != nullptr) {
    if (type_scope.TySym->Name->IsCompilerGeneratedType()) { return remember({}); }
    if (const auto fq_name = type_scope.TySym->FqName(); fq_name != nullptr and type_predicates::IsTypeFunc(
      *fq_name, type_scope)) {
      return remember({});
    }
  }

  // Gather every method visible on the type, from the type's own scope and from all of its super scopes, each tagged
  // with the scope that resolves the types in its signature.
  auto all_scopes = Vec<scopes::Scope const*>{&type_scope};
  all_scopes.AppendRange(type_scope.SupScopes());

  auto methods = Vec<Pair<scopes::Scope const*, asts::FunctionPrototypeAst const*>>();
  for (auto const *scope : all_scopes) {
    if (scope->AstNode == nullptr) { continue; }

    const auto impl = AstAs<asts::ClassPrototypeAst>(scope->AstNode) == nullptr
      ? asts::AstBody(scope->AstNode)
      : Vec<asts::Ast*>{};
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
    spp::utils::InternedId, Vec<Pair<scopes::Scope const*, asts::FunctionPrototypeAst const*>>>();
  for (auto const &entry : methods) {
    if (entry.second->AbstractAnnotation != nullptr) { continue; }
    concrete_by_name[entry.second->Name->NameId()].EmplaceBack(entry);
  }

  auto unimplemented = Vec<asts::FunctionPrototypeAst const*>();
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
  asts::TypeAst const &type,
  scopes::Scope const &scope)
  -> Vec<asts::ClassAttributeAst*> {
  // Driven off the same walk as "GetAllAttrs" so the two line up index for index, then resolved to an ast by name
  // within the scope the symbol came from. Enumerating the prototype's members directly is what let the two lists
  // drift, because the member list has no notion of the generic symbols the other walk skips.
  auto attr_asts = Vec<asts::ClassAttributeAst*>{};
  for (auto const &[sup_scope, sym] : CollectAttrSyms(type, scope)) {
    const auto cls_proto = sup_scope->AstNode->ToUnchecked<asts::ClassPrototypeAst>();
    auto *found = static_cast<asts::ClassAttributeAst*>(nullptr);
    for (auto const &member : cls_proto->Impl->Members) {
      const auto attr = member->To<asts::ClassAttributeAst>();
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
  asts::TypeAst const &type_sym,
  asts::IdentifierAst const &field_name,
  scopes::Scope const &scope)
  -> std::size_t {
  // A class superimposing "Gen"/"GenOnce"/a "FunXXX" gets that interface's fat-pointer fields prepended ahead of
  // its own declared attributes (see "ClassPrototypeAst::FillLlvmLayout"), so an attribute's declared index has
  // to be shifted past them.
  const auto base = type_predicates::GetSuperimposedFatPointerFieldCount(type_sym, scope);

  // Get all the attributes on the type.
  const auto all_attrs = GetAllAttrs(type_sym, scope);

  // Find the field index.
  for (auto index = 0uz; index < all_attrs.Len(); ++index) {
    if (*spp::get<0>(all_attrs[index]) == field_name) {
      return base + index;
    }
  }

  return base + all_attrs.Len();
}
