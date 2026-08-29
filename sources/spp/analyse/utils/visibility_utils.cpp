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
     * @param access_ast The ast attempting the access, which the error is reported against.
     * @param definition_scope The scope @p sym was defined in, whose module the access is measured from.
     * @param sm The scope manager, positioned at the accessing scope.
     * @param meta Associated metadata.
     * @param what The noun the error names the symbol by.
     */
    template <typename Sym>
    auto CheckModuleVisibility(
      Sym const &sym,
      asts::Ast const &access_ast,
      scopes::Scope const &definition_scope,
      scopes::ScopeManager const &sm,
      asts::meta::CompilerMetaData const &meta,
      char const *const what)
      -> void {
      using V = asts::utils::Visibility;
      using errors::SppAccessViolationError;
      if (meta.IgnoreAccessModifierViolations) { return; }
      if (sym.Visibility == V::kPublic) { return; }

      // Define the accessing module and the definition module.
      const auto accessing_module = sm.CurrentScope->ParentModule();
      const auto definition_module = definition_scope.ParentModule();
      const auto vis_name = VisibilityName(sym.Visibility);

      // Private: the defining module only.
      const auto good_private = accessing_module == definition_module;
      RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kPrivate and not good_private,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, vis_name, what));

      // Protected: and its descendant modules.
      const auto good_protected = good_private or genex::contains(accessing_module->Ancestors(), definition_module);
      RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kProtected and not good_protected,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, vis_name, what));

      // Package: and anything sharing a top-level module with it.
      const auto good_package = good_protected or accessing_module->TopLevelParentModule() == definition_module->
        TopLevelParentModule();
      RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kPackage and not good_package,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, vis_name, what));
    }
  }
}

auto spp::analyse::utils::visibility_utils::CheckTypeMemberVisibility(
  scopes::VariableSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &type_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  using V = asts::utils::Visibility;
  using errors::SppAccessViolationError;
  if (meta.IgnoreAccessModifierViolations) { return; }
  if (sym.Visibility == V::kPublic) { return; }

  const auto accessing_module = sm.CurrentScope->ParentModule();
  const auto definition_module = type_scope.ParentModule();
  auto enclosing_scope = sm.CurrentScope->GetEnclosingTypeScope(meta);
  enclosing_scope = enclosing_scope ? enclosing_scope->NonGenericScope : nullptr;
  const auto vis_name = VisibilityName(sym.Visibility);

  // Private class member: only accessible from the same class, in the same module.
  const auto good_private = enclosing_scope == &type_scope and accessing_module == definition_module;
  RaiseIf<SppAccessViolationError>(
    sym.Visibility == V::kPrivate and not good_private,
    {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, vis_name, "symbol"));

  // Protected class member: only accessible from the same or subclass, in the module that class was defined in.
  const auto good_protected = good_private or (enclosing_scope and
    genex::contains(enclosing_scope->SupScopes(), &type_scope) and accessing_module == enclosing_scope->ParentModule());
  RaiseIf<SppAccessViolationError>(
    sym.Visibility == V::kProtected and not good_protected,
    {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, vis_name, "symbol"));

  // Package class member: accessible within any module that is in this package.
  const auto good_package = good_protected or accessing_module->TopLevelParentModule() == definition_module->
    TopLevelParentModule();
  RaiseIf<SppAccessViolationError>(
    sym.Visibility == V::kPackage and not good_package,
    {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, vis_name, "symbol"));
}

auto spp::analyse::utils::visibility_utils::CheckModuleMemberVisibility(
  scopes::VariableSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &definition_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  CheckModuleVisibility(sym, access_ast, definition_scope, sm, meta, "symbol");
}

auto spp::analyse::utils::visibility_utils::CheckModuleTypeVisibility(
  scopes::TypeSymbol const &sym,
  asts::Ast const &access_ast,
  scopes::Scope const &definition_scope,
  scopes::ScopeManager const &sm,
  asts::meta::CompilerMetaData const &meta)
  -> void {
  CheckModuleVisibility(sym, access_ast, definition_scope, sm, meta, "type");
}
