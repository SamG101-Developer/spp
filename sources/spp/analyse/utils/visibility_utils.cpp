module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.visibility_utils;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.visibility;
import genex;

auto spp::analyse::utils::visibility_utils::VisibilityName(
    const asts::utils::Visibility vis)
    -> Str {
    using V = asts::utils::Visibility;
    switch (vis) {
        case V::kPublic: return "public";
        case V::kProtected: return "protected";
        case V::kPrivate: return "private";
        default:
            std::unreachable();
    }
    return "unknown";
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
    if (sym.Visibility == V::kPublic) { return; }

    const auto accessing_module = sm.CurrentScope->ParentModule();
    const auto definition_module = type_scope.ParentModule();
    auto enclosing_scope = sm.CurrentScope->GetEnclosingTypeScope(meta);
    enclosing_scope = enclosing_scope ? enclosing_scope->NonGenericScope : nullptr;

    // Private class member: only accessible from the same class, in the same module.
    const auto good_private = enclosing_scope == &type_scope and accessing_module == definition_module;
    RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kPrivate and not good_private,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), "symbol"));

    // Protected class member: only accessible from the same or subclass, in the module that class was defined in.
    const auto good_protected = good_private or (enclosing_scope and genex::contains(enclosing_scope->SupScopes(), &type_scope) and accessing_module == enclosing_scope->ParentModule());
    RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kProtected and not good_protected,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), "symbol"));

    // Package class member: accessible within any module that is in this package.
    const auto good_package = good_protected or accessing_module->TopLevelParentModule() == definition_module->TopLevelParentModule();
    RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kPackage and not good_package,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), "symbol"));
}

auto spp::analyse::utils::visibility_utils::CheckModuleMemberVisibility(
    scopes::VariableSymbol const &sym,
    asts::Ast const &access_ast,
    scopes::Scope const &definition_scope,
    scopes::ScopeManager const &sm)
    -> void {
    //
    using V = asts::utils::Visibility;
    using errors::SppAccessViolationError;
    if (sym.Visibility == V::kPublic) { return; }

    // Define the accessing module and the definition module.
    const auto accessing_module = sm.CurrentScope->ParentModule();
    const auto definition_module = definition_scope.ParentModule();

    // Private module member: only accessible from the same module.
    const auto good_private = accessing_module == definition_module;
    RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kPrivate and not good_private,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), "symbol"));

    // Protected module member: accessible from children modules.
    const auto good_protected = good_private or genex::contains(accessing_module->Ancestors(), definition_module);
    return RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kProtected and not good_protected,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), "symbol"));

    // Package module member: accessible within any module that is in the same package.
    const auto good_package = good_protected or accessing_module->TopLevelParentModule() == definition_module->TopLevelParentModule();
    RaiseIf<SppAccessViolationError>(
        sym.Visibility == V::kPackage and not good_package,
        {sm.CurrentScope, definition_module}, ERR_ARGS(access_ast, *sym.Name, VisibilityName(sym.Visibility), "symbol"));
}
