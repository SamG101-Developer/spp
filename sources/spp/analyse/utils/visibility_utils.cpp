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
import spp.utils.ice;
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
        default: Ice(std::format("unhandled visibility: {}", static_cast<int>(vis)));
      }
    }

    /// Whether a symbol is something a package declares, rather
    /// than a generic standing in one of its declarations (an
    /// alias's parameter), read wherever the compiler resolves it.
    auto IsPackageMember(VariableSymbol const &sym) -> bool {
      return sym.Kind != scopes::VariableKind::GnCompParam and sym.Kind != scopes::VariableKind::GnCompArg;
    }

    auto IsPackageMember(TypeSymbol const &sym) -> bool {
      return not sym.IsGn();
    }

    /// The ffi package a module member is in, when the module
    /// naming it is not its owner's: a dependency's package is
    /// for the dependency's own modules, which is how it is
    /// linked, not for the project's or another dependency's.
    template <typename Symbol>
    auto ForeignFfiPackage(
      Symbol const &sym, Scope const &definition_scope, ScopeManager const &sm) -> scopes::NamespaceSymbol const* {
      if (not IsPackageMember(sym)) { return nullptr; }
      auto const *const package_scope = definition_scope.GetTopLevelParentModule();
      auto const *const accessing_scope = sm.CurrentScope->GetTopLevelParentModule();
      if (package_scope == nullptr or accessing_scope == nullptr) { return nullptr; }

      auto const *const package = package_scope->LinkedNamespaceSymbol.get();
      auto const *const accessing = accessing_scope->LinkedNamespaceSymbol.get();
      if (package == nullptr or accessing == nullptr or not package->IsFfiPackage) { return nullptr; }
      return package->Owner == accessing->Owner ? nullptr : package;
    }

    /// Who an owner is, for an error: the project or a dependency.
    auto OwnerText(Opt<Str> const &owner) -> Str {
      return owner.has_value() ? "the dependency '" + *owner + "'" : Str("this project");
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
      if (ForeignFfiPackage(sym, definition_scope, sm) != nullptr) { return false; }
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

    /// The module a type member is defined in: a method's or
    /// attribute's own scope's, which a "sup" block over a type
    /// with no module of its own (a closure) still has; otherwise
    /// the type's.
    template <typename Symbol>
    auto DefinitionModuleOf(Symbol const &sym, Scope const &type_scope) -> Scope const* {
      if constexpr (std::same_as<Symbol, VariableSymbol>) {
        if (sym.ScopeDefinedIn != nullptr) {
          if (const auto module = sym.ScopeDefinedIn->GetParentModule(); module != nullptr) { return module; }
        }
      }
      return type_scope.GetParentModule();
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
          self_sym != nullptr and self_sym->LinkedScope != nullptr) {
          owner_scope = self_sym->LinkedScope->NonGnScope;
        }
      }

      // Get the module owning the access, and the module owning
      // the definition, by scope, for module comparisons.
      const auto accessing_module = sm.CurrentScope->GetParentModule();
      const auto definition_module = DefinitionModuleOf(sym, type_scope);

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
      const bool visible, Symbol const &sym, Ast const &access_ast, char const *what) -> void {
      // Flip the "visible" flag and conditionally raise on it.
      // All the information passed into the function is unified
      // in usage from callers.
      using errors::SppAccessViolationError;
      RaiseIf<SppAccessViolationError>(
        not visible,
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
      sym, type_scope, sm, meta), sym, access_ast, "symbol");
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
      sym, type_scope, sm, meta), sym, access_ast, "type");
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
  CheckFfiPackageOwner(sym, definition_scope, access_ast, sm, meta);
  RaiseIfNotVisible(
    IsModuleMemberVisibleImpl(
      sym, definition_scope, sm, meta), sym, access_ast, "symbol");
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
  CheckFfiPackageOwner(sym, definition_scope, access_ast, sm, meta);
  RaiseIfNotVisible(
    IsModuleMemberVisibleImpl(
      sym, definition_scope, sm, meta), sym, access_ast, "type");
}

namespace spp::analyse::utils::visibility_utils {
  namespace {
    /// Raises "SppFfiPackageNotOwnedError" for "CheckFfiPackageOwner".
    template <typename Symbol>
    auto CheckFfiPackageOwnerImpl(
      Symbol const &sym, Scope const &definition_scope, Ast const &access_ast,
      ScopeManager const &sm, CompilerMetaData const &meta) -> void {
      if (meta.IgnoreAccessModifierViolations) { return; }
      auto const *const package = ForeignFfiPackage(sym, definition_scope, sm);
      if (package == nullptr) { return; }

      // Held in locals so the views handed to the error outlive them.
      const auto package_name = package->Name->Val;
      const auto package_owner = OwnerText(package->Owner);
      const auto accessing_owner = OwnerText(
        sm.CurrentScope->GetTopLevelParentModule()->LinkedNamespaceSymbol->Owner);
      Raise<errors::SppFfiPackageNotOwnedError>(ERR_ARGS(
        access_ast, StrView(package_name), StrView(package_owner), StrView(accessing_owner)));
    }
  }
}

auto spp::analyse::utils::visibility_utils::CheckFfiPackageOwner(
  VariableSymbol const &sym, Scope const &definition_scope, Ast const &access_ast,
  ScopeManager const &sm, CompilerMetaData const &meta) -> void {
  CheckFfiPackageOwnerImpl(sym, definition_scope, access_ast, sm, meta);
}

auto spp::analyse::utils::visibility_utils::CheckFfiPackageOwner(
  TypeSymbol const &sym, Scope const &definition_scope, Ast const &access_ast,
  ScopeManager const &sm, CompilerMetaData const &meta) -> void {
  CheckFfiPackageOwnerImpl(sym, definition_scope, access_ast, sm, meta);
}
