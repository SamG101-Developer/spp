module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.visibility_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.visibility;
import genex;

namespace spp::analyse::utils::visibility_utils {
  namespace {
    auto VisibilityName(
      const asts::utils::Visibility vis)
      -> Str {
      using V = asts::utils::Visibility;
      switch (vis) {
        case V::kPublic: return "public";
        case V::kPackage: return "package";
        case V::kProtected: return "protected";
        case V::kPrivate: return "private";
        default: std::unreachable();
      }
      std::unreachable();
    }

    /**
     * Module-level visibility, which is one rule whatever the symbol is: private reaches the defining module, protected
     * extends to its descendants, and package to anything sharing a top-level module. Each level widens the one above
     * it, so the checks are cumulative rather than exclusive.
     * @param sym The symbol whose visibility is being checked.
     * @param definition_scope The scope @p sym was defined in, whose module the access is measured from.
     * @param sm The scope manager, positioned at the accessing scope.
     * @param meta Associated metadata.
     * @return Whether the symbol can be named from where @p sm is positioned.
     */
    template <typename Sym>
    auto IsModuleMemberVisibleImpl(
      Sym const &sym,
      scopes::Scope const &definition_scope,
      scopes::ScopeManager const &sm,
      asts::meta::CompilerMetaData const &meta)
      -> bool {
      using V = asts::utils::Visibility;
      if (meta.IgnoreAccessModifierViolations) { return true; }
      if (sym.Visibility == V::kPublic) { return true; }

      const auto accessing_module = sm.CurrentScope->ParentModule();
      const auto definition_module = definition_scope.ParentModule();

      // Private: the defining module only.
      const auto good_private = accessing_module == definition_module;
      if (sym.Visibility == V::kPrivate) { return good_private; }

      // Protected: and its descendant modules.
      const auto good_protected = good_private or genex::contains(accessing_module->Ancestors(), definition_module);
      if (sym.Visibility == V::kProtected) { return good_protected; }

      // Package: and anything sharing a top-level module with it.
      return good_protected or accessing_module->TopLevelParentModule() == definition_module->TopLevelParentModule();
    }

    /**
     * Raise if @p sym cannot be named from where @p sm is positioned, by the module-level rule.
     * @param sym The symbol whose visibility is being checked.
     * @param access_ast The ast attempting the access, which the error is reported against.
     * @param definition_scope The scope @p sym was defined in, whose module the access is measured from.
     * @param sm The scope manager, positioned at the accessing scope.
     * @param meta Associated metadata.
     * @param what The noun the error names the symbol by.
     */
    template <typename Sym>
    auto CheckModuleMemberVisibilityImpl(
      Sym const &sym,
      asts::Ast const &access_ast,
      scopes::Scope const &definition_scope,
      scopes::ScopeManager const &sm,
      asts::meta::CompilerMetaData const &meta,
      char const *const what)
      -> void {
      using errors::SppAccessViolationError;
      RaiseIf<SppAccessViolationError>(
        not IsModuleMemberVisibleImpl(sym, definition_scope, sm, meta),
        {definition_scope.ParentModule(), sm.CurrentScope},
        ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), what));
    }

    /**
     * Type-level visibility, which is one rule whatever the member is (an attribute, a method, a constant or a nested
     * type): private reaches the owning type in its own module, protected extends to its subtypes in theirs, and package
     * to anything sharing a top-level module with the owner. Each level widens the one above it.
     * @param sym The member whose visibility is being checked.
     * @param type_scope The non-generic scope of the type the member belongs to.
     * @param sm The scope manager, positioned at the accessing scope.
     * @param meta Associated metadata.
     * @return Whether the member can be named from where @p sm is positioned.
     */
    template <typename Sym>
    auto IsTypeMemberVisibleImpl(
      Sym const &sym,
      scopes::Scope const &type_scope,
      scopes::ScopeManager const &sm,
      asts::meta::CompilerMetaData const &meta)
      -> bool {
      using V = asts::utils::Visibility;
      if (meta.IgnoreAccessModifierViolations) { return true; }
      if (sym.Visibility == V::kPublic) { return true; }

      const auto accessing_module = sm.CurrentScope->ParentModule();
      const auto definition_module = type_scope.ParentModule();
      auto enclosing_scope = sm.CurrentScope->GetEnclosingTypeScope(meta);
      enclosing_scope = enclosing_scope ? enclosing_scope->NonGenericScope : nullptr;

      // Private: only from the same type, in the same module.
      const auto good_private = enclosing_scope == &type_scope and accessing_module == definition_module;
      if (sym.Visibility == V::kPrivate) { return good_private; }

      // Protected: and from its subtypes, in the module each was defined in.
      const auto good_protected = good_private or (enclosing_scope and
        genex::contains(enclosing_scope->SupScopes(), &type_scope) and accessing_module == enclosing_scope->
        ParentModule());
      if (sym.Visibility == V::kProtected) { return good_protected; }

      // Package: and from any module in the same package.
      return good_protected or accessing_module->TopLevelParentModule() == definition_module->TopLevelParentModule();
    }

    /**
     * Raise if @p sym cannot be named from where @p sm is positioned, by the type-level rule.
     * @param sym The member whose visibility is being checked.
     * @param access_ast The ast attempting the access, which the error is reported against.
     * @param type_scope The non-generic scope of the type the member belongs to.
     * @param sm The scope manager, positioned at the accessing scope.
     * @param meta Associated metadata.
     * @param what The noun the error names the member by.
     */
    template <typename Sym>
    auto CheckTypeMemberVisibilityImpl(
      Sym const &sym,
      asts::Ast const &access_ast,
      scopes::Scope const &type_scope,
      scopes::ScopeManager const &sm,
      asts::meta::CompilerMetaData const &meta,
      char const *const what)
      -> void {
      using errors::SppAccessViolationError;
      RaiseIf<SppAccessViolationError>(
        not IsTypeMemberVisibleImpl(sym, type_scope, sm, meta),
        {type_scope.ParentModule(), sm.CurrentScope},
        ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), what));
    }
  }
}

auto spp::analyse::utils::visibility_utils::IsTypeMemberVisible(
  scopes::VariableSymbol const &sym,
  scopes::Scope const &type_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> bool {
  return IsTypeMemberVisibleImpl(sym, type_scope, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckTypeMemberVisibility(
  scopes::VariableSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &type_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  CheckTypeMemberVisibilityImpl(sym, access_ast, type_scope, sm, meta, "symbol");
}

auto spp::analyse::utils::visibility_utils::CheckTypeTypeVisibility(
  scopes::TypeSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &type_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  CheckTypeMemberVisibilityImpl(sym, access_ast, type_scope, sm, meta, "type");
}

auto spp::analyse::utils::visibility_utils::IsModuleMemberVisible(
  scopes::VariableSymbol const &sym,
  scopes::Scope const &definition_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> bool {
  return IsModuleMemberVisibleImpl(sym, definition_scope, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckModuleMemberVisibility(
  scopes::VariableSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &definition_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  CheckModuleMemberVisibilityImpl(sym, access_ast, definition_scope, sm, meta, "symbol");
}

auto spp::analyse::utils::visibility_utils::CheckModuleTypeVisibility(
  scopes::TypeSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &definition_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  CheckModuleMemberVisibilityImpl(sym, access_ast, definition_scope, sm, meta, "type");
}
