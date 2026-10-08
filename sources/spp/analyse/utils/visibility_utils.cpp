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
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.visibility;
import genex;

namespace spp::analyse::utils::visibility_utils {
  namespace {
    /// [CHECKED]
    /// Internal helper to extract an error-printing name from
    /// the visibility enum.
    auto VisibilityName(
      const asts::utils::Visibility vis) -> Str {
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

    /// [CHECKED]
    /// Module-level visibility, which is one rule whatever the
    /// symbol is. Each level widens the one above it, so the
    /// checks are cumulative, not exclusive.
    /// - private: reaches the definition module.
    /// - protected: extends private to its descendants.
    /// - package: extends protected to the same top level module.
    /// - public: extends package to all modules.
    template <typename Symbol>
    auto IsModuleMemberVisibleImpl(
      Symbol const &sym, Scope const &definition_scope,
      ScopeManager const &sm, CompilerMetaData const &meta) -> bool {
      // Early return guard for visibility bypass or public
      // visibility.
      using V = asts::utils::Visibility;
      if (meta.IgnoreAccessModifierViolations) { return true; }
      if (sym.Visibility == V::kPublic) { return true; }

      // Get the module owning the access, and the module owning
      // the definition, by scope, for module comparisons.
      const auto accessing_module = sm.CurrentScope->GetParentModule();
      const auto definition_module = definition_scope.GetParentModule();

      // Private: the defining module only.
      const auto good_private = accessing_module == definition_module;
      if (sym.Visibility == V::kPrivate) { return good_private; }

      // Protected: and its descendant modules.
      const auto good_protected = good_private or genex::contains(
        accessing_module->GetAncestors(), definition_module);
      if (sym.Visibility == V::kProtected) { return good_protected; }

      // Package: and anything sharing a top-level module with it.
      return good_protected
        or accessing_module->GetTopLevelParentModule() == definition_module->GetTopLevelParentModule();
    }

    /// Type-level visibility, which is one rule whatever the
    /// member is (field, method, constant, nested type). Each
    /// level widens the one above it, so the checks are
    /// cumulative, not exclusive.
    /// - private: reaches the owning type in its own module.
    /// - protected: extends private to its subtypes in theirs.
    /// - package: extends protected to anything sharing a top-
    /// level module with the owner.
    /// - public: extends package to all types in all modules.
    template <typename Symbol>
    auto IsTypeMemberVisibleImpl(
      Symbol const &sym, Scope const &type_scope,
      ScopeManager const &sm, CompilerMetaData const &meta) -> bool {
      // Early return guard for visibility bypass or public
      // visibility.
      using V = asts::utils::Visibility;
      if (meta.IgnoreAccessModifierViolations) { return true; }
      if (sym.Visibility == V::kPublic) { return true; }

      // A member declared in a "sup" block belongs to the type
      // the block is over ("Self").
      auto const *owner_scope = &type_scope;
      if (AstAs<SupPrototypeFunctionsAst>(type_scope.AstNode) != nullptr
        or AstAs<SupPrototypeExtensionAst>(type_scope.AstNode) != nullptr) {
        if (const auto self_sym = type_scope.FindSelfSymbol(true);
          self_sym != nullptr and self_sym->LinkedScope != nullptr) { owner_scope = self_sym->LinkedScope->NonGnScope; }
      }

      // Get the module owning the access, and the module owning
      // the definition, by scope, for module comparisons.
      const auto accessing_module = sm.CurrentScope->GetParentModule();
      const auto definition_module = type_scope.GetParentModule();

      // The type the access is made from, read as the owner is:
      // what "Self" stands for there. A closure inherits the
      // "Self" of the method it is written in, so it reads as
      // that method's type.
      const auto accessing_self = sm.CurrentScope->FindSelfSymbol();
      const auto enclosing_scope = accessing_self != nullptr and accessing_self->LinkedScope != nullptr
        ? accessing_self->LinkedScope->NonGnScope
        : nullptr;

      // Private: only from the same type, in the same module.
      const auto good_private = enclosing_scope == owner_scope and accessing_module == definition_module;
      if (sym.Visibility == V::kPrivate) { return good_private; }

      // Protected: and from its subtypes, in the module each
      // was defined in.
      const auto good_protected = good_private or (enclosing_scope
        and genex::contains(enclosing_scope->GetSupScopes(), owner_scope)
        and accessing_module == enclosing_scope->GetParentModule());
      if (sym.Visibility == V::kProtected) { return good_protected; }

      // Package: and from any module in the same package.
      return good_protected
        or accessing_module->GetTopLevelParentModule() == definition_module->GetTopLevelParentModule();
    }

    /// Raises a unified error if a visibility checks fails.
    /// Sometimes, like for the LSP, we don't want to raise an
    /// error, so this isn't always called.
    template <typename Symbol>
    auto RaiseIfNotVisible(
      const bool visible, Symbol const &sym, Ast const &access_ast, Scope const &owner_scope,
      ScopeManager const &sm, char const *what) -> void {
      // Flip the "visible" flag and conditionally raise on it.
      // All the information passed into the function is unified
      // in usage from callers.
      using errors::SppAccessViolationError;
      RaiseIf<SppAccessViolationError>(
        not visible, {owner_scope.GetParentModule(), sm.CurrentScope},
        ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), what));
    }
  }
}

auto spp::analyse::utils::visibility_utils::IsTypeMemberVisible(
  VariableSymbol const &sym, Scope const &type_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> bool {
  return IsTypeMemberVisibleImpl(sym, type_scope, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckTypeMemberVisibility(
  VariableSymbol const &sym, Ast const &access_ast, Scope const &type_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> void {
  RaiseIfNotVisible(
    IsTypeMemberVisibleImpl(
      sym, type_scope, sm, meta), sym, access_ast, type_scope, sm, "symbol");
}

auto spp::analyse::utils::visibility_utils::IsTypeTypeVisible(
  TypeSymbol const &sym, Scope const &type_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> bool {
  return IsTypeMemberVisibleImpl(sym, type_scope, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckTypeTypeVisibility(
  TypeSymbol const &sym, Ast const &access_ast, Scope const &type_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> void {
  RaiseIfNotVisible(
    IsTypeMemberVisibleImpl(
      sym, type_scope, sm, meta), sym, access_ast, type_scope, sm, "type");
}

auto spp::analyse::utils::visibility_utils::IsModuleMemberVisible(
  VariableSymbol const &sym, Scope const &definition_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> bool {
  return IsModuleMemberVisibleImpl(
    sym, definition_scope, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckModuleMemberVisibility(
  VariableSymbol const &sym, Ast const &access_ast, Scope const &definition_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> void {
  RaiseIfNotVisible(
    IsModuleMemberVisibleImpl(
      sym, definition_scope, sm, meta), sym, access_ast, definition_scope, sm, "symbol");
}

auto spp::analyse::utils::visibility_utils::IsModuleTypeVisible(
  TypeSymbol const &sym, Scope const &definition_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> bool {
  return IsModuleMemberVisibleImpl(
    sym, definition_scope, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckModuleTypeVisibility(
  TypeSymbol const &sym, Ast const &access_ast, Scope const &definition_scope,
  ScopeManager const &sm, CompilerMetaData const &meta) -> void {
  RaiseIfNotVisible(
    IsModuleMemberVisibleImpl(
      sym, definition_scope, sm, meta), sym, access_ast, definition_scope, sm, "type");
}
